package tick

import (
	"context"
	"database/sql"
	"encoding/json"
	"path/filepath"
	"testing"
	"time"

	"cron/internal/crontab"
	"cron/internal/db"
	"cron/internal/event"

	"eventplane/outbox"
)

// harness stands up a real crontab store + outbox over a temp SQLite DB and the
// tick Worker over them. DBPath is left empty so outbox.New skips the §5.3
// concurrency probe (a single shared handle in a test).
func harness(t *testing.T) (*Worker, *crontab.Store, *sql.DB, context.Context) {
	t.Helper()
	ctx := context.Background()
	conn, err := db.Open(filepath.Join(t.TempDir(), "test.db"))
	if err != nil {
		t.Fatalf("open: %v", err)
	}
	t.Cleanup(func() { conn.Close() })
	if err := db.Migrate(ctx, conn); err != nil {
		t.Fatalf("migrate: %v", err)
	}
	store := crontab.NewStore(conn)
	ob, err := outbox.New(conn, outbox.Options{Source: "cron"})
	if err != nil {
		t.Fatalf("outbox.New: %v", err)
	}
	return New(conn, store, ob, nil), store, conn, ctx
}

// outboxRows reads every outbox row's type+payload, in seq order.
func outboxRows(t *testing.T, conn *sql.DB) []event.Payload {
	t.Helper()
	rows, err := conn.Query(`SELECT type, payload FROM outbox ORDER BY seq ASC`)
	if err != nil {
		t.Fatalf("query outbox: %v", err)
	}
	defer rows.Close()
	var out []event.Payload
	for rows.Next() {
		var typ, payload string
		if err := rows.Scan(&typ, &payload); err != nil {
			t.Fatalf("scan: %v", err)
		}
		var p event.Payload
		if err := json.Unmarshal([]byte(payload), &p); err != nil {
			t.Fatalf("unmarshal payload: %v", err)
		}
		out = append(out, p)
	}
	return out
}

// TestFire_AtMostOncePerSlot is the critical invariant: firing the SAME slot
// twice emits at most once per schedule (the slot-truncation + last_slot guard).
func TestFire_AtMostOncePerSlot(t *testing.T) {
	w, store, conn, ctx := harness(t)
	// Every-minute schedule so it matches any slot.
	if _, err := store.Create(ctx, "every", "* * * * *", time.Now()); err != nil {
		t.Fatalf("create: %v", err)
	}
	slot := time.Date(2026, 6, 6, 3, 0, 0, 0, time.UTC)

	n1, err := w.Fire(ctx, slot, slot)
	if err != nil {
		t.Fatalf("fire 1: %v", err)
	}
	n2, err := w.Fire(ctx, slot, slot) // same slot, restart-within-the-minute
	if err != nil {
		t.Fatalf("fire 2: %v", err)
	}
	if n1 != 1 || n2 != 0 {
		t.Fatalf("at-most-once violated: first fire=%d (want 1), second fire=%d (want 0)", n1, n2)
	}
	rows := outboxRows(t, conn)
	if len(rows) != 1 {
		t.Fatalf("want exactly 1 outbox row, got %d: %+v", len(rows), rows)
	}
	if rows[0].Name != "every" || rows[0].ScheduledFor != "2026-06-06T03:00:00Z" {
		t.Fatalf("payload wrong: %+v", rows[0])
	}
	// last_slot must be recorded.
	e, err := store.Get(ctx, "every")
	if err != nil {
		t.Fatalf("get: %v", err)
	}
	if e.LastSlot == nil || !Slot(*e.LastSlot).Equal(slot) {
		t.Fatalf("last_slot not recorded as %v: %+v", slot, e.LastSlot)
	}
}

// TestFire_NextSlotFiresAgain confirms the guard is per-slot, not permanent: a
// new matching slot fires again.
func TestFire_NextSlotFiresAgain(t *testing.T) {
	w, store, conn, ctx := harness(t)
	if _, err := store.Create(ctx, "every", "* * * * *", time.Now()); err != nil {
		t.Fatalf("create: %v", err)
	}
	slot1 := time.Date(2026, 6, 6, 3, 0, 0, 0, time.UTC)
	slot2 := slot1.Add(time.Minute)
	if _, err := w.Fire(ctx, slot1, slot1); err != nil {
		t.Fatalf("fire slot1: %v", err)
	}
	if _, err := w.Fire(ctx, slot2, slot2); err != nil {
		t.Fatalf("fire slot2: %v", err)
	}
	rows := outboxRows(t, conn)
	if len(rows) != 2 {
		t.Fatalf("want 2 rows (one per slot), got %d", len(rows))
	}
	if rows[0].ScheduledFor != "2026-06-06T03:00:00Z" || rows[1].ScheduledFor != "2026-06-06T03:01:00Z" {
		t.Fatalf("wrong slots in payloads: %+v", rows)
	}
}

// TestFire_MultipleSchedulesOneTick: several schedules matching one slot all fire
// in one tick, and a non-matching schedule does not.
func TestFire_MultipleSchedulesOneTick(t *testing.T) {
	w, store, conn, ctx := harness(t)
	now := time.Now()
	mustCreate(t, store, ctx, now, "every", "* * * * *")
	mustCreate(t, store, ctx, now, "top-of-hour", "0 * * * *")
	mustCreate(t, store, ctx, now, "never-now", "30 * * * *") // minute 30 only
	slot := time.Date(2026, 6, 6, 3, 0, 0, 0, time.UTC)        // minute 0

	n, err := w.Fire(ctx, slot, slot)
	if err != nil {
		t.Fatalf("fire: %v", err)
	}
	if n != 2 {
		t.Fatalf("want 2 fires (every + top-of-hour), got %d", n)
	}
	rows := outboxRows(t, conn)
	names := map[string]bool{}
	for _, r := range rows {
		names[r.Name] = true
	}
	if !names["every"] || !names["top-of-hour"] || names["never-now"] || len(rows) != 2 {
		t.Fatalf("wrong set fired: %+v", rows)
	}
}

// TestFire_FiredAtDivergesFromSlot: scheduled_for is the slot, fired_at is the
// actual emit time (they diverge after a restart blink).
func TestFire_FiredAtDivergesFromSlot(t *testing.T) {
	w, store, conn, ctx := harness(t)
	mustCreate(t, store, ctx, time.Now(), "every", "* * * * *")
	slot := time.Date(2026, 6, 6, 3, 0, 0, 0, time.UTC)
	firedAt := slot.Add(90 * time.Second) // a restart-late emit
	if _, err := w.Fire(ctx, slot, firedAt); err != nil {
		t.Fatalf("fire: %v", err)
	}
	rows := outboxRows(t, conn)
	if len(rows) != 1 {
		t.Fatalf("want 1 row, got %d", len(rows))
	}
	if rows[0].ScheduledFor != "2026-06-06T03:00:00Z" || rows[0].FiredAt != "2026-06-06T03:01:30Z" {
		t.Fatalf("scheduled_for/fired_at wrong: %+v", rows[0])
	}
}

func mustCreate(t *testing.T, s *crontab.Store, ctx context.Context, now time.Time, name, expr string) {
	t.Helper()
	if _, err := s.Create(ctx, name, expr, now); err != nil {
		t.Fatalf("create %q: %v", name, err)
	}
}
