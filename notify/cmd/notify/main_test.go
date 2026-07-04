package main

import (
	"bytes"
	"context"
	"encoding/json"
	"fmt"
	"go/ast"
	"go/parser"
	"go/token"
	"io"
	"net"
	"net/http"
	"net/http/httptest"
	"os"
	"os/exec"
	"path/filepath"
	"regexp"
	"strings"
	"testing"
	"time"

	"appkit/manifest"
	"registry"
)

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
		if bytes.HasPrefix(line, []byte("NOTIFY_DB_PATH=")) || bytes.HasPrefix(line, []byte("NOTIFY_GENERATION_PATH=")) {
			t.Fatalf("committed manifest.env contains runtime path line %q", line)
		}
	}
}

// R-8IAN-FB87
func TestManifestLibraryByteEqualsCommittedFile(t *testing.T) {
	got := manifest.Emit(manifest.Fields{
		App:      "notify",
		Mount:    "/srv/notify/",
		Default:  false,
		Port:     registry.MustPort("notify"),
		MCP:      true,
		Consumes: []string{"crm", "prompts"},
	})
	committed, err := os.ReadFile(filepath.Join("..", "..", "etc", "manifest.env"))
	if err != nil {
		t.Fatalf("read committed manifest.env: %v", err)
	}

	if got != string(committed) {
		t.Fatalf("manifest.Emit output != committed etc/manifest.env\n--- emit ---\n%s\n--- committed ---\n%s", got, committed)
	}
}

// R-4LKF-FB23
func TestNotifyBootsFromOpsctlLayoutAndServesHealth(t *testing.T) {
	root := t.TempDir()
	appRoot := filepath.Join(root, "notify")
	stateDir := filepath.Join(appRoot, "state")
	cacheDir := filepath.Join(appRoot, "cache")
	libexecDir := filepath.Join(appRoot, "libexec")
	binDir := filepath.Join(appRoot, "bin")
	etcDir := filepath.Join(appRoot, "etc")
	shareDir := filepath.Join(appRoot, "share")
	for _, dir := range []string{stateDir, cacheDir, libexecDir, binDir, etcDir, shareDir} {
		if err := os.MkdirAll(dir, 0o755); err != nil {
			t.Fatalf("mkdir %s: %v", dir, err)
		}
	}

	versionBytes, err := os.ReadFile(filepath.Join("..", "..", "VERSION"))
	if err != nil {
		t.Fatalf("read VERSION: %v", err)
	}
	version := strings.TrimSpace(string(versionBytes))
	if !regexp.MustCompile(`^v[0-9]+\.[0-9]+\.[0-9]+$`).MatchString(version) {
		t.Fatalf("VERSION = %q, want v-prefixed SemVer", version)
	}

	committedManifest, err := os.ReadFile(filepath.Join("..", "..", "etc", "manifest.env"))
	if err != nil {
		t.Fatalf("read committed manifest.env: %v", err)
	}
	etcVersionDir := filepath.Join(etcDir, version)
	shareVersionDir := filepath.Join(shareDir, version)
	for _, dir := range []string{etcVersionDir, shareVersionDir} {
		if err := os.MkdirAll(dir, 0o755); err != nil {
			t.Fatalf("mkdir %s: %v", dir, err)
		}
	}
	shippedManifest := filepath.Join(etcVersionDir, "manifest.env")
	if err := os.WriteFile(shippedManifest, committedManifest, 0o644); err != nil {
		t.Fatalf("write shipped manifest.env: %v", err)
	}
	if err := os.Symlink(version, filepath.Join(etcDir, "current")); err != nil {
		t.Fatalf("symlink etc/current: %v", err)
	}
	if err := os.Symlink(version, filepath.Join(shareDir, "current")); err != nil {
		t.Fatalf("symlink share/current: %v", err)
	}
	if resolved, err := filepath.EvalSymlinks(filepath.Join(etcDir, "current")); err != nil || resolved != etcVersionDir {
		t.Fatalf("etc/current resolves to %q err=%v, want %q", resolved, err, etcVersionDir)
	}
	if resolved, err := filepath.EvalSymlinks(filepath.Join(shareDir, "current")); err != nil || resolved != shareVersionDir {
		t.Fatalf("share/current resolves to %q err=%v, want %q", resolved, err, shareVersionDir)
	}
	selectedManifest, err := os.ReadFile(filepath.Join(etcDir, "current", "manifest.env"))
	if err != nil {
		t.Fatalf("read selected manifest.env: %v", err)
	}
	if !bytes.Equal(selectedManifest, committedManifest) {
		t.Fatalf("selected manifest.env differs from committed authored file\n--- selected ---\n%s\n--- committed ---\n%s", selectedManifest, committedManifest)
	}

	binary := filepath.Join(libexecDir, "notify-"+version)
	build := exec.Command("go", "build", "-o", binary, ".")
	build.Env = os.Environ()
	if out, err := build.CombinedOutput(); err != nil {
		t.Fatalf("go build notify: %v\n%s", err, out)
	}

	run := filepath.Join(binDir, "run")
	if err := os.Symlink("../libexec/notify-"+version, run); err != nil {
		t.Fatalf("symlink bin/run: %v", err)
	}
	if resolved, err := filepath.EvalSymlinks(run); err != nil || resolved != binary {
		t.Fatalf("bin/run resolves to %q err=%v, want %q", resolved, err, binary)
	}

	crmFeed := newIdleFeedServer(t)
	promptsFeed := newIdleFeedServer(t)
	ntfy := httptest.NewServer(http.NotFoundHandler())
	t.Cleanup(ntfy.Close)

	port := freeTCPPort(t)
	dbPath := filepath.Join(stateDir, "notify.db")
	generationPath := filepath.Join(cacheDir, "notify.db.generation")
	ctx, cancel := context.WithCancel(context.Background())
	defer cancel()

	var stdout, stderr bytes.Buffer
	cmd := exec.CommandContext(ctx, run, "serve")
	cmd.Env = testEnv(map[string]string{
		"IKIGENBA_DOMAIN":        "",
		"IKIGENBA_ROOT":          "",
		"NOTIFY_IP":              "127.0.0.1",
		"NOTIFY_PORT":            fmt.Sprintf("%d", port),
		"NOTIFY_DB_PATH":         dbPath,
		"NOTIFY_GENERATION_PATH": generationPath,
		"CRM_FEED_URL":           crmFeed.URL + "/feed",
		"PROMPTS_FEED_URL":       promptsFeed.URL + "/feed",
		"NOTIFY_NTFY_BASE_URL":   ntfy.URL,
		"NTFY_TOPIC":             "notify-test",
		"NTFY_API_KEY":           "notify-test-token",
	})
	cmd.Stdout = &stdout
	cmd.Stderr = &stderr
	if err := cmd.Start(); err != nil {
		t.Fatalf("start notify: %v", err)
	}
	done := make(chan error, 1)
	go func() { done <- cmd.Wait() }()
	defer stopProcess(cancel, done)

	doc := waitForHealth(t, port, done, &stdout, &stderr)
	if got := doc["service"]; got != "notify" {
		t.Fatalf("health service = %v, want notify; body=%v", got, doc)
	}
	if got := doc["status"]; got != "ok" {
		t.Fatalf("health status = %v, want ok; body=%v", got, doc)
	}
	if _, ok := doc["details"].(map[string]any); !ok {
		t.Fatalf("health details = %#v, want JSON object", doc["details"])
	}
	if _, err := os.Stat(dbPath); err != nil {
		t.Fatalf("notify did not create DB under state/: %v", err)
	}
	if _, err := os.Stat(generationPath); err != nil {
		t.Fatalf("notify did not create generation sidecar under cache/: %v", err)
	}
	if filepath.Dir(generationPath) != cacheDir {
		t.Fatalf("generation sidecar path %s is not under cache dir %s", generationPath, cacheDir)
	}
}

func TestResolveConsumerCfgDefaultsCRMFeedURLFromRegistry(t *testing.T) {
	// R-RGCF-4B2L
	cfg, err := resolveConsumerCfg(testGetenv(map[string]string{
		"NTFY_TOPIC":   "notify-test",
		"NTFY_API_KEY": "notify-test-token",
	}))
	if err != nil {
		t.Fatalf("resolveConsumerCfg: %v", err)
	}

	want := registry.BaseURL("crm") + "/feed"
	if cfg.feedURL != want {
		t.Fatalf("feedURL = %q, want %q", cfg.feedURL, want)
	}
	if cfg.feedURL != "http://127.0.0.1:3100/feed" {
		t.Fatalf("feedURL = %q, want registry's stable crm loopback feed", cfg.feedURL)
	}
}

func TestResolveConsumerCfgDefaultsPromptsFeedURLFromRegistry(t *testing.T) {
	// R-RGPF-4C3M
	cfg, err := resolveConsumerCfg(testGetenv(map[string]string{
		"NTFY_TOPIC":   "notify-test",
		"NTFY_API_KEY": "notify-test-token",
	}))
	if err != nil {
		t.Fatalf("resolveConsumerCfg: %v", err)
	}

	want := registry.BaseURL("prompts") + "/feed"
	if cfg.promptsFeedURL != want {
		t.Fatalf("promptsFeedURL = %q, want %q", cfg.promptsFeedURL, want)
	}
}

func TestResolveConsumerCfgFeedURLOverridesWinOverRegistryDefaults(t *testing.T) {
	// R-RGEO-4D4N
	cfg, err := resolveConsumerCfg(testGetenv(map[string]string{
		"CRM_FEED_URL":     "http://crm.example.test/custom-feed",
		"PROMPTS_FEED_URL": "http://prompts.example.test/custom-feed",
		"NTFY_TOPIC":       "notify-test",
		"NTFY_API_KEY":     "notify-test-token",
	}))
	if err != nil {
		t.Fatalf("resolveConsumerCfg: %v", err)
	}

	if cfg.feedURL != "http://crm.example.test/custom-feed" {
		t.Fatalf("feedURL = %q, want CRM_FEED_URL override", cfg.feedURL)
	}
	if cfg.promptsFeedURL != "http://prompts.example.test/custom-feed" {
		t.Fatalf("promptsFeedURL = %q, want PROMPTS_FEED_URL override", cfg.promptsFeedURL)
	}
}

func TestSpecPortComesFromRegistryNotifyPort(t *testing.T) {
	// R-RGSP-4A1K
	if got, ok := registry.Port("notify"); !ok || got != 3201 {
		t.Fatalf("registry.Port(%q) = %d, %v, want 3201, true", "notify", got, ok)
	}
	if got := registry.MustPort("notify"); got != 3201 {
		t.Fatalf("registry.MustPort(%q) = %d, want 3201", "notify", got)
	}
	if !specPortIsRegistryMustPort(t, filepath.Join("main.go"), "notify") {
		t.Fatalf("appkit.Spec Port is not registry.MustPort(%q)", "notify")
	}
}

func newIdleFeedServer(t *testing.T) *httptest.Server {
	t.Helper()
	srv := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		if r.URL.Path != "/feed" {
			http.NotFound(w, r)
			return
		}
		w.Header().Set("Content-Type", "text/event-stream")
		w.WriteHeader(http.StatusOK)
		if f, ok := w.(http.Flusher); ok {
			f.Flush()
		}
		<-r.Context().Done()
	}))
	t.Cleanup(srv.Close)
	return srv
}

func testGetenv(values map[string]string) func(string) string {
	return func(key string) string {
		return values[key]
	}
}

func specPortIsRegistryMustPort(t *testing.T, path, service string) bool {
	t.Helper()
	fset := token.NewFileSet()
	file, err := parser.ParseFile(fset, path, nil, 0)
	if err != nil {
		t.Fatalf("parse %s: %v", path, err)
	}

	found := false
	ast.Inspect(file, func(n ast.Node) bool {
		if found {
			return false
		}
		call, ok := n.(*ast.CallExpr)
		if !ok || !selectorIs(call.Fun, "appkit", "Main") || len(call.Args) != 1 {
			return true
		}
		lit, ok := call.Args[0].(*ast.CompositeLit)
		if !ok || !selectorIs(lit.Type, "appkit", "Spec") {
			return true
		}
		for _, elt := range lit.Elts {
			kv, ok := elt.(*ast.KeyValueExpr)
			if !ok || identName(kv.Key) != "Port" {
				continue
			}
			found = registryMustPortCall(kv.Value, service)
			return false
		}
		return true
	})
	return found
}

func registryMustPortCall(expr ast.Expr, service string) bool {
	call, ok := expr.(*ast.CallExpr)
	if !ok || !selectorIs(call.Fun, "registry", "MustPort") || len(call.Args) != 1 {
		return false
	}
	arg, ok := call.Args[0].(*ast.BasicLit)
	return ok && arg.Kind == token.STRING && arg.Value == fmt.Sprintf("%q", service)
}

func selectorIs(expr ast.Expr, pkg, name string) bool {
	sel, ok := expr.(*ast.SelectorExpr)
	if !ok {
		return false
	}
	return identName(sel.X) == pkg && sel.Sel.Name == name
}

func identName(expr ast.Expr) string {
	ident, ok := expr.(*ast.Ident)
	if !ok {
		return ""
	}
	return ident.Name
}

func testEnv(overrides map[string]string) []string {
	env := os.Environ()
	out := make([]string, 0, len(env)+len(overrides))
	for _, kv := range env {
		key, _, _ := strings.Cut(kv, "=")
		if _, ok := overrides[key]; ok {
			continue
		}
		out = append(out, kv)
	}
	for key, value := range overrides {
		out = append(out, key+"="+value)
	}
	return out
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
			t.Fatalf("notify exited before health: %v\nstdout:\n%s\nstderr:\n%s", err, stdout.String(), stderr.String())
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
	t.Fatalf("notify never served health at %s: %s\nstdout:\n%s\nstderr:\n%s", url, last, stdout.String(), stderr.String())
	return nil
}

func stopProcess(cancel context.CancelFunc, done <-chan error) {
	cancel()
	select {
	case <-done:
	case <-time.After(time.Second):
	}
}
