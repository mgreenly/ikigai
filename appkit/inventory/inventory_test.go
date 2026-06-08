package inventory

import (
	"os"
	"path/filepath"
	"testing"
)

// writeManifest creates <root>/<svc>/etc/manifest.env with the given contents.
func writeManifest(t *testing.T, root, svc, contents string) {
	t.Helper()
	dir := filepath.Join(root, svc, "etc")
	if err := os.MkdirAll(dir, 0o755); err != nil {
		t.Fatalf("mkdir %s: %v", dir, err)
	}
	if err := os.WriteFile(filepath.Join(dir, "manifest.env"), []byte(contents), 0o644); err != nil {
		t.Fatalf("write manifest: %v", err)
	}
}

// TestReadKeepsOnlyMCP: the dashboard manifest (no MCP) is omitted, a garbled
// manifest is skipped, and only the crm service (MCP=true) is returned.
func TestReadKeepsOnlyMCP(t *testing.T) {
	root := t.TempDir()
	writeManifest(t, root, "dashboard", "APP=dashboard\nMOUNT=/\nDEFAULT=true\nPORT=3000\n")
	writeManifest(t, root, "crm", "# crm service\nAPP=crm\nMOUNT=/srv/crm/\nPORT=3001\nMCP=true\nFEED=/feed\n")
	writeManifest(t, root, "broken", "this is not = = valid\n\x00garbage")

	got, err := Read(root)
	if err != nil {
		t.Fatalf("Read: %v", err)
	}
	if len(got) != 1 {
		t.Fatalf("got %d services, want 1: %+v", len(got), got)
	}
	if got[0].Name != "crm" {
		t.Errorf("Name = %q, want crm", got[0].Name)
	}
	if got[0].Mount != "/srv/crm/" {
		t.Errorf("Mount = %q, want /srv/crm/", got[0].Mount)
	}
	if got[0].Port != "3001" {
		t.Errorf("Port = %q, want 3001", got[0].Port)
	}
	if got[0].Feed != "/feed" {
		t.Errorf("Feed = %q, want /feed", got[0].Feed)
	}
}

// TestReadConsumerHasNoFeed: an MCP service without a FEED key (a consumer) is
// listed with an empty Feed but a populated Port.
func TestReadConsumerHasNoFeed(t *testing.T) {
	root := t.TempDir()
	writeManifest(t, root, "notify", "APP=notify\nMOUNT=/srv/notify/\nPORT=3003\nMCP=true\nCONSUMES=crm\n")

	got, err := Read(root)
	if err != nil {
		t.Fatalf("Read: %v", err)
	}
	if len(got) != 1 {
		t.Fatalf("got %d services, want 1: %+v", len(got), got)
	}
	if got[0].Port != "3003" {
		t.Errorf("Port = %q, want 3003", got[0].Port)
	}
	if got[0].Feed != "" {
		t.Errorf("Feed = %q, want empty", got[0].Feed)
	}
}

// TestReadSortsByName: multiple MCP services come back sorted by Name regardless
// of glob/filesystem order.
func TestReadSortsByName(t *testing.T) {
	root := t.TempDir()
	writeManifest(t, root, "wiki", "APP=wiki\nMOUNT=/srv/wiki/\nPORT=3006\nMCP=true\n")
	writeManifest(t, root, "crm", "APP=crm\nMOUNT=/srv/crm/\nPORT=3001\nMCP=true\n")
	writeManifest(t, root, "ledger", "APP=ledger\nMOUNT=/srv/ledger/\nPORT=3002\nMCP=true\n")

	got, err := Read(root)
	if err != nil {
		t.Fatalf("Read: %v", err)
	}
	want := []string{"crm", "ledger", "wiki"}
	if len(got) != len(want) {
		t.Fatalf("got %d services, want %d: %+v", len(got), len(want), got)
	}
	for i, name := range want {
		if got[i].Name != name {
			t.Errorf("services[%d].Name = %q, want %q", i, got[i].Name, name)
		}
	}
}
