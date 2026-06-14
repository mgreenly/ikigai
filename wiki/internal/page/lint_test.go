package page

import (
	"context"
	"testing"
)

// flagOpen inserts an open dup_flags row in canonical order.
func flagOpen(t *testing.T, s *Store, a, b string) {
	t.Helper()
	if a > b {
		a, b = b, a
	}
	tx, err := s.db.Begin()
	if err != nil {
		t.Fatalf("begin: %v", err)
	}
	if err := s.FlagDup(context.Background(), tx, a, b); err != nil {
		t.Fatalf("flag: %v", err)
	}
	if err := tx.Commit(); err != nil {
		t.Fatalf("commit: %v", err)
	}
}

func dupStatus(t *testing.T, s *Store, a, b string) (status string, runID string) {
	t.Helper()
	if a > b {
		a, b = b, a
	}
	var run *string
	err := s.db.QueryRow(`SELECT status, run_id FROM dup_flags WHERE subject_a=? AND subject_b=?`, a, b).Scan(&status, &run)
	if err != nil {
		t.Fatalf("dup status (%s,%s): %v", a, b, err)
	}
	if run != nil {
		runID = *run
	}
	return
}

func TestOpenDupPairsReJudgeGate(t *testing.T) {
	conn := newTestDB(t)
	s := NewStore(conn)
	ctx := context.Background()

	// Two subjects with pages at version 1 (insertPage stamps version 1).
	insertSubject(t, conn, "01A", "entity", "Acme")
	insertSubject(t, conn, "01B", "entity", "ACME")
	insertPage(t, conn, "01A", "Acme", "Acme body [01HX0000000000000000000001]")
	insertPage(t, conn, "01B", "ACME", "ACME body [01HX0000000000000000000002]")
	flagOpen(t, s, "01A", "01B")

	// Never judged → eligible.
	pairs, err := s.OpenDupPairs(ctx)
	if err != nil {
		t.Fatalf("open pairs: %v", err)
	}
	if len(pairs) != 1 {
		t.Fatalf("want 1 eligible pair, got %d", len(pairs))
	}

	// Stamp judged at the current versions (1,1) — a can't-tell-yet outcome.
	if err := s.StampJudged(ctx, "01A", "01B", 1, 1); err != nil {
		t.Fatalf("stamp: %v", err)
	}
	pairs, _ = s.OpenDupPairs(ctx)
	if len(pairs) != 0 {
		t.Fatalf("stamped-at-current pair should be skipped, got %d", len(pairs))
	}

	// Advance page A's version → the pair becomes eligible again (new evidence).
	if _, err := conn.Exec(`UPDATE pages SET version = 2 WHERE subject = '01A'`); err != nil {
		t.Fatalf("bump version: %v", err)
	}
	pairs, _ = s.OpenDupPairs(ctx)
	if len(pairs) != 1 {
		t.Fatalf("advanced page should re-open the pair, got %d", len(pairs))
	}
}

func TestDismissDupBlocksReFlag(t *testing.T) {
	conn := newTestDB(t)
	s := NewStore(conn)
	ctx := context.Background()
	insertSubject(t, conn, "01A", "entity", "A")
	insertSubject(t, conn, "01B", "entity", "B")
	flagOpen(t, s, "01A", "01B")

	if err := s.DismissDup(ctx, "01A", "01B", "run-9"); err != nil {
		t.Fatalf("dismiss: %v", err)
	}
	status, run := dupStatus(t, s, "01A", "01B")
	if status != "dismissed" || run != "run-9" {
		t.Fatalf("want dismissed/run-9, got %s/%s", status, run)
	}
	// A re-flag bounces off the UNIQUE pair (no new row, status preserved).
	flagOpen(t, s, "01A", "01B")
	status, _ = dupStatus(t, s, "01A", "01B")
	if status != "dismissed" {
		t.Fatalf("re-flag must not reopen a settled pair, got %s", status)
	}
}

func TestMergeSubjectsHardDeleteAndRepoint(t *testing.T) {
	conn := newTestDB(t)
	s := NewStore(conn)
	ctx := context.Background()

	// Winner 01A (older), loser 01B. A third subject 01C also flagged against the
	// loser — that open pair must be repointed to the winner.
	insertSubject(t, conn, "01A", "entity", "Acme")
	insertSubject(t, conn, "01B", "entity", "ACME Corp")
	insertSubject(t, conn, "01C", "entity", "Other")
	insertAlias(t, conn, "entity", "Acme", "01A")      // winner owns "acme"
	insertAlias(t, conn, "entity", "ACME Corp", "01B") // loser-unique → repointed to winner
	insertPage(t, conn, "01A", "Acme", "Acme is a maker. [01HX0000000000000000000001]")
	insertPage(t, conn, "01B", "ACME Corp", "ACME Corp makes things. [01HX0000000000000000000002]")
	flagOpen(t, s, "01A", "01B")
	flagOpen(t, s, "01B", "01C") // open pair naming the loser

	plan := MergePlan{
		Winner:        "01A",
		Loser:         "01B",
		CanonicalName: "Acme Corporation",
		Title:         "Acme Corporation",
		Body:          "Acme Corporation makes things. [01HX0000000000000000000001] [01HX0000000000000000000002]",
		Run:           "run-merge",
	}
	if err := s.MergeSubjects(ctx, plan); err != nil {
		t.Fatalf("merge: %v", err)
	}

	// Loser subject + page gone (no tombstone).
	var n int
	conn.QueryRow(`SELECT COUNT(1) FROM subjects WHERE id='01B'`).Scan(&n)
	if n != 0 {
		t.Fatal("loser subject must be hard-deleted")
	}
	conn.QueryRow(`SELECT COUNT(1) FROM pages WHERE subject='01B'`).Scan(&n)
	if n != 0 {
		t.Fatal("loser page must be deleted")
	}

	// Loser's aliases repointed to the winner (no alias still points at the loser).
	conn.QueryRow(`SELECT COUNT(1) FROM aliases WHERE subject_id='01B'`).Scan(&n)
	if n != 0 {
		t.Fatal("no alias may still point at the deleted loser")
	}
	conn.QueryRow(`SELECT COUNT(1) FROM aliases WHERE subject_id='01A' AND norm='acme corp'`).Scan(&n)
	if n != 1 {
		t.Fatal("loser's alias should be repointed to the winner")
	}

	// Winner canonical name + page updated.
	var canon, body string
	conn.QueryRow(`SELECT canonical_name FROM subjects WHERE id='01A'`).Scan(&canon)
	if canon != "Acme Corporation" {
		t.Fatalf("winner canonical_name = %q", canon)
	}
	conn.QueryRow(`SELECT body FROM pages WHERE subject='01A'`).Scan(&body)
	if body != plan.Body {
		t.Fatalf("winner page body not the folded body")
	}

	// The (A,B) pair is merged; the (B,C) pair was repointed to (A,C) open.
	status, run := dupStatus(t, s, "01A", "01B")
	if status != "merged" || run != "run-merge" {
		t.Fatalf("merged pair = %s/%s", status, run)
	}
	var st string
	if err := conn.QueryRow(`SELECT status FROM dup_flags WHERE subject_a='01A' AND subject_b='01C'`).Scan(&st); err != nil {
		t.Fatalf("repointed (A,C) pair not found: %v", err)
	}
	if st != "open" {
		t.Fatalf("repointed pair should stay open, got %s", st)
	}
	// The old (B,C) pair is gone.
	conn.QueryRow(`SELECT COUNT(1) FROM dup_flags WHERE subject_b='01B' OR subject_a='01B'`).Scan(&n)
	if n != 1 { // only the merged (A,B) row references B (the audit record)
		t.Fatalf("stale loser references in dup_flags: %d", n)
	}

	// FTS reflects the winner's new body, not the deleted loser's.
	conn.QueryRow(`SELECT COUNT(1) FROM pages_fts WHERE pages_fts MATCH 'Corporation'`).Scan(&n)
	if n != 1 {
		t.Fatalf("winner FTS row not synced (got %d)", n)
	}
}

// TestEnumerateSweepSubjects: every subject WITH a page is enumerated, ordered by
// id, carrying its type, canonical name, normalized aliases, and body; a page-less
// subject is excluded (it has no FTS content to flag against).
func TestEnumerateSweepSubjects(t *testing.T) {
	conn := newTestDB(t)
	s := NewStore(conn)
	ctx := context.Background()

	insertSubject(t, conn, "subj-A", TypeEntity, "Acme Corp")
	insertAlias(t, conn, TypeEntity, "Acme Corp", "subj-A")
	insertAlias(t, conn, TypeEntity, "Acme", "subj-A")
	insertPage(t, conn, "subj-A", "Acme Corp", "Acme makes things.")

	insertSubject(t, conn, "subj-B", TypeEntity, "Globex")
	insertPage(t, conn, "subj-B", "Globex", "Globex is a company.")

	// A page-less subject must be excluded.
	insertSubject(t, conn, "subj-Z", TypeEntity, "No Page Co")

	got, err := s.EnumerateSweepSubjects(ctx)
	if err != nil {
		t.Fatalf("enumerate: %v", err)
	}
	if len(got) != 2 {
		t.Fatalf("want 2 page-backed subjects, got %d (%+v)", len(got), got)
	}
	if got[0].SubjectID != "subj-A" || got[1].SubjectID != "subj-B" {
		t.Fatalf("not ordered by id: %v, %v", got[0].SubjectID, got[1].SubjectID)
	}
	if got[0].Body != "Acme makes things." {
		t.Fatalf("body not loaded: %q", got[0].Body)
	}
	if len(got[0].Aliases) != 2 {
		t.Fatalf("aliases not loaded: %v", got[0].Aliases)
	}
}

// TestFlagDupAuto: flags a pair in its own transaction, canonical-ordered and
// idempotent; a settled (dismissed) pair bounces off the conflict.
func TestFlagDupAuto(t *testing.T) {
	conn := newTestDB(t)
	s := NewStore(conn)
	ctx := context.Background()

	// Out-of-order args are canonical-ordered.
	if err := s.FlagDupAuto(ctx, "01B", "01A"); err != nil {
		t.Fatalf("flag: %v", err)
	}
	if st, _ := dupStatus(t, s, "01A", "01B"); st != "open" {
		t.Fatalf("want open, got %q", st)
	}
	// Idempotent re-flag does not error or duplicate.
	if err := s.FlagDupAuto(ctx, "01A", "01B"); err != nil {
		t.Fatalf("re-flag: %v", err)
	}
	var n int
	conn.QueryRow(`SELECT COUNT(1) FROM dup_flags WHERE subject_a='01A' AND subject_b='01B'`).Scan(&n)
	if n != 1 {
		t.Fatalf("want 1 row, got %d", n)
	}
	// Equal/empty ids are dropped.
	if err := s.FlagDupAuto(ctx, "01A", "01A"); err != nil {
		t.Fatalf("self-pair: %v", err)
	}
	// A settled (dismissed) pair bounces off (DO NOTHING leaves it dismissed).
	if _, err := conn.Exec(`UPDATE dup_flags SET status='dismissed' WHERE subject_a='01A' AND subject_b='01B'`); err != nil {
		t.Fatalf("settle: %v", err)
	}
	if err := s.FlagDupAuto(ctx, "01A", "01B"); err != nil {
		t.Fatalf("re-flag settled: %v", err)
	}
	if st, _ := dupStatus(t, s, "01A", "01B"); st != "dismissed" {
		t.Fatalf("settled pair must stay dismissed, got %q", st)
	}
}
