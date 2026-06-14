package ingest

import (
	"context"
	"net/http"
	"net/http/httptest"
	"path/filepath"
	"testing"

	"wiki/internal/db"
	"wiki/internal/events"
	"wiki/internal/inbox"

	"database/sql"

	_ "modernc.org/sqlite"
)

func newSvc(t *testing.T) (*Service, *sql.DB, *inbox.Store) {
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
	box, err := inbox.New(inbox.Options{DB: conn, BlobRoot: dir, InlineMax: 4096, MaxBytes: 16})
	if err != nil {
		t.Fatalf("inbox: %v", err)
	}
	return New(box, conn, nil, nil), conn, box
}

func TestIngestTextReceipt(t *testing.T) {
	svc, _, _ := newSvc(t)
	rec, err := svc.IngestText(context.Background(), "u@x", "Title", "", "[]", []byte("hello"))
	if err != nil {
		t.Fatalf("ingest_text: %v", err)
	}
	if rec.ID == "" || rec.SHA256 == "" {
		t.Errorf("receipt incomplete: %+v", rec)
	}
}

// TestIngestTextDefaultSource: an empty caller source defaults to mcp:ingest_text.
func TestIngestTextDefaultSource(t *testing.T) {
	svc, conn, _ := newSvc(t)
	rec, _ := svc.IngestText(context.Background(), "u@x", "", "", "[]", []byte("hi"))
	var source string
	conn.QueryRow(`SELECT source FROM inbox WHERE id=?`, rec.ID).Scan(&source)
	if source != "mcp:ingest_text" {
		t.Errorf("source = %q", source)
	}
}

// TestIngestTextOversizedRefused: over the cap → ErrTooLarge + a refusal emit.
func TestIngestTextOversizedRefused(t *testing.T) {
	dir := t.TempDir()
	conn, _ := db.Open(filepath.Join(dir, "wiki.db"))
	t.Cleanup(func() { conn.Close() })
	db.Migrate(context.Background(), conn)
	box, _ := inbox.New(inbox.Options{DB: conn, BlobRoot: dir, InlineMax: 4096, MaxBytes: 4})
	refuser := &fakeRefuser{}
	svc := New(box, conn, refuser, nil)
	_, err := svc.IngestText(context.Background(), "u@x", "", "", "[]", []byte("way too long"))
	if err == nil {
		t.Fatalf("expected oversized refusal")
	}
	if len(refuser.refused) != 1 || refuser.refused[0].Door != "ingest_text" {
		t.Errorf("ingest_refused not emitted correctly: %+v", refuser.refused)
	}
}

func TestIngestURLFetchAndAccept(t *testing.T) {
	srv := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		w.Header().Set("Content-Type", "text/plain; charset=utf-8")
		w.Write([]byte("page"))
	}))
	defer srv.Close()
	dir := t.TempDir()
	conn, _ := db.Open(filepath.Join(dir, "wiki.db"))
	t.Cleanup(func() { conn.Close() })
	db.Migrate(context.Background(), conn)
	box, _ := inbox.New(inbox.Options{DB: conn, BlobRoot: dir, InlineMax: 4096, MaxBytes: 131072})
	svc := New(box, conn, nil, srv.Client())
	rec, err := svc.IngestURL(context.Background(), "u@x", srv.URL, "[]")
	if err != nil {
		t.Fatalf("ingest_url: %v", err)
	}
	var source, mime string
	conn.QueryRow(`SELECT source, mime FROM inbox WHERE id=?`, rec.ID).Scan(&source, &mime)
	if source != "url:"+srv.URL {
		t.Errorf("source = %q", source)
	}
	if mime != "text/plain" {
		t.Errorf("mime = %q (charset should be stripped)", mime)
	}
}

// TestStatusPending: a fresh, un-integrated arrival reports pending.
func TestStatusPending(t *testing.T) {
	svc, _, _ := newSvc(t)
	rec, _ := svc.IngestText(context.Background(), "u@x", "", "", "[]", []byte("hi"))
	st, err := svc.Status(context.Background(), rec.ID)
	if err != nil {
		t.Fatalf("status: %v", err)
	}
	if st.State != "pending" {
		t.Errorf("state = %q, want pending", st.State)
	}
}

// TestStatusReflectsRun: a succeeded run for the row reports succeeded; a dead row
// reports dead.
func TestStatusReflectsRun(t *testing.T) {
	svc, conn, _ := newSvc(t)
	rec, _ := svc.IngestText(context.Background(), "u@x", "", "", "[]", []byte("hi"))
	conn.Exec(`INSERT INTO runs (id, job, caused_by, status, started_at) VALUES ('R1','document-pass',?,?,1)`, rec.ID, "succeeded")
	st, _ := svc.Status(context.Background(), rec.ID)
	if st.State != "succeeded" {
		t.Errorf("state = %q, want succeeded", st.State)
	}

	conn.Exec(`UPDATE inbox SET dead_at=99 WHERE id=?`, rec.ID)
	st, _ = svc.Status(context.Background(), rec.ID)
	if st.State != "dead" {
		t.Errorf("state = %q, want dead", st.State)
	}
}

// TestStatusUnknownID: an unknown id is a not-found error.
func TestStatusUnknownID(t *testing.T) {
	svc, _, _ := newSvc(t)
	if _, err := svc.Status(context.Background(), "nope"); err == nil {
		t.Errorf("expected not-found for unknown id")
	}
}

type fakeRefuser struct{ refused []events.IngestRefused }

func (r *fakeRefuser) IngestRefused(_ context.Context, ev events.IngestRefused) error {
	r.refused = append(r.refused, ev)
	return nil
}
