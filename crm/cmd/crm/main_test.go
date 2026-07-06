package main

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
	"strings"
	"testing"
	"time"

	"appkit/manifest"
	appweb "appkit/web"
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
		if bytes.HasPrefix(line, []byte("CRM_DB_PATH=")) || bytes.HasPrefix(line, []byte("CRM_GENERATION_PATH=")) {
			t.Fatalf("committed manifest.env contains runtime path line %q", line)
		}
	}
}

// R-8IAN-FB87
func TestManifestLibraryByteEqualsCommittedFile(t *testing.T) {
	got := manifest.Emit(manifest.Fields{
		App:     "crm",
		Mount:   "/srv/crm/",
		Default: false,
		Port:    3100,
		MCP:     true,
		Feed:    "/feed",
		Extras: []manifest.KV{
			{Key: "OUTBOX_RETENTION_DAYS", Value: "7"},
			{Key: "OUTBOX_RETENTION_MAX_ROWS", Value: "1000000"},
		},
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
func TestCRMBootsFromOpsctlLayoutAndServesHealth(t *testing.T) {
	root := t.TempDir()
	appRoot := filepath.Join(root, "crm")
	stateDir := filepath.Join(appRoot, "state")
	cacheDir := filepath.Join(appRoot, "cache")
	libexecDir := filepath.Join(appRoot, "libexec")
	binDir := filepath.Join(appRoot, "bin")
	etcDir := filepath.Join(appRoot, "etc")
	for _, dir := range []string{stateDir, cacheDir, libexecDir, binDir, etcDir} {
		if err := os.MkdirAll(dir, 0o755); err != nil {
			t.Fatalf("mkdir %s: %v", dir, err)
		}
	}

	binary := filepath.Join(libexecDir, "crm-vtest")
	build := exec.Command("go", "build", "-o", binary, ".")
	build.Env = os.Environ()
	if out, err := build.CombinedOutput(); err != nil {
		t.Fatalf("go build crm: %v\n%s", err, out)
	}

	run := filepath.Join(binDir, "run")
	if err := os.Symlink("../libexec/crm-vtest", run); err != nil {
		t.Fatalf("symlink bin/run: %v", err)
	}
	if resolved, err := filepath.EvalSymlinks(run); err != nil || resolved != binary {
		t.Fatalf("bin/run resolves to %q err=%v, want %q", resolved, err, binary)
	}

	port := freeTCPPort(t)
	dbPath := filepath.Join(stateDir, "crm.db")
	generationPath := filepath.Join(cacheDir, "crm.db.generation")
	ctx, cancel := context.WithCancel(context.Background())
	defer cancel()

	var stdout, stderr bytes.Buffer
	cmd := exec.CommandContext(ctx, run, "serve")
	cmd.Env = append(os.Environ(),
		"CRM_IP=127.0.0.1",
		fmt.Sprintf("CRM_PORT=%d", port),
		"CRM_DB_PATH="+dbPath,
		"CRM_GENERATION_PATH="+generationPath,
		"CRM_WWW_PATH="+wwwRoot(t),
	)
	cmd.Stdout = &stdout
	cmd.Stderr = &stderr
	if err := cmd.Start(); err != nil {
		t.Fatalf("start crm: %v", err)
	}
	done := make(chan error, 1)
	go func() { done <- cmd.Wait() }()
	defer stopProcess(cancel, done)

	doc := waitForHealth(t, port, done, &stdout, &stderr)
	if got := doc["service"]; got != "crm" {
		t.Fatalf("health service = %v, want crm; body=%v", got, doc)
	}
	if got := doc["status"]; got != "ok" {
		t.Fatalf("health status = %v, want ok; body=%v", got, doc)
	}
	if _, ok := doc["details"].(map[string]any); !ok {
		t.Fatalf("health details = %#v, want JSON object", doc["details"])
	}
	if _, err := os.Stat(dbPath); err != nil {
		t.Fatalf("crm did not create DB under state/: %v", err)
	}
	if _, err := os.Stat(generationPath); err != nil {
		t.Fatalf("crm did not create generation sidecar under cache/: %v", err)
	}
	if filepath.Dir(generationPath) != cacheDir {
		t.Fatalf("generation sidecar path %s is not under cache dir %s", generationPath, cacheDir)
	}
}

func TestWWWSiteLoadsRealShareTree(t *testing.T) {
	// R-MTM5-0PXH
	root := wwwRoot(t)
	if strings.Contains(root, "internal/web") {
		t.Fatalf("WWW root %q points at deleted internal web package", root)
	}

	site := loadWWW(t)
	rec := httptest.NewRecorder()
	if err := site.Render(rec, "landing.html", landingData("crm-real", "v1.2.3")); err != nil {
		t.Fatalf("render landing.html from share/www: %v", err)
	}
	if rec.Code != http.StatusOK || !strings.Contains(rec.Body.String(), "<title>crm-real") {
		t.Fatalf("share/www landing render = status %d body:\n%s", rec.Code, rec.Body.String())
	}

	for _, rel := range []string{
		"landing.html",
		filepath.Join("static", "tokens.css"),
		filepath.Join("static", "fonts", "space-grotesk.woff2"),
		filepath.Join("static", "fonts", "ibm-plex-sans.woff2"),
		filepath.Join("static", "fonts", "ibm-plex-mono-400.woff2"),
		filepath.Join("static", "fonts", "ibm-plex-mono-500.woff2"),
	} {
		info, err := os.Stat(filepath.Join(root, rel))
		if err != nil {
			t.Fatalf("share/www missing %s: %v", rel, err)
		}
		if info.IsDir() || info.Size() == 0 {
			t.Fatalf("share/www/%s is not a non-empty file: dir=%v size=%d", rel, info.IsDir(), info.Size())
		}
	}
}

func TestWWWSiteRendersLandingWithServiceAndVersion(t *testing.T) {
	// R-LAND-2K7P
	rec := renderLanding(t, "crm-test", "v9.8.7")

	if rec.Code != http.StatusOK {
		t.Fatalf("status = %d, want %d", rec.Code, http.StatusOK)
	}
	if got := rec.Header().Get("Content-Type"); got != "text/html; charset=utf-8" {
		t.Fatalf("Content-Type = %q, want text/html; charset=utf-8", got)
	}

	body := rec.Body.String()
	if count := strings.Count(body, "crm-test"); count != 3 {
		t.Fatalf("service name count = %d, want 3 in title, heading, and details\n%s", count, body)
	}
	if count := strings.Count(body, "v9.8.7"); count != 1 {
		t.Fatalf("version count = %d, want 1\n%s", count, body)
	}
}

func TestWWWSiteLinksOnlyAppLocalStaticAssets(t *testing.T) {
	// R-LAND-4M9Q
	// R-LAND-6N3R
	// R-SU82-2M8W
	rec := renderLanding(t, "crm", "dev")

	body := rec.Body.String()
	if !strings.Contains(body, `href="static/tokens.css"`) {
		t.Fatalf("landing HTML did not link local tokens.css:\n%s", body)
	}
	for _, forbidden := range []string{`href="/static/tokens.css"`, "dashboard", "/srv/dashboard", "https://", "http://"} {
		if strings.Contains(body, forbidden) {
			t.Fatalf("landing HTML contains forbidden cross-service asset reference %q:\n%s", forbidden, body)
		}
	}
}

func TestWWWSitePreloadsSelfServedFontFiles(t *testing.T) {
	// R-SVFY-GDZL
	rec := renderLanding(t, "crm", "dev")

	head := htmlHead(t, rec.Body.String())
	site := loadWWW(t)
	for _, font := range []string{"space-grotesk.woff2", "ibm-plex-sans.woff2"} {
		preload := `<link rel="preload" as="font" type="font/woff2" crossorigin href="static/fonts/` + font + `">`
		if !strings.Contains(head, preload) {
			t.Fatalf("landing head missing font preload %q:\n%s", preload, head)
		}

		req := httptest.NewRequest(http.MethodGet, "/static/tokens.css", nil)
		static := httptest.NewRecorder()
		site.Static().ServeHTTP(static, req)
		if !strings.Contains(static.Body.String(), `url('fonts/`+font+`')`) {
			t.Fatalf("tokens.css does not use matching self-served URL for %s:\n%s", font, static.Body.String())
		}
	}
}

func TestWWWSiteUsesCanonicalServiceLayout(t *testing.T) {
	// R-LAND-8P5S
	rec := renderLanding(t, "crm", "dev")

	body := rec.Body.String()
	for _, want := range []string{
		`<main>`,
		`<section aria-labelledby="page-title">`,
		`<div class="eyebrow">Contacts CRM</div>`,
		`<h1 id="page-title">crm</h1>`,
		`Crm keeps contacts, organizations, and deals in SQLite and publishes typed contact events to the event plane.`,
		`<dl aria-label="Service details">`,
		`<dd><code>POST /mcp</code></dd>`,
		`class="version"`,
	} {
		if !strings.Contains(body, want) {
			t.Fatalf("landing HTML missing %q:\n%s", want, body)
		}
	}
}

func TestWWWStaticServesTokensAndFonts(t *testing.T) {
	// R-ASST-2B8C
	// R-ASST-4D1E
	site := loadWWW(t)
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

			site.Static().ServeHTTP(rec, req)

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

func TestWWWTokensCSSDeclaresFontFaces(t *testing.T) {
	// R-ASST-6F3G
	// R-ST05-OUI7
	body := readWWWStatic(t, "/static/tokens.css")
	for _, want := range []string{
		`@font-face`,
		`url('fonts/space-grotesk.woff2')`,
		`url('fonts/ibm-plex-sans.woff2')`,
		`url('fonts/ibm-plex-mono-400.woff2')`,
		`url('fonts/ibm-plex-mono-500.woff2')`,
		`font-family: 'Space Grotesk'`,
		`font-family: 'IBM Plex Mono'`,
	} {
		if !strings.Contains(body, want) {
			t.Fatalf("tokens.css missing %q:\n%s", want, body)
		}
	}
	if strings.Contains(body, `url('/static/fonts/`) {
		t.Fatalf("tokens.css still contains origin-absolute font URL:\n%s", body)
	}
}

func TestWWWTokensCSSUsesOptionalFontDisplayForEveryFontFace(t *testing.T) {
	// R-SRS9-B2RI
	body := readWWWStatic(t, "/static/tokens.css")
	if strings.Contains(body, "font-display: swap") {
		t.Fatalf("tokens.css still contains font-display swap:\n%s", body)
	}
	if faces, optional := strings.Count(body, "@font-face"), strings.Count(body, "font-display: optional"); optional != faces {
		t.Fatalf("font-display optional count = %d, want one for each of %d @font-face blocks:\n%s", optional, faces, body)
	}
}

func TestExactRootRouteDoesNotShadowMCPOrUnknownPaths(t *testing.T) {
	// R-ROUT-3T2V
	// R-ROUT-7Y6Z
	mux := http.NewServeMux()
	mux.Handle("GET /{$}", landingHandler(loadWWW(t), "crm", "dev"))
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
	if root.Code != http.StatusOK || !strings.Contains(root.Body.String(), `<h1 id="page-title">crm</h1>`) {
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
			if strings.Contains(rec.Body.String(), `<h1 id="page-title">crm</h1>`) {
				t.Fatalf("GET %s returned landing page: status=%d body=%q", tc.path, rec.Code, rec.Body.String())
			}
		})
	}

	nope := httptest.NewRecorder()
	mux.ServeHTTP(nope, httptest.NewRequest(http.MethodGet, "/nope", nil))
	if nope.Code == http.StatusOK && strings.Contains(nope.Body.String(), `<h1 id="page-title">crm</h1>`) {
		t.Fatalf("GET /nope returned landing page: status=%d body=%q", nope.Code, nope.Body.String())
	}
	if nope.Code != http.StatusNotFound {
		t.Fatalf("GET /nope status = %d, want %d", nope.Code, http.StatusNotFound)
	}
}

func TestCompositionRootEnablesChassisWWWAndKeepsMCPWiring(t *testing.T) {
	// R-MUU1-EHO6
	// R-ROUT-5W4X
	src, err := os.ReadFile("main.go")
	if err != nil {
		t.Fatal(err)
	}
	main := string(src)

	for _, want := range []string{
		`WWW:        true,`,
		`rt.Handle("GET /{$}", http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {`,
		`rt.WWW().Render(w, "landing.html", struct {`,
		`Service string`,
		`Version string`,
		`rt.Handle("POST /mcp", rt.RequireIdentity(`,
		`mcp.NewHandler(svc, rt.Version(), rt.Service(), rt.Health(),`,
		`rt.Events(), rt.Subscriptions())`,
	} {
		if !strings.Contains(main, want) {
			t.Fatalf("cmd/crm/main.go missing %q", want)
		}
	}
	for _, forbidden := range []string{
		`"crm/internal/web"`,
		`rt.Handle("GET /static/"`,
		`web.LandingHandler`,
		`web.StaticHandler`,
	} {
		if strings.Contains(main, forbidden) {
			t.Fatalf("cmd/crm/main.go still contains %q", forbidden)
		}
	}

	landingLine := lineContaining(t, main, `rt.Handle("GET /{$}"`)
	if strings.Contains(landingLine, "RequireIdentity") {
		t.Fatalf("landing route is identity-gated: %s", landingLine)
	}
}

func TestNoCRMWebEmbedsRemainOutsideExistingEmbeddedPackages(t *testing.T) {
	needle := "go:" + "embed"
	for _, root := range []string{"../../cmd", "../../internal"} {
		err := filepath.WalkDir(root, func(path string, entry os.DirEntry, err error) error {
			if err != nil {
				return err
			}
			if entry.IsDir() {
				if path == "../../internal/db" || path == "../../internal/mcp" {
					return filepath.SkipDir
				}
				return nil
			}
			if filepath.Ext(path) != ".go" {
				return nil
			}
			src, err := os.ReadFile(path)
			if err != nil {
				return err
			}
			if strings.Contains(string(src), needle) {
				t.Fatalf("%s still contains %s", path, needle)
			}
			return nil
		})
		if err != nil {
			t.Fatalf("scan %s: %v", root, err)
		}
	}
}

func TestNginxLandingLocationIsExactMatchAndSessionGated(t *testing.T) {
	conf := readNginxConfig(t)
	block := nginxLocationBlock(t, conf, "location = /srv/crm/ {")

	// R-NGNX-2H5J
	if strings.Contains(conf, "location /srv/crm/ {\n    auth_request /_session-authn;") {
		t.Fatalf("prefix /srv/crm/ location is session-gated instead of exact landing location:\n%s", conf)
	}
	if block == "" {
		t.Fatal("landing exact-match location block is empty")
	}
	if prefixBlock := nginxLocationBlock(t, conf, "location /srv/crm/ {"); prefixBlock == block {
		t.Fatal("exact landing location was not distinct from bearer-gated prefix location")
	}

	// R-NGNX-4K7L
	if !strings.Contains(block, "auth_request /_session-authn;") {
		t.Fatalf("landing location missing session auth_request:\n%s", block)
	}
	if strings.Contains(block, "auth_request /_authn;") {
		t.Fatalf("landing location is bearer-gated instead of session-gated:\n%s", block)
	}
	if !strings.Contains(block, "proxy_set_header X-Owner-Email $crm_session_owner;") {
		t.Fatalf("landing location does not forward session owner identity:\n%s", block)
	}

	// R-NGNX-6M9N
	if !strings.Contains(block, "proxy_pass http://127.0.0.1:3100/;") {
		t.Fatalf("landing location does not proxy to upstream root with trailing slash:\n%s", block)
	}
}

func TestNginxExistingServiceLocationsSurvive(t *testing.T) {
	conf := readNginxConfig(t)

	// R-NGNX-8P1Q
	prefix := nginxLocationBlock(t, conf, "location /srv/crm/ {")
	if !strings.Contains(prefix, "auth_request /_authn;") {
		t.Fatalf("service prefix location missing bearer auth_request:\n%s", prefix)
	}
	if !strings.Contains(conf, "location = /srv/crm/feed { return 404; }") {
		t.Fatalf("feed denial location missing:\n%s", conf)
	}
	prm := nginxLocationBlock(t, conf, "location = /srv/crm/.well-known/oauth-protected-resource {")
	if strings.Contains(prm, "auth_request") {
		t.Fatalf("PRM bootstrap location unexpectedly gated:\n%s", prm)
	}
	if !strings.Contains(prm, "proxy_pass http://127.0.0.1:3100/.well-known/oauth-protected-resource;") {
		t.Fatalf("PRM bootstrap location missing upstream proxy_pass:\n%s", prm)
	}
}

func TestNginxStaticLocationIsSessionGatedAndProxiesStaticHandler(t *testing.T) {
	conf := readNginxConfig(t)
	block := nginxLocationBlock(t, conf, "location /srv/crm/static/ {")

	// R-SWNU-U5QA
	for _, want := range []string{
		"auth_request /_session-authn;",
		"proxy_pass http://127.0.0.1:3100/static/;",
		"proxy_set_header Host $host;",
		"proxy_set_header X-Forwarded-Proto $scheme;",
		"proxy_http_version 1.1;",
	} {
		if !strings.Contains(block, want) {
			t.Fatalf("static location missing %q:\n%s", want, block)
		}
	}
	if strings.Contains(block, "auth_request /_authn;") {
		t.Fatalf("static location is bearer-gated instead of session-gated:\n%s", block)
	}

	if landing := nginxLocationBlock(t, conf, "location = /srv/crm/ {"); !strings.Contains(landing, "auth_request /_session-authn;") {
		t.Fatalf("landing exact location changed unexpectedly:\n%s", landing)
	}
	if prefix := nginxLocationBlock(t, conf, "location /srv/crm/ {"); !strings.Contains(prefix, "auth_request /_authn;") {
		t.Fatalf("bearer prefix location changed unexpectedly:\n%s", prefix)
	}
	if !strings.Contains(conf, "location = /srv/crm/feed { return 404; }") {
		t.Fatalf("feed denial location changed unexpectedly:\n%s", conf)
	}
	prm := nginxLocationBlock(t, conf, "location = /srv/crm/.well-known/oauth-protected-resource {")
	if strings.Contains(prm, "auth_request") {
		t.Fatalf("PRM bootstrap location changed unexpectedly:\n%s", prm)
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
			t.Fatalf("crm exited before health: %v\nstdout:\n%s\nstderr:\n%s", err, stdout.String(), stderr.String())
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
	t.Fatalf("crm never served health at %s: %s\nstdout:\n%s\nstderr:\n%s", url, last, stdout.String(), stderr.String())
	return nil
}

func stopProcess(cancel context.CancelFunc, done <-chan error) {
	cancel()
	select {
	case <-done:
	case <-time.After(time.Second):
	}
}

func loadWWW(t *testing.T) *appweb.Site {
	t.Helper()
	site, err := appweb.Load(wwwRoot(t))
	if err != nil {
		t.Fatalf("load share/www: %v", err)
	}
	return site
}

func wwwRoot(t *testing.T) string {
	t.Helper()
	root, err := filepath.Abs(filepath.Join("..", "..", "share", "www"))
	if err != nil {
		t.Fatalf("resolve share/www: %v", err)
	}
	return root
}

func renderLanding(t *testing.T, service, version string) *httptest.ResponseRecorder {
	t.Helper()
	rec := httptest.NewRecorder()
	if err := loadWWW(t).Render(rec, "landing.html", landingData(service, version)); err != nil {
		t.Fatalf("render landing.html: %v", err)
	}
	return rec
}

func landingHandler(site *appweb.Site, service, version string) http.Handler {
	return http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		if r.URL.Path != "/" {
			http.NotFound(w, r)
			return
		}
		if err := site.Render(w, "landing.html", landingData(service, version)); err != nil {
			http.Error(w, "template error", http.StatusInternalServerError)
		}
	})
}

func landingData(service, version string) struct {
	Service string
	Version string
} {
	return struct {
		Service string
		Version string
	}{
		Service: service,
		Version: version,
	}
}

func readWWWStatic(t *testing.T, path string) string {
	t.Helper()
	req := httptest.NewRequest(http.MethodGet, path, nil)
	rec := httptest.NewRecorder()
	loadWWW(t).Static().ServeHTTP(rec, req)
	if rec.Code != http.StatusOK {
		t.Fatalf("GET %s status = %d, want %d\n%s", path, rec.Code, http.StatusOK, rec.Body.String())
	}
	return rec.Body.String()
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

func htmlHead(t *testing.T, body string) string {
	t.Helper()
	start := strings.Index(body, "<head>")
	if start == -1 {
		t.Fatalf("HTML missing head opener:\n%s", body)
	}
	end := strings.Index(body[start:], "</head>")
	if end == -1 {
		t.Fatalf("HTML missing head closer:\n%s", body)
	}
	return body[start : start+end+len("</head>")]
}

func readNginxConfig(t *testing.T) string {
	t.Helper()
	src, err := os.ReadFile(filepath.Join("..", "..", "etc", "nginx.conf"))
	if err != nil {
		t.Fatal(err)
	}
	return string(src)
}

func nginxLocationBlock(t *testing.T, conf, opener string) string {
	t.Helper()
	start := strings.Index(conf, opener)
	if start == -1 {
		t.Fatalf("nginx config missing %q", opener)
	}
	bodyStart := start + len(opener)
	endRel := strings.Index(conf[bodyStart:], "\n}")
	if endRel == -1 {
		t.Fatalf("nginx config location %q has no closing brace", opener)
	}
	return conf[start : bodyStart+endRel+len("\n}")]
}
