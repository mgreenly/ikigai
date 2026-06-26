package sites

import (
	"context"
	"net/http"
	"net/http/httptest"
	"os"
	"path/filepath"
	"sort"
	"testing"
)

// readWorking walks dir and returns the set of regular files relative to dir
// (slash-separated), mirroring how the sync verb obtains existingRel.
func readWorking(t *testing.T, dir string) []string {
	t.Helper()
	var out []string
	err := filepath.WalkDir(dir, func(p string, d os.DirEntry, err error) error {
		if err != nil {
			return err
		}
		if d.IsDir() {
			return nil
		}
		rel, _ := filepath.Rel(dir, p)
		out = append(out, filepath.ToSlash(rel))
		return nil
	})
	if err != nil {
		t.Fatalf("walk %q: %v", dir, err)
	}
	sort.Strings(out)
	return out
}

func TestReconcile_WritesOverwritesDeletes(t *testing.T) {
	root := t.TempDir()

	// Seed an existing working tree: keep.html (overwritten), old.css (deleted,
	// absent upstream), and a nested stale file (also deleted).
	mustWrite(t, root, "keep.html", "OLD")
	mustWrite(t, root, "old.css", "stale")
	mustWrite(t, root, "sub/gone.js", "stale")

	desired := map[string][]byte{
		"keep.html":      []byte("NEW"),      // overwrite
		"new.txt":        []byte("fresh"),    // brand new
		"assets/img.bin": []byte{0x00, 0xff}, // new, binary-safe (no UTF-8 check)
	}
	existing := readWorking(t, root)

	written, deleted, err := Reconcile(root, desired, existing)
	if err != nil {
		t.Fatalf("Reconcile: %v", err)
	}
	if written != 3 {
		t.Errorf("written = %d, want 3", written)
	}
	if deleted != 2 {
		t.Errorf("deleted = %d, want 2", deleted)
	}

	// keep.html overwritten.
	if got := mustRead(t, root, "keep.html"); got != "NEW" {
		t.Errorf("keep.html = %q, want NEW", got)
	}
	// new files present.
	if got := mustRead(t, root, "new.txt"); got != "fresh" {
		t.Errorf("new.txt = %q, want fresh", got)
	}
	if got := mustRead(t, root, "assets/img.bin"); got != "\x00\xff" {
		t.Errorf("assets/img.bin = %q, want binary bytes", got)
	}
	// deleted files gone.
	for _, p := range []string{"old.css", "sub/gone.js"} {
		if _, err := os.Stat(filepath.Join(root, filepath.FromSlash(p))); !os.IsNotExist(err) {
			t.Errorf("%s should be deleted, stat err = %v", p, err)
		}
	}

	// Final path set matches desired exactly.
	want := []string{"assets/img.bin", "keep.html", "new.txt"}
	got := readWorking(t, root)
	if len(got) != len(want) {
		t.Fatalf("final set = %v, want %v", got, want)
	}
	for i := range want {
		if got[i] != want[i] {
			t.Fatalf("final set = %v, want %v", got, want)
		}
	}
}

func TestReconcile_PathEscapeRejected(t *testing.T) {
	root := t.TempDir()
	// A sentinel directory a sibling to root; the escape must not write into it.
	outside := t.TempDir()

	desired := map[string][]byte{
		"../" + filepath.Base(outside) + "/evil.txt": []byte("pwned"),
	}
	written, deleted, err := Reconcile(root, desired, nil)
	if err == nil {
		t.Fatalf("expected confinement error, got nil (written=%d deleted=%d)", written, deleted)
	}
	// Nothing written anywhere.
	if entries, _ := os.ReadDir(root); len(entries) != 0 {
		t.Errorf("root should be empty after rejected escape, has %d entries", len(entries))
	}
	if _, statErr := os.Stat(filepath.Join(outside, "evil.txt")); !os.IsNotExist(statErr) {
		t.Errorf("escape wrote outside root: stat err = %v", statErr)
	}
}

func TestReconcile_AbsolutePathRejected(t *testing.T) {
	root := t.TempDir()
	desired := map[string][]byte{
		"/etc/evil": []byte("x"),
	}
	if _, _, err := Reconcile(root, desired, nil); err == nil {
		t.Fatal("expected error for absolute path key, got nil")
	}
}

func TestHTTPMirrorClient_ListPagination(t *testing.T) {
	// Fake /list returning two pages stitched by the cursor loop. Page 1 carries
	// next_cursor; page 2 omits it (terminates).
	var seenCursors []string
	srv := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		if r.URL.Path != "/list" {
			http.Error(w, "not found", http.StatusNotFound)
			return
		}
		cursor := r.URL.Query().Get("cursor")
		seenCursors = append(seenCursors, cursor)
		w.Header().Set("Content-Type", "application/json")
		if cursor == "" {
			_, _ = w.Write([]byte(`{"files":[
				{"path":"/site/a.html","size":3,"hash":"HASH_A","rev":"r1","updated_at":"t1"},
				{"path":"/site/b.css","size":4,"hash":"HASH_B","rev":"r2","updated_at":"t2"}
			],"next_cursor":"/site/b.css"}`))
			return
		}
		// Second page: no next_cursor ⇒ terminate.
		_, _ = w.Write([]byte(`{"files":[
			{"path":"/site/c.js","size":5,"hash":"HASH_C","rev":"r3","updated_at":"t3"}
		]}`))
	}))
	defer srv.Close()

	c := NewMirrorClient(srv.URL)
	files, err := c.List(context.Background(), "/site")
	if err != nil {
		t.Fatalf("List: %v", err)
	}
	if len(files) != 3 {
		t.Fatalf("stitched files = %d, want 3", len(files))
	}
	wantPaths := []string{"/site/a.html", "/site/b.css", "/site/c.js"}
	for i, w := range wantPaths {
		if files[i].Path != w {
			t.Errorf("files[%d].Path = %q, want %q", i, files[i].Path, w)
		}
	}
	// Full hash carried through.
	if files[0].Hash != "HASH_A" {
		t.Errorf("files[0].Hash = %q, want HASH_A", files[0].Hash)
	}
	// Cursor loop: first request empty cursor, second the page-1 next_cursor.
	if len(seenCursors) != 2 || seenCursors[0] != "" || seenCursors[1] != "/site/b.css" {
		t.Errorf("cursor sequence = %v, want [\"\" \"/site/b.css\"]", seenCursors)
	}
}

func TestHTTPMirrorClient_Fetch(t *testing.T) {
	srv := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		if r.URL.Path == "/content" && r.URL.Query().Get("path") == "/site/a.html" {
			_, _ = w.Write([]byte("hello"))
			return
		}
		http.Error(w, "not found", http.StatusNotFound)
	}))
	defer srv.Close()

	c := NewMirrorClient(srv.URL)
	body, err := c.Fetch(context.Background(), "/site/a.html")
	if err != nil {
		t.Fatalf("Fetch: %v", err)
	}
	if string(body) != "hello" {
		t.Errorf("body = %q, want hello", string(body))
	}
	// Non-200 ⇒ error.
	if _, err := c.Fetch(context.Background(), "/missing"); err == nil {
		t.Error("expected error for non-200 fetch, got nil")
	}
}

// mustWrite writes content to root/rel (creating parents), failing the test on
// error.
func mustWrite(t *testing.T, root, rel, content string) {
	t.Helper()
	p := filepath.Join(root, filepath.FromSlash(rel))
	if err := os.MkdirAll(filepath.Dir(p), 0o755); err != nil {
		t.Fatalf("mkdir for %q: %v", rel, err)
	}
	if err := os.WriteFile(p, []byte(content), 0o644); err != nil {
		t.Fatalf("write %q: %v", rel, err)
	}
}

// mustRead reads root/rel as a string, failing the test on error.
func mustRead(t *testing.T, root, rel string) string {
	t.Helper()
	b, err := os.ReadFile(filepath.Join(root, filepath.FromSlash(rel)))
	if err != nil {
		t.Fatalf("read %q: %v", rel, err)
	}
	return string(b)
}
