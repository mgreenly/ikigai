package tick

import (
	"bufio"
	"context"
	"database/sql"
	"encoding/json"
	"net/http"
	"net/http/httptest"
	"path/filepath"
	"strings"
	"testing"
	"time"

	appkitdb "appkit/db"

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
	conn, err := appkitdb.Open(filepath.Join(t.TempDir(), "test.db"))
	if err != nil {
		t.Fatalf("open: %v", err)
	}
	t.Cleanup(func() { conn.Close() })
	migs, err := appkitdb.LoadMigrations(db.FS, "migrations")
	if err != nil {
		t.Fatalf("load migrations: %v", err)
	}
	if err := appkitdb.Migrate(ctx, conn, migs); err != nil {
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
type outboxRow struct {
	Kind    string
	Subject string
	Payload event.Payload
}

// outboxRows reads every outbox row's routing address and payload, in seq order.
func outboxRows(t *testing.T, conn *sql.DB) []outboxRow {
	t.Helper()
	rows, err := conn.Query(`SELECT kind, subject, payload FROM outbox ORDER BY seq ASC`)
	if err != nil {
		t.Fatalf("query outbox: %v", err)
	}
	defer rows.Close()
	var out []outboxRow
	for rows.Next() {
		var row outboxRow
		var payload string
		if err := rows.Scan(&row.Kind, &row.Subject, &payload); err != nil {
			t.Fatalf("scan: %v", err)
		}
		if err := json.Unmarshal([]byte(payload), &row.Payload); err != nil {
			t.Fatalf("unmarshal payload: %v", err)
		}
		out = append(out, row)
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
	if rows[0].Payload.Name != "every" || rows[0].Payload.ScheduledFor != "2026-06-06T03:00:00Z" {
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
	if rows[0].Payload.ScheduledFor != "2026-06-06T03:00:00Z" || rows[1].Payload.ScheduledFor != "2026-06-06T03:01:00Z" {
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
	slot := time.Date(2026, 6, 6, 3, 0, 0, 0, time.UTC)       // minute 0

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
		names[r.Payload.Name] = true
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
	if rows[0].Payload.ScheduledFor != "2026-06-06T03:00:00Z" || rows[0].Payload.FiredAt != "2026-06-06T03:01:30Z" {
		t.Fatalf("scheduled_for/fired_at wrong: %+v", rows[0].Payload)
	}
}

// R-PQH6-2RYI
func TestFireUsesTickKindAndScheduleSubjectsAtomically(t *testing.T) {
	w, store, conn, ctx := harness(t)
	now := time.Now()
	mustCreate(t, store, ctx, now, "nightly", "* * * * *")
	mustCreate(t, store, ctx, now, "bill-sweep", "* * * * *")
	slot := time.Date(2026, 6, 6, 3, 0, 0, 0, time.UTC)

	if n, err := w.Fire(ctx, slot, slot); err != nil || n != 2 {
		t.Fatalf("first Fire = (%d, %v), want (2, nil)", n, err)
	}
	if n, err := w.Fire(ctx, slot, slot); err != nil || n != 0 {
		t.Fatalf("second Fire = (%d, %v), want (0, nil)", n, err)
	}
	rows := outboxRows(t, conn)
	if len(rows) != 2 {
		t.Fatalf("outbox rows = %d, want 2", len(rows))
	}
	subjects := map[string]bool{}
	for _, row := range rows {
		if row.Kind != event.Kind {
			t.Fatalf("outbox kind = %q, want %q", row.Kind, event.Kind)
		}
		subjects[row.Subject] = true
		entry, err := store.Get(ctx, row.Payload.Name)
		if err != nil || entry.LastSlot == nil || !tickSlotEqual(*entry.LastSlot, slot) {
			t.Fatalf("last_slot for %q was not committed with its outbox row: %+v, %v", row.Payload.Name, entry, err)
		}
	}
	if !subjects["/nightly"] || !subjects["/bill-sweep"] || len(subjects) != 2 {
		t.Fatalf("outbox subjects = %v, want /nightly and /bill-sweep", subjects)
	}
}

func tickSlotEqual(a, b time.Time) bool { return Slot(a).Equal(Slot(b)) }

// R-PU4V-836L
func TestFireServesCanonicalRoutingEnvelope(t *testing.T) {
	w, store, _, ctx := harness(t)
	mustCreate(t, store, ctx, time.Now(), "bill-sweep", "* * * * *")
	slot := time.Date(2026, 6, 6, 3, 0, 0, 0, time.UTC)
	if n, err := w.Fire(ctx, slot, slot); err != nil || n != 1 {
		t.Fatalf("Fire = (%d, %v), want (1, nil)", n, err)
	}
	srv := httptest.NewServer(w.ob.FeedHandler())
	defer srv.Close()
	req, err := http.NewRequestWithContext(ctx, http.MethodGet, srv.URL, nil)
	if err != nil {
		t.Fatal(err)
	}
	resp, err := http.DefaultClient.Do(req)
	if err != nil {
		t.Fatal(err)
	}
	defer resp.Body.Close()
	scanner := bufio.NewScanner(resp.Body)
	var frame []string
	for scanner.Scan() {
		line := scanner.Text()
		frame = append(frame, line)
		if line == "" && len(frame) > 1 {
			joined := strings.Join(frame, "\n")
			if strings.Contains(joined, "event: cron:tick/bill-sweep") {
				data := ""
				for _, field := range frame {
					if strings.HasPrefix(field, "data: ") {
						data = strings.TrimPrefix(field, "data: ")
						break
					}
				}
				var envelope map[string]json.RawMessage
				if err := json.Unmarshal([]byte(data), &envelope); err != nil {
					t.Fatalf("decode event envelope: %v", err)
				}
				if _, ok := envelope["type"]; ok || envelope["kind"] == nil || envelope["subject"] == nil {
					t.Fatalf("routing envelope = %s", data)
				}
				return
			}
			frame = nil
		}
	}
	t.Fatalf("did not receive cron routing frame: %v", scanner.Err())
}

func mustCreate(t *testing.T, s *crontab.Store, ctx context.Context, now time.Time, name, expr string) {
	t.Helper()
	if _, err := s.Create(ctx, name, expr, now); err != nil {
		t.Fatalf("create %q: %v", name, err)
	}
}
