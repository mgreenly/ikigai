package dropbox

import (
	"encoding/json"
	"net/http"
	"net/http/httptest"
	"net/url"
	"testing"
)

// listResponse is the decoded /list JSON body shape ({files:[…], next_cursor?}).
type listResponse struct {
	Files []struct {
		Path      string `json:"path"`
		Size      int64  `json:"size"`
		Hash      string `json:"hash"`
		Rev       string `json:"rev"`
		UpdatedAt string `json:"updated_at"`
	} `json:"files"`
	NextCursor string `json:"next_cursor"`
}

// doList issues a GET against the ListHandler with the given query and returns
// the recorder. It reuses newContentService's seeding helpers (content_test.go).
func doList(t *testing.T, h http.Handler, q url.Values) *httptest.ResponseRecorder {
	t.Helper()
	req := httptest.NewRequest(http.MethodGet, "/list?"+q.Encode(), nil)
	rec := httptest.NewRecorder()
	h.ServeHTTP(rec, req)
	return rec
}

func TestListHandler_LeavesLoopbackGuardToCompositionRoot(t *testing.T) {
	svc, conn, mirror := newContentService(t)
	seedFile(t, svc, conn, mirror, "/a.txt", "rev1", []byte("a"))
	h := svc.ListHandler()

	// Transport headers are deliberately not interpreted by the domain handler;
	// the composition root owns the shared loopback guard.
	for _, hdr := range []string{"X-Forwarded-Proto", "X-Owner-Email"} {
		req := httptest.NewRequest(http.MethodGet, "/list", nil)
		req.Header.Set(hdr, "set")
		rec := httptest.NewRecorder()
		h.ServeHTTP(rec, req)
		if rec.Code != http.StatusOK {
			t.Fatalf("with %s present: status = %d, want domain handler response", hdr, rec.Code)
		}
	}
}

func TestListHandler_HappyPathOrderedFullHash(t *testing.T) {
	svc, conn, mirror := newContentService(t)
	// Seed out of order; the index orders by path_lower, so the response is sorted.
	seedFile(t, svc, conn, mirror, "/b.txt", "rev-b", []byte("bbbb"))
	seedFile(t, svc, conn, mirror, "/a.txt", "rev-a", []byte("aa"))

	h := svc.ListHandler()
	rec := doList(t, h, url.Values{})
	if rec.Code != http.StatusOK {
		t.Fatalf("status = %d, want 200 (body %q)", rec.Code, rec.Body.String())
	}
	if ct := rec.Header().Get("Content-Type"); ct != "application/json" {
		t.Fatalf("Content-Type = %q, want application/json", ct)
	}

	var resp listResponse
	if err := json.Unmarshal(rec.Body.Bytes(), &resp); err != nil {
		t.Fatalf("decode: %v", err)
	}
	if len(resp.Files) != 2 {
		t.Fatalf("files = %d, want 2", len(resp.Files))
	}
	if resp.Files[0].Path != "/a.txt" || resp.Files[1].Path != "/b.txt" {
		t.Fatalf("order = [%q, %q], want [/a.txt /b.txt]", resp.Files[0].Path, resp.Files[1].Path)
	}
	// Full hash (ADR decision 4): seedFile stores "hash-"+rev as content_hash, so
	// the response must carry the whole value, not an 8-char abbreviation.
	if resp.Files[0].Hash != "hash-rev-a" {
		t.Fatalf("hash = %q, want full %q", resp.Files[0].Hash, "hash-rev-a")
	}
	if resp.Files[0].Size != 2 || resp.Files[0].Rev != "rev-a" {
		t.Fatalf("size/rev = %d/%q, want 2/rev-a", resp.Files[0].Size, resp.Files[0].Rev)
	}
	// A short page (len < limit) carries no next_cursor.
	if resp.NextCursor != "" {
		t.Fatalf("next_cursor = %q, want empty on a short page", resp.NextCursor)
	}
}

func TestListHandler_PrefixScoping(t *testing.T) {
	svc, conn, mirror := newContentService(t)
	seedFile(t, svc, conn, mirror, "/notes/x.md", "r1", []byte("x"))
	seedFile(t, svc, conn, mirror, "/notes/y.md", "r2", []byte("y"))
	seedFile(t, svc, conn, mirror, "/other/z.md", "r3", []byte("z"))

	h := svc.ListHandler()
	q := url.Values{}
	q.Set("path", "/notes")
	rec := doList(t, h, q)
	if rec.Code != http.StatusOK {
		t.Fatalf("status = %d, want 200", rec.Code)
	}
	var resp listResponse
	if err := json.Unmarshal(rec.Body.Bytes(), &resp); err != nil {
		t.Fatalf("decode: %v", err)
	}
	if len(resp.Files) != 2 {
		t.Fatalf("files = %d, want 2 under /notes", len(resp.Files))
	}
	for _, f := range resp.Files {
		if f.Path != "/notes/x.md" && f.Path != "/notes/y.md" {
			t.Fatalf("unexpected path %q in /notes scope", f.Path)
		}
	}
}

func TestListHandler_LimitClamp(t *testing.T) {
	svc, conn, mirror := newContentService(t)
	seedFile(t, svc, conn, mirror, "/a.txt", "r1", []byte("a"))
	seedFile(t, svc, conn, mirror, "/b.txt", "r2", []byte("b"))
	h := svc.ListHandler()

	// limit=0 and a >1000 value both clamp to the 1000 default → both files
	// returned, no pagination.
	for _, lim := range []string{"0", "5000", "-3", "notanint"} {
		q := url.Values{}
		q.Set("limit", lim)
		rec := doList(t, h, q)
		var resp listResponse
		if err := json.Unmarshal(rec.Body.Bytes(), &resp); err != nil {
			t.Fatalf("limit=%s decode: %v", lim, err)
		}
		if len(resp.Files) != 2 || resp.NextCursor != "" {
			t.Fatalf("limit=%s: files=%d next=%q, want 2 files no cursor", lim, len(resp.Files), resp.NextCursor)
		}
	}
}

func TestListHandler_PaginationViaCursor(t *testing.T) {
	svc, conn, mirror := newContentService(t)
	seedFile(t, svc, conn, mirror, "/a.txt", "r1", []byte("a"))
	seedFile(t, svc, conn, mirror, "/b.txt", "r2", []byte("b"))
	seedFile(t, svc, conn, mirror, "/c.txt", "r3", []byte("c"))
	h := svc.ListHandler()

	// limit=2 → a full first page with next_cursor.
	q := url.Values{}
	q.Set("limit", "2")
	rec := doList(t, h, q)
	var page1 listResponse
	if err := json.Unmarshal(rec.Body.Bytes(), &page1); err != nil {
		t.Fatalf("page1 decode: %v", err)
	}
	if len(page1.Files) != 2 {
		t.Fatalf("page1 files = %d, want 2", len(page1.Files))
	}
	if page1.NextCursor == "" {
		t.Fatalf("page1 next_cursor empty, want a cursor on a full page")
	}
	if page1.Files[0].Path != "/a.txt" || page1.Files[1].Path != "/b.txt" {
		t.Fatalf("page1 = [%q, %q], want [/a.txt /b.txt]", page1.Files[0].Path, page1.Files[1].Path)
	}

	// Pass the cursor back → the remaining tail, a short page that terminates.
	q2 := url.Values{}
	q2.Set("limit", "2")
	q2.Set("cursor", page1.NextCursor)
	rec2 := doList(t, h, q2)
	var page2 listResponse
	if err := json.Unmarshal(rec2.Body.Bytes(), &page2); err != nil {
		t.Fatalf("page2 decode: %v", err)
	}
	if len(page2.Files) != 1 || page2.Files[0].Path != "/c.txt" {
		t.Fatalf("page2 = %d files (first %v), want 1 (/c.txt)", len(page2.Files), page2.Files)
	}
	if page2.NextCursor != "" {
		t.Fatalf("page2 next_cursor = %q, want empty (terminates on short page)", page2.NextCursor)
	}
}
