package page

import (
	"context"
	"database/sql"
	"fmt"
)

// The lint read/write surface (design §6): the dup-queue work list, the full
// per-subject evidence a dup judge reads, and the single-transaction subject
// merge lint-dups performs on a `merge` verdict. These are distinct from the
// integration write surface (write.go): lint-dups runs ONE TRANSACTION PER PAIR
// (per-pair recovery via the queue itself — design §6), not the end-of-run
// manifest commit, so its writes open and own their own *sql.Tx here.

// DupPair is one open candidate-duplicate pair the lint-dups work list yields
// (design §6). subject_a < subject_b (the FlagDup canonical order). The judged_*
// versions are the page versions stamped at the last `can't-tell-yet` judgment;
// the work list skips a pair only when NEITHER page has advanced past its stamp
// (new evidence is the only license to re-judge).
type DupPair struct {
	SubjectA string
	SubjectB string
}

// OpenDupPairs returns the open dup_flags pairs eligible for judging (design §6):
// status='open', and — the re-judge gate — a pair whose pages were already judged
// at a version (judged_version_a/_b set) is skipped UNLESS at least one of its
// pages has advanced past the stamp. A never-judged pair (NULL stamps) is always
// eligible. Order is by (subject_a, subject_b) so a run sweeps deterministically.
//
// The eligibility predicate is expressed in SQL against the live pages.version so
// the work list reflects the current state with no extra round-trip: a pair is
// eligible when either judged_version is NULL (never judged) OR the corresponding
// page's current version exceeds the stamped version.
func (s *Store) OpenDupPairs(ctx context.Context) ([]DupPair, error) {
	rows, err := s.db.QueryContext(ctx, `
		SELECT d.subject_a, d.subject_b
		FROM dup_flags d
		LEFT JOIN pages pa ON pa.subject = d.subject_a
		LEFT JOIN pages pb ON pb.subject = d.subject_b
		WHERE d.status = 'open'
		  AND (
		        d.judged_version_a IS NULL
		     OR d.judged_version_b IS NULL
		     OR COALESCE(pa.version, 0) > d.judged_version_a
		     OR COALESCE(pb.version, 0) > d.judged_version_b
		  )
		ORDER BY d.subject_a, d.subject_b`)
	if err != nil {
		return nil, fmt.Errorf("page: open dup pairs: %w", err)
	}
	defer rows.Close()
	var out []DupPair
	for rows.Next() {
		var p DupPair
		if err := rows.Scan(&p.SubjectA, &p.SubjectB); err != nil {
			return nil, fmt.Errorf("page: open dup pairs scan: %w", err)
		}
		out = append(out, p)
	}
	return out, rows.Err()
}

// SweepSubject is one subject's self-description the zero-LLM lint-sweep (design
// §6, P9b) builds its two candidate FTS queries from: the subject's id+type drive
// the type-scoped candidate query and the self-exclusion; CanonicalName + Aliases
// form the name/alias query (FTS lane 1); Body forms the claim/body query (FTS
// lane 2). It mirrors what the document pass feeds Candidates, but sourced from an
// EXISTING registry subject rather than an extracted one — the sweep co-examines
// pages that disjoint integration-time writers never saw together (Bob-from-email
// vs Robert-from-CRM). A subject with no page row has an empty Body (its
// name/alias query still runs).
type SweepSubject struct {
	SubjectID     string
	Type          Type
	CanonicalName string
	Aliases       []string
	Body          string
}

// EnumerateSweepSubjects returns every registry subject that has a page (only a
// subject with FTS content can be a candidate, so a page-less subject can neither
// be flagged nor flag others), each with the fields lint-sweep builds its two
// candidate queries from. It is read-only and ordered by id so a sweep walks the
// registry deterministically (the property the per-job test and the eval-harness
// threshold scoring rely on). Aliases are the normalized keys (the same key space
// resolution and the name/alias FTS lane operate in).
func (s *Store) EnumerateSweepSubjects(ctx context.Context) ([]SweepSubject, error) {
	rows, err := s.db.QueryContext(ctx, `
		SELECT s.id, s.type, s.canonical_name, p.body
		FROM subjects s
		JOIN pages p ON p.subject = s.id
		ORDER BY s.id`)
	if err != nil {
		return nil, fmt.Errorf("page: enumerate sweep subjects: %w", err)
	}
	defer rows.Close()
	var out []SweepSubject
	for rows.Next() {
		var ss SweepSubject
		if err := rows.Scan(&ss.SubjectID, &ss.Type, &ss.CanonicalName, &ss.Body); err != nil {
			return nil, fmt.Errorf("page: enumerate sweep subjects scan: %w", err)
		}
		out = append(out, ss)
	}
	if err := rows.Err(); err != nil {
		return nil, err
	}
	// Fill each subject's normalized alias list (one extra query per subject; the
	// sweep is a wide, rare-cadence scan, so a per-subject alias read is acceptable
	// and keeps the enumeration query a simple join).
	for i := range out {
		aliasRows, err := s.db.QueryContext(ctx,
			`SELECT norm FROM aliases WHERE subject_id = ? ORDER BY norm`, out[i].SubjectID)
		if err != nil {
			return nil, fmt.Errorf("page: enumerate sweep aliases: %w", err)
		}
		for aliasRows.Next() {
			var a string
			if err := aliasRows.Scan(&a); err != nil {
				aliasRows.Close()
				return nil, fmt.Errorf("page: enumerate sweep alias scan: %w", err)
			}
			out[i].Aliases = append(out[i].Aliases, a)
		}
		if err := aliasRows.Err(); err != nil {
			aliasRows.Close()
			return nil, err
		}
		aliasRows.Close()
	}
	return out, nil
}

// FlagDupAuto inserts one candidate-duplicate pair (canonical order, idempotent)
// in its OWN short transaction — the form lint-sweep needs, which (unlike the
// document-pass FlagDup that rides the end-of-run tx) has no surrounding
// transaction. It is flag-only: the pair UNIQUE makes it idempotent and a settled
// (merged/dismissed) pair bounces off (ON CONFLICT DO NOTHING). Equal/empty ids
// are dropped (a subject is never its own duplicate).
func (s *Store) FlagDupAuto(ctx context.Context, a, b string) error {
	if a == b || a == "" || b == "" {
		return nil
	}
	tx, err := s.db.BeginTx(ctx, nil)
	if err != nil {
		return fmt.Errorf("page: begin flag-dup tx: %w", err)
	}
	defer tx.Rollback()
	if err := s.FlagDup(ctx, tx, a, b); err != nil {
		return err
	}
	if err := tx.Commit(); err != nil {
		return fmt.Errorf("page: commit flag-dup: %w", err)
	}
	return nil
}

// DupSubject is the full evidence the dup judge reads for one side of a pair
// (design §6: "both full pages + complete alias lists"). CanonicalName + the full
// (normalized) alias list + the whole page body — never a truncated excerpt: the
// judge is the better-informed second look the §6 design demands, so it gets
// everything. Version is the page's current version (stamped into judged_version_*
// on a can't-tell-yet verdict, the re-judge gate). A subject with no page yet has
// an empty Body and Version 0.
type DupSubject struct {
	SubjectID     string
	Type          Type
	CanonicalName string
	Aliases       []string
	Title         string
	Body          string
	Version       int
}

// ReadDupSubject loads the full judge evidence for one subject (design §6). It is
// read-only (a plain *sql.DB read): the judge produces a verdict; the merge
// transaction applies it.
func (s *Store) ReadDupSubject(ctx context.Context, subjectID string) (DupSubject, error) {
	d := DupSubject{SubjectID: subjectID}
	if err := s.db.QueryRowContext(ctx,
		`SELECT type, canonical_name FROM subjects WHERE id = ?`, subjectID,
	).Scan(&d.Type, &d.CanonicalName); err != nil {
		if err == sql.ErrNoRows {
			return d, fmt.Errorf("page: read dup subject: %q not found", subjectID)
		}
		return d, fmt.Errorf("page: read dup subject: %w", err)
	}

	rows, err := s.db.QueryContext(ctx,
		`SELECT norm FROM aliases WHERE subject_id = ? ORDER BY norm`, subjectID)
	if err != nil {
		return d, fmt.Errorf("page: read dup subject aliases: %w", err)
	}
	defer rows.Close()
	for rows.Next() {
		var a string
		if err := rows.Scan(&a); err != nil {
			return d, fmt.Errorf("page: read dup subject alias scan: %w", err)
		}
		d.Aliases = append(d.Aliases, a)
	}
	if err := rows.Err(); err != nil {
		return d, err
	}

	if err := s.db.QueryRowContext(ctx,
		`SELECT title, body, version FROM pages WHERE subject = ?`, subjectID,
	).Scan(&d.Title, &d.Body, &d.Version); err != nil && err != sql.ErrNoRows {
		return d, fmt.Errorf("page: read dup subject page: %w", err)
	}
	return d, nil
}

// StampJudged records a `can't-tell-yet` judgment (design §6): the only write that
// verdict makes is stamping the page versions examined into judged_version_a/_b on
// the OPEN dup row, so the work-list re-judge gate skips it until a page advances.
// The status stays 'open'. It is a single guarded UPDATE (no transaction needed).
func (s *Store) StampJudged(ctx context.Context, a, b string, versionA, versionB int) error {
	if a > b {
		a, b = b, a
		versionA, versionB = versionB, versionA
	}
	if _, err := s.db.ExecContext(ctx,
		`UPDATE dup_flags SET judged_version_a = ?, judged_version_b = ?
		  WHERE subject_a = ? AND subject_b = ? AND status = 'open'`,
		versionA, versionB, a, b,
	); err != nil {
		return fmt.Errorf("page: stamp judged (%q,%q): %w", a, b, err)
	}
	return nil
}

// DismissDup records a `dismiss` verdict (design §6): the pair is definitely two
// different subjects, so the row → 'dismissed' permanently (which blocks
// re-flagging — the pair UNIQUE bounces a future FlagDup off the settled row). run
// is the lint-dups run that decided. A single guarded UPDATE; only an open row is
// touched.
func (s *Store) DismissDup(ctx context.Context, a, b, run string) error {
	if a > b {
		a, b = b, a
	}
	if _, err := s.db.ExecContext(ctx,
		`UPDATE dup_flags SET status = 'dismissed', run_id = ?
		  WHERE subject_a = ? AND subject_b = ? AND status = 'open'`,
		run, a, b,
	); err != nil {
		return fmt.Errorf("page: dismiss dup (%q,%q): %w", a, b, err)
	}
	return nil
}

// MergeSubjects performs the lint-dups subject merge in ONE transaction (design
// §6): the LOSER is hard-deleted and everything that pointed at it is repointed to
// the WINNER. The caller has already decided winner/loser mechanically (older ULID
// wins — design §6) and has the folded prose body + canonical name from the fold
// call. In one transaction this:
//
//   - repoints the loser's aliases to the winner (ON CONFLICT DO NOTHING — a name
//     the winner already owns is dropped, never duplicated past UNIQUE(type,norm));
//   - rewrites every OTHER open dup_flags pair that named the loser to name the
//     winner instead, in canonical order, de-duping against existing pairs and
//     dropping a pair that would become (winner,winner);
//   - writes the winner's folded page (body + canonical title) and bumps its
//     version, keeping pages_fts in sync (the external-content sync, no triggers);
//   - deletes the loser's page (+ its FTS row) and subjects row — NO tombstone
//     (design §6: dup_flags is the audit record; a stale loser id reachable
//     anywhere is a bug to crash on);
//   - sets the winner's canonical_name to the judge's pick and marks THIS pair
//     'merged' with the deciding run.
//
// A subject id that is reachable nowhere after this is the invariant the no-split
// rule rests on. The transaction is all-or-nothing: a failure rolls the whole
// merge back and the pair stays open for the queue to retry (per-pair recovery).
func (s *Store) MergeSubjects(ctx context.Context, m MergePlan) error {
	tx, err := s.db.BeginTx(ctx, nil)
	if err != nil {
		return fmt.Errorf("page: begin merge tx: %w", err)
	}
	defer tx.Rollback()

	if err := s.mergeSubjectsTx(ctx, tx, m); err != nil {
		return err
	}
	if err := tx.Commit(); err != nil {
		return fmt.Errorf("page: commit merge: %w", err)
	}
	return nil
}

// MergePlan is the decided, ready-to-apply subject merge (design §6). Winner and
// Loser are the mechanically-chosen ids (older ULID wins); CanonicalName is the
// judge's pick; Title/Body is the fold call's single merged page; Run is the
// lint-dups run id stamped on the merged dup row.
type MergePlan struct {
	Winner        string
	Loser         string
	CanonicalName string
	Title         string
	Body          string
	Run           string
}

func (s *Store) mergeSubjectsTx(ctx context.Context, tx *sql.Tx, m MergePlan) error {
	if m.Winner == "" || m.Loser == "" || m.Winner == m.Loser {
		return fmt.Errorf("page: merge plan invalid (winner=%q loser=%q)", m.Winner, m.Loser)
	}

	// 1. Repoint the loser's aliases to the winner. A name the winner already owns
	// would collide on UNIQUE(type,norm) — drop it (ON CONFLICT DO NOTHING), then
	// delete the loser's now-orphan alias rows.
	if _, err := tx.ExecContext(ctx,
		`UPDATE OR IGNORE aliases SET subject_id = ? WHERE subject_id = ?`,
		m.Winner, m.Loser,
	); err != nil {
		return fmt.Errorf("page: merge repoint aliases: %w", err)
	}
	if _, err := tx.ExecContext(ctx,
		`DELETE FROM aliases WHERE subject_id = ?`, m.Loser,
	); err != nil {
		return fmt.Errorf("page: merge delete loser aliases: %w", err)
	}

	// 2. Rewrite OTHER open dup_flags pairs that named the loser → name the winner,
	// in canonical order, de-duping. A pair that would become (winner,winner) is
	// dropped. The (winner,loser) pair being merged is EXCLUDED here — it becomes
	// the 'merged' audit row in step 5. We delete-then-reinsert because the
	// canonical ordering may flip.
	if err := s.repointDupFlags(ctx, tx, m.Winner, m.Loser); err != nil {
		return err
	}

	// 3. Write the winner's folded page (body + canonical title), bump version, sync
	// FTS. Read the OLD row first so the FTS 'delete' strips the right tokens.
	if err := s.writeFoldedPage(ctx, tx, m.Winner, m.Title, m.Body); err != nil {
		return err
	}

	// 4. Delete the loser's page (+ FTS row) and subjects row — no tombstone.
	if err := s.deleteSubjectPage(ctx, tx, m.Loser); err != nil {
		return err
	}
	if _, err := tx.ExecContext(ctx, `DELETE FROM subjects WHERE id = ?`, m.Loser); err != nil {
		return fmt.Errorf("page: merge delete loser subject: %w", err)
	}

	// 5. Winner canonical_name + mark THIS pair merged.
	if _, err := tx.ExecContext(ctx,
		`UPDATE subjects SET canonical_name = ? WHERE id = ?`, m.CanonicalName, m.Winner,
	); err != nil {
		return fmt.Errorf("page: merge set canonical name: %w", err)
	}
	a, b := m.Winner, m.Loser
	if a > b {
		a, b = b, a
	}
	if _, err := tx.ExecContext(ctx,
		`UPDATE dup_flags SET status = 'merged', run_id = ?
		  WHERE subject_a = ? AND subject_b = ? AND status = 'open'`,
		m.Run, a, b,
	); err != nil {
		return fmt.Errorf("page: merge mark pair merged: %w", err)
	}
	return nil
}

// repointDupFlags rewrites every OTHER open dup pair that referenced the loser so
// it references the winner instead, preserving canonical order and the pair
// UNIQUE. A resulting (winner,winner) self-pair is dropped. Settled (merged/
// dismissed) rows that named the loser are left as the historical audit record.
func (s *Store) repointDupFlags(ctx context.Context, tx *sql.Tx, winner, loser string) error {
	// Canonical order of the pair being merged (excluded — it becomes the audit row).
	ma, mb := winner, loser
	if ma > mb {
		ma, mb = mb, ma
	}
	rows, err := tx.QueryContext(ctx,
		`SELECT subject_a, subject_b FROM dup_flags
		  WHERE status = 'open' AND (subject_a = ? OR subject_b = ?)
		    AND NOT (subject_a = ? AND subject_b = ?)`,
		loser, loser, ma, mb)
	if err != nil {
		return fmt.Errorf("page: merge scan loser dup pairs: %w", err)
	}
	type pair struct{ a, b string }
	var affected []pair
	for rows.Next() {
		var p pair
		if err := rows.Scan(&p.a, &p.b); err != nil {
			rows.Close()
			return fmt.Errorf("page: merge scan loser dup pair: %w", err)
		}
		affected = append(affected, p)
	}
	rows.Close()
	if err := rows.Err(); err != nil {
		return err
	}

	for _, p := range affected {
		// Delete the old row first (its key changes).
		if _, err := tx.ExecContext(ctx,
			`DELETE FROM dup_flags WHERE subject_a = ? AND subject_b = ?`, p.a, p.b,
		); err != nil {
			return fmt.Errorf("page: merge delete repoint pair: %w", err)
		}
		other := p.a
		if other == loser {
			other = p.b
		}
		if other == winner {
			continue // (winner,winner) self-pair — drop it
		}
		x, y := winner, other
		if x > y {
			x, y = y, x
		}
		if _, err := tx.ExecContext(ctx,
			`INSERT INTO dup_flags (subject_a, subject_b) VALUES (?, ?)
			 ON CONFLICT(subject_a, subject_b) DO NOTHING`, x, y,
		); err != nil {
			return fmt.Errorf("page: merge reinsert repoint pair: %w", err)
		}
	}
	return nil
}

// writeFoldedPage upserts the winner's merged page body+title and bumps version,
// syncing pages_fts. Unlike the integration UpsertPage it is NOT version-guarded:
// lint-dups runs serially per pair and the fold body is the authoritative merged
// content. A winner with no page yet (rare — a subject merged before its first
// page) is inserted at version 0.
func (s *Store) writeFoldedPage(ctx context.Context, tx *sql.Tx, subject, title, body string) error {
	var oldTitle, oldBody string
	var rowid int64
	existed := true
	err := tx.QueryRowContext(ctx,
		`SELECT rowid, title, body FROM pages WHERE subject = ?`, subject,
	).Scan(&rowid, &oldTitle, &oldBody)
	if err == sql.ErrNoRows {
		existed = false
	} else if err != nil {
		return fmt.Errorf("page: merge read winner page: %w", err)
	}

	if existed {
		if _, err := tx.ExecContext(ctx,
			`UPDATE pages SET title = ?, body = ?, version = version + 1 WHERE subject = ?`,
			title, body, subject,
		); err != nil {
			return fmt.Errorf("page: merge update winner page: %w", err)
		}
		if _, err := tx.ExecContext(ctx,
			`INSERT INTO pages_fts(pages_fts, rowid, title, body) VALUES ('delete', ?, ?, ?)`,
			rowid, oldTitle, oldBody,
		); err != nil {
			return fmt.Errorf("page: merge fts delete old winner: %w", err)
		}
		if _, err := tx.ExecContext(ctx,
			`INSERT INTO pages_fts(rowid, title, body) VALUES (?, ?, ?)`,
			rowid, title, body,
		); err != nil {
			return fmt.Errorf("page: merge fts reinsert winner: %w", err)
		}
		return nil
	}

	res, err := tx.ExecContext(ctx,
		`INSERT INTO pages (subject, title, body, version) VALUES (?, ?, ?, 0)`,
		subject, title, body,
	)
	if err != nil {
		return fmt.Errorf("page: merge insert winner page: %w", err)
	}
	newRowid, err := res.LastInsertId()
	if err != nil {
		return fmt.Errorf("page: merge winner page rowid: %w", err)
	}
	if _, err := tx.ExecContext(ctx,
		`INSERT INTO pages_fts(rowid, title, body) VALUES (?, ?, ?)`,
		newRowid, title, body,
	); err != nil {
		return fmt.Errorf("page: merge fts insert winner: %w", err)
	}
	return nil
}

// deleteSubjectPage deletes a subject's page row and its external-content FTS row
// (read the OLD content first so the FTS 'delete' strips the right tokens). A
// subject with no page is a clean no-op.
func (s *Store) deleteSubjectPage(ctx context.Context, tx *sql.Tx, subject string) error {
	var oldTitle, oldBody string
	var rowid int64
	err := tx.QueryRowContext(ctx,
		`SELECT rowid, title, body FROM pages WHERE subject = ?`, subject,
	).Scan(&rowid, &oldTitle, &oldBody)
	if err == sql.ErrNoRows {
		return nil
	}
	if err != nil {
		return fmt.Errorf("page: merge read loser page: %w", err)
	}
	if _, err := tx.ExecContext(ctx,
		`INSERT INTO pages_fts(pages_fts, rowid, title, body) VALUES ('delete', ?, ?, ?)`,
		rowid, oldTitle, oldBody,
	); err != nil {
		return fmt.Errorf("page: merge fts delete loser: %w", err)
	}
	if _, err := tx.ExecContext(ctx, `DELETE FROM pages WHERE subject = ?`, subject); err != nil {
		return fmt.Errorf("page: merge delete loser page: %w", err)
	}
	return nil
}
