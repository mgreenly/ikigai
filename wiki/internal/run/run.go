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
	"errors"
	"fmt"
	"math/rand"
	"strings"
	"time"

	"wiki/internal/integrate"
	"wiki/internal/page"
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

// MaxCommitAttempts is the conflict-loop bound (design §3): cap 3 commit attempts
// per run, then fail cleanly naming conflict-retry exhaustion. This is a SECOND,
// distinct budget from the run-level WIKI_RUN_ATTEMPTS_MAX (N runs per causing
// row): 3 commit attempts INSIDE one run vs. N runs across the row's lifetime.
// Both the lost-update arm (P7b) and the duplicate-mint arm (P7b2) share it.
const MaxCommitAttempts = 3

// ConflictError is an optimistic-commit conflict surfaced by Commit (design §3):
// the version-guarded page update affected zero rows because a concurrent run
// advanced the page past the base version merge read (the LOST-UPDATE race). It
// names the conflicting subject so the conflict loop (runClaim) re-runs merge for
// THAT page only — no diagnosis step, the failing statement identifies the page —
// and recommits. The transaction was already rolled back when this is returned.
type ConflictError struct {
	// Subject is the subject id of the page whose version guard failed — the one
	// page the conflict loop re-merges (the others' merge output is unaffected).
	Subject string
}

func (e *ConflictError) Error() string {
	return fmt.Sprintf("run: optimistic-commit conflict on subject %q (lost update)", e.Subject)
}

// DuplicateMintError is the SECOND optimistic-commit conflict surfaced by Commit
// (design §3, P7b2): a subject this run minted as not-yet-registered collided with
// a concurrent run that minted the same subject first — the loser's alias insert hit
// UNIQUE(type, norm) against the winner's freshly-committed alias (the DUPLICATE-MINT
// race). It names the conflicting subject so the conflict loop (runClaim) RESTARTS AT
// RESOLVE for THAT subject only — never re-extracting (nothing another run did
// invalidates what this document said), the lookup now hitting the winner's aliases.
// The transaction was already rolled back when this is returned. It is distinct from
// *ConflictError (lost update → re-merge) precisely because the recovery differs: a
// lost update re-runs merge, a duplicate mint re-runs resolve.
type DuplicateMintError struct {
	// Subject is the manifest subject id of the colliding freshly-minted subject —
	// the one the conflict loop re-resolves (its alias now resolves onto the winner).
	Subject string
}

func (e *DuplicateMintError) Error() string {
	return fmt.Sprintf("run: duplicate-mint conflict on subject %q (alias UNIQUE)", e.Subject)
}

// ErrConflictRetryExhausted is the clean failure when the conflict loop hits
// MaxCommitAttempts (design §3): distinct from the run-level retry budget, but the
// run still fails and the row's failure policy applies (the error string names
// conflict-retry exhaustion for the human; §7 counts it toward the threshold).
var ErrConflictRetryExhausted = fmt.Errorf("run: conflict-retry exhaustion (%d commit attempts)", MaxCommitAttempts)

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

// Pages is the registry/pages write surface the end-of-run transaction applies
// inside its commit (design §4.5). It is *page.Store narrowed to an interface so
// run is testable with a fake and so the dependency is explicit. Every method
// takes the commit's *sql.Tx — the writes land atomically with the run's terminal
// `succeeded` (zero mid-run partial writes). A nil Pages keeps Commit's pre-P7a
// behavior (terminal status + inbox stamp only) — used by cron/digest no-op runs
// whose Manifest carries no pages.
type Pages interface {
	EnsureSubject(ctx context.Context, tx *sql.Tx, subj page.Subject, createdByRun string) error
	InsertAlias(ctx context.Context, tx *sql.Tx, a page.Alias) error
	// UpsertPage writes a page guarded by the manifest's per-page base version (the
	// optimistic-commit guard, design §3): a stale base version yields
	// page.ErrVersionConflict, which Commit surfaces as a *ConflictError naming the
	// subject so the conflict loop re-runs merge for that page only (P7b).
	UpsertPage(ctx context.Context, tx *sql.Tx, subject, title, body string, baseVersion int) error
	FlagDup(ctx context.Context, tx *sql.Tx, a, b string) error
	InsertStaleNote(ctx context.Context, tx *sql.Tx, id, subject, note, cites, runID string) error
	// ReadPageTx returns a page's current title/body ON THE COMMIT'S *sql.Tx
	// (ok=false if it has none yet) — the OLD body the §6.1 citation-preservation
	// gate diffs against the merged body (P7b). It MUST run on the same tx as the
	// guarded upsert: a read on a separate *sql.DB connection while this write tx
	// holds SQLite's write lock deadlocks until busy_timeout.
	ReadPageTx(ctx context.Context, tx *sql.Tx, subject string) (title, body string, ok bool, err error)
}

// Store owns the runs table on db. Constructed once at the composition root; the
// id minter, clock, randomness, retry threshold, eventplane outbox, and the pages
// write surface are injectable for tests.
type Store struct {
	db          *sql.DB
	newID       func() string
	now         func() time.Time
	randFloat   func() float64 // [0,1) jitter source (design §7 random(2–4))
	attemptsMax int
	outbox      Outbox
	pages       Pages
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
	// Pages is the registry/pages write surface applied inside the end-of-run
	// commit (design §4.5). Nil keeps the pre-P7a behavior (status + stamp only) —
	// a Manifest carrying subjects then commits its terminal status but writes no
	// pages, which is only correct for an empty (cron/digest no-op) Manifest.
	Pages Pages
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
		pages:       opts.Pages,
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

	// --- The Manifest-consuming write set (P7a): pages, registry inserts,
	// dup_flags, stale_notes, and the pages_fts sync — all inside this one
	// transaction (design §4.5). The write set is derived from the manifest's
	// subjects (m.WriteSet()), never a parallel structure. A nil Pages surface
	// keeps the pre-P7a behavior (terminal status + stamp only), correct only for
	// an empty (cron/digest no-op) Manifest. ---
	if err := s.applyManifest(ctx, tx, runID, m); err != nil {
		return err
	}

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

// applyManifest writes the Manifest's pages, registry inserts, dup_flags, and
// stale_notes inside the end-of-run transaction (design §4.5). It runs per-subject
// in manifest order (deterministic), then folds the manifest's dup_pairs and
// stale_notes. A nil Pages surface (cron/digest no-op runs) is a no-op as long as
// the Manifest carries no subjects; a non-empty Manifest with no Pages surface is
// a wiring bug and is rejected loudly rather than silently dropping page writes.
func (s *Store) applyManifest(ctx context.Context, tx *sql.Tx, runID string, m *integrate.Manifest) error {
	if s.pages == nil {
		if len(m.Subjects) > 0 || len(m.DupPairs) > 0 || len(m.StaleNotes) > 0 {
			return fmt.Errorf("run: commit has a populated Manifest but no Pages write surface wired")
		}
		return nil
	}

	for i := range m.Subjects {
		subj := m.Subjects[i]
		if subj.SubjectID == "" {
			return fmt.Errorf("run: manifest subject %d (%q) has no resolved id", i, subj.Name)
		}

		// Registry: the subjects row (idempotent; occurred_at first-writer-wins) and
		// every alias key for this subject's surface forms.
		canonical := strings.TrimSpace(subj.Name)
		if err := s.pages.EnsureSubject(ctx, tx, page.Subject{
			ID:            subj.SubjectID,
			Type:          subj.Type,
			Kind:          subj.Kind,
			CanonicalName: canonical,
			OccurredAt:    subj.OccurredAt,
		}, runID); err != nil {
			return fmt.Errorf("run: %w", err)
		}
		for _, norm := range page.KeySet(subj.Name, subj.Aliases) {
			if err := s.pages.InsertAlias(ctx, tx, page.Alias{
				Type:      subj.Type,
				Norm:      norm,
				SubjectID: subj.SubjectID,
			}); err != nil {
				// A different subject already owns this (type, norm) key → the
				// duplicate-mint race (design §3, P7b2). Surface a *DuplicateMintError
				// naming the subject so the conflict loop re-resolves THAT subject only
				// (its alias now lands on the winner). The transaction is rolled back by
				// the deferred Rollback when this returns.
				if errors.Is(err, page.ErrAliasConflict) {
					return &DuplicateMintError{Subject: subj.SubjectID}
				}
				return fmt.Errorf("run: %w", err)
			}
		}

		// The page: write merge's rewritten prose body + title, keeping pages_fts in
		// sync (the §4.5 external-content sync, per-page, no triggers).
		if subj.TargetPage != "" {
			// §6.1 citation-preservation gate (P7b): the OLD body's citations minus the
			// NEW body's must EXACTLY equal merge's declared `superseded`. Undeclared
			// loss = paraphrased-away evidence = a failed call (the transaction never
			// commits — returning here rolls it back via the deferred Rollback). The gate
			// runs against the page's CURRENT body; a concurrent write that changes it is
			// caught by the version guard below, which re-merges against the fresh body.
			oldTitle, oldBody, existed, err := s.pages.ReadPageTx(ctx, tx, subj.TargetPage)
			_ = oldTitle
			if err != nil {
				return fmt.Errorf("run: read old page for §6.1 gate %q: %w", subj.TargetPage, err)
			}
			if existed {
				if err := checkCitationPreservation(oldBody, subj.PageBody, subj.Superseded); err != nil {
					return fmt.Errorf("run: %w", err)
				}
			}
			// Version-guarded upsert (design §3): a stale base version (a concurrent run
			// advanced the page) yields page.ErrVersionConflict, which we surface as a
			// *ConflictError naming the subject so the conflict loop re-merges THAT page
			// only and recommits (P7b's lost-update arm). The transaction is rolled back
			// by the deferred Rollback when this returns.
			if err := s.pages.UpsertPage(ctx, tx, subj.TargetPage, subj.PageTitle, subj.PageBody, subj.BaseVersion); err != nil {
				if errors.Is(err, page.ErrVersionConflict) {
					return &ConflictError{Subject: subj.SubjectID}
				}
				return fmt.Errorf("run: %w", err)
			}
		}
	}

	// dup_flags from the manifest's candidate pairs (canonical order; FlagDup sorts
	// and is the only inserter).
	for _, p := range m.DupPairs {
		if err := s.pages.FlagDup(ctx, tx, p.SubjectA, p.SubjectB); err != nil {
			return fmt.Errorf("run: %w", err)
		}
	}

	// stale_notes the merge agent surfaced — written here, in the existing commit
	// (the transaction owns the write, never merge's side effects — §6).
	for _, sn := range m.StaleNotes {
		if err := s.pages.InsertStaleNote(ctx, tx, s.newID(), sn.Subject, sn.Note,
			strings.Join(sn.Cites, " "), runID); err != nil {
			return fmt.Errorf("run: %w", err)
		}
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

// CountConflict bumps runs.conflicts for one run (design §3): each optimistic-commit
// collision the conflict loop handles is recorded on the run, so "is the pool
// creating problems" is a query, not archaeology. It is a plain UPDATE (no
// transaction) called between conflict-loop attempts, before the re-merge.
func (s *Store) CountConflict(ctx context.Context, runID string) error {
	if _, err := s.db.ExecContext(ctx,
		`UPDATE runs SET conflicts = conflicts + 1 WHERE id = ?`, runID,
	); err != nil {
		return fmt.Errorf("run: count conflict: %w", err)
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
