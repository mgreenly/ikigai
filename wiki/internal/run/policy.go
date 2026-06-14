// Failure policy — bounded retries + dead-letter (design §7, §8).
//
// The frame is bounded retries + dead-letter: transient failures self-heal via
// exponential backoff, persistent failures stop spending money by dead-lettering,
// and the row stays durable and visible (it never moves — re-queue is just
// clearing dead_at). Three nullable epoch-ms columns on inbox carry the policy
// state; the pending predicate (selection's, design §3/§7) is their conjunction:
//
//	pending = integrated_by = '' AND dead_at IS NULL
//	          AND (ineligible_until IS NULL OR ineligible_until <= now)
//
// `runs` stays the single source of truth — there is NO denormalized failure
// counter; the since-re-queue attempt count is a COUNT(*) over runs. The check
// lives AT FAILURE TIME (here), applied by whatever marks the run (Fail and the
// boot sweep when marking crashed); selection stays policy-free. ALL failures
// count toward the threshold (no exempt type — code can't reliably classify
// transient vs. persistent, and an exempt type would retry forever at full cost).
package run

import (
	"context"
	"database/sql"
	"encoding/json"
	"fmt"
	"time"
)

// applyFailurePolicy is the design-§7 decision, applied at failure time against
// the causing inbox row. It counts attempts since the last re-queue; at the
// threshold it dead-letters the row (sets dead_at, clears ineligible_until, emits
// wiki.row_dead_lettered in that same transaction — design §8), otherwise it arms
// backoff (sets ineligible_until). lastError is the failing run's error string,
// carried into the dead-letter event's last_error.
func (s *Store) applyFailurePolicy(ctx context.Context, causedBy, lastError string) error {
	failures, err := s.failuresSinceRequeue(ctx, causedBy)
	if err != nil {
		return err
	}
	if failures >= s.attemptsMax {
		return s.deadLetter(ctx, causedBy, lastError)
	}
	return s.armBackoff(ctx, causedBy, failures)
}

// failuresSinceRequeue counts failed+crashed runs for the causing row since the
// last re-queue (design §7): re-queue (clearing dead_at + setting requeued_at)
// grants a fresh budget, so only attempts after requeued_at count. requeued_at is
// NULL until a human intervenes, so COALESCE(requeued_at, 0) admits all attempts.
func (s *Store) failuresSinceRequeue(ctx context.Context, causedBy string) (int, error) {
	var requeuedAt sql.NullInt64
	if err := s.db.QueryRowContext(ctx,
		`SELECT requeued_at FROM inbox WHERE id = ?`, causedBy,
	).Scan(&requeuedAt); err != nil {
		return 0, fmt.Errorf("run: read requeued_at: %w", err)
	}
	var floor int64
	if requeuedAt.Valid {
		floor = requeuedAt.Int64
	}
	var n int
	if err := s.db.QueryRowContext(ctx,
		`SELECT COUNT(*) FROM runs
		  WHERE caused_by = ? AND status IN (?, ?) AND started_at > ?`,
		causedBy, StatusFailed, StatusCrashed, floor,
	).Scan(&n); err != nil {
		return 0, fmt.Errorf("run: count failures since requeue: %w", err)
	}
	return n, nil
}

// armBackoff sets ineligible_until on the causing row (design §7's backoff): it is
// set on EVERY failed run (one delay mechanism, no transient/persistent dispatch).
// Formula: now + random(2–4) × avg_run_duration × 2^(failures−1), where
// avg_run_duration is the recent average from runs floored at 60s, the jitter
// desynchronizes failed rows, and the exponent grows with the attempt count so an
// outage doesn't burn the whole budget. The threshold bounds the exponent, so no
// cap is needed.
func (s *Store) armBackoff(ctx context.Context, causedBy string, failures int) error {
	delay := s.backoffDelay(ctx, failures)
	until := s.now().Add(delay).UTC().UnixMilli()
	if _, err := s.db.ExecContext(ctx,
		`UPDATE inbox SET ineligible_until = ? WHERE id = ?`,
		until, causedBy,
	); err != nil {
		return fmt.Errorf("run: arm backoff: %w", err)
	}
	return nil
}

// backoffDelay computes the design-§7 backoff interval. failures is the
// since-re-queue attempt count (≥1 at the first failure); the exponent is
// failures−1 so the first backoff is one base interval.
func (s *Store) backoffDelay(ctx context.Context, failures int) time.Duration {
	avg := s.avgRunDuration(ctx)
	jitter := 2.0 + 2.0*s.randFloat() // random in [2, 4)
	exp := 1
	for i := 1; i < failures; i++ {
		exp *= 2 // 2^(failures−1)
	}
	return time.Duration(jitter * float64(avg) * float64(exp))
}

// avgRunDuration is the recent average run duration from terminal runs, floored at
// 60s (design §7). A DB with no completed runs yet uses the floor.
func (s *Store) avgRunDuration(ctx context.Context) time.Duration {
	var avgMs sql.NullFloat64
	if err := s.db.QueryRowContext(ctx,
		`SELECT AVG(finished_at - started_at) FROM runs
		  WHERE finished_at IS NOT NULL AND finished_at >= started_at`,
	).Scan(&avgMs); err != nil || !avgMs.Valid {
		return avgRunFloor
	}
	d := time.Duration(avgMs.Float64) * time.Millisecond
	if d < avgRunFloor {
		return avgRunFloor
	}
	return d
}

// deadLetter parks the causing row at the threshold (design §7/§8): set dead_at,
// clear ineligible_until in the SAME UPDATE (a dead row is parked, not waiting),
// and emit wiki.row_dead_lettered IN THAT TRANSACTION so the notification fact and
// the durable mark commit atomically. The doorbell is rung after commit. A
// double-call is a harmless no-op (the WHERE dead_at IS NULL guard) so a crash
// after a prior dead-letter doesn't double-emit.
func (s *Store) deadLetter(ctx context.Context, causedBy, lastError string) error {
	tx, err := s.db.BeginTx(ctx, nil)
	if err != nil {
		return fmt.Errorf("run: begin dead-letter tx: %w", err)
	}
	defer tx.Rollback()

	deadAt := s.now().UTC().UnixMilli()
	res, err := tx.ExecContext(ctx,
		`UPDATE inbox SET dead_at = ?, ineligible_until = NULL
		  WHERE id = ? AND dead_at IS NULL`,
		deadAt, causedBy,
	)
	if err != nil {
		return fmt.Errorf("run: mark dead: %w", err)
	}
	if n, _ := res.RowsAffected(); n == 0 {
		// Already dead-lettered — nothing to do, don't re-emit.
		return tx.Commit()
	}

	if s.outbox != nil {
		var source, title string
		if err := tx.QueryRowContext(ctx,
			`SELECT source, title FROM inbox WHERE id = ?`, causedBy,
		).Scan(&source, &title); err != nil {
			return fmt.Errorf("run: read dead-letter payload: %w", err)
		}
		payload, err := json.Marshal(rowDeadLetteredPayload{
			InboxID:   causedBy,
			Source:    source,
			Title:     title,
			LastError: lastError,
		})
		if err != nil {
			return fmt.Errorf("run: marshal dead-letter payload: %w", err)
		}
		if err := s.outbox.Append(tx, outboxEvent{Type: EventRowDeadLettered, Payload: payload}); err != nil {
			return fmt.Errorf("run: append dead-letter event: %w", err)
		}
	}

	if err := tx.Commit(); err != nil {
		return fmt.Errorf("run: commit dead-letter: %w", err)
	}
	if s.outbox != nil {
		s.outbox.Ring()
	}
	return nil
}

// Requeue clears the dead mark and grants a fresh retry budget (design §7): set
// dead_at back to NULL, clear any backoff, and stamp requeued_at to scope the
// retry counter to attempts after this human intervention (preserving THAT a human
// intervened, and WHEN). The row becomes pending again on the next selection scan.
func (s *Store) Requeue(ctx context.Context, causedBy string) error {
	now := s.now().UTC().UnixMilli()
	if _, err := s.db.ExecContext(ctx,
		`UPDATE inbox SET dead_at = NULL, ineligible_until = NULL, requeued_at = ?
		  WHERE id = ?`,
		now, causedBy,
	); err != nil {
		return fmt.Errorf("run: requeue: %w", err)
	}
	return nil
}
