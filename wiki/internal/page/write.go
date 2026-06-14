package page

import (
	"context"
	"database/sql"
	"fmt"
)

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
// the UNIQUE(type, norm) lookup key (design §4.3). A re-seen alias for the SAME
// subject is a harmless no-op; a collision with a DIFFERENT subject is the
// duplicate-mint guard and is left to surface as the UNIQUE violation P7b2 handles
// (here, in the happy path, it would error — which is correct, the happy path
// never mints a colliding alias). On the happy path we use ON CONFLICT DO NOTHING
// so re-asserting a subject's own existing aliases is free.
func (s *Store) InsertAlias(ctx context.Context, tx *sql.Tx, a Alias) error {
	if a.Norm == "" {
		return nil // a blank alias never enters the index (KeySet already drops these)
	}
	if _, err := tx.ExecContext(ctx,
		`INSERT INTO aliases (type, norm, subject_id) VALUES (?, ?, ?)
		 ON CONFLICT(type, norm) DO NOTHING`,
		a.Type, a.Norm, a.SubjectID,
	); err != nil {
		return fmt.Errorf("page: insert alias %q/%q → %q: %w", a.Type, a.Norm, a.SubjectID, err)
	}
	return nil
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
// version is bumped on every write (the optimistic-commit guard — §3); the
// conflict-handling WHERE guard and the conflict loops are P7b/P7b2. UpsertPage is
// the single-writer happy path: it reads the OLD row, upserts, and syncs FTS.
func (s *Store) UpsertPage(ctx context.Context, tx *sql.Tx, subject, title, body string) error {
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
		// Delete the OLD FTS content at the page's rowid with the OLD values, THEN
		// update the page row, THEN re-insert the NEW FTS content. Order matters:
		// the 'delete' must see the OLD column values, which is why it precedes the
		// UPDATE (design §4.5).
		if _, err := tx.ExecContext(ctx,
			`INSERT INTO pages_fts(pages_fts, rowid, title, body) VALUES ('delete', ?, ?, ?)`,
			rowid, oldTitle, oldBody,
		); err != nil {
			return fmt.Errorf("page: fts delete old %q: %w", subject, err)
		}
		if _, err := tx.ExecContext(ctx,
			`UPDATE pages SET title = ?, body = ?, version = version + 1 WHERE subject = ?`,
			title, body, subject,
		); err != nil {
			return fmt.Errorf("page: update page %q: %w", subject, err)
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
