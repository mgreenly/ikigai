package consume

import (
	"context"
	"encoding/json"
	"path/filepath"
	"sync"
	"testing"
	"time"

	"eventplane/consumer"

	"prompts/internal/db"
	"prompts/internal/prompt"
	"prompts/internal/sandbox"
)

// countingRunner is a prompt.Runner stub that records Spawn calls and never
// auto-completes a run, so a run stays 'running' after a fire.
type countingRunner struct {
	mu      sync.Mutex
	spawned int
}

func (r *countingRunner) Spawn(_ prompt.Run) {
	r.mu.Lock()
	r.spawned++
	r.mu.Unlock()
}
func (r *countingRunner) Cancel(_ string) bool { return false }
func (r *countingRunner) count() int {
	r.mu.Lock()
	defer r.mu.Unlock()
	return r.spawned
}

// TestSmoke_HandlerAgainstRealServiceAndDB wires the REAL consume.Handler to the
// REAL prompt.Service + Store over a temp SQLite DB, with a stubbed runner (no
// Claude). It asserts the multi-upstream fan-in end-to-end: a NON-cron (dropbox)
// event with a matching trigger fires the runner once, the prompt has a running
// run whose row carries the trigger context (source/type/event_id); and with
// full concurrency a second event for the same prompt fires a SECOND run.
func TestSmoke_HandlerAgainstRealServiceAndDB(t *testing.T) {
	t.Setenv("ANTHROPIC_API_KEY", "sk-test")
	ctx := context.Background()

	conn, err := db.Open(filepath.Join(t.TempDir(), "prompts.db"))
	if err != nil {
		t.Fatalf("db.Open: %v", err)
	}
	t.Cleanup(func() { conn.Close() })
	if err := db.Migrate(ctx, conn); err != nil {
		t.Fatalf("db.Migrate: %v", err)
	}
	runsDir := t.TempDir()
	sb, err := sandbox.New(runsDir)
	if err != nil {
		t.Fatalf("sandbox.New: %v", err)
	}
	store := prompt.NewStore(conn)
	cr := &countingRunner{}
	svc := prompt.NewService(store, sb, runsDir, cr)

	// A real prompt with a real NON-cron trigger.
	p, err := svc.Create(ctx, "owner@example.com", prompt.CreateInput{
		UserPrompt: "do the thing", Config: prompt.Config{Model: "haiku"},
	})
	if err != nil {
		t.Fatalf("Create: %v", err)
	}
	if _, err := svc.SetTrigger(ctx, "owner@example.com", p.ID, "dropbox", "file.created"); err != nil {
		t.Fatalf("SetTrigger: %v", err)
	}

	// Production wiring: fire = RunByEvent, lookup = PromptsForEvent, source baked
	// into the per-upstream Handler.
	fire := func(ctx context.Context, id, source, evType, eventID string, payload []byte) error {
		_, err := svc.RunByEvent(ctx, id, source, evType, eventID, payload)
		return err
	}
	h := Handler(fire, svc.PromptsForEvent, "dropbox", nil)

	ev := consumer.Event{Type: "file.created", ID: "e1", Source: "dropbox", Payload: json.RawMessage(`{"path":"/x"}`)}

	// First event: fires once, the prompt has a running run carrying the context.
	if err := h(ctx, ev); err != nil {
		t.Fatalf("handler #1: %v", err)
	}
	waitFor(t, func() bool { return cr.count() == 1 })

	detail, err := svc.Get(ctx, "owner@example.com", p.ID)
	if err != nil {
		t.Fatalf("Get: %v", err)
	}
	if detail.LastRun == nil || detail.LastRun.Status != prompt.RunRunning {
		t.Fatalf("expected a running run after fire, got %+v", detail.LastRun)
	}
	if lr := detail.LastRun; lr.TriggerSource != "dropbox" || lr.TriggerType != "file.created" || lr.TriggerEventID != "e1" {
		t.Fatalf("non-cron event must populate run trigger context, got %+v", lr)
	}

	// Second event for the same prompt: full concurrency → a SECOND run fires.
	if err := h(ctx, ev); err != nil {
		t.Fatalf("handler #2: %v", err)
	}
	waitFor(t, func() bool { return cr.count() == 2 })
}

func waitFor(t *testing.T, cond func() bool) {
	t.Helper()
	deadline := time.Now().Add(2 * time.Second)
	for time.Now().Before(deadline) {
		if cond() {
			return
		}
		time.Sleep(2 * time.Millisecond)
	}
	t.Fatal("condition not met within deadline")
}
