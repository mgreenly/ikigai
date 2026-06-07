package session

import (
	"context"
	"errors"
	"testing"
)

// TestStore_SetTrigger_UpsertReplaces asserts the 1:1 contract: a second
// SetTrigger for the same session_id REPLACES the prior trigger rather than
// inserting a second row, and preserves created_at while advancing updated_at.
func TestStore_SetTrigger_UpsertReplaces(t *testing.T) {
	ctx := context.Background()
	store := newTestStore(t)
	sess := seedSession(t, store, "o@example.com", StatusIdle)

	if err := store.SetTrigger(ctx, Trigger{
		SessionID: sess.ID, TriggerEvent: "cron.nightly", MaxStalenessSecs: 300, MaxAttempts: 3,
	}); err != nil {
		t.Fatalf("first SetTrigger: %v", err)
	}
	first, err := store.GetTrigger(ctx, sess.ID)
	if err != nil {
		t.Fatalf("GetTrigger after first: %v", err)
	}

	// Replace with a different event + knobs.
	if err := store.SetTrigger(ctx, Trigger{
		SessionID: sess.ID, TriggerEvent: "cron.hourly", MaxStalenessSecs: 60, MaxAttempts: 5,
	}); err != nil {
		t.Fatalf("second SetTrigger: %v", err)
	}
	second, err := store.GetTrigger(ctx, sess.ID)
	if err != nil {
		t.Fatalf("GetTrigger after second: %v", err)
	}

	if second.TriggerEvent != "cron.hourly" || second.MaxStalenessSecs != 60 || second.MaxAttempts != 5 {
		t.Fatalf("replace did not take: %+v", second)
	}
	if second.CreatedAt != first.CreatedAt {
		t.Fatalf("created_at must be preserved across replace: %q -> %q", first.CreatedAt, second.CreatedAt)
	}

	// Exactly one row for the session (1:1).
	var n int
	if err := store.db.QueryRowContext(ctx,
		`SELECT COUNT(*) FROM session_triggers WHERE session_id = ?`, sess.ID,
	).Scan(&n); err != nil {
		t.Fatalf("count: %v", err)
	}
	if n != 1 {
		t.Fatalf("expected exactly one trigger row (1:1), got %d", n)
	}
}

// TestStore_TriggersForEvent_FanOut asserts the event→sessions fan-out returns
// every session that subscribed to a type and excludes others.
func TestStore_TriggersForEvent_FanOut(t *testing.T) {
	ctx := context.Background()
	store := newTestStore(t)
	a := seedSession(t, store, "o@example.com", StatusIdle)
	b := seedSession(t, store, "o@example.com", StatusIdle)
	c := seedSession(t, store, "o@example.com", StatusIdle)

	mustSet(t, store, a.ID, "cron.nightly")
	mustSet(t, store, b.ID, "cron.nightly")
	mustSet(t, store, c.ID, "cron.hourly")

	got, err := store.TriggersForEvent(ctx, "cron.nightly")
	if err != nil {
		t.Fatalf("TriggersForEvent: %v", err)
	}
	if len(got) != 2 {
		t.Fatalf("expected 2 triggers for cron.nightly, got %d", len(got))
	}
	for _, tr := range got {
		if tr.SessionID == c.ID {
			t.Fatalf("cron.hourly session leaked into cron.nightly fan-out")
		}
	}
}

// TestStore_ClearTrigger asserts clear removes the row and a no-match is
// ErrNotFound.
func TestStore_ClearTrigger(t *testing.T) {
	ctx := context.Background()
	store := newTestStore(t)
	sess := seedSession(t, store, "o@example.com", StatusIdle)
	mustSet(t, store, sess.ID, "cron.nightly")

	if err := store.ClearTrigger(ctx, sess.ID); err != nil {
		t.Fatalf("ClearTrigger: %v", err)
	}
	if _, err := store.GetTrigger(ctx, sess.ID); !errors.Is(err, ErrNotFound) {
		t.Fatalf("expected ErrNotFound after clear, got %v", err)
	}
	if err := store.ClearTrigger(ctx, sess.ID); !errors.Is(err, ErrNotFound) {
		t.Fatalf("expected ErrNotFound clearing absent trigger, got %v", err)
	}
}

func mustSet(t *testing.T, store *Store, sessionID, event string) {
	t.Helper()
	if err := store.SetTrigger(context.Background(), Trigger{
		SessionID: sessionID, TriggerEvent: event, MaxStalenessSecs: 300, MaxAttempts: 3,
	}); err != nil {
		t.Fatalf("SetTrigger: %v", err)
	}
}
