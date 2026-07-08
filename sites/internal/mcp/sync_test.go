package mcp

import (
	"context"
	"os"
	"path/filepath"
	"sort"
	"strings"
	"testing"

	"sites/internal/sites"
)

// fakeMirror is an in-memory sites.MirrorClient: a subtree of mirror paths →
// bytes. List returns every entry whose path sits under the requested prefix;
// Fetch returns one entry's bytes. It carries no network/HTTP — the sync verb's
// logic is exercised against this fake (the HTTP client itself is unit-tested in
// the sites package, Phase 6).
type fakeMirror struct {
	files map[string][]byte // full mirror path → bytes
}

func (f *fakeMirror) List(_ context.Context, prefix string) ([]sites.MirrorFile, error) {
	p := strings.TrimRight(prefix, "/")
	var out []sites.MirrorFile
	for path, data := range f.files {
		if p == "" || path == p || strings.HasPrefix(path, p+"/") {
			out = append(out, sites.MirrorFile{Path: path, Size: int64(len(data))})
		}
	}
	// Deterministic order so assertions are stable.
	sort.Slice(out, func(i, j int) bool { return out[i].Path < out[j].Path })
	return out, nil
}

func (f *fakeMirror) Fetch(_ context.Context, path string) ([]byte, error) {
	data, ok := f.files[path]
	if !ok {
		return nil, sites.ErrNotFound
	}
	return data, nil
}

// readWorking reads a file relative to a private site's directory, failing the
// test if it is absent.
func readWorking(t *testing.T, h *testHandler, slug, rel string) string {
	t.Helper()
	b, err := os.ReadFile(filepath.Join(h.layout.SiteDir(false, slug), filepath.FromSlash(rel)))
	if err != nil {
		t.Fatalf("read private site %s/%s: %v", slug, rel, err)
	}
	return string(b)
}

// TestSyncNewSlug: syncing to an absent slug creates the row + private site
// directory, writes every upstream file (keyed relative to source_path), reports
// the right counts, and keeps the site private.
func TestSyncNewSlug(t *testing.T) {
	h, _ := newTestHandler(t, &fakeMirror{files: map[string][]byte{
		"/sites/marketing/index.html":   []byte("<h1>home</h1>"),
		"/sites/marketing/css/app.css":  []byte("body{}"),
		"/sites/marketing/img/logo.png": {0x89, 0x50, 0x4e, 0x47}, // binary-safe
	}})

	out := callOK(t, h, tool("sync"), map[string]any{"source_path": "/sites/marketing"})
	if out["slug"] != "marketing" {
		t.Fatalf("slug = %v, want marketing", out["slug"])
	}
	if got := out["written"]; got != float64(3) {
		t.Fatalf("written = %v, want 3", got)
	}
	if got := out["deleted"]; got != float64(0) {
		t.Fatalf("deleted = %v, want 0", got)
	}

	// Files landed relative to source_path.
	if readWorking(t, h, "marketing", "index.html") != "<h1>home</h1>" {
		t.Fatal("index.html content mismatch")
	}
	if readWorking(t, h, "marketing", "css/app.css") != "body{}" {
		t.Fatal("css/app.css content mismatch")
	}

	// Row exists and is stamped with the source path, and remains private.
	site, err := h.store.Get(context.Background(), "marketing")
	if err != nil {
		t.Fatalf("get marketing: %v", err)
	}
	if site.SourcePath != "/sites/marketing" {
		t.Fatalf("source_path = %q, want /sites/marketing", site.SourcePath)
	}
	if site.Public {
		t.Fatal("sync must not make a new site public")
	}
}

// TestSyncExistingReconciles: a second sync over an existing site writes new
// upstream files and deletes working files that vanished upstream.
func TestSyncExistingReconciles(t *testing.T) {
	mirror := &fakeMirror{files: map[string][]byte{
		"/site/a.html": []byte("a"),
		"/site/b.html": []byte("b"),
	}}
	h, _ := newTestHandler(t, mirror)

	if out := callOK(t, h, tool("sync"), map[string]any{"source_path": "/site", "slug": "blog"}); out["written"] != float64(2) {
		t.Fatalf("first sync written = %v, want 2", out["written"])
	}

	// Upstream changes: a.html updated, b.html removed, c.html added.
	mirror.files = map[string][]byte{
		"/site/a.html": []byte("a2"),
		"/site/c.html": []byte("c"),
	}
	out := callOK(t, h, tool("sync"), map[string]any{"source_path": "/site", "slug": "blog"})
	if out["written"] != float64(2) {
		t.Fatalf("second sync written = %v, want 2", out["written"])
	}
	if out["deleted"] != float64(1) {
		t.Fatalf("second sync deleted = %v, want 1", out["deleted"])
	}
	if readWorking(t, h, "blog", "a.html") != "a2" {
		t.Fatal("a.html not overwritten")
	}
	if readWorking(t, h, "blog", "c.html") != "c" {
		t.Fatal("c.html not written")
	}
	if _, err := os.Stat(filepath.Join(h.layout.SiteDir(false, "blog"), "b.html")); !os.IsNotExist(err) {
		t.Fatalf("b.html should be deleted, stat err = %v", err)
	}
}

// TestSyncSlugDerivation: a valid basename auto-derives the slug; an invalid
// basename with no explicit slug is a validation error.
func TestSyncSlugDerivation(t *testing.T) {
	h, _ := newTestHandler(t, &fakeMirror{files: map[string][]byte{
		"/x/Marketing Site/index.html": []byte("x"),
	}})

	// Valid basename derives.
	out := callOK(t, h, tool("sync"), map[string]any{"source_path": "/projects/good-slug"})
	if out["slug"] != "good-slug" {
		t.Fatalf("derived slug = %v, want good-slug", out["slug"])
	}

	// Invalid basename ("Marketing Site" has a space + uppercase), no slug given.
	env := callErr(t, h, tool("sync"), map[string]any{"source_path": "/x/Marketing Site"})
	if env["code"] != "validation" {
		t.Fatalf("error code = %v, want validation", env["code"])
	}
	if msg, _ := env["message"].(string); !strings.Contains(msg, "slug") {
		t.Fatalf("validation message should mention slug, got %q", msg)
	}
}

// TestSyncMissingSourcePath: source_path is required.
func TestSyncMissingSourcePath(t *testing.T) {
	h, _ := newTestHandler(t, &fakeMirror{files: map[string][]byte{}})
	env := callErr(t, h, tool("sync"), map[string]any{})
	if env["code"] != "validation" {
		t.Fatalf("error code = %v, want validation", env["code"])
	}
}

func TestSyncPublicSiteUsesPublicDirectory(t *testing.T) {
	mirror := &fakeMirror{files: map[string][]byte{
		"/feed/index.html": []byte("v1"),
	}}
	h, _ := newTestHandler(t, mirror)

	callOK(t, h, tool("sync"), map[string]any{"source_path": "/feed", "slug": "live"})
	callOK(t, h, tool("set_visibility"), map[string]any{"name": "live", "public": true})

	// Re-sync with new bytes; a public site must reconcile in its public dir.
	mirror.files = map[string][]byte{"/feed/index.html": []byte("v2")}
	callOK(t, h, tool("sync"), map[string]any{"source_path": "/feed", "slug": "live"})

	publicPath := filepath.Join(h.layout.SiteDir(true, "live"), "index.html")
	b, err := os.ReadFile(publicPath)
	if err != nil {
		t.Fatalf("read public file: %v", err)
	}
	if string(b) != "v2" {
		t.Fatalf("public content = %q, want v2", b)
	}
	after, err := h.store.Get(context.Background(), "live")
	if err != nil {
		t.Fatalf("get live: %v", err)
	}
	if !after.Public {
		t.Fatal("site should remain public after sync")
	}
}
