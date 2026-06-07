package mcp

import (
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

	w := call(t, h, "ikigenba_sites_write", map[string]any{
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

	r := call(t, h, "ikigenba_sites_read", map[string]any{
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
	call(t, h, "ikigenba_sites_write", map[string]any{
		"site": "demo", "file_path": "page.txt", "content": "alpha beta",
	})

	e := call(t, h, "ikigenba_sites_edit", map[string]any{
		"site": "demo", "file_path": "page.txt",
		"old_string": "beta", "new_string": "gamma",
	})
	if e.IsError {
		t.Fatalf("edit returned error: %s", payloadText(e))
	}
	r := call(t, h, "ikigenba_sites_read", map[string]any{"site": "demo", "file_path": "page.txt"})
	if !strings.Contains(payloadText(r), "alpha gamma") {
		t.Fatalf("edit not applied: %q", payloadText(r))
	}
}

// TestFileGlob globs a pattern and asserts matches come back.
func TestFileGlob(t *testing.T) {
	h, _ := fileToolsHandler(t)
	call(t, h, "ikigenba_sites_write", map[string]any{"site": "demo", "file_path": "a.html", "content": "x"})
	call(t, h, "ikigenba_sites_write", map[string]any{"site": "demo", "file_path": "b.html", "content": "y"})
	call(t, h, "ikigenba_sites_write", map[string]any{"site": "demo", "file_path": "c.txt", "content": "z"})

	g := call(t, h, "ikigenba_sites_glob", map[string]any{"site": "demo", "pattern": "*.html"})
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
	call(t, h, "ikigenba_sites_write", map[string]any{"site": "demo", "file_path": "needle.txt", "content": "find-this-string"})

	g := call(t, h, "ikigenba_sites_grep", map[string]any{
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
	w := call(t, h, "ikigenba_sites_write", map[string]any{
		"site": "demo", "file_path": "../../../tmp/sites-escape-probe", "content": "pwned",
	})
	if !w.IsError {
		t.Fatalf("relative traversal write should be rejected: %s", payloadText(w))
	}

	// Absolute path on write → error.
	wa := call(t, h, "ikigenba_sites_write", map[string]any{
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
	r := call(t, h, "ikigenba_sites_read", map[string]any{
		"site": "demo", "file_path": "../../../etc/passwd",
	})
	if !r.IsError {
		t.Fatalf("relative traversal read should be rejected: %s", payloadText(r))
	}
	if strings.Contains(payloadText(r), "root:") {
		t.Fatalf("read escaped the sandbox and leaked /etc/passwd: %q", payloadText(r))
	}

	// Absolute path read of /etc/passwd → error (no escape).
	ra := call(t, h, "ikigenba_sites_read", map[string]any{
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

// TestFileMissingSite asserts a missing or unknown site yields a clean error.
func TestFileMissingSite(t *testing.T) {
	h, _ := fileToolsHandler(t)

	// Missing site argument.
	e := callErr(t, h, "ikigenba_sites_write", map[string]any{
		"file_path": "x.html", "content": "y",
	})
	if e["code"] != "invalid_site" {
		t.Fatalf("expected invalid_site, got %+v", e)
	}

	// Unknown site.
	e2 := callErr(t, h, "ikigenba_sites_read", map[string]any{
		"site": "no-such-site", "file_path": "x.html",
	})
	if e2["code"] != "not_found" {
		t.Fatalf("expected not_found, got %+v", e2)
	}
}
