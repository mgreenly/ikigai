package main

import (
	"bytes"
	"context"
	"encoding/json"
	"fmt"
	"io"
	"net"
	"net/http"
	"os"
	"os/exec"
	"path/filepath"
	"reflect"
	"testing"
	"time"

	"appkit/manifest"
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

// writeManifest creates <root>/<svc>/etc/current/manifest.env with the given
// contents, matching the on-box layout readers resolve through the current symlink.
func writeManifest(t *testing.T, root, svc, contents string) {
	t.Helper()
	dir := filepath.Join(root, svc, "etc", "current")
	if err := os.MkdirAll(dir, 0o755); err != nil {
		t.Fatalf("mkdir: %v", err)
	}
	if err := os.WriteFile(filepath.Join(dir, "manifest.env"), []byte(contents), 0o644); err != nil {
		t.Fatalf("write manifest: %v", err)
	}
}

// R-8DF1-W89F
func TestCommittedManifestIsPortable(t *testing.T) {
	committed, err := os.ReadFile(filepath.Join("..", "..", "etc", "manifest.env"))
	if err != nil {
		t.Fatalf("read committed manifest.env: %v", err)
	}
	if bytes.Contains(committed, []byte("/opt/")) {
		t.Fatalf("committed manifest.env contains on-box /opt/ path:\n%s", committed)
	}
	for _, line := range bytes.Split(committed, []byte("\n")) {
		if bytes.HasPrefix(line, []byte("DASHBOARD_DB_PATH=")) || bytes.HasPrefix(line, []byte("DASHBOARD_GENERATION_PATH=")) {
			t.Fatalf("committed manifest.env contains runtime path line %q", line)
		}
	}
}

// R-8IAN-FB87
func TestManifestLibraryByteEqualsCommittedFile(t *testing.T) {
	got := manifest.Emit(manifest.Fields{
		App:     "dashboard",
		Mount:   "/",
		Default: true,
		Port:    3000,
	})
	committed, err := os.ReadFile(filepath.Join("..", "..", "etc", "manifest.env"))
	if err != nil {
		t.Fatalf("read committed manifest.env: %v", err)
	}

	if got != string(committed) {
		t.Fatalf("manifest.Emit output != committed etc/manifest.env\n--- emit ---\n%s\n--- committed ---\n%s", got, committed)
	}
}

// TestDeriveResources confirms the AS resource list is derived from the on-box
// MCP manifests: one <origin><mount>mcp per MCP=true service, sorted by name, and
// the dashboard's own (MCP-less) manifest never self-lists.
func TestDeriveResources(t *testing.T) {
	root := t.TempDir()
	writeManifest(t, root, "crm", "APP=crm\nMOUNT=/srv/crm/\nPORT=3100\nMCP=true\nFEED=/feed\n")
	writeManifest(t, root, "ledger", "APP=ledger\nMOUNT=/srv/ledger/\nPORT=3101\nMCP=true\n")
	// A consumer-only service with no MCP and the dashboard's own manifest must NOT
	// appear in the resource list.
	writeManifest(t, root, "notify", "APP=notify\nMOUNT=/srv/notify/\nPORT=3201\nCONSUMES=crm\n")
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

// R-4LKF-FB23
func TestDashboardBootsFromOpsctlLayoutAndServesHealth(t *testing.T) {
	root := t.TempDir()
	appRoot := filepath.Join(root, "dashboard")
	stateDir := filepath.Join(appRoot, "state")
	cacheDir := filepath.Join(appRoot, "cache")
	libexecDir := filepath.Join(appRoot, "libexec")
	binDir := filepath.Join(appRoot, "bin")
	for _, dir := range []string{stateDir, cacheDir, libexecDir, binDir} {
		if err := os.MkdirAll(dir, 0o755); err != nil {
			t.Fatalf("mkdir %s: %v", dir, err)
		}
	}

	writeManifest(t, root, "dashboard", "APP=dashboard\nMOUNT=/\nDEFAULT=true\nPORT=3000\n")
	writeManifest(t, root, "crm", "APP=crm\nMOUNT=/srv/crm/\nPORT=3100\nMCP=true\n")

	binary := filepath.Join(libexecDir, "dashboard-vtest")
	build := exec.Command("go", "build", "-o", binary, ".")
	build.Env = os.Environ()
	if out, err := build.CombinedOutput(); err != nil {
		t.Fatalf("go build dashboard: %v\n%s", err, out)
	}
	run := filepath.Join(binDir, "run")
	if err := os.Symlink("../libexec/dashboard-vtest", run); err != nil {
		t.Fatalf("symlink bin/run: %v", err)
	}
	if resolved, err := filepath.EvalSymlinks(run); err != nil || resolved != binary {
		t.Fatalf("bin/run resolves to %q err=%v, want %q", resolved, err, binary)
	}

	port := freeTCPPort(t)
	dbPath := filepath.Join(stateDir, "dashboard.db")
	generationPath := filepath.Join(cacheDir, "dashboard.db.generation")
	ctx, cancel := context.WithCancel(context.Background())
	defer cancel()

	var stdout, stderr bytes.Buffer
	cmd := exec.CommandContext(ctx, run, "serve")
	cmd.Env = append(os.Environ(),
		"DASHBOARD_IP=127.0.0.1",
		fmt.Sprintf("DASHBOARD_PORT=%d", port),
		"DASHBOARD_DB_PATH="+dbPath,
		"DASHBOARD_GENERATION_PATH="+generationPath,
		"DASHBOARD_MANIFEST_ROOT="+root,
		"DASHBOARD_PUBLIC_BASE_URL=https://int.ikigenba.com",
		"GOOGLE_CLIENT_ID=test-client",
		"GOOGLE_CLIENT_SECRET=test-secret",
		"GOOGLE_WORKSPACE_DOMAIN=example.com",
	)
	cmd.Stdout = &stdout
	cmd.Stderr = &stderr
	if err := cmd.Start(); err != nil {
		t.Fatalf("start dashboard: %v", err)
	}
	done := make(chan error, 1)
	go func() { done <- cmd.Wait() }()
	defer stopProcess(cancel, done)

	doc := waitForHealth(t, port, done, &stdout, &stderr)
	if got := doc["service"]; got != "dashboard" {
		t.Fatalf("health service = %v, want dashboard; body=%v", got, doc)
	}
	if details, ok := doc["details"].(map[string]any); !ok || details == nil {
		t.Fatalf("health details = %#v, want JSON object", doc["details"])
	}
	if _, err := os.Stat(dbPath); err != nil {
		t.Fatalf("dashboard did not create DB under state/: %v", err)
	}
	if filepath.Dir(generationPath) != cacheDir {
		t.Fatalf("generation sidecar path %s is not under cache dir %s", generationPath, cacheDir)
	}
}

func freeTCPPort(t *testing.T) int {
	t.Helper()
	ln, err := net.Listen("tcp", "127.0.0.1:0")
	if err != nil {
		t.Fatalf("listen for free port: %v", err)
	}
	defer ln.Close()
	return ln.Addr().(*net.TCPAddr).Port
}

func waitForHealth(t *testing.T, port int, done <-chan error, stdout, stderr *bytes.Buffer) map[string]any {
	t.Helper()
	url := fmt.Sprintf("http://127.0.0.1:%d/health", port)
	client := http.Client{Timeout: 250 * time.Millisecond}
	deadline := time.Now().Add(5 * time.Second)
	var last string
	for time.Now().Before(deadline) {
		select {
		case err := <-done:
			t.Fatalf("dashboard exited before health: %v\nstdout:\n%s\nstderr:\n%s", err, stdout.String(), stderr.String())
		default:
		}

		resp, err := client.Get(url)
		if err == nil {
			body, readErr := io.ReadAll(resp.Body)
			closeErr := resp.Body.Close()
			if resp.StatusCode == http.StatusOK && readErr == nil && closeErr == nil {
				var doc map[string]any
				if err := json.Unmarshal(body, &doc); err != nil {
					t.Fatalf("decode health JSON: %v\nbody:\n%s", err, body)
				}
				return doc
			}
			last = fmt.Sprintf("status=%d read=%v close=%v body=%s", resp.StatusCode, readErr, closeErr, body)
		} else {
			last = err.Error()
		}
		time.Sleep(100 * time.Millisecond)
	}
	t.Fatalf("dashboard never served health at %s: %s\nstdout:\n%s\nstderr:\n%s", url, last, stdout.String(), stderr.String())
	return nil
}

func stopProcess(cancel context.CancelFunc, done <-chan error) {
	cancel()
	select {
	case <-done:
	case <-time.After(time.Second):
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
