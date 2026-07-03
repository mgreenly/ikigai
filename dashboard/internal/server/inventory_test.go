package server

import (
	"net/http"
	"os"
	"path/filepath"
	"strings"
	"testing"
)

// writeManifest creates <root>/<svc>/etc/current/manifest.env with the given
// contents, matching the on-box layout readers resolve through the current symlink.
func writeManifest(t *testing.T, root, svc, contents string) {
	t.Helper()
	dir := filepath.Join(root, svc, "etc", "current")
	if err := os.MkdirAll(dir, 0o755); err != nil {
		t.Fatalf("mkdir %s: %v", dir, err)
	}
	if err := os.WriteFile(filepath.Join(dir, "manifest.env"), []byte(contents), 0o644); err != nil {
		t.Fatalf("write manifest: %v", err)
	}
}

// TestInventoryServices: GET /services lists the crm service (MCP=true) with its
// resource URL self-templated from the request host, and omits the dashboard
// (no MCP=true). The response is 200 application/json.
func TestInventoryServices(t *testing.T) {
	root := t.TempDir()
	writeManifest(t, root, "dashboard", "APP=dashboard\nMOUNT=/\nDEFAULT=true\n")
	writeManifest(t, root, "crm", "APP=crm\nMOUNT=/srv/crm/\nMCP=true\n")

	opts := newServerDeps(t).opts()
	opts.ManifestRoot = root
	srv, err := New(opts)
	if err != nil {
		t.Fatalf("New: %v", err)
	}

	rec := do(t, srv, "GET", "https://int.ikigenba.com/services",
		map[string]string{"X-Forwarded-Proto": "https"})
	if rec.Code != http.StatusOK {
		t.Fatalf("status = %d, want 200", rec.Code)
	}
	if ct := rec.Header().Get("Content-Type"); !strings.HasPrefix(ct, "application/json") {
		t.Errorf("Content-Type = %q, want application/json", ct)
	}
	body := rec.Body.String()
	if !strings.Contains(body, `"name":"crm"`) {
		t.Errorf("body missing crm name:\n%s", body)
	}
	if !strings.Contains(body, `"mount":"/srv/crm/"`) {
		t.Errorf("body missing crm mount:\n%s", body)
	}
	if !strings.Contains(body, `"resource":"https://int.ikigenba.com/srv/crm/mcp"`) {
		t.Errorf("body missing crm resource:\n%s", body)
	}
	if strings.Contains(body, "dashboard") {
		t.Errorf("dashboard (no MCP) leaked into inventory:\n%s", body)
	}
}
