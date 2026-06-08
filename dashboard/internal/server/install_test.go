package server

import (
	"net/http"
	"strings"
	"testing"

	"appkit/inventory"
)

// TestMcpInstalls builds the connect view from a service list and checks the
// per-client commands carry the service's local name and self-templated resource
// URL, in inventory order.
func TestMcpInstalls(t *testing.T) {
	svcs := []inventory.Service{
		{Name: "crm", Mount: "/srv/crm/"},
		{Name: "notes", Mount: "/srv/notes/"},
	}
	installs := mcpInstalls("https", "int.ikigenba.com", svcs)
	if len(installs) != 2 {
		t.Fatalf("got %d installs, want 2", len(installs))
	}
	crm := installs[0]
	if crm.ID != "crm" || crm.Name != "crm" {
		t.Errorf("first install ID/Name = %q/%q, want crm/crm", crm.ID, crm.Name)
	}
	if len(crm.Cards) != 2 {
		t.Fatalf("crm has %d cards, want 2 (Claude Code, Codex)", len(crm.Cards))
	}
	// The registration handle is namespaced "ikigenba_<svc>"; the resource URL is
	// NOT prefixed — it stays the bare /srv/<svc>/mcp endpoint.
	wantInstall := `\claude mcp add --transport http ikigenba_crm https://int.ikigenba.com/srv/crm/mcp`
	if crm.Cards[0].InstallCommand != wantInstall {
		t.Errorf("Claude install = %q, want %q", crm.Cards[0].InstallCommand, wantInstall)
	}
	if crm.Cards[0].RemoveCommand != `\claude mcp remove ikigenba_crm` {
		t.Errorf("Claude remove = %q", crm.Cards[0].RemoveCommand)
	}
	if strings.Contains(crm.Cards[0].InstallCommand, "ikigenba_crm/mcp") {
		t.Errorf("resource URL must not be prefixed: %q", crm.Cards[0].InstallCommand)
	}
	if crm.Cards[1].Name != "Codex" {
		t.Errorf("second card = %q, want Codex", crm.Cards[1].Name)
	}
}

// TestIndexConnectBlock: the logged-in index renders the "Connect an MCP client"
// section with a service dropdown and the crm service's install snippet pointing
// at its self-templated resource URL.
func TestIndexConnectBlock(t *testing.T) {
	root := t.TempDir()
	writeManifest(t, root, "dashboard", "APP=dashboard\nMOUNT=/\nDEFAULT=true\n")
	writeManifest(t, root, "crm", "APP=crm\nMOUNT=/srv/crm/\nMCP=true\n")

	opts := newServerDeps(t).opts()
	opts.ManifestRoot = root
	srv, err := New(opts)
	if err != nil {
		t.Fatalf("New: %v", err)
	}
	sess := liveSession(t, srv)

	rec := do(t, srv, "GET", "https://int.ikigenba.com/", map[string]string{
		"Cookie":            sess.Name + "=" + sess.Value,
		"X-Forwarded-Proto": "https",
	})
	if rec.Code != http.StatusOK {
		t.Fatalf("status = %d, want 200", rec.Code)
	}
	body := rec.Body.String()
	if !strings.Contains(body, "Connect an MCP client") {
		t.Errorf("index missing connect heading:\n%s", body)
	}
	if !strings.Contains(body, `id="mcp-select"`) {
		t.Errorf("index missing service dropdown:\n%s", body)
	}
	if !strings.Contains(body, `<option value="crm"`) {
		t.Errorf("index missing crm option:\n%s", body)
	}
	if !strings.Contains(body, `\claude mcp add --transport http ikigenba_crm https://int.ikigenba.com/srv/crm/mcp`) {
		t.Errorf("index missing crm install snippet:\n%s", body)
	}
	// The dashboard itself (no MCP=true) must not appear as a connect target.
	if strings.Contains(body, `<option value="dashboard"`) {
		t.Errorf("dashboard leaked into connect dropdown:\n%s", body)
	}
}

// TestIndexConnectBlockLoggedOut: the connect block is logged-in only — a
// visitor with no session sees the landing, not the install snippets.
func TestIndexConnectBlockLoggedOut(t *testing.T) {
	root := t.TempDir()
	writeManifest(t, root, "crm", "APP=crm\nMOUNT=/srv/crm/\nMCP=true\n")

	opts := newServerDeps(t).opts()
	opts.ManifestRoot = root
	srv, err := New(opts)
	if err != nil {
		t.Fatalf("New: %v", err)
	}

	rec := do(t, srv, "GET", "https://int.ikigenba.com/", nil)
	if strings.Contains(rec.Body.String(), "Connect an MCP client") {
		t.Errorf("connect block shown to logged-out visitor:\n%s", rec.Body.String())
	}
}
