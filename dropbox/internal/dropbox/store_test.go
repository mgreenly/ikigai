package dropbox

import (
	"context"
	"database/sql"
	"errors"
	"testing"

	"dropbox/internal/db"

	_ "modernc.org/sqlite"
)

// openStoreDB opens a real temp sqlite DB and runs the chassis migrations so the
// store exercises the actual 002_dropbox schema.
func openStoreDB(t *testing.T) *sql.DB {
	t.Helper()
	conn, err := sql.Open("sqlite", "file:"+t.TempDir()+"/dropbox.db?_pragma=foreign_keys(ON)")
	if err != nil {
		t.Fatalf("open: %v", err)
	}
	conn.SetMaxOpenConns(1)
	t.Cleanup(func() { conn.Close() })
	if err := db.Migrate(context.Background(), conn); err != nil {
		t.Fatalf("migrate: %v", err)
	}
	return conn
}

// withTx runs fn inside a single tx and commits, the way the engine composes
// store calls. Fatals on any error.
func withTx(t *testing.T, conn *sql.DB, fn func(tx *sql.Tx)) {
	t.Helper()
	tx, err := conn.Begin()
	if err != nil {
		t.Fatalf("begin: %v", err)
	}
	fn(tx)
	if err := tx.Commit(); err != nil {
		t.Fatalf("commit: %v", err)
	}
}

func TestUpsertGetRoundTripCaseInsensitive(t *testing.T) {
	conn := openStoreDB(t)
	s := NewStore()

	withTx(t, conn, func(tx *sql.Tx) {
		if err := s.UpsertFile(tx, "/Inbox/Report.pdf", "rev1", "hash1", 100, "2026-06-04T00:00:00Z"); err != nil {
			t.Fatalf("upsert: %v", err)
		}
	})

	// Exact-case round-trip.
	withTx(t, conn, func(tx *sql.Tx) {
		fr, err := s.GetFile(tx, "/Inbox/Report.pdf")
		if err != nil {
			t.Fatalf("get exact: %v", err)
		}
		if fr.Path != "/Inbox/Report.pdf" || fr.Rev != "rev1" || fr.Size != 100 {
			t.Fatalf("round-trip mismatch: %+v", fr)
		}
		if fr.PathLower != "/inbox/report.pdf" {
			t.Fatalf("path_lower not folded: %q", fr.PathLower)
		}
	})

	// Case-mismatched query still hits the row and returns the verbatim display path.
	withTx(t, conn, func(tx *sql.Tx) {
		fr, err := s.GetFile(tx, "/inbox/REPORT.PDF")
		if err != nil {
			t.Fatalf("get case-mismatch: %v", err)
		}
		if fr.Path != "/Inbox/Report.pdf" {
			t.Fatalf("expected display path preserved, got %q", fr.Path)
		}
	})

	// Upsert on a case-only-different display path updates the same row (PK on path
	// is exact, so verify the folded lookup still resolves to one row with new rev).
	withTx(t, conn, func(tx *sql.Tx) {
		if err := s.UpsertFile(tx, "/Inbox/Report.pdf", "rev2", "hash2", 200, "2026-06-04T01:00:00Z"); err != nil {
			t.Fatalf("upsert update: %v", err)
		}
	})
	withTx(t, conn, func(tx *sql.Tx) {
		fr, err := s.GetFile(tx, "/inbox/report.pdf")
		if err != nil {
			t.Fatalf("get after update: %v", err)
		}
		if fr.Rev != "rev2" || fr.Size != 200 {
			t.Fatalf("update not applied: %+v", fr)
		}
	})

	// Missing path → ErrNotFound.
	withTx(t, conn, func(tx *sql.Tx) {
		if _, err := s.GetFile(tx, "/nope.txt"); !errors.Is(err, ErrNotFound) {
			t.Fatalf("expected ErrNotFound, got %v", err)
		}
	})
}

func TestDeleteSubtreePrefixBoundary(t *testing.T) {
	conn := openStoreDB(t)
	s := NewStore()

	seed := []string{
		"/foo",          // the folder path itself (if indexed as a file)
		"/foo/bar.txt",  // under prefix
		"/foo/a/b.txt",  // deeper under prefix
		"/Foo/Baz.txt",  // under prefix, different case
		"/foobar",       // sibling — must NOT match
		"/foobar/x.txt", // sibling subtree — must NOT match
		"/other.txt",    // unrelated
	}
	withTx(t, conn, func(tx *sql.Tx) {
		for i, p := range seed {
			if err := s.UpsertFile(tx, p, "rev", "hash", int64(i+1), "2026-06-04T00:00:00Z"); err != nil {
				t.Fatalf("seed %s: %v", p, err)
			}
		}
	})

	var deleted []FileRow
	withTx(t, conn, func(tx *sql.Tx) {
		var err error
		deleted, err = s.DeleteSubtree(tx, "/Foo") // case-mismatched folder path
		if err != nil {
			t.Fatalf("delete subtree: %v", err)
		}
	})

	gotDeleted := map[string]bool{}
	for _, fr := range deleted {
		gotDeleted[fr.Path] = true
	}
	wantDeleted := []string{"/foo", "/foo/bar.txt", "/foo/a/b.txt", "/Foo/Baz.txt"}
	if len(deleted) != len(wantDeleted) {
		t.Fatalf("deleted %d rows, want %d: %v", len(deleted), len(wantDeleted), gotDeleted)
	}
	for _, p := range wantDeleted {
		if !gotDeleted[p] {
			t.Fatalf("expected %q deleted (display path returned), got %v", p, gotDeleted)
		}
	}

	// Siblings and unrelated rows survive.
	withTx(t, conn, func(tx *sql.Tx) {
		for _, p := range []string{"/foobar", "/foobar/x.txt", "/other.txt"} {
			if _, err := s.GetFile(tx, p); err != nil {
				t.Fatalf("sibling %q should survive: %v", p, err)
			}
		}
		// The deleted prefix rows are gone.
		if _, err := s.GetFile(tx, "/foo/bar.txt"); !errors.Is(err, ErrNotFound) {
			t.Fatalf("expected /foo/bar.txt gone, got %v", err)
		}
	})
}

func TestListFiles(t *testing.T) {
	conn := openStoreDB(t)
	s := NewStore()

	// Seed a mix of subtrees and a sibling that must not match the /foo prefix.
	// Insert order is intentionally NOT path_lower order to prove ORDER BY.
	seed := []string{
		"/other.txt",
		"/foo/b.txt",
		"/Foo/A.txt", // case-different display, folds under /foo
		"/foo",       // the folder path itself
		"/foobar",    // sibling — must NOT match /foo
		"/foo/sub/c.txt",
	}
	withTx(t, conn, func(tx *sql.Tx) {
		for i, p := range seed {
			if err := s.UpsertFile(tx, p, "rev", "hash", int64(i+1), "2026-06-04T00:00:00Z"); err != nil {
				t.Fatalf("seed %s: %v", p, err)
			}
		}
	})

	collect := func(prefix, after string, limit int) []string {
		var got []string
		withTx(t, conn, func(tx *sql.Tx) {
			rows, err := s.ListFiles(tx, prefix, after, limit)
			if err != nil {
				t.Fatalf("list files: %v", err)
			}
			for _, fr := range rows {
				got = append(got, fr.PathLower)
			}
		})
		return got
	}

	// Empty prefix returns every row in path_lower order.
	all := collect("", "", 100)
	wantAll := []string{
		"/foo", "/foo/a.txt", "/foo/b.txt", "/foo/sub/c.txt", "/foobar", "/other.txt",
	}
	if !equalStrings(all, wantAll) {
		t.Fatalf("empty prefix = %v, want %v", all, wantAll)
	}

	// Prefix scoping: /foo matches /foo and its subtree, NOT /foobar.
	scoped := collect("/foo", "", 100)
	wantScoped := []string{"/foo", "/foo/a.txt", "/foo/b.txt", "/foo/sub/c.txt"}
	if !equalStrings(scoped, wantScoped) {
		t.Fatalf("/foo prefix = %v, want %v", scoped, wantScoped)
	}

	// limit bounds the page.
	page1 := collect("/foo", "", 2)
	if !equalStrings(page1, []string{"/foo", "/foo/a.txt"}) {
		t.Fatalf("limit page1 = %v", page1)
	}

	// after cursor stitches the next page with no overlap/gap.
	page2 := collect("/foo", page1[len(page1)-1], 2)
	if !equalStrings(page2, []string{"/foo/b.txt", "/foo/sub/c.txt"}) {
		t.Fatalf("cursor page2 = %v", page2)
	}
}

// TestListFilesService_FoldsPrefix exercises Service.List end to end: it folds
// the incoming display-path prefix, so a mixed-case /FOO scopes to the folded
// subtree, and "/" means "list everything".
func TestListFilesService_FoldsPrefix(t *testing.T) {
	conn := openStoreDB(t)
	svc := NewService(conn)

	seed := []string{"/Foo/A.txt", "/foo/b.txt", "/other.txt"}
	withTx(t, conn, func(tx *sql.Tx) {
		for i, p := range seed {
			if err := svc.Store.UpsertFile(tx, p, "rev", "hash", int64(i+1), "t"); err != nil {
				t.Fatalf("seed %s: %v", p, err)
			}
		}
	})

	// Mixed-case prefix folds and scopes to the /foo subtree.
	rows, err := svc.List("/FOO", "", 100)
	if err != nil {
		t.Fatalf("list /FOO: %v", err)
	}
	var got []string
	for _, fr := range rows {
		got = append(got, fr.PathLower)
	}
	if !equalStrings(got, []string{"/foo/a.txt", "/foo/b.txt"}) {
		t.Fatalf("Service.List(/FOO) = %v", got)
	}

	// "/" means list everything (no prefix bound).
	rows, err = svc.List("/", "", 100)
	if err != nil {
		t.Fatalf("list /: %v", err)
	}
	if len(rows) != len(seed) {
		t.Fatalf("Service.List(/) returned %d rows, want %d", len(rows), len(seed))
	}
}

func equalStrings(a, b []string) bool {
	if len(a) != len(b) {
		return false
	}
	for i := range a {
		if a[i] != b[i] {
			return false
		}
	}
	return true
}

func TestTotalSize(t *testing.T) {
	conn := openStoreDB(t)
	s := NewStore()

	// Empty → 0.
	withTx(t, conn, func(tx *sql.Tx) {
		total, err := s.TotalSize(tx)
		if err != nil {
			t.Fatalf("total empty: %v", err)
		}
		if total != 0 {
			t.Fatalf("expected 0 on empty, got %d", total)
		}
	})

	sizes := []int64{10, 25, 100}
	var want int64
	withTx(t, conn, func(tx *sql.Tx) {
		for i, sz := range sizes {
			want += sz
			if err := s.UpsertFile(tx, "/f"+string(rune('a'+i)), "rev", "hash", sz, "t"); err != nil {
				t.Fatalf("seed: %v", err)
			}
		}
	})
	withTx(t, conn, func(tx *sql.Tx) {
		total, err := s.TotalSize(tx)
		if err != nil {
			t.Fatalf("total: %v", err)
		}
		if total != want {
			t.Fatalf("SUM(size) = %d, want %d", total, want)
		}
	})
}

func TestMarkErrorAndFailedCount(t *testing.T) {
	conn := openStoreDB(t)
	s := NewStore()

	withTx(t, conn, func(tx *sql.Tx) {
		if err := s.UpsertFile(tx, "/poison.bin", "rev", "hash", 5, "t"); err != nil {
			t.Fatalf("seed poison: %v", err)
		}
		if err := s.UpsertFile(tx, "/ok.txt", "rev", "hash", 5, "t"); err != nil {
			t.Fatalf("seed ok: %v", err)
		}
	})

	// No errors yet.
	withTx(t, conn, func(tx *sql.Tx) {
		n, err := s.FailedFiles(tx)
		if err != nil {
			t.Fatalf("failed count: %v", err)
		}
		if n != 0 {
			t.Fatalf("expected 0 failed, got %d", n)
		}
	})

	withTx(t, conn, func(tx *sql.Tx) {
		if err := s.MarkError(tx, "/Poison.bin", "disk full"); err != nil { // case-mismatch
			t.Fatalf("mark error: %v", err)
		}
	})

	withTx(t, conn, func(tx *sql.Tx) {
		fr, err := s.GetFile(tx, "/poison.bin")
		if err != nil {
			t.Fatalf("get poison: %v", err)
		}
		if !fr.Error.Valid || fr.Error.String != "disk full" {
			t.Fatalf("error column not set: %+v", fr.Error)
		}
		n, err := s.FailedFiles(tx)
		if err != nil {
			t.Fatalf("failed count: %v", err)
		}
		if n != 1 {
			t.Fatalf("expected 1 failed, got %d", n)
		}
	})

	// A successful re-upsert clears the poison error.
	withTx(t, conn, func(tx *sql.Tx) {
		if err := s.UpsertFile(tx, "/poison.bin", "rev2", "hash2", 6, "t2"); err != nil {
			t.Fatalf("re-upsert: %v", err)
		}
		n, err := s.FailedFiles(tx)
		if err != nil {
			t.Fatalf("failed count: %v", err)
		}
		if n != 0 {
			t.Fatalf("expected error cleared, got %d failed", n)
		}
	})

	// MarkError on an absent path → ErrNotFound.
	withTx(t, conn, func(tx *sql.Tx) {
		if err := s.MarkError(tx, "/gone.txt", "x"); !errors.Is(err, ErrNotFound) {
			t.Fatalf("expected ErrNotFound, got %v", err)
		}
	})
}

func TestCursorRoundTrip(t *testing.T) {
	conn := openStoreDB(t)
	s := NewStore()

	// First boot: no cursor.
	withTx(t, conn, func(tx *sql.Tx) {
		_, ok, err := s.GetCursor(tx)
		if err != nil {
			t.Fatalf("get cursor: %v", err)
		}
		if ok {
			t.Fatalf("expected no cursor on first boot")
		}
	})

	withTx(t, conn, func(tx *sql.Tx) {
		if err := s.SetCursor(tx, "cursor-abc", "2026-06-04T00:00:00Z"); err != nil {
			t.Fatalf("set cursor: %v", err)
		}
	})
	withTx(t, conn, func(tx *sql.Tx) {
		c, ok, err := s.GetCursor(tx)
		if err != nil {
			t.Fatalf("get cursor: %v", err)
		}
		if !ok || c != "cursor-abc" {
			t.Fatalf("cursor round-trip failed: ok=%v c=%q", ok, c)
		}
	})

	// Update the singleton.
	withTx(t, conn, func(tx *sql.Tx) {
		if err := s.SetCursor(tx, "cursor-def", "2026-06-04T01:00:00Z"); err != nil {
			t.Fatalf("update cursor: %v", err)
		}
	})
	withTx(t, conn, func(tx *sql.Tx) {
		c, _, err := s.GetCursor(tx)
		if err != nil {
			t.Fatalf("get cursor: %v", err)
		}
		if c != "cursor-def" {
			t.Fatalf("expected updated cursor, got %q", c)
		}
	})
}
