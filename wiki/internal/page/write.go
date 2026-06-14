package page

import (
	"context"
	"database/sql"
	"errors"
	"fmt"
)

// ErrVersionConflict is the optimistic-commit conflict (design §3): a page update
// guarded by WHERE subject=? AND version=? affected zero rows because another run
// committed a newer version of the page between merge's read and this commit (the
// LOST-UPDATE race). The end-of-run transaction rolls back and the conflict loop
// (P7b) re-runs merge for that page only against the fresh version, then recommits.
// It is a sentinel so the commit can distinguish a genuine concurrency conflict
// (retry) from a real write error (fail).
var ErrVersionConflict = errors.New("page: optimistic-commit version conflict")

// ErrAliasConflict is the duplicate-mint conflict (design §3, P7b2): an alias row
// for (type, norm) already exists pointing at a DIFFERENT subject_id than the one
// this run is minting. Two runs both minted a not-yet-registered subject; SQLite's
// single writer serializes the commits and the loser's alias insert collides with
// the winner's freshly-committed alias. The end-of-run transaction rolls back and
// the conflict loop (P7b2) RESTARTS AT RESOLVE for the colliding subject only — the
// lookup now hits the winner's aliases. It is a sentinel so the commit can
// distinguish this bridging-evidence collision (re-resolve) from a real write error.
// A same-subject_id re-insert is NOT a conflict — it is the harmless idempotent
// no-op the happy path relies on.
var ErrAliasConflict = errors.New("page: duplicate-mint alias conflict")

// The write half of the registry/pages layer (design §4.5): the operations the
// end-of-run transaction (P7a, internal/run) applies inside ONE SQLite
// transaction. Every method here takes a *sql.Tx — Store never opens its own
// transaction for these, because the whole point of the end-of-run commit is that
// pages, registry inserts, dup_flags, stale_notes, and the pages_fts sync land
// atomically with the run's terminal `succeeded` (zero mid-run partial writes).
//
// Reads (ResolveByKeys / Candidates / ReadExcerpt in store.go) take a *sql.DB and
// run with no lock held during resolution; these writes take a *sql.Tx and run in
// the commit. Keeping the two surfaces apart is the design's "resolution produces
// a plan, the commit applies it."

// EnsureSubject inserts the subjects row for a subject the manifest minted, if it
// is not already present (design §4.1, §4.5 "registry inserts"). A resolved
// subject already has its row, so the INSERT … ON CONFLICT DO NOTHING makes the
// call idempotent — a re-resolved existing subject is a no-op, a freshly-minted
// one is inserted. occurredAt is first-writer-wins (events only): the COALESCE
// keeps any existing value rather than overwriting it (§4.1), and an empty value
// stays NULL.
func (s *Store) EnsureSubject(ctx context.Context, tx *sql.Tx, subj Subject, createdByRun string) error {
	var occ any
	if subj.OccurredAt != "" {
		occ = subj.OccurredAt
	}
	if _, err := tx.ExecContext(ctx,
		`INSERT INTO subjects (id, type, kind, canonical_name, created_by_run, occurred_at)
		 VALUES (?, ?, ?, ?, ?, ?)
		 ON CONFLICT(id) DO UPDATE SET
		   occurred_at = COALESCE(subjects.occurred_at, excluded.occurred_at)`,
		subj.ID, subj.Type, subj.Kind, subj.CanonicalName, createdByRun, occ,
	); err != nil {
		return fmt.Errorf("page: ensure subject %q: %w", subj.ID, err)
	}
	return nil
}

// InsertAlias inserts one alias row (the normalized key → subject), idempotent on
// the UNIQUE(type, norm) lookup key (design §4.3). It distinguishes the two kinds
// of UNIQUE(type, norm) collision the design §3 duplicate-mint arm (P7b2) hinges
// on:
//
//   - a re-seen alias for the SAME subject_id is a harmless idempotent no-op (the
//     happy path re-asserts a subject's own existing aliases for free —
//     ON CONFLICT DO NOTHING semantics, preserved here);
//   - an alias whose (type, norm) already points at a DIFFERENT subject_id is the
//     DUPLICATE-MINT collision: two runs both minted a not-yet-registered subject
//     and this is the loser. It returns ErrAliasConflict so the end-of-run
//     transaction rolls back and the conflict loop restarts at resolve for the
//     colliding subject (the lookup now hits the winner's freshly-committed
//     aliases). This is also the "bridging evidence" path (a found-it alias
//     attachment hitting a different subject_id is routed through the same arm —
//     design §3).
//
// The insert uses ON CONFLICT(type, norm) DO NOTHING and then, on a zero-row
// result, reads the existing owner: same subject → no-op, different subject →
// ErrAliasConflict. Both the read and the insert run on the commit's *sql.Tx, so
// the decision sees exactly what the about-to-commit transaction would.
func (s *Store) InsertAlias(ctx context.Context, tx *sql.Tx, a Alias) error {
	if a.Norm == "" {
		return nil // a blank alias never enters the index (KeySet already drops these)
	}
	res, err := tx.ExecContext(ctx,
		`INSERT INTO aliases (type, norm, subject_id) VALUES (?, ?, ?)
		 ON CONFLICT(type, norm) DO NOTHING`,
		a.Type, a.Norm, a.SubjectID,
	)
	if err != nil {
		return fmt.Errorf("page: insert alias %q/%q → %q: %w", a.Type, a.Norm, a.SubjectID, err)
	}
	if n, _ := res.RowsAffected(); n > 0 {
		return nil // inserted fresh — no collision
	}
	// Zero rows inserted: the (type, norm) key already exists. Read its owner to
	// tell a same-subject no-op from a different-subject duplicate-mint conflict.
	var owner string
	if err := tx.QueryRowContext(ctx,
		`SELECT subject_id FROM aliases WHERE type = ? AND norm = ?`, a.Type, a.Norm,
	).Scan(&owner); err != nil {
		return fmt.Errorf("page: read alias owner %q/%q: %w", a.Type, a.Norm, err)
	}
	if owner != a.SubjectID {
		// A different subject already owns this key — the duplicate-mint loser.
		return fmt.Errorf("page: alias %q/%q owned by %q, not %q: %w",
			a.Type, a.Norm, owner, a.SubjectID, ErrAliasConflict)
	}
	return nil // same subject re-asserting its own alias — harmless no-op
}

// UpsertPage writes a page's prose body + frontmatter title and keeps pages_fts
// in sync IN THE SAME TRANSACTION, per design §4.5. pages_fts is an
// external-content FTS5 index with NO triggers, so the sync is explicit and
// per-page:
//
//   - a CREATED page inserts the FTS row at the page's rowid;
//   - an UPDATED page first issues the FTS5 'delete' command with the OLD
//     title/body (read here BEFORE the pages row is updated — a bare
//     DELETE … WHERE rowid would re-read the already-updated content row and strip
//     the wrong tokens, silently diverging the index), then re-inserts the new row.
//
// version is the OPTIMISTIC-COMMIT guard (design §3): baseVersion is the value
// merge read for this page (the manifest's per-page BaseVersion slot — P7a records
// it, P7b consumes it). The UPDATE is guarded by WHERE subject=? AND version=? and
// bumps version+1; zero rows affected means another run committed a newer version
// in the gap (the LOST-UPDATE race) → UpsertPage returns ErrVersionConflict and the
// caller's conflict loop (P7b) re-runs merge for this page only and recommits. A
// CREATED page has no prior version to guard (its conflict is a duplicate-mint,
// caught by the registry UNIQUE — P7b2 — not here). UpsertPage reads the OLD row,
// upserts under the guard, and syncs FTS — all on the commit's *sql.Tx.
func (s *Store) UpsertPage(ctx context.Context, tx *sql.Tx, subject, title, body string, baseVersion int) error {
	// Read the OLD row (title, body, rowid) BEFORE the update, so the FTS 'delete'
	// strips exactly the tokens the index currently holds.
	var oldTitle, oldBody string
	var rowid int64
	existed := true
	err := tx.QueryRowContext(ctx,
		`SELECT rowid, title, body FROM pages WHERE subject = ?`, subject,
	).Scan(&rowid, &oldTitle, &oldBody)
	if err == sql.ErrNoRows {
		existed = false
	} else if err != nil {
		return fmt.Errorf("page: upsert read old %q: %w", subject, err)
	}

	if existed {
		// Guard the UPDATE on the base version merge read (design §3): zero rows means
		// a concurrent run advanced the page past baseVersion → ErrVersionConflict. We
		// do this UPDATE FIRST (before touching FTS), so a conflict short-circuits with
		// no FTS side effect to undo within this (about-to-roll-back) transaction.
		res, err := tx.ExecContext(ctx,
			`UPDATE pages SET title = ?, body = ?, version = version + 1
			  WHERE subject = ? AND version = ?`,
			title, body, subject, baseVersion,
		)
		if err != nil {
			return fmt.Errorf("page: update page %q: %w", subject, err)
		}
		if n, _ := res.RowsAffected(); n == 0 {
			return fmt.Errorf("page: %q: %w", subject, ErrVersionConflict)
		}
		// The guarded UPDATE succeeded; now sync FTS: delete the OLD content at the
		// page's rowid with the OLD values, then re-insert the NEW content (design
		// §4.5). The 'delete' uses the OLD title/body captured before the UPDATE.
		if _, err := tx.ExecContext(ctx,
			`INSERT INTO pages_fts(pages_fts, rowid, title, body) VALUES ('delete', ?, ?, ?)`,
			rowid, oldTitle, oldBody,
		); err != nil {
			return fmt.Errorf("page: fts delete old %q: %w", subject, err)
		}
		if _, err := tx.ExecContext(ctx,
			`INSERT INTO pages_fts(rowid, title, body) VALUES (?, ?, ?)`,
			rowid, title, body,
		); err != nil {
			return fmt.Errorf("page: fts reinsert %q: %w", subject, err)
		}
		return nil
	}

	// A created page: insert the pages row, then the FTS row at its rowid.
	res, err := tx.ExecContext(ctx,
		`INSERT INTO pages (subject, title, body, version) VALUES (?, ?, ?, 0)`,
		subject, title, body,
	)
	if err != nil {
		return fmt.Errorf("page: insert page %q: %w", subject, err)
	}
	newRowid, err := res.LastInsertId()
	if err != nil {
		return fmt.Errorf("page: insert page rowid %q: %w", subject, err)
	}
	if _, err := tx.ExecContext(ctx,
		`INSERT INTO pages_fts(rowid, title, body) VALUES (?, ?, ?)`,
		newRowid, title, body,
	); err != nil {
		return fmt.Errorf("page: fts insert new %q: %w", subject, err)
	}
	return nil
}

// ReadPageTx returns a page's current title/body ON THE COMMIT'S *sql.Tx (ok=false
// if the page has none yet). The §6.1 citation-preservation gate (P7b) reads the
// OLD body this way — INSIDE the end-of-run transaction — rather than via the
// DB-level ReadPage: a read on a separate *sql.DB connection while this write tx
// holds SQLite's write lock deadlocks until busy_timeout (the write tx never
// commits while the read connection waits, and vice versa). Reading on the same tx
// sees the page as the guarded UPDATE will, with no second connection.
func (s *Store) ReadPageTx(ctx context.Context, tx *sql.Tx, subject string) (title, body string, ok bool, err error) {
	e := tx.QueryRowContext(ctx,
		`SELECT title, body FROM pages WHERE subject = ?`, subject,
	).Scan(&title, &body)
	if e == sql.ErrNoRows {
		return "", "", false, nil
	}
	if e != nil {
		return "", "", false, fmt.Errorf("page: read page (tx) %q: %w", subject, e)
	}
	return title, body, true, nil
}

// FlagDup inserts one candidate-duplicate pair in canonical order (smaller ULID
// first), idempotent on the UNIQUE(subject_a, subject_b) pair key (design §6: "one
// helper FlagDup(x,y) — sorts, INSERT … ON CONFLICT DO NOTHING"). It is the ONLY
// inserter of dup_flags; the CHECK(subject_a < subject_b) crashes a mis-ordered
// insert rather than silently duplicating, so the sort here is load-bearing. A
// pair of equal ids is dropped (a subject is never its own duplicate).
func (s *Store) FlagDup(ctx context.Context, tx *sql.Tx, a, b string) error {
	if a == b || a == "" || b == "" {
		return nil
	}
	if a > b {
		a, b = b, a
	}
	if _, err := tx.ExecContext(ctx,
		`INSERT INTO dup_flags (subject_a, subject_b) VALUES (?, ?)
		 ON CONFLICT(subject_a, subject_b) DO NOTHING`,
		a, b,
	); err != nil {
		return fmt.Errorf("page: flag dup (%q,%q): %w", a, b, err)
	}
	return nil
}

// InsertStaleNote appends one staleness note (design §6 / §12 #4) inside the
// commit. The merge agent surfaces these through the manifest's StaleNotes
// carrier; the end-of-run transaction OWNS the write (it never reaches into
// merge's side effects). id/run_id/status are filled here per §12 #4; cites is the
// new evidence that makes the repair legal. There is no UNIQUE on stale_notes —
// per-subject batching merges duplicates at repair time (lint-stale, P9c).
func (s *Store) InsertStaleNote(ctx context.Context, tx *sql.Tx, id, subject, note, cites, runID string) error {
	if _, err := tx.ExecContext(ctx,
		`INSERT INTO stale_notes (id, subject, note, cites, run_id, status)
		 VALUES (?, ?, ?, ?, ?, 'open')`,
		id, subject, note, cites, runID,
	); err != nil {
		return fmt.Errorf("page: insert stale note %q: %w", id, err)
	}
	return nil
}

// ReadVersion returns the current pages.version for a subject (the base version
// merge reads at merge-read time and records into the manifest's per-page
// BaseVersion slot — design §3 "the version merge read"). A subject with no page
// yet reports version 0 (a created page starts at 0). This is the read P7a's
// merge stage uses to populate the slot P7b's optimistic-commit guard consumes.
func (s *Store) ReadVersion(ctx context.Context, subject string) (int, error) {
	var v int
	err := s.db.QueryRowContext(ctx,
		`SELECT version FROM pages WHERE subject = ?`, subject,
	).Scan(&v)
	if err == sql.ErrNoRows {
		return 0, nil
	}
	if err != nil {
		return 0, fmt.Errorf("page: read version %q: %w", subject, err)
	}
	return v, nil
}

// ReadPage returns a subject's current page title + body (the merge agent's
// read-page tool, design §4.4). A subject with no page yet reports ok=false with
// empty content (merge then builds a fresh page from the registry row + claims).
func (s *Store) ReadPage(ctx context.Context, subject string) (title, body string, ok bool, err error) {
	e := s.db.QueryRowContext(ctx,
		`SELECT title, body FROM pages WHERE subject = ?`, subject,
	).Scan(&title, &body)
	if e == sql.ErrNoRows {
		return "", "", false, nil
	}
	if e != nil {
		return "", "", false, fmt.Errorf("page: read page %q: %w", subject, e)
	}
	return title, body, true, nil
}
