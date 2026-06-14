package page

import (
	"context"
	"database/sql"
	"errors"
	"testing"
)

// withTx runs fn inside a transaction against a migrated DB and commits, so the
// write methods (which all take a *sql.Tx, as the end-of-run transaction does) are
// exercised exactly as the commit drives them.
func withTx(t *testing.T, conn *sql.DB, fn func(tx *sql.Tx)) {
	t.Helper()
	tx, err := conn.BeginTx(context.Background(), nil)
	if err != nil {
		t.Fatalf("begin tx: %v", err)
	}
	fn(tx)
	if err := tx.Commit(); err != nil {
		t.Fatalf("commit tx: %v", err)
	}
}

func ftsMatchCount(t *testing.T, conn *sql.DB, col, term string) int {
	t.Helper()
	var n int
	q := `SELECT COUNT(1) FROM pages_fts WHERE pages_fts.` + col + ` MATCH ?`
	if err := conn.QueryRow(q, `"`+term+`"`).Scan(&n); err != nil {
		t.Fatalf("fts match %s/%s: %v", col, term, err)
	}
	return n
}

// TestUpsertPageCreateSyncsFTS proves a created page inserts the FTS row at the
// page's rowid: a MATCH over the new body returns it.
func TestUpsertPageCreateSyncsFTS(t *testing.T) {
	conn := newTestDB(t)
	s := NewStore(conn)
	insertSubject(t, conn, "subj-1", TypeEntity, "Acme")

	withTx(t, conn, func(tx *sql.Tx) {
		if err := s.UpsertPage(context.Background(), tx, "subj-1", "Acme", "Acme builds anvils [01HX]", 0); err != nil {
			t.Fatalf("upsert create: %v", err)
		}
	})

	if got := ftsMatchCount(t, conn, "body", "anvils"); got != 1 {
		t.Fatalf("new page not in FTS: anvils match = %d, want 1", got)
	}
	var v int
	conn.QueryRow(`SELECT version FROM pages WHERE subject='subj-1'`).Scan(&v)
	if v != 0 {
		t.Fatalf("created page version = %d, want 0", v)
	}
}

// TestUpsertPageUpdateSyncsFTS proves the 'delete'-then-insert sync (not a stale
// append): after an UPDATE the OLD body no longer matches while the NEW body does,
// and version is bumped.
func TestUpsertPageUpdateSyncsFTS(t *testing.T) {
	conn := newTestDB(t)
	s := NewStore(conn)
	insertSubject(t, conn, "subj-1", TypeEntity, "Acme")

	withTx(t, conn, func(tx *sql.Tx) {
		if err := s.UpsertPage(context.Background(), tx, "subj-1", "Acme", "Acme builds anvils", 0); err != nil {
			t.Fatalf("upsert create: %v", err)
		}
	})
	withTx(t, conn, func(tx *sql.Tx) {
		// Base version 0 (what the create left) — the guarded update succeeds and bumps.
		if err := s.UpsertPage(context.Background(), tx, "subj-1", "Acme", "Acme builds rockets", 0); err != nil {
			t.Fatalf("upsert update: %v", err)
		}
	})

	if got := ftsMatchCount(t, conn, "body", "anvils"); got != 0 {
		t.Fatalf("OLD body still in FTS: anvils match = %d, want 0 (stale-append bug)", got)
	}
	if got := ftsMatchCount(t, conn, "body", "rockets"); got != 1 {
		t.Fatalf("NEW body not in FTS: rockets match = %d, want 1", got)
	}
	var v int
	conn.QueryRow(`SELECT version FROM pages WHERE subject='subj-1'`).Scan(&v)
	if v != 1 {
		t.Fatalf("updated page version = %d, want 1", v)
	}
}

// TestFlagDupCanonicalAndIdempotent proves FlagDup sorts to canonical order and is
// idempotent on the pair key.
func TestFlagDupCanonicalAndIdempotent(t *testing.T) {
	conn := newTestDB(t)
	s := NewStore(conn)
	withTx(t, conn, func(tx *sql.Tx) {
		// Mis-ordered input must be canonicalized (the CHECK would crash otherwise).
		if err := s.FlagDup(context.Background(), tx, "bbb", "aaa"); err != nil {
			t.Fatalf("flagdup 1: %v", err)
		}
		if err := s.FlagDup(context.Background(), tx, "aaa", "bbb"); err != nil {
			t.Fatalf("flagdup 2 (dup): %v", err)
		}
	})
	var a, b string
	var n int
	conn.QueryRow(`SELECT COUNT(1) FROM dup_flags`).Scan(&n)
	if n != 1 {
		t.Fatalf("dup_flags rows = %d, want 1 (idempotent)", n)
	}
	conn.QueryRow(`SELECT subject_a, subject_b FROM dup_flags`).Scan(&a, &b)
	if a != "aaa" || b != "bbb" {
		t.Fatalf("dup pair = (%q,%q), want (aaa,bbb) canonical order", a, b)
	}
}

// TestInsertStaleNote proves a stale note lands open with its cites/run_id.
func TestInsertStaleNote(t *testing.T) {
	conn := newTestDB(t)
	s := NewStore(conn)
	withTx(t, conn, func(tx *sql.Tx) {
		if err := s.InsertStaleNote(context.Background(), tx, "note-1", "subj-1", "looks stale", "01HX", "run-1"); err != nil {
			t.Fatalf("insert stale note: %v", err)
		}
	})
	var subject, note, cites, runID, status string
	if err := conn.QueryRow(
		`SELECT subject, note, cites, run_id, status FROM stale_notes WHERE id='note-1'`,
	).Scan(&subject, &note, &cites, &runID, &status); err != nil {
		t.Fatalf("read stale note: %v", err)
	}
	if subject != "subj-1" || note != "looks stale" || cites != "01HX" || runID != "run-1" || status != "open" {
		t.Fatalf("stale note = (%q,%q,%q,%q,%q)", subject, note, cites, runID, status)
	}
}

// TestEnsureSubjectIdempotentOccurredAtFirstWriterWins proves EnsureSubject is
// idempotent and keeps the first occurred_at (events first-writer-wins, §4.1).
func TestEnsureSubjectIdempotentOccurredAtFirstWriterWins(t *testing.T) {
	conn := newTestDB(t)
	s := NewStore(conn)
	withTx(t, conn, func(tx *sql.Tx) {
		if err := s.EnsureSubject(context.Background(), tx, Subject{
			ID: "e-1", Type: TypeEvent, CanonicalName: "Launch", OccurredAt: "2024-01",
		}, "run-1"); err != nil {
			t.Fatalf("ensure 1: %v", err)
		}
		// Second writer with a different occurred_at must NOT overwrite the first.
		if err := s.EnsureSubject(context.Background(), tx, Subject{
			ID: "e-1", Type: TypeEvent, CanonicalName: "Launch", OccurredAt: "2025-09",
		}, "run-2"); err != nil {
			t.Fatalf("ensure 2: %v", err)
		}
	})
	var occ string
	conn.QueryRow(`SELECT occurred_at FROM subjects WHERE id='e-1'`).Scan(&occ)
	if occ != "2024-01" {
		t.Fatalf("occurred_at = %q, want 2024-01 (first-writer-wins)", occ)
	}
}

// TestReadVersionNoPage proves a not-yet-created page reports version 0 (the base
// version slot merge records for a new subject).
func TestReadVersionNoPage(t *testing.T) {
	conn := newTestDB(t)
	s := NewStore(conn)
	v, err := s.ReadVersion(context.Background(), "missing")
	if err != nil {
		t.Fatalf("read version: %v", err)
	}
	if v != 0 {
		t.Fatalf("version = %d, want 0 for a no-page subject", v)
	}
}

// TestInsertAliasSameSubjectIsNoOp proves re-asserting a subject's OWN existing
// alias is a harmless idempotent no-op (the happy-path re-assertion P7b2 must not
// turn into a conflict).
func TestInsertAliasSameSubjectIsNoOp(t *testing.T) {
	conn := newTestDB(t)
	s := NewStore(conn)
	insertSubject(t, conn, "subj-1", TypeEntity, "Acme")

	withTx(t, conn, func(tx *sql.Tx) {
		a := Alias{Type: TypeEntity, Norm: "acme", SubjectID: "subj-1"}
		if err := s.InsertAlias(context.Background(), tx, a); err != nil {
			t.Fatalf("first insert: %v", err)
		}
		// Re-insert the same (type, norm) → same subject: idempotent no-op, no error.
		if err := s.InsertAlias(context.Background(), tx, a); err != nil {
			t.Fatalf("same-subject re-insert must be a no-op, got: %v", err)
		}
	})

	var n int
	conn.QueryRow(`SELECT COUNT(1) FROM aliases WHERE type=? AND norm='acme'`, TypeEntity).Scan(&n)
	if n != 1 {
		t.Fatalf("alias rows = %d, want exactly 1", n)
	}
}

// TestInsertAliasDifferentSubjectConflicts proves the DUPLICATE-MINT guard (P7b2):
// an alias whose (type, norm) already points at a DIFFERENT subject_id returns
// ErrAliasConflict (so the end-of-run transaction rolls back and the conflict loop
// restarts at resolve).
func TestInsertAliasDifferentSubjectConflicts(t *testing.T) {
	conn := newTestDB(t)
	s := NewStore(conn)
	insertSubject(t, conn, "winner", TypeEntity, "Acme")
	insertSubject(t, conn, "loser", TypeEntity, "Acme")

	// The winner commits the alias first.
	withTx(t, conn, func(tx *sql.Tx) {
		if err := s.InsertAlias(context.Background(), tx,
			Alias{Type: TypeEntity, Norm: "acme", SubjectID: "winner"}); err != nil {
			t.Fatalf("winner insert: %v", err)
		}
	})

	// The loser mints the same (type, norm) against a DIFFERENT subject → conflict.
	// Roll back the loser's tx before reading on the conn: an open write tx holds
	// SQLite's write lock and a separate-connection read would deadlock until
	// busy_timeout (the very hazard ReadPageTx documents).
	tx, err := conn.BeginTx(context.Background(), nil)
	if err != nil {
		t.Fatalf("begin: %v", err)
	}
	err = s.InsertAlias(context.Background(), tx,
		Alias{Type: TypeEntity, Norm: "acme", SubjectID: "loser"})
	tx.Rollback()
	if !errors.Is(err, ErrAliasConflict) {
		t.Fatalf("different-subject alias must yield ErrAliasConflict, got: %v", err)
	}

	// The winner's alias is untouched.
	var owner string
	conn.QueryRow(`SELECT subject_id FROM aliases WHERE type=? AND norm='acme'`, TypeEntity).Scan(&owner)
	if owner != "winner" {
		t.Fatalf("alias owner = %q, want winner (loser must not overwrite)", owner)
	}
}
