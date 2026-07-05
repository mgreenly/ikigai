package telemetry

import (
	"os"
	"path/filepath"
	"reflect"
	"testing"
)

func TestServicesReturnsMCPNamesAndExcludesDashboard(t *testing.T) {
	root := t.TempDir()
	writeManifest(t, root, "dashboard", "APP=dashboard\nMOUNT=/\nPORT=8080\n")
	writeManifest(t, root, "wiki", "APP=wiki\nMCP=true\nMOUNT=/wiki\nPORT=12002\n")
	writeManifest(t, root, "crm", "APP=crm\nMCP=true\nMOUNT=/crm\nPORT=12001\n")

	names, err := services(root)
	if err != nil {
		t.Fatalf("services returned error: %v", err)
	}
	// R-FC2Q-CG3J
	if !reflect.DeepEqual(names, []string{"crm", "wiki"}) {
		t.Fatalf("service names = %#v, want %#v", names, []string{"crm", "wiki"})
	}
}

func writeManifest(t *testing.T, root, app, content string) {
	t.Helper()
	dir := filepath.Join(root, app, "etc", "current")
	if err := os.MkdirAll(dir, 0o755); err != nil {
		t.Fatalf("create manifest dir: %v", err)
	}
	if err := os.WriteFile(filepath.Join(dir, "manifest.env"), []byte(content), 0o644); err != nil {
		t.Fatalf("write manifest: %v", err)
	}
}
