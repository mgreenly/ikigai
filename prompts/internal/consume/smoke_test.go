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
	"prompts/internal/sandbox"
	"prompts/internal/session"
)

// countingRunner is a session.Runner stub that records Spawn calls and never
// auto-completes a run, so a session stays 'running' after a fire (exercising the
// real session.status serialization guard).
type countingRunner struct {
	mu      sync.Mutex
	spawned int
}

func (r *countingRunner) Spawn(_ session.Session, _ session.Run) {
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
// REAL session.Service + Store over a temp SQLite DB, with a stubbed runner (no
// Claude). It asserts: a cron event with a matching trigger fires the runner once
// and flips the session to running; a second event for the now-running session is
// skipped by the status serialization guard (no second spawn). This is the local
// smoke described in the P7 verification.
func TestSmoke_HandlerAgainstRealServiceAndDB(t *testing.T) {
	t.Setenv("ANTHROPIC_API_KEY", "sk-test")
	ctx := context.Background()

	conn, err := db.Open(filepath.Join(t.TempDir(), "agent.db"))
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
	store := session.NewStore(conn)
	cr := &countingRunner{}
	svc := session.NewService(store, sb, t.TempDir(), cr)

	// A real session with a real trigger.
	sess, err := svc.Create(ctx, "owner@example.com", session.CreateInput{
		Prompt: "do the thing", Config: session.Config{Model: "haiku"},
	})
	if err != nil {
		t.Fatalf("Create: %v", err)
	}
	if _, err := svc.SetTrigger(ctx, "owner@example.com", sess.ID, session.SetTriggerInput{
		TriggerEvent: "cron.nightly",
	}); err != nil {
		t.Fatalf("SetTrigger: %v", err)
	}

	// Production wiring: fire = RunByID, lookup = TriggersForEvent.
	fire := func(ctx context.Context, id, triggerEvent, scheduledFor string) error {
		_, err := svc.RunByID(ctx, id, triggerEvent, scheduledFor)
		return err
	}
	h := Handler(fire, svc.TriggersForEvent, nil)

	payload, _ := json.Marshal(cronPayload{
		Name: "nightly", ScheduledFor: time.Now().UTC().Format(time.RFC3339),
	})
	ev := consumer.Event{Type: "cron.nightly", ID: "e1", Source: "cron", Payload: payload}

	// First event: fires once, session → running.
	if err := h(ctx, ev); err != nil {
		t.Fatalf("handler #1: %v", err)
	}
	waitFor(t, func() bool { return cr.count() == 1 })

	detail, err := svc.Get(ctx, "owner@example.com", sess.ID)
	if err != nil {
		t.Fatalf("Get: %v", err)
	}
	if detail.Status != session.StatusRunning {
		t.Fatalf("expected session running after fire, got %q", detail.Status)
	}

	// Second event while running: staleness-fresh but BUSY → no second spawn.
	if err := h(ctx, ev); err != nil {
		t.Fatalf("handler #2: %v", err)
	}
	time.Sleep(30 * time.Millisecond) // allow any (erroneous) spawn to land
	if cr.count() != 1 {
		t.Fatalf("busy session must not be re-fired; spawn count = %d", cr.count())
	}
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
