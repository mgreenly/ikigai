// Package run owns the runs lifecycle (design §4.5) — the provenance key for every
// integration. One run row per ATTEMPT: insert `running` before the run executes
// (the one write outside the commit), then exactly one terminal state follows:
// succeeded (set INSIDE the end-of-run commit, atomic with pages/registry/stamps),
// failed (clean — the transaction never commits, the row stays pending), or
// crashed (the process died; the boot sweep flips orphaned `running` → `crashed`).
//
// The load-bearing piece P4 freezes is the GENERIC end-of-run transaction wrapper:
// it TAKES A MANIFEST and atomically writes the run's terminal `succeeded` +
// `integrated_by` stamp. The real page/registry/dup/stale/fts writes arrive in
// P7a, but the transaction SHAPE — and its Manifest input — is built and tested
// here, so the swap from stub to real integrator (P7a/P8) fills behavior into a
// fixed shape rather than reshaping the spine. STAMP ONLY AT COMMIT, NEVER AT
// CLAIM (design §3): a crash can only ever leave a row pending, so restart
// re-selects it; no cleanup.
package run

import (
	"context"
	"database/sql"
	"encoding/json"
	"fmt"
	"math/rand"
	"time"

	"wiki/internal/integrate"
)

// Status values for the runs.status column (design §4.5).
const (
	StatusRunning   = "running"
	StatusSucceeded = "succeeded"
	StatusFailed    = "failed"
	StatusCrashed   = "crashed"
)

// EventRowDeadLettered is the eventplane event type emitted when a row exhausts
// its retry budget (design §8 / §12.3). Its payload is rowDeadLetteredPayload.
const EventRowDeadLettered = "wiki.row_dead_lettered"

// defaultAttemptsMax is the WIKI_RUN_ATTEMPTS_MAX default (design §7): five
// exponentially-spaced attempts span ~2.5h before a row dead-letters.
const defaultAttemptsMax = 5

// avgRunFloor is the floor on the recent average run duration used in the backoff
// formula (design §7): `avg_run_duration` from recent runs, floored at 60s.
const avgRunFloor = 60 * time.Second

// Outbox is the eventplane producer surface the failure path needs: append an
// event onto an existing transaction (so the dead-letter event commits atomically
// with the dead_at mark — design §8) and ring the doorbell after commit. Narrowed
// to an interface so run is testable without a real *outbox.Outbox and so a nil
// Outbox is a clean no-op (no eventplane wired yet in some tests). *outbox.Outbox
// satisfies it.
type Outbox interface {
	Append(tx *sql.Tx, ev outboxEvent) error
	Ring()
}

// outboxEvent mirrors outbox.Event without importing the package at the type
// level — the composition root passes an adapter. (Kept tiny on purpose.)
type outboxEvent = struct {
	Type    string
	Payload json.RawMessage
}

// rowDeadLetteredPayload is the wiki.row_dead_lettered payload (design §12.3).
type rowDeadLetteredPayload struct {
	InboxID   string `json:"inbox_id"`
	Source    string `json:"source"`
	Title     string `json:"title"`
	LastError string `json:"last_error"`
}

// Store owns the runs table on db. Constructed once at the composition root; the
// id minter, clock, randomness, retry threshold, and eventplane outbox are
// injectable for tests.
type Store struct {
	db          *sql.DB
	newID       func() string
	now         func() time.Time
	randFloat   func() float64 // [0,1) jitter source (design §7 random(2–4))
	attemptsMax int
	outbox      Outbox
}

// Options configures a Store. DB is required; the rest default to production.
type Options struct {
	DB    *sql.DB
	NewID func() string
	Now   func() time.Time
	// RandFloat is the [0,1) source the backoff jitter draws from (design §7's
	// random(2–4) factor). Defaults to math/rand; injectable for deterministic
	// time-driven tests.
	RandFloat func() float64
	// AttemptsMax is WIKI_RUN_ATTEMPTS_MAX (design §7); ≤0 uses the default of 5.
	AttemptsMax int
	// Outbox emits wiki.row_dead_lettered in the dead-letter transaction (design
	// §8). Nil disables the event (the mark still happens) — for tests/no-eventplane.
	Outbox Outbox
}

// New validates options and returns a ready Store.
func New(opts Options) (*Store, error) {
	if opts.DB == nil {
		return nil, fmt.Errorf("run: DB is required")
	}
	s := &Store{
		db:          opts.DB,
		newID:       opts.NewID,
		now:         opts.Now,
		randFloat:   opts.RandFloat,
		attemptsMax: opts.AttemptsMax,
		outbox:      opts.Outbox,
	}
	if s.newID == nil {
		s.newID = newULID
	}
	if s.now == nil {
		s.now = time.Now
	}
	if s.randFloat == nil {
		s.randFloat = rand.Float64
	}
	if s.attemptsMax <= 0 {
		s.attemptsMax = defaultAttemptsMax
	}
	return s, nil
}

// Begin inserts the `running` row for one attempt and returns the new run id (a
// ULID). This is the ONE write outside the end-of-run commit (design §4.5):
// insert `running` before the run executes. job is the integrator's job name
// (runs.job); causedBy is the inbox id of the causing row (runs.caused_by).
func (s *Store) Begin(ctx context.Context, job, causedBy string) (string, error) {
	id := s.newID()
	startedAt := s.now().UTC().UnixMilli()
	if _, err := s.db.ExecContext(ctx,
		`INSERT INTO runs (id, job, caused_by, status, started_at)
		 VALUES (?, ?, ?, ?, ?)`,
		id, job, causedBy, StatusRunning, startedAt,
	); err != nil {
		return "", fmt.Errorf("run: insert running: %w", err)
	}
	return id, nil
}

// Commit is the GENERIC end-of-run transaction wrapper (design §4.5 "one SQLite
// transaction at end of run"). It TAKES A MANIFEST and, in one transaction,
// writes the run's terminal `succeeded` + the causing row's `integrated_by` stamp
// — atomically, so a crash mid-run can only leave the row pending. The real
// page/registry/dup_flags/stale_notes/pages_fts writes land in P7a; P4 freezes
// the SHAPE and the Manifest input. The caller passes the run id from Begin, the
// causing inbox row id to stamp, and the Manifest the integrator produced.
//
// Document/event rows are stamped here, inside the commit (atomic with the
// writes). Cron rows are NOT stamped here — they get a worker-local
// completion-time join (StampCron) once all bound entries succeed (design §4.5).
// stampInbox controls whether the inbox stamp runs: true for the document pass,
// false for a cron entry's per-entry run (the cron row is stamped separately).
func (s *Store) Commit(ctx context.Context, runID, causedBy string, m *integrate.Manifest, stampInbox bool) error {
	if m == nil {
		return fmt.Errorf("run: Commit requires a non-nil Manifest")
	}
	tx, err := s.db.BeginTx(ctx, nil)
	if err != nil {
		return fmt.Errorf("run: begin commit tx: %w", err)
	}
	defer tx.Rollback()

	// --- The Manifest-consuming write set lands here in P7a (pages, registry,
	// dup_flags, stale_notes, pages_fts). P4 freezes the transaction shape and its
	// Manifest input; the spine round-trips a populated Manifest through it. The
	// WriteSet is derived from the manifest's subjects (never a parallel
	// structure) so a future P7a fills page writes against exactly these pages. ---
	_ = m.WriteSet()

	finishedAt := s.now().UTC().UnixMilli()
	if _, err := tx.ExecContext(ctx,
		`UPDATE runs SET status = ?, finished_at = ? WHERE id = ?`,
		StatusSucceeded, finishedAt, runID,
	); err != nil {
		return fmt.Errorf("run: stamp succeeded: %w", err)
	}

	if stampInbox {
		// Stamp the causing row's integrated_by INSIDE this commit (atomic with the
		// terminal status). The WHERE integrated_by='' makes a double-stamp a
		// harmless no-op and never clears a prior stamp (design §4.5: permanent).
		if _, err := tx.ExecContext(ctx,
			`UPDATE inbox SET integrated_by = ? WHERE id = ? AND integrated_by = ''`,
			runID, causedBy,
		); err != nil {
			return fmt.Errorf("run: stamp inbox: %w", err)
		}
	}

	if err := tx.Commit(); err != nil {
		return fmt.Errorf("run: commit: %w", err)
	}
	return nil
}

// Fail records a clean failure for a run (design §4.5): the transaction was never
// committed and the causing row stays pending, so this flips the run row to
// `failed` with its error for accounting and never touches the inbox stamp. It
// THEN applies the failure policy (design §7) to the causing row — backoff or
// dead-letter — at failure time, exactly where the policy is meant to live
// ("the check lives at failure time, applied by whatever marks the run").
func (s *Store) Fail(ctx context.Context, runID, causedBy string, runErr error) error {
	finishedAt := s.now().UTC().UnixMilli()
	msg := ""
	if runErr != nil {
		msg = runErr.Error()
	}
	if _, err := s.db.ExecContext(ctx,
		`UPDATE runs SET status = ?, finished_at = ?, error = ? WHERE id = ?`,
		StatusFailed, finishedAt, msg, runID,
	); err != nil {
		return fmt.Errorf("run: mark failed: %w", err)
	}
	if err := s.applyFailurePolicy(ctx, causedBy, msg); err != nil {
		return fmt.Errorf("run: apply failure policy: %w", err)
	}
	return nil
}

// StampCron is the cron row's worker-local completion-time join (design §4.5).
// After a worker finishes a bound digest entry, it asks "do all bound entries for
// this cron row now have a succeeded run?"; if yes it stamps the cron row. The
// caller supplies the set of bound entry job names; the stamp fires only when
// EVERY bound entry has at least one succeeded run caused by this cron row. The
// WHERE integrated_by='' makes a double-stamp race a harmless no-op.
//
// runID is the run that should own the stamp (the finishing entry's run).
// Returns true if the cron row was stamped by this call.
func (s *Store) StampCron(ctx context.Context, runID, cronRowID string, boundJobs []string) (bool, error) {
	if len(boundJobs) == 0 {
		// A cron row no job binds is stamped immediately as a no-op (design §3).
		// There is no run for a no-op, so mint a synthetic run id to stamp with —
		// a stamp of '' would read back as still-pending and the worker would
		// re-select the row forever.
		if runID == "" {
			runID = s.newID()
		}
		return s.stampCronRow(ctx, runID, cronRowID)
	}
	// Every bound job must have a succeeded run caused by this cron row.
	for _, job := range boundJobs {
		var n int
		if err := s.db.QueryRowContext(ctx,
			`SELECT COUNT(1) FROM runs WHERE caused_by = ? AND job = ? AND status = ?`,
			cronRowID, job, StatusSucceeded,
		).Scan(&n); err != nil {
			return false, fmt.Errorf("run: cron completion probe: %w", err)
		}
		if n == 0 {
			return false, nil // not all entries done yet
		}
	}
	return s.stampCronRow(ctx, runID, cronRowID)
}

func (s *Store) stampCronRow(ctx context.Context, runID, cronRowID string) (bool, error) {
	res, err := s.db.ExecContext(ctx,
		`UPDATE inbox SET integrated_by = ? WHERE id = ? AND integrated_by = ''`,
		runID, cronRowID,
	)
	if err != nil {
		return false, fmt.Errorf("run: stamp cron row: %w", err)
	}
	n, _ := res.RowsAffected()
	return n > 0, nil
}

// SweepOrphans is the boot sweep (design §3/§4.5): in-flight is RAM membership
// wiped on crash, so any `running` run left in the DB at boot is an orphan from a
// crashed process. Flip every orphaned `running` → `crashed` (counting one
// attempt; it does not gate re-selection — the pending inbox row drives that),
// THEN apply the failure policy to each orphan's causing row — the boot sweep
// "does the identical thing when marking crashed" as Fail (design §7). Returns
// the number of rows swept.
func (s *Store) SweepOrphans(ctx context.Context) (int64, error) {
	// Collect the orphans' causing rows BEFORE flipping, so the since-re-queue
	// COUNT(*) the policy runs sees the crashed attempt (now terminal) and decides
	// backoff vs dead-letter on the same basis Fail does.
	rows, err := s.db.QueryContext(ctx,
		`SELECT caused_by FROM runs WHERE status = ?`, StatusRunning)
	if err != nil {
		return 0, fmt.Errorf("run: sweep scan: %w", err)
	}
	var causedBy []string
	for rows.Next() {
		var cb string
		if err := rows.Scan(&cb); err != nil {
			rows.Close()
			return 0, fmt.Errorf("run: sweep scan row: %w", err)
		}
		causedBy = append(causedBy, cb)
	}
	rows.Close()
	if err := rows.Err(); err != nil {
		return 0, fmt.Errorf("run: sweep rows: %w", err)
	}

	finishedAt := s.now().UTC().UnixMilli()
	res, err := s.db.ExecContext(ctx,
		`UPDATE runs SET status = ?, finished_at = ? WHERE status = ?`,
		StatusCrashed, finishedAt, StatusRunning,
	)
	if err != nil {
		return 0, fmt.Errorf("run: sweep orphans: %w", err)
	}
	n, _ := res.RowsAffected()

	for _, cb := range causedBy {
		if err := s.applyFailurePolicy(ctx, cb, "run crashed (boot sweep)"); err != nil {
			return n, fmt.Errorf("run: sweep apply policy: %w", err)
		}
	}
	return n, nil
}
