package runner

import (
	"context"
	"encoding/json"
	"os"
	"path/filepath"
	"strings"
	"sync"
	"testing"
	"time"

	"agentkit/agent"
	"agentkit/provider"
	"agentkit/wire"
	"prompts/internal/db"
	"prompts/internal/ids"
	"prompts/internal/prompt"
	"prompts/internal/sandbox"
)

// fakeToolSource is a minimal agent.ToolSource that owns no tools — enough to
// be threaded into agent.Run without changing the run's behavior, while letting
// tests observe that the discover seam was invoked with the run's identity.
type fakeToolSource struct{}

func (fakeToolSource) Descriptors() []provider.Tool { return nil }
func (fakeToolSource) Owns(string) bool             { return false }
func (fakeToolSource) Dispatch(context.Context, string, json.RawMessage) (wire.ToolResultBlock, error) {
	return wire.ToolResultBlock{}, nil
}

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
func newTestRunner(t *testing.T, ttl time.Duration, fc provider.Client) (*Runner, *prompt.Store) {
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
	store := prompt.NewStore(conn)

	sb, err := sandbox.New(filepath.Join(t.TempDir(), "sandboxes"))
	if err != nil {
		t.Fatalf("sandbox.New: %v", err)
	}

	r := New(store, sb, ttl, t.TempDir())
	r.newClient = func(apiKey, model string) (provider.Client, error) { return fc, nil }
	return r, store
}

// seedRunning inserts a prompt, makes a run-scoped sandbox, materializes the
// run's input/ on disk (what the runner now reads from), then opens a running
// run on it — mirroring Service.Run, so the runner can take it terminal.
func seedRunning(t *testing.T, store *prompt.Store, sb *sandbox.Manager, runsDir string) (prompt.Prompt, prompt.Run) {
	t.Helper()
	ctx := context.Background()
	now := time.Now().UTC().Format(time.RFC3339Nano)
	sess := prompt.Prompt{
		ID:         ids.NewULID(),
		OwnerEmail: "owner@example.com",
		Name:       "n",
		UserPrompt: "do the thing",
		Config:     prompt.Config{Provider: "anthropic", Model: "claude-haiku-4-5"},
		CreatedAt:  now,
		UpdatedAt:  now,
	}
	if err := store.InsertPrompt(ctx, sess); err != nil {
		t.Fatalf("InsertPrompt: %v", err)
	}
	runID := ids.NewULID()
	// Per-run sandbox is keyed by run_id (runs/<run_id>/sandbox).
	if err := sb.Create(runID); err != nil {
		t.Fatalf("sandbox.Create: %v", err)
	}
	run := prompt.Run{
		ID:         runID,
		PromptID:   sess.ID,
		OwnerEmail: sess.OwnerEmail,
		PromptName: sess.Name,
		Status:     prompt.RunRunning,
		StartedAt:  now,
		LogPath:    filepath.Join(runsDir, runID, "output.jsonl"),
	}
	writeRunInput(t, runsDir, runID, sess.UserPrompt, sess.SystemPrompt, sess.Config)
	if err := store.InsertRun(ctx, run); err != nil {
		t.Fatalf("InsertRun: %v", err)
	}
	return sess, run
}

// writeRunInput pins a run's execution inputs to runs/<run_id>/input/, the
// disk source the runner reads (mirrors Service.materializeInput).
func writeRunInput(t *testing.T, runsDir, runID, userPrompt, sysPrompt string, cfg prompt.Config) {
	t.Helper()
	inputDir := filepath.Join(runsDir, runID, "input")
	if err := os.MkdirAll(inputDir, 0o755); err != nil {
		t.Fatalf("mkdir input: %v", err)
	}
	if err := os.WriteFile(filepath.Join(inputDir, "user_prompt.txt"), []byte(userPrompt), 0o644); err != nil {
		t.Fatalf("write user_prompt: %v", err)
	}
	if err := os.WriteFile(filepath.Join(inputDir, "system_prompt.txt"), []byte(sysPrompt), 0o644); err != nil {
		t.Fatalf("write system_prompt: %v", err)
	}
	cfgJSON, err := json.Marshal(cfg)
	if err != nil {
		t.Fatalf("marshal config: %v", err)
	}
	if err := os.WriteFile(filepath.Join(inputDir, "config.json"), cfgJSON, 0o644); err != nil {
		t.Fatalf("write config: %v", err)
	}
}

// waitRun polls the store until the run reaches a terminal status or the
// deadline passes. Returns the final run row.
func waitRun(t *testing.T, store *prompt.Store, sessionID string) prompt.Run {
	t.Helper()
	deadline := time.Now().Add(5 * time.Second)
	for time.Now().Before(deadline) {
		run, err := store.GetLatestRun(context.Background(), sessionID)
		if err != nil {
			t.Fatalf("GetLatestRun: %v", err)
		}
		if run != nil && run.Status != prompt.RunRunning {
			return *run
		}
		time.Sleep(5 * time.Millisecond)
	}
	t.Fatalf("run for session %s did not reach a terminal state", sessionID)
	return prompt.Run{}
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

	r.Spawn(run)
	got := waitRun(t, store, sess.ID)

	if got.Status != prompt.RunSucceeded {
		t.Fatalf("run status = %q, want succeeded (error=%q)", got.Status, got.Error)
	}
	if got.Error != "" {
		t.Fatalf("run error = %q, want empty", got.Error)
	}
	if got.EndedAt == "" {
		t.Fatalf("run ended_at empty")
	}

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

// TestSpawn_DiscoversSuiteTools asserts the runner builds an in-run suite
// ToolSource at spawn via the injectable discover seam, calling it with the
// run's OwnerEmail/PromptID, and that the resulting source is threaded into the
// engine (the run completes successfully with the fake source wired). It reuses
// the fake-client seam so no real Anthropic call is made.
func TestSpawn_DiscoversSuiteTools(t *testing.T) {
	fc := &fakeClient{events: []provider.Event{
		provider.EventTextDelta{Text: "ok"},
		provider.EventDone{StopReason: "end_turn"},
	}}
	runsDir := t.TempDir()
	r, store := newTestRunner(t, time.Minute, fc)
	sess, run := seedRunning(t, store, r.sandbox, runsDir)

	var (
		mu          sync.Mutex
		calls       int
		gotOwner    string
		gotPromptID string
	)
	r.discover = func(ctx context.Context, owner, promptID string) agent.ToolSource {
		mu.Lock()
		calls++
		gotOwner = owner
		gotPromptID = promptID
		mu.Unlock()
		return fakeToolSource{}
	}

	r.Spawn(run)
	got := waitRun(t, store, sess.ID)

	if got.Status != prompt.RunSucceeded {
		t.Fatalf("run status = %q, want succeeded (error=%q)", got.Status, got.Error)
	}

	mu.Lock()
	defer mu.Unlock()
	if calls != 1 {
		t.Fatalf("discover seam called %d times, want exactly 1", calls)
	}
	if gotOwner != run.OwnerEmail {
		t.Fatalf("discover owner = %q, want %q", gotOwner, run.OwnerEmail)
	}
	if gotPromptID != run.PromptID {
		t.Fatalf("discover promptID = %q, want %q", gotPromptID, run.PromptID)
	}
}

// TestNew_DefaultDiscoverWired confirms the default construction (no seam
// override) installs a working discover closure over the configured
// manifestRoot — a smoke assertion that the default path is wired and returns a
// non-nil ToolSource (suite.Discover's best-effort contract) without standing up
// real peers.
func TestNew_DefaultDiscoverWired(t *testing.T) {
	ctx := context.Background()
	conn, err := db.Open(filepath.Join(t.TempDir(), "prompts.db"))
	if err != nil {
		t.Fatalf("db.Open: %v", err)
	}
	t.Cleanup(func() { conn.Close() })
	if err := db.Migrate(ctx, conn); err != nil {
		t.Fatalf("db.Migrate: %v", err)
	}
	sb, err := sandbox.New(filepath.Join(t.TempDir(), "sandboxes"))
	if err != nil {
		t.Fatalf("sandbox.New: %v", err)
	}

	r := New(prompt.NewStore(conn), sb, time.Minute, t.TempDir())
	if r.discover == nil {
		t.Fatalf("New left discover seam nil")
	}
	if src := r.discover(ctx, "owner@example.com", "p_123"); src == nil {
		t.Fatalf("default discover returned nil ToolSource; want non-nil (best-effort contract)")
	}
}

func TestCancel(t *testing.T) {
	fc := &fakeClient{block: true}
	runsDir := t.TempDir()
	r, store := newTestRunner(t, time.Minute, fc)
	sess, run := seedRunning(t, store, r.sandbox, runsDir)

	r.Spawn(run)

	// Wait until the run goroutine has registered its cancel func, then cancel
	// by run_id.
	var cancelled bool
	deadline := time.Now().Add(2 * time.Second)
	for time.Now().Before(deadline) {
		if r.Cancel(run.ID) {
			cancelled = true
			break
		}
		time.Sleep(2 * time.Millisecond)
	}
	if !cancelled {
		t.Fatalf("Cancel never returned true")
	}

	got := waitRun(t, store, sess.ID)
	if got.Status != prompt.RunCancelled {
		t.Fatalf("run status = %q, want cancelled", got.Status)
	}
	if got.Error != "cancelled" {
		t.Fatalf("run error = %q, want \"cancelled\"", got.Error)
	}

	// Cancelling an absent run returns false.
	if r.Cancel("no-such-run") {
		t.Fatalf("Cancel of absent run returned true")
	}
}

func TestTTLFires(t *testing.T) {
	fc := &fakeClient{block: true}
	runsDir := t.TempDir()
	r, store := newTestRunner(t, 50*time.Millisecond, fc)
	sess, run := seedRunning(t, store, r.sandbox, runsDir)

	r.Spawn(run)
	got := waitRun(t, store, sess.ID)

	if got.Status != prompt.RunFailed {
		t.Fatalf("run status = %q, want failed", got.Status)
	}
	if got.Error != "run TTL exceeded" {
		t.Fatalf("run error = %q, want \"run TTL exceeded\"", got.Error)
	}
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
	if run == nil || run.Status != prompt.RunFailed {
		t.Fatalf("swept run status = %v, want failed", run)
	}
}
