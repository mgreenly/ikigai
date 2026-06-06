package main

import (
	"os"
	"path/filepath"
	"reflect"
	"testing"
)

// noEnv is a getenv that reports every variable as unset.
func noEnv(string) string { return "" }

// envMap returns a getenv backed by m.
func envMap(m map[string]string) func(string) string {
	return func(k string) string { return m[k] }
}

// The fixed-verb dispatcher (serve/version/manifest/migrate/backup/restore) is
// appkit's now and is tested in appkit; this file covers the dashboard-domain
// helpers that stayed app-side: the AS resource derivation, the admin-list parse,
// and the required-secret presence check.

// writeManifest creates <root>/<svc>/etc/manifest.env with the given contents.
func writeManifest(t *testing.T, root, svc, contents string) {
	t.Helper()
	dir := filepath.Join(root, svc, "etc")
	if err := os.MkdirAll(dir, 0o755); err != nil {
		t.Fatalf("mkdir: %v", err)
	}
	if err := os.WriteFile(filepath.Join(dir, "manifest.env"), []byte(contents), 0o644); err != nil {
		t.Fatalf("write manifest: %v", err)
	}
}

// TestDeriveResources confirms the AS resource list is derived from the on-box
// MCP manifests: one <origin><mount>mcp per MCP=true service, sorted by name, and
// the dashboard's own (MCP-less) manifest never self-lists.
func TestDeriveResources(t *testing.T) {
	root := t.TempDir()
	writeManifest(t, root, "crm", "APP=crm\nMOUNT=/srv/crm/\nPORT=3001\nMCP=true\nFEED=/feed\n")
	writeManifest(t, root, "ledger", "APP=ledger\nMOUNT=/srv/ledger/\nPORT=3002\nMCP=true\n")
	// A consumer-only service with no MCP and the dashboard's own manifest must NOT
	// appear in the resource list.
	writeManifest(t, root, "notify", "APP=notify\nMOUNT=/srv/notify/\nPORT=3003\nCONSUMES=crm\n")
	writeManifest(t, root, "dashboard", "APP=dashboard\nMOUNT=/\nDEFAULT=true\nPORT=3000\n")

	got, err := deriveResources(root, "https://int.ikigenba.com")
	if err != nil {
		t.Fatalf("deriveResources: %v", err)
	}
	want := []string{
		"https://int.ikigenba.com/srv/crm/mcp",
		"https://int.ikigenba.com/srv/ledger/mcp",
	}
	if !reflect.DeepEqual(got, want) {
		t.Errorf("deriveResources = %v, want %v", got, want)
	}
}

// TestDeriveResourcesEmpty: a manifest root with no MCP services yields an empty
// list (main's caller turns that into a hard boot failure).
func TestDeriveResourcesEmpty(t *testing.T) {
	got, err := deriveResources(t.TempDir(), "https://int.ikigenba.com")
	if err != nil {
		t.Fatalf("deriveResources: %v", err)
	}
	if len(got) != 0 {
		t.Errorf("deriveResources on empty root = %v, want empty", got)
	}
}

func TestSplitList(t *testing.T) {
	cases := []struct {
		in   string
		want []string
	}{
		{"", nil},
		{" , ,", nil},
		{"a@x.com", []string{"a@x.com"}},
		{" a@x.com , b@x.com ", []string{"a@x.com", "b@x.com"}},
	}
	for _, tc := range cases {
		if got := splitList(tc.in); !reflect.DeepEqual(got, tc.want) {
			t.Errorf("splitList(%q) = %v, want %v", tc.in, got, tc.want)
		}
	}
}

// TestRequireEnv reports every missing variable at once and passes when all are set.
func TestRequireEnv(t *testing.T) {
	if err := requireEnv(noEnv, "A", "B"); err == nil {
		t.Fatal("want error when both vars are unset")
	} else if got := err.Error(); !contains(got, "A") || !contains(got, "B") {
		t.Errorf("error %q should name both A and B", got)
	}
	full := envMap(map[string]string{"A": "1", "B": "2"})
	if err := requireEnv(full, "A", "B"); err != nil {
		t.Errorf("want nil when all set, got %v", err)
	}
}

func contains(s, sub string) bool {
	for i := 0; i+len(sub) <= len(s); i++ {
		if s[i:i+len(sub)] == sub {
			return true
		}
	}
	return false
}
