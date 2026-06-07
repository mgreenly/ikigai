// Package tick is cron's minute-aligned firing worker (event-triggering
// decisions §2 "cron implementation detail"). It wakes aligned to the wall-clock
// minute boundary, computes the current slot = now.Truncate(minute) in UTC, and
// for every crontab row whose schedule matches the slot AND whose last_slot is
// not already this slot, emits one cron.<name> event.
//
// The at-most-once-per-(schedule, slot) guarantee — the critical invariant — is
// the product of two things done together: (1) the slot is minute-truncated, so
// the same wall-clock minute always yields the same slot value; (2) each fire is
// one per-schedule transaction that BOTH Appends the outbox event AND writes
// last_slot = slot, so "emitted" and "recorded as emitted" commit atomically. A
// second tick for the same slot (a restart-within-the-minute) finds last_slot
// already equal and skips. A single Ring() after the scan wakes the feed.
//
// The core tick logic (Fire) takes an explicit slot time so it is unit-testable
// against a fixed clock; only the Run loop touches the wall clock.
package tick

import (
	"context"
	"database/sql"
	"fmt"
	"log/slog"
	"time"

	"cron/internal/cron"
	"cron/internal/crontab"
	"cron/internal/event"

	"eventplane/outbox"
)

// slotFormat is how a slot is stored in crontab.last_slot: minute-truncated UTC
// RFC3339. It must round-trip exactly so the string compare in the guard holds.
const slotFormat = time.RFC3339

// Worker owns the firing loop. It holds the chassis single-writer DB handle (for
// the per-schedule transactions), the crontab store (to enumerate schedules),
// the outbox (Append + Ring), and a logger.
type Worker struct {
	db    *sql.DB
	store *crontab.Store
	ob    *outbox.Outbox
	log   *slog.Logger
}

// New builds a tick Worker. db is the shared appkit DB handle; ob is the
// producer outbox injected via Spec.Producer.
func New(db *sql.DB, store *crontab.Store, ob *outbox.Outbox, logger *slog.Logger) *Worker {
	if logger == nil {
		logger = slog.Default()
	}
	return &Worker{db: db, store: store, ob: ob, log: logger}
}

// Slot truncates t to its minute in UTC — the canonical slot value for a given
// wall-clock minute.
func Slot(t time.Time) time.Time { return t.UTC().Truncate(time.Minute) }

// Run drives the worker for the serve lifetime: it sleeps to the next wall-clock
// minute boundary, fires that slot, and repeats until ctx is cancelled. It is a
// Spec.Workers entry; a returned error (only on ctx cancel → nil) unwinds the
// server alongside it.
func (w *Worker) Run(ctx context.Context) error {
	for {
		next := nextMinute(time.Now())
		select {
		case <-ctx.Done():
			return nil
		case <-time.After(time.Until(next)):
		}
		slot := Slot(time.Now())
		if n, err := w.Fire(ctx, slot, time.Now()); err != nil {
			w.log.Error("cron tick failed", "slot", slot.Format(slotFormat), "error", err)
		} else if n > 0 {
			w.log.Info("cron tick fired", "slot", slot.Format(slotFormat), "count", n)
		}
	}
}

// Fire is the testable core of one tick. For the given slot it scans every
// crontab row, and for each row whose parsed schedule matches the slot and whose
// last_slot != slot, runs ONE per-schedule transaction: Append the cron.<name>
// event AND UPDATE last_slot = slot (atomic "emitted" + "recorded"). It calls
// Ring() once after the scan if anything fired, and returns the number of events
// emitted. firedAt is the actual emit time stamped into the payload; pass the
// same value as wall-clock now in production, an explicit value in tests.
//
// The (schedule, slot) at-most-once guarantee: a row already at last_slot == slot
// is skipped, so calling Fire twice for the same slot emits at most once per
// schedule. An expr that no longer parses is logged and skipped (the row is left
// untouched) rather than stalling the whole tick.
func (w *Worker) Fire(ctx context.Context, slot, firedAt time.Time) (int, error) {
	slot = Slot(slot)
	slotStr := slot.Format(slotFormat)

	entries, err := w.store.List(ctx)
	if err != nil {
		return 0, fmt.Errorf("tick: list crontab: %w", err)
	}

	fired := 0
	for _, e := range entries {
		// Double-emit guard: already fired for this slot.
		if e.LastSlot != nil && Slot(*e.LastSlot).Equal(slot) {
			continue
		}
		sched, err := cron.Parse(e.Expr)
		if err != nil {
			// A row whose stored expr no longer parses cannot fire; log and skip
			// rather than stall the tick. (Create/update validates at the boundary,
			// so this is defensive.)
			w.log.Error("cron skipping unparseable expr", "name", e.Name, "expr", e.Expr, "error", err)
			continue
		}
		if !sched.Matches(slot) {
			continue
		}
		if err := w.fireOne(ctx, e.Name, slot, firedAt, slotStr); err != nil {
			return fired, fmt.Errorf("tick: fire %q: %w", e.Name, err)
		}
		fired++
	}

	if fired > 0 {
		w.ob.Ring()
	}
	return fired, nil
}

// fireOne runs the single per-schedule transaction: Append the event AND set
// last_slot = slot, committed atomically. The UPDATE is guarded by
// last_slot IS NOT slot so a concurrent fire for the same slot is a no-op write
// (belt-and-suspenders with the in-scan guard); a 0-row update means another
// writer already recorded the slot, so the tx is rolled back without emitting a
// duplicate.
func (w *Worker) fireOne(ctx context.Context, name string, slot, firedAt time.Time, slotStr string) error {
	ev, err := event.Build(name, slot, firedAt)
	if err != nil {
		return err
	}
	tx, err := w.db.BeginTx(ctx, nil)
	if err != nil {
		return err
	}
	defer tx.Rollback()

	res, err := tx.ExecContext(ctx,
		`UPDATE crontab SET last_slot = ? WHERE name = ? AND (last_slot IS NULL OR last_slot <> ?)`,
		slotStr, name, slotStr)
	if err != nil {
		return err
	}
	n, err := res.RowsAffected()
	if err != nil {
		return err
	}
	if n == 0 {
		// Already recorded for this slot (or row gone) — do not emit a duplicate.
		return nil
	}
	if err := w.ob.Append(tx, ev); err != nil {
		return err
	}
	return tx.Commit()
}

// nextMinute returns the next wall-clock minute boundary strictly after t (so a
// tick that runs exactly on a boundary still sleeps to the following one rather
// than busy-looping).
func nextMinute(t time.Time) time.Time {
	return t.Truncate(time.Minute).Add(time.Minute)
}
