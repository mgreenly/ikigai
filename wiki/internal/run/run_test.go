package run

import (
	"context"
	"database/sql"
	"errors"
	"fmt"
	"path/filepath"
	"testing"
	"time"

	"wiki/internal/db"
	"wiki/internal/integrate"

	_ "modernc.org/sqlite"
)

// newStore stands up a migrated in-temp-dir SQLite DB and a run.Store with a
// deterministic id sequence and a fixed clock.
func newStore(t *testing.T) (*Store, *sql.DB) {
	t.Helper()
	dir := t.TempDir()
	conn, err := db.Open(filepath.Join(dir, "wiki.db"))
	if err != nil {
		t.Fatalf("open: %v", err)
	}
	t.Cleanup(func() { conn.Close() })
	if err := db.Migrate(context.Background(), conn); err != nil {
		t.Fatalf("migrate: %v", err)
	}
	var n int
	s, err := New(Options{
		DB:    conn,
		NewID: func() string { n++; return fmt.Sprintf("run-%02d", n) },
		Now:   func() time.Time { return time.Unix(1, 0).UTC() },
	})
	if err != nil {
		t.Fatalf("new store: %v", err)
	}
	return s, conn
}

// insertInbox inserts a pending inbox row so the stamp has a target.
func insertInbox(t *testing.T, conn *sql.DB, id, kind, source string) {
	t.Helper()
	_, err := conn.Exec(
		`INSERT INTO inbox (id, owner, kind, source, sha256, size, received_at, integrated_by)
		 VALUES (?, 'u@x', ?, ?, 'sha', 1, 1, '')`,
		id, kind, source)
	if err != nil {
		t.Fatalf("insert inbox: %v", err)
	}
}

func statusOf(t *testing.T, conn *sql.DB, runID string) string {
	t.Helper()
	var st string
	if err := conn.QueryRow(`SELECT status FROM runs WHERE id=?`, runID).Scan(&st); err != nil {
		t.Fatalf("status: %v", err)
	}
	return st
}

func stampOf(t *testing.T, conn *sql.DB, inboxID string) string {
	t.Helper()
	var by string
	if err := conn.QueryRow(`SELECT integrated_by FROM inbox WHERE id=?`, inboxID).Scan(&by); err != nil {
		t.Fatalf("stamp: %v", err)
	}
	return by
}

// TestBeginInsertsRunning proves the `running` row is the one write outside the
// commit (design §4.5).
func TestBeginInsertsRunning(t *testing.T) {
	s, conn := newStore(t)
	insertInbox(t, conn, "doc-1", "document", "mcp:x")
	id, err := s.Begin(context.Background(), "document-pass", "doc-1")
	if err != nil {
		t.Fatalf("begin: %v", err)
	}
	if got := statusOf(t, conn, id); got != StatusRunning {
		t.Fatalf("status = %q, want running", got)
	}
}

// TestCommitStampsSucceededAndInbox proves the end-of-run transaction atomically
// writes terminal succeeded + the inbox stamp, round-tripping a POPULATED Manifest
// (including the per-page base version slot) through the commit.
func TestCommitStampsSucceededAndInbox(t *testing.T) {
	s, conn := newStore(t)
	insertInbox(t, conn, "doc-1", "document", "mcp:x")
	id, _ := s.Begin(context.Background(), "document-pass", "doc-1")

	m := &integrate.Manifest{
		Subjects: []integrate.Subject{{
			Type: integrate.TypeEntity, Name: "Acme", Aliases: []string{"acme"},
			SubjectID: "subj-1", TargetPage: "subj-1", BaseVersion: 7,
			OccurredAt: "", Superseded: []string{"old-1"},
			Claims: []integrate.Claim{{Text: "c", Cites: []string{"doc-1"}}},
		}},
		StaleNotes: []integrate.StaleNote{{Subject: "subj-1", Note: "n", Cites: []string{"doc-1"}}},
		DupPairs:   []integrate.DupPair{{SubjectA: "a", SubjectB: "b"}},
	}
	if got := m.WriteSet(); len(got) != 1 || got[0] != "subj-1" {
		t.Fatalf("write set = %v, want [subj-1]", got)
	}

	if err := s.Commit(context.Background(), id, "doc-1", m, true); err != nil {
		t.Fatalf("commit: %v", err)
	}
	if got := statusOf(t, conn, id); got != StatusSucceeded {
		t.Fatalf("status = %q, want succeeded", got)
	}
	if got := stampOf(t, conn, "doc-1"); got != id {
		t.Fatalf("stamp = %q, want %q", got, id)
	}
}

// TestCommitNilManifestRejected — the transaction requires a Manifest.
func TestCommitNilManifestRejected(t *testing.T) {
	s, conn := newStore(t)
	insertInbox(t, conn, "doc-1", "document", "mcp:x")
	id, _ := s.Begin(context.Background(), "document-pass", "doc-1")
	if err := s.Commit(context.Background(), id, "doc-1", nil, true); err == nil {
		t.Fatal("expected nil-manifest rejection")
	}
}

// TestFailLeavesRowPending — a clean failure flips the run to failed and never
// stamps the inbox (the row stays pending for retry — design §4.5).
func TestFailLeavesRowPending(t *testing.T) {
	s, conn := newStore(t)
	insertInbox(t, conn, "doc-1", "document", "mcp:x")
	id, _ := s.Begin(context.Background(), "document-pass", "doc-1")
	if err := s.Fail(context.Background(), id, "doc-1", errors.New("boom")); err != nil {
		t.Fatalf("fail: %v", err)
	}
	if got := statusOf(t, conn, id); got != StatusFailed {
		t.Fatalf("status = %q, want failed", got)
	}
	if got := stampOf(t, conn, "doc-1"); got != "" {
		t.Fatalf("stamp = %q, want empty (row stays pending)", got)
	}
}

// TestSweepOrphans — the boot sweep flips orphaned running → crashed.
func TestSweepOrphans(t *testing.T) {
	s, conn := newStore(t)
	insertInbox(t, conn, "doc-1", "document", "mcp:x")
	id, _ := s.Begin(context.Background(), "document-pass", "doc-1")
	n, err := s.SweepOrphans(context.Background())
	if err != nil {
		t.Fatalf("sweep: %v", err)
	}
	if n != 1 {
		t.Fatalf("swept %d, want 1", n)
	}
	if got := statusOf(t, conn, id); got != StatusCrashed {
		t.Fatalf("status = %q, want crashed", got)
	}
}

// TestStampCronAllEntriesSucceeded — the completion-time join stamps only once
// every bound entry has a succeeded run caused by the cron row.
func TestStampCronAllEntriesSucceeded(t *testing.T) {
	s, conn := newStore(t)
	insertInbox(t, conn, "cron-1", "event", "cron:daily")

	// One bound entry succeeds → not yet (two bound).
	r1, _ := s.Begin(context.Background(), "crm-digest", "cron-1")
	if err := s.Commit(context.Background(), r1, "cron-1", &integrate.Manifest{}, false); err != nil {
		t.Fatalf("commit r1: %v", err)
	}
	stamped, err := s.StampCron(context.Background(), r1, "cron-1", []string{"crm-digest", "ledger-digest"})
	if err != nil {
		t.Fatalf("stampcron: %v", err)
	}
	if stamped {
		t.Fatal("stamped with one of two entries done")
	}
	if got := stampOf(t, conn, "cron-1"); got != "" {
		t.Fatalf("stamp = %q, want empty", got)
	}

	// Second bound entry succeeds → now stamp.
	r2, _ := s.Begin(context.Background(), "ledger-digest", "cron-1")
	if err := s.Commit(context.Background(), r2, "cron-1", &integrate.Manifest{}, false); err != nil {
		t.Fatalf("commit r2: %v", err)
	}
	stamped, err = s.StampCron(context.Background(), r2, "cron-1", []string{"crm-digest", "ledger-digest"})
	if err != nil {
		t.Fatalf("stampcron 2: %v", err)
	}
	if !stamped {
		t.Fatal("not stamped with both entries done")
	}
	if got := stampOf(t, conn, "cron-1"); got != r2 {
		t.Fatalf("stamp = %q, want %q", got, r2)
	}
}

// TestStampCronNoBoundJobsImmediate — a cron row no job binds is stamped
// immediately as a no-op (design §3).
func TestStampCronNoBoundJobsImmediate(t *testing.T) {
	s, conn := newStore(t)
	insertInbox(t, conn, "cron-1", "event", "cron:orphan")
	stamped, err := s.StampCron(context.Background(), "run-x", "cron-1", nil)
	if err != nil {
		t.Fatalf("stampcron: %v", err)
	}
	if !stamped {
		t.Fatal("no-job cron row not stamped immediately")
	}
	if got := stampOf(t, conn, "cron-1"); got != "run-x" {
		t.Fatalf("stamp = %q, want run-x", got)
	}
}
