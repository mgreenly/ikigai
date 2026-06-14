package inbox

import (
	"context"
	"crypto/sha256"
	"database/sql"
	"encoding/hex"
	"os"
	"path/filepath"
	"strings"
	"testing"

	"wiki/internal/db"

	_ "modernc.org/sqlite"
)

// newStore stands up a migrated in-temp-dir SQLite DB + a blob root and returns a
// Store with a deterministic id sequence so dup/identity assertions are stable.
func newStore(t *testing.T, inlineMax int, maxBytes int64) (*Store, *sql.DB, string) {
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
	st, err := New(Options{
		DB:        conn,
		BlobRoot:  dir,
		InlineMax: inlineMax,
		MaxBytes:  maxBytes,
		NewID:     func() string { n++; return "id-" + string(rune('A'+n-1)) },
	})
	if err != nil {
		t.Fatalf("new store: %v", err)
	}
	return st, conn, dir
}

func TestAcceptInlinePath(t *testing.T) {
	st, conn, _ := newStore(t, 4096, 131072)
	rec, err := st.Accept(context.Background(), "u@x", KindDocument, "mcp:ingest_text", "text/plain", "T", "[]", []byte("hello"))
	if err != nil {
		t.Fatalf("accept: %v", err)
	}
	sum := sha256.Sum256([]byte("hello"))
	if rec.SHA256 != hex.EncodeToString(sum[:]) {
		t.Errorf("sha256 not computed correctly: %q", rec.SHA256)
	}
	if rec.Dup {
		t.Errorf("first arrival should not be a dup")
	}
	var blob int
	var content []byte
	if err := conn.QueryRow(`SELECT blob, content FROM inbox WHERE id=?`, rec.ID).Scan(&blob, &content); err != nil {
		t.Fatalf("read row: %v", err)
	}
	if blob != 0 {
		t.Errorf("small payload should be inline (blob=0), got %d", blob)
	}
	if string(content) != "hello" {
		t.Errorf("inline content = %q", content)
	}
}

func TestAcceptSpillPath(t *testing.T) {
	st, conn, dir := newStore(t, 4, 131072) // inlineMax=4 forces spill of "hello"
	rec, err := st.Accept(context.Background(), "u@x", KindDocument, "mcp:ingest_text", "", "", "[]", []byte("hello"))
	if err != nil {
		t.Fatalf("accept: %v", err)
	}
	var blob int
	var content []byte
	if err := conn.QueryRow(`SELECT blob, content FROM inbox WHERE id=?`, rec.ID).Scan(&blob, &content); err != nil {
		t.Fatalf("read row: %v", err)
	}
	if blob != 1 {
		t.Errorf("large payload should spill (blob=1), got %d", blob)
	}
	if content != nil {
		t.Errorf("spilled row content should be NULL, got %q", content)
	}
	// Blob lives at blobs/<aa>/<sha256> and holds the bytes.
	bp := filepath.Join(dir, "blobs", rec.SHA256[:2], rec.SHA256)
	b, err := os.ReadFile(bp)
	if err != nil {
		t.Fatalf("read blob: %v", err)
	}
	if string(b) != "hello" {
		t.Errorf("blob content = %q", b)
	}
}

// TestSHA256AlwaysStored: both paths compute and store the hash.
func TestSHA256AlwaysStored(t *testing.T) {
	st, conn, _ := newStore(t, 4, 131072)
	for _, payload := range []string{"x", "a-much-longer-payload-that-spills"} {
		rec, err := st.Accept(context.Background(), "u@x", KindDocument, "s", "", "", "[]", []byte(payload))
		if err != nil {
			t.Fatalf("accept: %v", err)
		}
		var stored string
		if err := conn.QueryRow(`SELECT sha256 FROM inbox WHERE id=?`, rec.ID).Scan(&stored); err != nil {
			t.Fatalf("read sha: %v", err)
		}
		sum := sha256.Sum256([]byte(payload))
		if stored != hex.EncodeToString(sum[:]) {
			t.Errorf("payload %q: stored sha %q wrong", payload, stored)
		}
	}
}

// TestDupFlagOnReAccept: identical bytes → two rows, second receipt Dup=true.
func TestDupFlagOnReAccept(t *testing.T) {
	st, conn, _ := newStore(t, 4096, 131072)
	r1, _ := st.Accept(context.Background(), "u@x", KindDocument, "s", "", "", "[]", []byte("same"))
	r2, _ := st.Accept(context.Background(), "u@x", KindDocument, "s", "", "", "[]", []byte("same"))
	if r1.Dup {
		t.Errorf("first should not be dup")
	}
	if !r2.Dup {
		t.Errorf("second identical-bytes arrival should be dup")
	}
	if r1.ID == r2.ID {
		t.Errorf("two arrivals must be two distinct rows")
	}
	var count int
	conn.QueryRow(`SELECT COUNT(*) FROM inbox WHERE sha256=?`, r1.SHA256).Scan(&count)
	if count != 2 {
		t.Errorf("identical content should yield 2 rows (no UNIQUE on sha256), got %d", count)
	}
}

// TestOversizedRefused: a payload over the cap is refused loudly with ErrTooLarge
// and writes NO row.
func TestOversizedRefused(t *testing.T) {
	st, conn, _ := newStore(t, 4096, 8) // cap = 8 bytes
	_, err := st.Accept(context.Background(), "u@x", KindDocument, "s", "", "", "[]", []byte("way too long"))
	if err == nil || !strings.Contains(err.Error(), "size cap") {
		t.Fatalf("expected ErrTooLarge, got %v", err)
	}
	var count int
	conn.QueryRow(`SELECT COUNT(*) FROM inbox`).Scan(&count)
	if count != 0 {
		t.Errorf("oversized refusal must write no row, found %d", count)
	}
}

// TestReadPayloadBothPaths: ReadPayload dispatches on the row, returning bytes for
// both inline and spilled storage.
func TestReadPayloadBothPaths(t *testing.T) {
	st, conn, _ := newStore(t, 5, 131072)
	for _, payload := range []string{"tiny", "this-one-spills-past-five"} {
		rec, err := st.Accept(context.Background(), "u@x", KindDocument, "s", "", "", "[]", []byte(payload))
		if err != nil {
			t.Fatalf("accept: %v", err)
		}
		var row Row
		var blob int
		if err := conn.QueryRow(`SELECT sha256, content, blob FROM inbox WHERE id=?`, rec.ID).
			Scan(&row.SHA256, &row.Content, &blob); err != nil {
			t.Fatalf("read row: %v", err)
		}
		row.Blob = blob == 1
		got, err := st.ReadPayload(row)
		if err != nil {
			t.Fatalf("ReadPayload: %v", err)
		}
		if string(got) != payload {
			t.Errorf("ReadPayload = %q, want %q", got, payload)
		}
	}
}

// TestNudgeFiresAfterCommit: a configured nudge is invoked once per successful
// accept (the worker doorbell, no-op until P4 wires it).
func TestNudgeFiresAfterCommit(t *testing.T) {
	dir := t.TempDir()
	conn, _ := db.Open(filepath.Join(dir, "wiki.db"))
	t.Cleanup(func() { conn.Close() })
	db.Migrate(context.Background(), conn)
	var nudges int
	st, err := New(Options{DB: conn, BlobRoot: dir, InlineMax: 4096, MaxBytes: 131072, Nudge: func() { nudges++ }})
	if err != nil {
		t.Fatalf("new: %v", err)
	}
	st.Accept(context.Background(), "u@x", KindDocument, "s", "", "", "[]", []byte("hi"))
	if nudges != 1 {
		t.Errorf("nudge fired %d times, want 1", nudges)
	}
}

// TestPendingDefault: a fresh arrival is pending (integrated_by=''), so a crash
// before integration loses nothing — the row is durable and still selectable.
func TestPendingDefault(t *testing.T) {
	st, conn, _ := newStore(t, 4096, 131072)
	rec, _ := st.Accept(context.Background(), "u@x", KindDocument, "s", "", "", "[]", []byte("hi"))
	var integratedBy string
	conn.QueryRow(`SELECT integrated_by FROM inbox WHERE id=?`, rec.ID).Scan(&integratedBy)
	if integratedBy != "" {
		t.Errorf("fresh arrival should be pending (integrated_by=''), got %q", integratedBy)
	}
}

// TestTagsValidation: a non-JSON tags string is rejected; empty defaults to "[]".
func TestTagsValidation(t *testing.T) {
	st, conn, _ := newStore(t, 4096, 131072)
	if _, err := st.Accept(context.Background(), "u@x", KindDocument, "s", "", "", "not json", []byte("x")); err == nil {
		t.Errorf("invalid tags JSON should be rejected")
	}
	rec, err := st.Accept(context.Background(), "u@x", KindDocument, "s", "", "", "", []byte("y"))
	if err != nil {
		t.Fatalf("empty tags accept: %v", err)
	}
	var tags string
	conn.QueryRow(`SELECT tags FROM inbox WHERE id=?`, rec.ID).Scan(&tags)
	if tags != "[]" {
		t.Errorf("empty tags should default to [], got %q", tags)
	}
}
