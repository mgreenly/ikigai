package web

import (
	"bytes"
	"context"
	"encoding/json"
	"fmt"
	"io"
	"net"
	"net/http"
	"net/http/httptest"
	"os"
	"os/exec"
	"path/filepath"
	"strconv"
	"strings"
	"testing"
	"time"
)

func TestLandingHandlerRendersHTMLWithServiceAndVersion(t *testing.T) {
	// R-LAND-7Q3D
	// R-LAND-3T9H
	// R-LAND-9R5F
	// R-LAND-1S7G
	req := httptest.NewRequest(http.MethodGet, "/", nil)
	rec := httptest.NewRecorder()

	LandingHandler("scripts-test", "v9.8.7").ServeHTTP(rec, req)

	if rec.Code != http.StatusOK {
		t.Fatalf("status = %d, want %d", rec.Code, http.StatusOK)
	}
	if got := rec.Header().Get("Content-Type"); got != "text/html; charset=utf-8" {
		t.Fatalf("Content-Type = %q, want text/html; charset=utf-8", got)
	}

	body := rec.Body.String()
	if count := strings.Count(body, "scripts-test"); count != 3 {
		t.Fatalf("service name count = %d, want 3 in title, heading, and service detail\n%s", count, body)
	}
	if count := strings.Count(body, "v9.8.7"); count != 1 {
		t.Fatalf("version count = %d, want 1\n%s", count, body)
	}

	other := httptest.NewRecorder()
	LandingHandler("other-service", "build-123").ServeHTTP(other, req)
	normalizedBody := strings.ReplaceAll(strings.ReplaceAll(body, "scripts-test", "{{service}}"), "v9.8.7", "{{version}}")
	normalizedOther := strings.ReplaceAll(strings.ReplaceAll(other.Body.String(), "other-service", "{{service}}"), "build-123", "{{version}}")
	if normalizedBody != normalizedOther {
		t.Fatalf("landing HTML changed beyond service/version substitutions:\n%s\n---\n%s", normalizedBody, normalizedOther)
	}
}

func TestLandingHandlerLinksOnlyAppLocalStaticAssets(t *testing.T) {
	// R-ASST-7Y1N
	req := httptest.NewRequest(http.MethodGet, "/", nil)
	rec := httptest.NewRecorder()

	LandingHandler("scripts", "dev").ServeHTTP(rec, req)

	body := rec.Body.String()
	if !strings.Contains(body, `href="/static/tokens.css"`) {
		t.Fatalf("landing HTML did not link local tokens.css:\n%s", body)
	}
	for _, forbidden := range []string{"dashboard", "/srv/dashboard", "https://", "http://"} {
		if strings.Contains(body, forbidden) {
			t.Fatalf("landing HTML contains forbidden cross-service asset reference %q:\n%s", forbidden, body)
		}
	}
}

func TestLandingHandlerRendersHomeLinkToDashboardApex(t *testing.T) {
	// R-HOME-8R2V
	req := httptest.NewRequest(http.MethodGet, "/", nil)
	rec := httptest.NewRecorder()

	LandingHandler("scripts", "dev").ServeHTTP(rec, req)

	if rec.Code != http.StatusOK {
		t.Fatalf("status = %d, want %d", rec.Code, http.StatusOK)
	}

	body := rec.Body.String()
	if !strings.Contains(body, `<main>`+"\n"+`    <a class="home" href="/">Home</a>`) {
		t.Fatalf("landing HTML does not put the Home link first in the body:\n%s", body)
	}
	if count := strings.Count(body, `href="/"`); count != 1 {
		t.Fatalf(`href="/" count = %d, want exactly one dashboard-apex link:\n%s`, count, body)
	}
	for _, want := range []string{
		`.home {`,
		`top: var(--space-8);`,
		`position: relative;`,
		`.home:hover,`,
		`.home:focus-visible {`,
	} {
		if !strings.Contains(body, want) {
			t.Fatalf("landing HTML missing home style %q:\n%s", want, body)
		}
	}
}

func TestLandingHandlerUsesCronCanonicalStructureForScripts(t *testing.T) {
	req := httptest.NewRequest(http.MethodGet, "/", nil)
	rec := httptest.NewRecorder()

	LandingHandler("scripts", "dev").ServeHTTP(rec, req)

	body := rec.Body.String()
	for _, want := range []string{
		`<title>scripts · scripts</title>`,
		`<link rel="stylesheet" href="/static/tokens.css">`,
		`<a class="home" href="/">Home</a>`,
		`<section aria-labelledby="page-title">`,
		`<div class="eyebrow">Script runner</div>`,
		`<h1 id="page-title">scripts</h1>`,
		`Scripts runs deterministic Python scripts wired to suite events and publishes completion events back to the event plane.`,
		`<dl aria-label="Service details">`,
		`<dt>API</dt>`,
		`<dd><code>POST /mcp</code></dd>`,
		`class="version"`,
	} {
		if !strings.Contains(body, want) {
			t.Fatalf("landing HTML missing %q:\n%s", want, body)
		}
	}
	for _, forbidden := range []string{`class="shell"`, `class="card"`, `Scheduled event emitter`, `minute boundaries`} {
		if strings.Contains(body, forbidden) {
			t.Fatalf("landing HTML contains stale markup %q:\n%s", forbidden, body)
		}
	}
}

func TestStaticHandlerServesTokensAndFonts(t *testing.T) {
	// R-ASST-5X8M
	// R-ASST-9Z3P
	cases := []struct {
		path        string
		contentType string
	}{
		{path: "/static/tokens.css", contentType: "text/css; charset=utf-8"},
		{path: "/static/fonts/space-grotesk.woff2", contentType: "font/woff2"},
		{path: "/static/fonts/ibm-plex-sans.woff2", contentType: "font/woff2"},
		{path: "/static/fonts/ibm-plex-mono-400.woff2", contentType: "font/woff2"},
		{path: "/static/fonts/ibm-plex-mono-500.woff2", contentType: "font/woff2"},
	}

	for _, tc := range cases {
		t.Run(tc.path, func(t *testing.T) {
			req := httptest.NewRequest(http.MethodGet, tc.path, nil)
			rec := httptest.NewRecorder()

			StaticHandler().ServeHTTP(rec, req)

			if rec.Code != http.StatusOK {
				t.Fatalf("status = %d, want %d", rec.Code, http.StatusOK)
			}
			if got := rec.Header().Get("Content-Type"); got != tc.contentType {
				t.Fatalf("Content-Type = %q, want %q", got, tc.contentType)
			}
			if rec.Body.Len() == 0 {
				t.Fatal("body is empty")
			}
		})
	}
}

func TestTokensCSSDeclaresEmbeddedFontFaces(t *testing.T) {
	req := httptest.NewRequest(http.MethodGet, "/static/tokens.css", nil)
	rec := httptest.NewRecorder()

	StaticHandler().ServeHTTP(rec, req)

	body := rec.Body.String()
	for _, want := range []string{
		`@font-face`,
		`url('/static/fonts/space-grotesk.woff2')`,
		`url('/static/fonts/ibm-plex-sans.woff2')`,
		`url('/static/fonts/ibm-plex-mono-400.woff2')`,
		`url('/static/fonts/ibm-plex-mono-500.woff2')`,
		`font-family: 'Space Grotesk'`,
		`font-family: 'IBM Plex Mono'`,
	} {
		if !strings.Contains(body, want) {
			t.Fatalf("tokens.css missing %q:\n%s", want, body)
		}
	}
}

func TestExactRootRouteDoesNotShadowExistingPaths(t *testing.T) {
	// R-ROUT-8U2J
	// R-ROUT-1V4K
	// R-ROUT-3W6L
	mux := http.NewServeMux()
	mux.Handle("GET /{$}", LandingHandler("scripts", "dev"))
	mux.HandleFunc("GET /health", func(w http.ResponseWriter, r *http.Request) {
		_, _ = io.WriteString(w, "health")
	})
	mux.HandleFunc("GET /feed", func(w http.ResponseWriter, r *http.Request) {
		_, _ = io.WriteString(w, "feed")
	})
	mux.HandleFunc("GET /.well-known/prm", func(w http.ResponseWriter, r *http.Request) {
		_, _ = io.WriteString(w, "prm")
	})
	mux.HandleFunc("POST /mcp", func(w http.ResponseWriter, r *http.Request) {
		_, _ = io.WriteString(w, "mcp")
	})

	root := httptest.NewRecorder()
	mux.ServeHTTP(root, httptest.NewRequest(http.MethodGet, "/", nil))
	if root.Code != http.StatusOK || !strings.Contains(root.Body.String(), `<h1 id="page-title">scripts</h1>`) {
		t.Fatalf("root did not dispatch landing handler: status=%d body=%q", root.Code, root.Body.String())
	}

	mcp := httptest.NewRecorder()
	mux.ServeHTTP(mcp, httptest.NewRequest(http.MethodPost, "/mcp", nil))
	if mcp.Code != http.StatusOK || mcp.Body.String() != "mcp" {
		t.Fatalf("POST /mcp = status %d body %q, want stub handler", mcp.Code, mcp.Body.String())
	}

	for _, tc := range []struct {
		path string
		body string
	}{
		{path: "/health", body: "health"},
		{path: "/feed", body: "feed"},
		{path: "/.well-known/prm", body: "prm"},
	} {
		t.Run(tc.path, func(t *testing.T) {
			rec := httptest.NewRecorder()
			mux.ServeHTTP(rec, httptest.NewRequest(http.MethodGet, tc.path, nil))

			if rec.Code != http.StatusOK || rec.Body.String() != tc.body {
				t.Fatalf("GET %s = status %d body %q, want stub handler body %q", tc.path, rec.Code, rec.Body.String(), tc.body)
			}
			if strings.Contains(rec.Body.String(), `<h1 id="page-title">scripts</h1>`) {
				t.Fatalf("GET %s returned landing page: status=%d body=%q", tc.path, rec.Code, rec.Body.String())
			}
		})
	}

	nope := httptest.NewRecorder()
	mux.ServeHTTP(nope, httptest.NewRequest(http.MethodGet, "/nope", nil))
	if nope.Code == http.StatusOK && strings.Contains(nope.Body.String(), `<h1 id="page-title">scripts</h1>`) {
		t.Fatalf("GET /nope returned landing page: status=%d body=%q", nope.Code, nope.Body.String())
	}
	if nope.Code != http.StatusNotFound {
		t.Fatalf("GET /nope status = %d, want %d", nope.Code, http.StatusNotFound)
	}
}

func TestCompositionRootMountsLandingUngatedAndKeepsMCPWiring(t *testing.T) {
	src, err := os.ReadFile("../../cmd/scripts/main.go")
	if err != nil {
		t.Fatal(err)
	}
	main := string(src)

	for _, want := range []string{
		`rt.Handle("GET /{$}", web.LandingHandler(rt.Service(), rt.Version()))`,
		`rt.Handle("GET /static/", web.StaticHandler())`,
		`rt.Handle("POST /mcp", rt.RequireIdentity(`,
		`mcp.NewHandler(svc, rt.Version(), rt.Service(), rt.Health())`,
	} {
		if !strings.Contains(main, want) {
			t.Fatalf("cmd/scripts/main.go missing %q", want)
		}
	}

	landingLine := lineContaining(t, main, `web.LandingHandler`)
	if strings.Contains(landingLine, "RequireIdentity") {
		t.Fatalf("landing route is identity-gated: %s", landingLine)
	}
}

func TestCompositionRootAdoptsNewScriptsLayout(t *testing.T) {
	// R-4LKF-FB23
	src, err := os.ReadFile("../../cmd/scripts/main.go")
	if err != nil {
		t.Fatal(err)
	}
	main := string(src)

	for _, want := range []string{
		`setDefaultEnv("SCRIPTS_DB_PATH", filepath.Join("state", "scripts.db"))`,
		`setDefaultEnv("SCRIPTS_GENERATION_PATH", filepath.Join("cache", "scripts.db.generation"))`,
		`dbPath := config.EnvOr(os.Getenv, "SCRIPTS_DB_PATH", filepath.Join("state", "scripts.db"))`,
		`runsDir := filepath.Join(rootDir, "runs")`,
		`os.MkdirAll(runsDir, 0o700)`,
		`run := runner.New(store, rootDir, runTTL)`,
		`svc := script.NewService(store, runsDir, run)`,
	} {
		if !strings.Contains(main, want) {
			t.Fatalf("cmd/scripts/main.go missing new-layout wiring %q", want)
		}
	}
	if strings.Contains(main, `filepath.Join(filepath.Dir(dbPath), "data")`) {
		t.Fatalf("cmd/scripts/main.go still derives run data under the DB state directory")
	}

	manifest, err := os.ReadFile("../../etc/manifest.env")
	if err != nil {
		t.Fatal(err)
	}
	for _, want := range []string{
		"SCRIPTS_DB_PATH=state/scripts.db\n",
		"SCRIPTS_GENERATION_PATH=cache/scripts.db.generation\n",
	} {
		if !strings.Contains(string(manifest), want) {
			t.Fatalf("manifest.env missing exported unit env %q", want)
		}
	}

	for _, path := range []string{"../../bin/backup", "../../bin/restore"} {
		body, err := os.ReadFile(path)
		if err != nil {
			t.Fatal(err)
		}
		text := string(body)
		if !strings.Contains(text, "is retired; use opsctl") {
			t.Fatalf("%s is not retired in favor of opsctl:\n%s", path, text)
		}
		for _, forbidden := range []string{"aws s3", "ssh ", "systemctl stop"} {
			if strings.Contains(text, forbidden) {
				t.Fatalf("%s still contains service-owned backup/restore mechanism %q:\n%s", path, forbidden, text)
			}
		}
	}
}

func TestFreshSetupBootsFromNewScriptsLayoutAndPassesHealth(t *testing.T) {
	// R-4LKF-FB23
	tmp := t.TempDir()
	root := filepath.Join(tmp, "opt", "scripts")
	stateDir := filepath.Join(root, "state")
	cacheDir := filepath.Join(root, "cache")
	runsDir := filepath.Join(root, "runs")
	libexecDir := filepath.Join(root, "libexec")
	binDir := filepath.Join(root, "bin")
	for _, dir := range []string{stateDir, cacheDir, libexecDir, binDir} {
		if err := os.MkdirAll(dir, 0o750); err != nil {
			t.Fatalf("mkdir %s: %v", dir, err)
		}
	}
	if _, err := os.Stat(runsDir); !os.IsNotExist(err) {
		t.Fatalf("runs dir exists before boot (stat err=%v)", err)
	}

	version := "v1.2.3"
	bin := filepath.Join(libexecDir, "scripts-"+version)
	build := exec.Command("go", "build", "-o", bin, "../../cmd/scripts")
	build.Stdout = os.Stdout
	build.Stderr = os.Stderr
	if err := build.Run(); err != nil {
		t.Fatalf("build scripts binary: %v", err)
	}
	runLink := filepath.Join(binDir, "run")
	if err := os.Symlink(filepath.Join("..", "libexec", filepath.Base(bin)), runLink); err != nil {
		t.Fatalf("create bin/run symlink: %v", err)
	}
	if target, err := os.Readlink(runLink); err != nil || target != filepath.Join("..", "libexec", "scripts-"+version) {
		t.Fatalf("bin/run symlink = (%q, %v), want ../libexec/scripts-%s", target, err, version)
	}

	port := freeLoopbackPort(t)
	addr := net.JoinHostPort("127.0.0.1", strconv.Itoa(port))
	dbPath := filepath.Join(stateDir, "scripts.db")
	genPath := filepath.Join(cacheDir, "scripts.db.generation")
	stdout, stderr := startScriptsBinaryAndReachHealth(t, root, runLink, addr, map[string]string{
		"IKIGENBA_DOMAIN":           "example.test",
		"SCRIPTS_PORT":              strconv.Itoa(port),
		"SCRIPTS_DB_PATH":           filepath.Join("state", "scripts.db"),
		"SCRIPTS_GENERATION_PATH":   filepath.Join("cache", "scripts.db.generation"),
		"SCRIPTS_CRON_FEED_URL":     "http://" + addr + "/feed",
		"SCRIPTS_CRM_FEED_URL":      "http://" + addr + "/feed",
		"SCRIPTS_LEDGER_FEED_URL":   "http://" + addr + "/feed",
		"SCRIPTS_DROPBOX_FEED_URL":  "http://" + addr + "/feed",
		"SCRIPTS_PROMPTS_FEED_URL":  "http://" + addr + "/feed",
		"SCRIPTS_CRON_FROM":         "tail",
		"SCRIPTS_CRM_FROM":          "tail",
		"SCRIPTS_LEDGER_FROM":       "tail",
		"SCRIPTS_DROPBOX_FROM":      "tail",
		"SCRIPTS_PROMPTS_FROM":      "tail",
		"OUTBOX_RETENTION_DAYS":     "7",
		"OUTBOX_RETENTION_MAX_ROWS": "1000000",
	})
	if info, err := os.Stat(dbPath); err != nil || info.IsDir() {
		t.Fatalf("db file after boot = (%v, %v), want file\nstdout:\n%s\nstderr:\n%s", info, err, stdout, stderr)
	}
	if generation, err := os.ReadFile(genPath); err != nil || strings.TrimSpace(string(generation)) == "" {
		t.Fatalf("generation sidecar after boot = %q, %v; want non-empty file\nstdout:\n%s\nstderr:\n%s", generation, err, stdout, stderr)
	}
	if info, err := os.Stat(runsDir); err != nil || !info.IsDir() {
		t.Fatalf("runs dir after boot = (%v, %v), want recreated directory\nstdout:\n%s\nstderr:\n%s", info, err, stdout, stderr)
	}
	if _, err := os.Stat(filepath.Join(stateDir, "runs")); !os.IsNotExist(err) {
		t.Fatalf("state/runs should not exist; runs are rebuildable outside state (stat err=%v)", err)
	}
	if _, err := os.Stat(filepath.Join(root, "backup")); !os.IsNotExist(err) {
		t.Fatalf("service-owned backup path should not exist; backup/restore is opsctl-owned (stat err=%v)", err)
	}
}

func startScriptsBinaryAndReachHealth(t *testing.T, root, bin, addr string, env map[string]string) (string, string) {
	t.Helper()
	ctx, cancel := context.WithTimeout(context.Background(), 15*time.Second)
	defer cancel()

	cmd := exec.CommandContext(ctx, bin, "serve", "--port", portFromAddr(t, addr))
	cmd.Dir = root
	cmd.Env = append(os.Environ(), "HOME="+root)
	for k, v := range env {
		cmd.Env = append(cmd.Env, k+"="+v)
	}
	var stdout, stderr bytes.Buffer
	cmd.Stdout = &stdout
	cmd.Stderr = &stderr

	if err := cmd.Start(); err != nil {
		t.Fatalf("start scripts binary: %v", err)
	}

	waitErr := waitForScriptsHealth(ctx, addr)
	if waitErr == nil {
		if err := cmd.Process.Signal(os.Interrupt); err != nil {
			t.Fatalf("interrupt scripts binary: %v\nstdout:\n%s\nstderr:\n%s", err, stdout.String(), stderr.String())
		}
	} else if cmd.Process != nil {
		_ = cmd.Process.Kill()
	}

	done := make(chan error, 1)
	go func() { done <- cmd.Wait() }()
	var err error
	select {
	case err = <-done:
	case <-time.After(5 * time.Second):
		_ = cmd.Process.Kill()
		err = fmt.Errorf("timed out waiting for scripts binary to exit")
	}
	if waitErr != nil {
		t.Fatalf("scripts binary did not reach /health: %v\nwait: %v\nstdout:\n%s\nstderr:\n%s", waitErr, err, stdout.String(), stderr.String())
	}
	if err != nil {
		t.Fatalf("scripts binary exited after health with %v, want graceful SIGINT shutdown\nstdout:\n%s\nstderr:\n%s", err, stdout.String(), stderr.String())
	}
	return stdout.String(), stderr.String()
}

func freeLoopbackPort(t *testing.T) int {
	t.Helper()
	ln, err := net.Listen("tcp", "127.0.0.1:0")
	if err != nil {
		t.Fatalf("allocate loopback port: %v", err)
	}
	defer ln.Close()
	return ln.Addr().(*net.TCPAddr).Port
}

func portFromAddr(t *testing.T, addr string) string {
	t.Helper()
	_, port, err := net.SplitHostPort(addr)
	if err != nil {
		t.Fatalf("split addr %q: %v", addr, err)
	}
	return port
}

func waitForScriptsHealth(ctx context.Context, addr string) error {
	client := &http.Client{Timeout: 200 * time.Millisecond}
	tick := time.NewTicker(25 * time.Millisecond)
	defer tick.Stop()
	var lastErr error
	for {
		req, err := http.NewRequestWithContext(ctx, http.MethodGet, "http://"+addr+"/health", nil)
		if err != nil {
			return err
		}
		resp, err := client.Do(req)
		if err == nil {
			var body map[string]any
			decodeErr := json.NewDecoder(resp.Body).Decode(&body)
			closeErr := resp.Body.Close()
			if resp.StatusCode == http.StatusOK && decodeErr == nil && closeErr == nil &&
				body["status"] == "ok" && body["service"] == "scripts" {
				return nil
			}
			lastErr = fmt.Errorf("health response status=%d body=%v decode=%v close=%v", resp.StatusCode, body, decodeErr, closeErr)
		} else {
			lastErr = err
		}

		select {
		case <-ctx.Done():
			if lastErr != nil {
				return lastErr
			}
			return ctx.Err()
		case <-tick.C:
		}
	}
}

func lineContaining(t *testing.T, text, needle string) string {
	t.Helper()
	for _, line := range strings.Split(text, "\n") {
		if strings.Contains(line, needle) {
			return line
		}
	}
	t.Fatalf("no line contains %q", needle)
	return ""
}
