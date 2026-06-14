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
	"fmt"
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

// Store owns the runs table on db. Constructed once at the composition root; the
// id minter and clock are injectable for tests.
type Store struct {
	db    *sql.DB
	newID func() string
	now   func() time.Time
}

// Options configures a Store. DB is required; NewID/Now default to production.
type Options struct {
	DB    *sql.DB
	NewID func() string
	Now   func() time.Time
}

// New validates options and returns a ready Store.
func New(opts Options) (*Store, error) {
	if opts.DB == nil {
		return nil, fmt.Errorf("run: DB is required")
	}
	s := &Store{db: opts.DB, newID: opts.NewID, now: opts.Now}
	if s.newID == nil {
		s.newID = newULID
	}
	if s.now == nil {
		s.now = time.Now
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
// committed and the causing row stays pending, so this only flips the run row to
// `failed` with its error for accounting. It does NOT touch the inbox stamp.
func (s *Store) Fail(ctx context.Context, runID string, runErr error) error {
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
// attempt; it does not gate re-selection — the pending inbox row drives that).
// Returns the number of rows swept.
func (s *Store) SweepOrphans(ctx context.Context) (int64, error) {
	finishedAt := s.now().UTC().UnixMilli()
	res, err := s.db.ExecContext(ctx,
		`UPDATE runs SET status = ?, finished_at = ? WHERE status = ?`,
		StatusCrashed, finishedAt, StatusRunning,
	)
	if err != nil {
		return 0, fmt.Errorf("run: sweep orphans: %w", err)
	}
	n, _ := res.RowsAffected()
	return n, nil
}
