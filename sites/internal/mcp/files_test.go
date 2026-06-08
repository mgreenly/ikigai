package mcp

import (
	"crypto/md5"
	"fmt"
	"os"
	"path/filepath"
	"strings"
	"testing"

	"sites/internal/sites"
)

// fileToolsHandler stands up a Handler and creates a site "demo" so its working
// dir exists, returning the handler and the SITES_ROOT for filesystem
// assertions. It drives site creation through the same tools/call entry point
// the other tests use.
func fileToolsHandler(t *testing.T) (*Handler, string) {
	t.Helper()
	h, root := newTestHandler(t)
	callOK(t, h, "ikigenba_sites_create", map[string]any{"name": "demo"})
	return h, root
}

// TestFileWriteReadRoundtrip writes a file and reads it back through the bridge.
func TestFileWriteReadRoundtrip(t *testing.T) {
	h, root := fileToolsHandler(t)

	w := call(t, h, "ikigenba_sites_file_write", map[string]any{
		"site":      "demo",
		"file_path": "index.html",
		"content":   "<h1>hello</h1>",
	})
	if w.IsError {
		t.Fatalf("write returned error: %s", payloadText(w))
	}
	// The file actually lands under the working dir, not elsewhere.
	onDisk := filepath.Join(root, sites.WorkingSeg, "demo", "index.html")
	b, err := os.ReadFile(onDisk)
	if err != nil || string(b) != "<h1>hello</h1>" {
		t.Fatalf("file not written under working dir: %v (%q)", err, string(b))
	}

	r := call(t, h, "ikigenba_sites_file_read", map[string]any{
		"site":      "demo",
		"file_path": "index.html",
	})
	if r.IsError {
		t.Fatalf("read returned error: %s", payloadText(r))
	}
	if !strings.Contains(payloadText(r), "<h1>hello</h1>") {
		t.Fatalf("read content mismatch: %q", payloadText(r))
	}
}

// TestFileEdit edits a file and confirms the change via read-back.
func TestFileEdit(t *testing.T) {
	h, _ := fileToolsHandler(t)
	call(t, h, "ikigenba_sites_file_write", map[string]any{
		"site": "demo", "file_path": "page.txt", "content": "alpha beta",
	})

	e := call(t, h, "ikigenba_sites_file_edit", map[string]any{
		"site": "demo", "file_path": "page.txt",
		"old_string": "beta", "new_string": "gamma",
	})
	if e.IsError {
		t.Fatalf("edit returned error: %s", payloadText(e))
	}
	r := call(t, h, "ikigenba_sites_file_read", map[string]any{"site": "demo", "file_path": "page.txt"})
	if !strings.Contains(payloadText(r), "alpha gamma") {
		t.Fatalf("edit not applied: %q", payloadText(r))
	}
}

// TestFileGlob globs a pattern and asserts matches come back.
func TestFileGlob(t *testing.T) {
	h, _ := fileToolsHandler(t)
	call(t, h, "ikigenba_sites_file_write", map[string]any{"site": "demo", "file_path": "a.html", "content": "x"})
	call(t, h, "ikigenba_sites_file_write", map[string]any{"site": "demo", "file_path": "b.html", "content": "y"})
	call(t, h, "ikigenba_sites_file_write", map[string]any{"site": "demo", "file_path": "c.txt", "content": "z"})

	g := call(t, h, "ikigenba_sites_file_glob", map[string]any{"site": "demo", "pattern": "*.html"})
	if g.IsError {
		t.Fatalf("glob returned error: %s", payloadText(g))
	}
	out := payloadText(g)
	if !strings.Contains(out, "a.html") || !strings.Contains(out, "b.html") {
		t.Fatalf("glob missing matches: %q", out)
	}
	if strings.Contains(out, "c.txt") {
		t.Fatalf("glob matched non-html file: %q", out)
	}
}

// TestFileGrep greps file contents and asserts the match is found.
func TestFileGrep(t *testing.T) {
	h, _ := fileToolsHandler(t)
	call(t, h, "ikigenba_sites_file_write", map[string]any{"site": "demo", "file_path": "needle.txt", "content": "find-this-string"})

	g := call(t, h, "ikigenba_sites_file_grep", map[string]any{
		"site": "demo", "pattern": "find-this-string", "output_mode": "files_with_matches",
	})
	if g.IsError {
		t.Fatalf("grep returned error: %s", payloadText(g))
	}
	if !strings.Contains(payloadText(g), "needle.txt") {
		t.Fatalf("grep did not find the match: %q", payloadText(g))
	}
}

// TestFileTraversalRejected is the security-critical case: relative '..' escapes
// and absolute paths are rejected, and nothing is read/written outside the
// working dir.
func TestFileTraversalRejected(t *testing.T) {
	h, root := fileToolsHandler(t)

	// Relative traversal on write → error, and no file outside the sandbox.
	w := call(t, h, "ikigenba_sites_file_write", map[string]any{
		"site": "demo", "file_path": "../../../tmp/sites-escape-probe", "content": "pwned",
	})
	if !w.IsError {
		t.Fatalf("relative traversal write should be rejected: %s", payloadText(w))
	}

	// Absolute path on write → error.
	wa := call(t, h, "ikigenba_sites_file_write", map[string]any{
		"site": "demo", "file_path": "/tmp/sites-escape-probe-abs", "content": "pwned",
	})
	if !wa.IsError {
		t.Fatalf("absolute path write should be rejected: %s", payloadText(wa))
	}

	// Neither probe escaped the sandbox.
	if _, err := os.Stat("/tmp/sites-escape-probe"); err == nil {
		t.Fatalf("relative traversal escaped the sandbox: /tmp/sites-escape-probe exists")
	}
	if _, err := os.Stat("/tmp/sites-escape-probe-abs"); err == nil {
		t.Fatalf("absolute path escaped the sandbox: /tmp/sites-escape-probe-abs exists")
	}

	// Relative traversal on read of a real outside file → error (no escape).
	r := call(t, h, "ikigenba_sites_file_read", map[string]any{
		"site": "demo", "file_path": "../../../etc/passwd",
	})
	if !r.IsError {
		t.Fatalf("relative traversal read should be rejected: %s", payloadText(r))
	}
	if strings.Contains(payloadText(r), "root:") {
		t.Fatalf("read escaped the sandbox and leaked /etc/passwd: %q", payloadText(r))
	}

	// Absolute path read of /etc/passwd → error (no escape).
	ra := call(t, h, "ikigenba_sites_file_read", map[string]any{
		"site": "demo", "file_path": "/etc/passwd",
	})
	if !ra.IsError {
		t.Fatalf("absolute path read should be rejected: %s", payloadText(ra))
	}
	if strings.Contains(payloadText(ra), "root:") {
		t.Fatalf("absolute read escaped the sandbox and leaked /etc/passwd: %q", payloadText(ra))
	}

	_ = root
}

// TestFileList writes a couple of files and asserts file_list reports each with
// its working-root-relative path, size, and md5; that path scopes the walk; and
// that the error cases yield the stable codes.
func TestFileList(t *testing.T) {
	h, _ := fileToolsHandler(t)

	const indexContent = "<h1>hi</h1>"
	const cssContent = "body{}"
	if w := call(t, h, "ikigenba_sites_file_write", map[string]any{"site": "demo", "file_path": "index.html", "content": indexContent}); w.IsError {
		t.Fatalf("write index.html: %s", payloadText(w))
	}
	callOK(t, h, "ikigenba_sites_mkdir", map[string]any{"name": "demo", "path": "css"})
	if w := call(t, h, "ikigenba_sites_file_write", map[string]any{"site": "demo", "file_path": "css/site.css", "content": cssContent}); w.IsError {
		t.Fatalf("write css/site.css: %s", payloadText(w))
	}

	indexMD5 := fmt.Sprintf("%x", md5.Sum([]byte(indexContent)))
	cssMD5 := fmt.Sprintf("%x", md5.Sum([]byte(cssContent)))

	// fileEntries indexes a file_list result's "files" array by path.
	fileEntries := func(m map[string]any) map[string]map[string]any {
		out := map[string]map[string]any{}
		arr, ok := m["files"].([]any)
		if !ok {
			t.Fatalf("files is not an array: %+v", m)
		}
		for _, e := range arr {
			em, ok := e.(map[string]any)
			if !ok {
				t.Fatalf("file entry is not an object: %+v", e)
			}
			out[em["path"].(string)] = em
		}
		return out
	}

	full := callOK(t, h, "ikigenba_sites_file_list", map[string]any{"site": "demo"})
	entries := fileEntries(full)
	if len(entries) != 2 {
		t.Fatalf("expected 2 files, got %+v", full)
	}
	idx, ok := entries["index.html"]
	if !ok {
		t.Fatalf("index.html missing: %+v", full)
	}
	if idx["size"].(float64) != float64(len(indexContent)) {
		t.Errorf("index.html size = %v, want %d", idx["size"], len(indexContent))
	}
	if idx["md5"] != indexMD5 {
		t.Errorf("index.html md5 = %v, want %v", idx["md5"], indexMD5)
	}
	css, ok := entries["css/site.css"]
	if !ok {
		t.Fatalf("css/site.css missing: %+v", full)
	}
	if css["size"].(float64) != float64(len(cssContent)) {
		t.Errorf("css/site.css size = %v, want %d", css["size"], len(cssContent))
	}
	if css["md5"] != cssMD5 {
		t.Errorf("css/site.css md5 = %v, want %v", css["md5"], cssMD5)
	}

	// Scoped walk: only the css subtree, but paths stay relative to the root.
	scoped := callOK(t, h, "ikigenba_sites_file_list", map[string]any{"site": "demo", "path": "css"})
	scopedEntries := fileEntries(scoped)
	if len(scopedEntries) != 1 {
		t.Fatalf("scoped list expected 1 file, got %+v", scoped)
	}
	if _, ok := scopedEntries["css/site.css"]; !ok {
		t.Fatalf("scoped list missing css/site.css: %+v", scoped)
	}

	// Error cases.
	if e := callErr(t, h, "ikigenba_sites_file_list", map[string]any{}); e["code"] != "invalid_site" {
		t.Fatalf("expected invalid_site, got %+v", e)
	}
	if e := callErr(t, h, "ikigenba_sites_file_list", map[string]any{"site": "no-such-site"}); e["code"] != "not_found" {
		t.Fatalf("expected not_found, got %+v", e)
	}
	if e := callErr(t, h, "ikigenba_sites_file_list", map[string]any{"site": "demo", "path": "../.."}); e["code"] != "path_escapes_working_dir" {
		t.Fatalf("expected path_escapes_working_dir, got %+v", e)
	}
}

// TestFileWriteAppend exercises the native file_write: append concatenates,
// while the default mode truncates, and the append success payload carries
// "appended": true.
func TestFileWriteAppend(t *testing.T) {
	h, _ := fileToolsHandler(t)

	callOK(t, h, "ikigenba_sites_file_write", map[string]any{
		"site": "demo", "file_path": "index.html", "content": "a",
	})
	ap := callOK(t, h, "ikigenba_sites_file_write", map[string]any{
		"site": "demo", "file_path": "index.html", "content": "b", "append": true,
	})
	if ap["appended"] != true {
		t.Fatalf("append call payload missing appended:true: %+v", ap)
	}
	r := call(t, h, "ikigenba_sites_file_read", map[string]any{"site": "demo", "file_path": "index.html"})
	if !strings.Contains(payloadText(r), "ab") {
		t.Fatalf("append did not concatenate: %q", payloadText(r))
	}

	// Default mode truncates, not appends.
	callOK(t, h, "ikigenba_sites_file_write", map[string]any{"site": "demo", "file_path": "index.html", "content": "x"})
	callOK(t, h, "ikigenba_sites_file_write", map[string]any{"site": "demo", "file_path": "index.html", "content": "y"})
	r2 := call(t, h, "ikigenba_sites_file_read", map[string]any{"site": "demo", "file_path": "index.html"})
	if strings.Contains(payloadText(r2), "xy") {
		t.Fatalf("default write should truncate, not append: %q", payloadText(r2))
	}
	if !strings.Contains(payloadText(r2), "y") {
		t.Fatalf("default write should leave only the last content: %q", payloadText(r2))
	}
}

// TestFileMissingSite asserts a missing or unknown site yields a clean error.
func TestFileMissingSite(t *testing.T) {
	h, _ := fileToolsHandler(t)

	// Missing site argument.
	e := callErr(t, h, "ikigenba_sites_file_write", map[string]any{
		"file_path": "x.html", "content": "y",
	})
	if e["code"] != "invalid_site" {
		t.Fatalf("expected invalid_site, got %+v", e)
	}

	// Unknown site.
	e2 := callErr(t, h, "ikigenba_sites_file_read", map[string]any{
		"site": "no-such-site", "file_path": "x.html",
	})
	if e2["code"] != "not_found" {
		t.Fatalf("expected not_found, got %+v", e2)
	}
}
