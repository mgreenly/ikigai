package runner

import (
	"context"
	"os"
	"path/filepath"
	"strings"
	"testing"
	"time"

	"prompts/internal/db"
	"agentkit/provider"
	"prompts/internal/ids"
	"prompts/internal/sandbox"
	"prompts/internal/session"
)

// fakeClient is a provider.Client whose Stream returns a pre-canned sequence
// of events. If block is true, Stream emits nothing and instead blocks until
// the context is cancelled, then closes the channel — modelling a hung run.
type fakeClient struct {
	events []provider.Event
	block  bool
}

func (f *fakeClient) Stream(ctx context.Context, req provider.Request) (<-chan provider.Event, error) {
	ch := make(chan provider.Event)
	go func() {
		defer close(ch)
		if f.block {
			<-ctx.Done()
			return
		}
		for _, ev := range f.events {
			select {
			case ch <- ev:
			case <-ctx.Done():
				return
			}
		}
	}()
	return ch, nil
}

// newTestRunner builds a Runner backed by a real temp store + sandbox, with
// the client factory replaced by one that always returns fc.
func newTestRunner(t *testing.T, ttl time.Duration, fc provider.Client) (*Runner, *session.Store) {
	t.Helper()
	ctx := context.Background()
	conn, err := db.Open(filepath.Join(t.TempDir(), "prompts.db"))
	if err != nil {
		t.Fatalf("db.Open: %v", err)
	}
	t.Cleanup(func() { conn.Close() })
	if err := db.Migrate(ctx, conn); err != nil {
		t.Fatalf("db.Migrate: %v", err)
	}
	store := session.NewStore(conn)

	sb, err := sandbox.New(filepath.Join(t.TempDir(), "sandboxes"))
	if err != nil {
		t.Fatalf("sandbox.New: %v", err)
	}

	r := New(store, sb, ttl)
	r.newClient = func(apiKey, model string) (provider.Client, error) { return fc, nil }
	return r, store
}

// seedRunning inserts an idle session, makes its sandbox, then opens a running
// run on it and flips the session to running — mirroring Service.Run, so the
// runner can take it terminal.
func seedRunning(t *testing.T, store *session.Store, sb *sandbox.Manager, runsDir string) (session.Session, session.Run) {
	t.Helper()
	ctx := context.Background()
	now := time.Now().UTC().Format(time.RFC3339Nano)
	sess := session.Session{
		ID:         ids.NewULID(),
		OwnerEmail: "owner@example.com",
		Name:       "n",
		Prompt:     "do the thing",
		Config:     session.Config{Provider: "anthropic", Model: "haiku"},
		Status:     session.StatusRunning,
		CreatedAt:  now,
		UpdatedAt:  now,
	}
	if err := store.InsertSession(ctx, sess); err != nil {
		t.Fatalf("InsertSession: %v", err)
	}
	if err := sb.Create(sess.ID); err != nil {
		t.Fatalf("sandbox.Create: %v", err)
	}
	runID := ids.NewULID()
	run := session.Run{
		ID:        runID,
		SessionID: sess.ID,
		Status:    session.RunRunning,
		StartedAt: now,
		LogPath:   filepath.Join(runsDir, sess.ID, runID+".jsonl"),
	}
	if err := store.InsertRun(ctx, run); err != nil {
		t.Fatalf("InsertRun: %v", err)
	}
	return sess, run
}

// waitRun polls the store until the run reaches a terminal status or the
// deadline passes. Returns the final run row.
func waitRun(t *testing.T, store *session.Store, sessionID string) session.Run {
	t.Helper()
	deadline := time.Now().Add(5 * time.Second)
	for time.Now().Before(deadline) {
		run, err := store.GetLatestRun(context.Background(), sessionID)
		if err != nil {
			t.Fatalf("GetLatestRun: %v", err)
		}
		if run != nil && run.Status != session.RunRunning {
			return *run
		}
		time.Sleep(5 * time.Millisecond)
	}
	t.Fatalf("run for session %s did not reach a terminal state", sessionID)
	return session.Run{}
}

func assertSessionIdle(t *testing.T, store *session.Store, owner, id string) {
	t.Helper()
	sess, err := store.GetSession(context.Background(), owner, id)
	if err != nil {
		t.Fatalf("GetSession: %v", err)
	}
	if sess.Status != session.StatusIdle {
		t.Fatalf("session status = %q, want idle", sess.Status)
	}
}

func TestSpawn_TerminalSuccess(t *testing.T) {
	fc := &fakeClient{events: []provider.Event{
		provider.EventTextDelta{Text: "all done"},
		provider.EventUsage{InputTokens: 12, OutputTokens: 7},
		provider.EventDone{StopReason: "end_turn"},
	}}
	runsDir := t.TempDir()
	r, store := newTestRunner(t, time.Minute, fc)
	sess, run := seedRunning(t, store, r.sandbox, runsDir)

	r.Spawn(sess, run)
	got := waitRun(t, store, sess.ID)

	if got.Status != session.RunSucceeded {
		t.Fatalf("run status = %q, want succeeded (error=%q)", got.Status, got.Error)
	}
	if got.Error != "" {
		t.Fatalf("run error = %q, want empty", got.Error)
	}
	if got.EndedAt == "" {
		t.Fatalf("run ended_at empty")
	}
	assertSessionIdle(t, store, sess.OwnerEmail, sess.ID)

	data, err := os.ReadFile(run.LogPath)
	if err != nil {
		t.Fatalf("read log: %v", err)
	}
	logStr := string(data)
	if !strings.Contains(logStr, "all done") {
		t.Fatalf("log missing emitted assistant text: %s", logStr)
	}
	if !strings.Contains(logStr, `"type":"result"`) {
		t.Fatalf("log missing result event: %s", logStr)
	}
	if got.UsageJSON == "" {
		t.Fatalf("usage_json empty; want captured usage")
	}
	if !strings.Contains(got.UsageJSON, "usage") {
		t.Fatalf("usage_json = %q, want usage totals", got.UsageJSON)
	}
}

func TestCancel(t *testing.T) {
	fc := &fakeClient{block: true}
	runsDir := t.TempDir()
	r, store := newTestRunner(t, time.Minute, fc)
	sess, run := seedRunning(t, store, r.sandbox, runsDir)

	r.Spawn(sess, run)

	// Wait until the run goroutine has registered its cancel func, then cancel.
	var cancelled bool
	deadline := time.Now().Add(2 * time.Second)
	for time.Now().Before(deadline) {
		if r.Cancel(sess.ID) {
			cancelled = true
			break
		}
		time.Sleep(2 * time.Millisecond)
	}
	if !cancelled {
		t.Fatalf("Cancel never returned true")
	}

	got := waitRun(t, store, sess.ID)
	if got.Status != session.RunCancelled {
		t.Fatalf("run status = %q, want cancelled", got.Status)
	}
	if got.Error != "cancelled" {
		t.Fatalf("run error = %q, want \"cancelled\"", got.Error)
	}
	assertSessionIdle(t, store, sess.OwnerEmail, sess.ID)

	// Cancelling an absent session returns false.
	if r.Cancel("no-such-session") {
		t.Fatalf("Cancel of absent session returned true")
	}
}

func TestTTLFires(t *testing.T) {
	fc := &fakeClient{block: true}
	runsDir := t.TempDir()
	r, store := newTestRunner(t, 50*time.Millisecond, fc)
	sess, run := seedRunning(t, store, r.sandbox, runsDir)

	r.Spawn(sess, run)
	got := waitRun(t, store, sess.ID)

	if got.Status != session.RunFailed {
		t.Fatalf("run status = %q, want failed", got.Status)
	}
	if got.Error != "run TTL exceeded" {
		t.Fatalf("run error = %q, want \"run TTL exceeded\"", got.Error)
	}
	assertSessionIdle(t, store, sess.OwnerEmail, sess.ID)
}

func TestRecover(t *testing.T) {
	fc := &fakeClient{} // unused; Recover does not run the engine
	runsDir := t.TempDir()
	r, store := newTestRunner(t, time.Minute, fc)
	// Seed a running session+run but never spawn it — it is an orphan.
	sess, _ := seedRunning(t, store, r.sandbox, runsDir)

	n, err := r.Recover(context.Background())
	if err != nil {
		t.Fatalf("Recover: %v", err)
	}
	if n < 1 {
		t.Fatalf("Recover swept %d runs, want >= 1", n)
	}

	run, err := store.GetLatestRun(context.Background(), sess.ID)
	if err != nil {
		t.Fatalf("GetLatestRun: %v", err)
	}
	if run == nil || run.Status != session.RunFailed {
		t.Fatalf("swept run status = %v, want failed", run)
	}
	assertSessionIdle(t, store, sess.OwnerEmail, sess.ID)
}
