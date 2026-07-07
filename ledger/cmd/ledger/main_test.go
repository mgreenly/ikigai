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
	"regexp"
	"strings"
	"testing"
	"time"

	"appkit/manifest"
	appweb "appkit/web"
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
		if bytes.HasPrefix(line, []byte("LEDGER_DB_PATH=")) || bytes.HasPrefix(line, []byte("LEDGER_GENERATION_PATH=")) {
			t.Fatalf("committed manifest.env contains runtime path line %q", line)
		}
	}
}

// R-8IAN-FB87
// R-4WLS-RJH6
// R-4VDW-DRQH
func TestManifestLibraryByteEqualsCommittedFile(t *testing.T) {
	got := manifest.Emit(manifest.Fields{
		App:     "ledger",
		Mount:   "/srv/ledger/",
		Default: false,
		// This delegates the otherwise-inline appkit.Spec port proof to the same registry lookup.
		Port: registry.MustPort("ledger"),
		MCP:  true,
		Feed: "/feed",
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
func TestLedgerBootsFromOpsctlLayoutAndServesHealth(t *testing.T) {
	root := t.TempDir()
	appRoot := filepath.Join(root, "ledger")
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
	copyTree(t, wwwRoot(t), filepath.Join(shareVersionDir, "www"))
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

	binary := filepath.Join(libexecDir, "ledger-"+version)
	build := exec.Command("go", "build", "-o", binary, ".")
	build.Env = os.Environ()
	if out, err := build.CombinedOutput(); err != nil {
		t.Fatalf("go build ledger: %v\n%s", err, out)
	}

	run := filepath.Join(binDir, "run")
	if err := os.Symlink("../libexec/ledger-"+version, run); err != nil {
		t.Fatalf("symlink bin/run: %v", err)
	}
	if resolved, err := filepath.EvalSymlinks(run); err != nil || resolved != binary {
		t.Fatalf("bin/run resolves to %q err=%v, want %q", resolved, err, binary)
	}

	port := freeTCPPort(t)
	dbPath := filepath.Join(stateDir, "ledger.db")
	generationPath := filepath.Join(cacheDir, "ledger.db.generation")
	ctx, cancel := context.WithCancel(context.Background())
	defer cancel()

	var stdout, stderr bytes.Buffer
	cmd := exec.CommandContext(ctx, run, "serve")
	cmd.Env = append(os.Environ(),
		"LEDGER_IP=127.0.0.1",
		fmt.Sprintf("LEDGER_PORT=%d", port),
		"LEDGER_DB_PATH="+dbPath,
		"LEDGER_GENERATION_PATH="+generationPath,
		"LEDGER_WWW_PATH="+filepath.Join(shareDir, "current", "www"),
	)
	cmd.Stdout = &stdout
	cmd.Stderr = &stderr
	if err := cmd.Start(); err != nil {
		t.Fatalf("start ledger: %v", err)
	}
	done := make(chan error, 1)
	go func() { done <- cmd.Wait() }()
	defer stopProcess(cancel, done)

	doc := waitForHealth(t, port, done, &stdout, &stderr)
	if got := doc["service"]; got != "ledger" {
		t.Fatalf("health service = %v, want ledger; body=%v", got, doc)
	}
	if got := doc["status"]; got != "ok" {
		t.Fatalf("health status = %v, want ok; body=%v", got, doc)
	}
	if _, ok := doc["details"].(map[string]any); !ok {
		t.Fatalf("health details = %#v, want JSON object", doc["details"])
	}
	if _, err := os.Stat(dbPath); err != nil {
		t.Fatalf("ledger did not create DB under state/: %v", err)
	}
	if _, err := os.Stat(generationPath); err != nil {
		t.Fatalf("ledger did not create generation sidecar under cache/: %v", err)
	}
	if filepath.Dir(generationPath) != cacheDir {
		t.Fatalf("generation sidecar path %s is not under cache dir %s", generationPath, cacheDir)
	}
}

func TestLedgerSpecEnablesChassisWWWAndKeepsMCPWiring(t *testing.T) {
	spec := ledgerSpec()

	// R-509H-WUP9
	if !spec.WWW {
		t.Fatal("ledgerSpec().WWW = false, want true")
	}
	if !spec.MCP {
		t.Fatal("ledgerSpec().MCP = false, want true")
	}
	if spec.Feed != "/feed" {
		t.Fatalf("ledgerSpec().Feed = %q, want /feed", spec.Feed)
	}

	src, err := os.ReadFile("main.go")
	if err != nil {
		t.Fatal(err)
	}
	main := string(src)
	for _, want := range []string{
		`WWW:        true,`,
		`rt.Handle("GET /{$}", landingHandler(rt.WWW(), rt.Service(), rt.Version()))`,
		`rt.Handle("POST /mcp", rt.RequireIdentity(`,
		`Producer: func(ob *outbox.Outbox) error {`,
	} {
		if !strings.Contains(main, want) {
			t.Fatalf("cmd/ledger/main.go missing %q", want)
		}
	}

	// R-51HE-AMFY
	for _, forbidden := range []string{
		`"ledger/internal/web"`,
		`rt.Handle("GET /static/`,
		`web.LandingHandler`,
		`web.StaticHandler`,
	} {
		if strings.Contains(main, forbidden) {
			t.Fatalf("cmd/ledger/main.go still contains %q", forbidden)
		}
	}
}

func TestWWWSiteLoadsRealShareTree(t *testing.T) {
	root := wwwRoot(t)
	if strings.Contains(root, "internal/web") {
		t.Fatalf("WWW root %q points at deleted internal web package", root)
	}

	site := loadWWW(t)
	rec := httptest.NewRecorder()
	if err := site.Render(rec, "landing.html", landingData("ledger-real", "v1.2.3")); err != nil {
		t.Fatalf("render landing.html from share/www: %v", err)
	}
	if rec.Code != http.StatusOK || !strings.Contains(rec.Body.String(), "<title>ledger-real") {
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

func TestWWWSiteRendersLandingWithServiceVersionAndHTMLContentType(t *testing.T) {
	rec := renderLanding(t, `ledger <service>`, `v1&2`)

	// R-LAND-3C9D
	if rec.Code != http.StatusOK {
		t.Fatalf("status = %d, want %d", rec.Code, http.StatusOK)
	}
	if got := rec.Header().Get("Content-Type"); got != "text/html; charset=utf-8" {
		t.Fatalf("content-type = %q, want text/html; charset=utf-8", got)
	}

	body := rec.Body.String()
	// R-LAND-5E1F
	if !strings.Contains(body, "ledger &lt;service&gt;") {
		t.Fatalf("rendered body did not contain escaped service name: %s", body)
	}
	if !strings.Contains(body, ">v1&amp;2<") {
		t.Fatalf("rendered body did not contain escaped version: %s", body)
	}
}

func TestWWWSiteReferencesOnlyDocumentRelativeLocalStaticAssets(t *testing.T) {
	rec := renderLanding(t, "ledger", "1.2.3")
	body := rec.Body.String()

	// R-LAND-7G2H
	// R-7EJP-A1NB
	if !strings.Contains(body, `href="static/tokens.css"`) {
		t.Fatalf("landing markup does not link share/www tokens.css: %s", body)
	}
	if strings.Contains(body, `href="/static/tokens.css"`) {
		t.Fatalf("landing markup uses origin-absolute tokens.css href: %s", body)
	}
	for _, forbidden := range []string{"http://", "https://", "fonts.googleapis.com", "dashboard"} {
		if strings.Contains(body, forbidden) {
			t.Fatalf("landing markup references external or dashboard asset %q: %s", forbidden, body)
		}
	}
}

func TestWWWSitePreloadsSelfServedFontFiles(t *testing.T) {
	rec := renderLanding(t, "ledger", "1.2.3")
	head := htmlHead(t, rec.Body.String())
	css := readWWWStatic(t, "/static/tokens.css")

	// R-7FRL-NTE0
	for _, font := range []string{"space-grotesk.woff2", "ibm-plex-sans.woff2"} {
		tag := linkTagContaining(t, head, `href="static/fonts/`+font+`"`)
		for _, want := range []string{
			`rel="preload"`,
			`as="font"`,
			`type="font/woff2"`,
			`crossorigin`,
			`href="static/fonts/` + font + `"`,
		} {
			if !strings.Contains(tag, want) {
				t.Fatalf("landing font preload for %s missing %q: %s", font, want, tag)
			}
		}
		if !strings.Contains(css, `url('fonts/`+font+`')`) {
			t.Fatalf("tokens.css missing matching @font-face src for preloaded font %s", font)
		}
	}
	if strings.Contains(head, "ibm-plex-mono-400.woff2") || strings.Contains(head, "ibm-plex-mono-500.woff2") {
		t.Fatalf("landing head preloads mono font: %s", head)
	}
}

func TestWWWSiteLandingIncludesHomeLinkToDashboardApex(t *testing.T) {
	rec := renderLanding(t, "ledger", "1.2.3")
	body := rec.Body.String()

	// R-HOME-4M6R
	for _, want := range []string{
		`<a class="home" href="/">Home</a>`,
		".home {",
		"position: absolute;",
		"top: var(--space-8);",
		"position: relative;",
		".home:hover,\n    .home:focus-visible",
		"color: var(--color-text);",
	} {
		if !strings.Contains(body, want) {
			t.Fatalf("body missing Home link content %q: %s", want, body)
		}
	}
	if strings.Contains(body, ">Dashboard</a>") {
		t.Fatalf("landing markup used Dashboard link text instead of Home: %s", body)
	}
}

func TestWWWSiteLandingAppliesCarbonTypeScale(t *testing.T) {
	rec := renderLanding(t, "ledger", "1.2.3")
	body := rec.Body.String()

	// R-LAND-9J4K
	for _, want := range []string{
		"width: min(100% - 32px, 960px)",
		"font-family: var(--font-display)",
		"font-size: clamp(40px, 8vw, var(--text-display-size))",
		"line-height: var(--text-display-lh)",
		"font-family: var(--font-mono)",
		"font-size: var(--text-label-size)",
		"<code>POST /mcp</code>",
	} {
		if !strings.Contains(body, want) {
			t.Fatalf("landing markup missing Carbon styling %q in body: %s", want, body)
		}
	}
}

func TestExactRootRouteDispatchesToLanding(t *testing.T) {
	rec := httptest.NewRecorder()
	composedMux(t, http.NotFoundHandler()).ServeHTTP(rec, httptest.NewRequest(http.MethodGet, "/", nil))

	if rec.Code != http.StatusOK {
		t.Fatalf("status = %d, want %d", rec.Code, http.StatusOK)
	}
	body := rec.Body.String()
	if !strings.Contains(body, "ledger") || !strings.Contains(body, "9.9.9-test") {
		t.Fatalf("GET / did not reach landing handler: %s", body)
	}
}

func TestExactRootRouteDoesNotCaptureNonRootPaths(t *testing.T) {
	for _, path := range []string{"/health", "/feed", "/.well-known/prm", "/nope"} {
		rec := httptest.NewRecorder()
		composedMux(t, http.NotFoundHandler()).ServeHTTP(rec, httptest.NewRequest(http.MethodGet, path, nil))

		// R-ROUT-2M6N
		if rec.Code != http.StatusNotFound {
			t.Fatalf("GET %s status = %d, want %d", path, rec.Code, http.StatusNotFound)
		}
		if strings.Contains(rec.Body.String(), "ledger") {
			t.Fatalf("GET %s returned the landing page", path)
		}
	}
}

func TestExactRootRouteDoesNotShadowMCP(t *testing.T) {
	stub := http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		w.WriteHeader(http.StatusTeapot)
		_, _ = w.Write([]byte("mcp-stub"))
	})

	rec := httptest.NewRecorder()
	composedMux(t, stub).ServeHTTP(rec, httptest.NewRequest(http.MethodPost, "/mcp", nil))

	if rec.Code != http.StatusTeapot || rec.Body.String() != "mcp-stub" {
		t.Fatalf("POST /mcp did not reach stub: code=%d body=%q", rec.Code, rec.Body.String())
	}
	if strings.Contains(rec.Body.String(), "ledger") {
		t.Fatalf("POST /mcp was shadowed by the landing page")
	}
}

func TestWWWStaticServesTokensCSSWithContentType(t *testing.T) {
	rec := readWWWStaticResponse(t, "/static/tokens.css")

	// R-ROUT-4P8Q
	if rec.Code != http.StatusOK {
		t.Fatalf("tokens.css status = %d, want %d", rec.Code, http.StatusOK)
	}
	if got := rec.Header().Get("Content-Type"); got != "text/css; charset=utf-8" {
		t.Fatalf("tokens.css content-type = %q, want text/css; charset=utf-8", got)
	}
}

func TestWWWStaticKeepsAssetsUnderStaticPath(t *testing.T) {
	for _, path := range []string{"/tokens.css", "/srv/ledger/static/tokens.css", "/static/missing.css"} {
		rec := readWWWStaticResponse(t, path)

		// R-ROUT-6R1S
		if rec.Code != http.StatusNotFound {
			t.Fatalf("GET %s status = %d, want %d", path, rec.Code, http.StatusNotFound)
		}
	}
}

func TestWWWTokensCSSDefinesSelfHostedFonts(t *testing.T) {
	body := readWWWStatic(t, "/static/tokens.css")

	// R-ASST-3T7V
	for _, want := range []string{
		"@font-face",
		"font-family: 'Space Grotesk'",
		"font-family: 'IBM Plex Sans'",
		"font-family: 'IBM Plex Mono'",
		`url('fonts/space-grotesk.woff2')`,
		`url('fonts/ibm-plex-mono-500.woff2')`,
	} {
		if !strings.Contains(body, want) {
			t.Fatalf("tokens.css missing %q", want)
		}
	}
	if strings.Contains(body, "@import") || strings.Contains(body, "fonts.googleapis.com") {
		t.Fatalf("tokens.css contains external font loading: %s", body)
	}
}

func TestWWWTokensCSSUsesOptionalFontDisplay(t *testing.T) {
	body := readWWWStatic(t, "/static/tokens.css")

	// R-7AW0-4QF8
	if got := strings.Count(body, "font-display: optional;"); got != 4 {
		t.Fatalf("tokens.css optional font-display count = %d, want 4", got)
	}
	if strings.Contains(body, "font-display: swap") {
		t.Fatalf("tokens.css still contains font-display swap: %s", body)
	}
	if got := strings.Count(body, "@font-face"); got != 4 {
		t.Fatalf("tokens.css @font-face count = %d, want 4", got)
	}
}

func TestWWWTokensCSSUsesDocumentRelativeFontURLs(t *testing.T) {
	body := readWWWStatic(t, "/static/tokens.css")

	// R-7DBS-W9WM
	if strings.Contains(body, `url('/static/fonts/`) {
		t.Fatalf("tokens.css contains origin-absolute font URL: %s", body)
	}
	for _, want := range []string{
		`url('fonts/space-grotesk.woff2')`,
		`url('fonts/ibm-plex-sans.woff2')`,
		`url('fonts/ibm-plex-mono-400.woff2')`,
		`url('fonts/ibm-plex-mono-500.woff2')`,
	} {
		if !strings.Contains(body, want) {
			t.Fatalf("tokens.css missing document-relative font URL %q", want)
		}
	}
}

func TestWWWStaticServesRealWoff2Bytes(t *testing.T) {
	for _, path := range []string{
		"/static/fonts/space-grotesk.woff2",
		"/static/fonts/ibm-plex-sans.woff2",
		"/static/fonts/ibm-plex-mono-400.woff2",
		"/static/fonts/ibm-plex-mono-500.woff2",
	} {
		rec := readWWWStaticResponse(t, path)

		// R-ASST-5W9X
		if rec.Code != http.StatusOK {
			t.Fatalf("GET %s status = %d, want %d", path, rec.Code, http.StatusOK)
		}
		if got := rec.Header().Get("Content-Type"); got != "font/woff2" {
			t.Fatalf("GET %s content-type = %q, want font/woff2", path, got)
		}
		if rec.Body.Len() < 1024 {
			t.Fatalf("GET %s body length = %d, want real font bytes", path, rec.Body.Len())
		}
	}
}

func TestWWWTokensCSSContainsCarbonNeutralPalette(t *testing.T) {
	body := readWWWStatic(t, "/static/tokens.css")

	// R-ASST-7Y2Z
	for _, want := range []string{
		"Carbon — Design Tokens",
		"--layout-max-width: 1120px",
		"--text-display-size:   56px",
		"--text-label-size:     12px",
		"--color-bg:            #FFFFFF",
		"--color-text:          #09090B",
	} {
		if !strings.Contains(body, want) {
			t.Fatalf("tokens.css missing Carbon token %q", want)
		}
	}
}

func TestNginxLandingLocationIsExactSessionGatedRoot(t *testing.T) {
	conf := readNginxConfig(t)
	landing := nginxLocationBlock(t, conf, "location = /srv/ledger/ {")
	prefix := nginxLocationBlock(t, conf, "location /srv/ledger/ {")

	// R-NGNX-2B4C
	if landing == prefix {
		t.Fatal("exact landing location resolved to the bearer-gated prefix block")
	}

	// R-NGNX-4D6E
	if !strings.Contains(landing, "auth_request /_session-authn;") {
		t.Fatalf("landing location missing session auth_request: %s", landing)
	}
	if strings.Contains(landing, "auth_request /_authn;") {
		t.Fatalf("landing location uses bearer auth_request: %s", landing)
	}

	// R-NGNX-6F8G
	// R-4XTP-5B7V
	if !strings.Contains(landing, "proxy_pass "+registry.BaseURL("ledger")+"/;") {
		t.Fatalf("landing location does not proxy to upstream root with trailing slash: %s", landing)
	}
}

func TestNginxFragmentRetainsBearerAndBootstrapLocations(t *testing.T) {
	conf := readNginxConfig(t)

	prefix := nginxLocationBlock(t, conf, "location /srv/ledger/ {")
	reemit := nginxLocationBlock(t, conf, "location @ledger_authn_500 {")
	prm := nginxLocationBlock(t, conf, "location = /srv/ledger/.well-known/oauth-protected-resource {")

	// R-NGNX-8H1J
	if !strings.Contains(prefix, "auth_request /_authn;") {
		t.Fatalf("prefix location missing bearer auth_request: %s", prefix)
	}
	if !strings.Contains(reemit, "return 429;") || !strings.Contains(reemit, "return 500;") {
		t.Fatalf("authn 500 re-emit location missing expected returns: %s", reemit)
	}
	if !strings.Contains(prm, "proxy_pass "+registry.BaseURL("ledger")+"/.well-known/oauth-protected-resource;") {
		t.Fatalf("PRM bootstrap location missing expected proxy_pass: %s", prm)
	}
}

func TestNginxStaticLocationIsSessionGated(t *testing.T) {
	conf := readNginxConfig(t)

	landing := nginxLocationBlock(t, conf, "location = /srv/ledger/ {")
	static := nginxLocationBlock(t, conf, "location /srv/ledger/static/ {")
	prefix := nginxLocationBlock(t, conf, "location /srv/ledger/ {")
	reemit := nginxLocationBlock(t, conf, "location @ledger_authn_500 {")
	prm := nginxLocationBlock(t, conf, "location = /srv/ledger/.well-known/oauth-protected-resource {")

	// R-7GZI-1L4P
	if !strings.Contains(static, "auth_request /_session-authn;") {
		t.Fatalf("static location missing session auth_request: %s", static)
	}
	// R-4XTP-5B7V
	if !strings.Contains(static, "proxy_pass "+registry.BaseURL("ledger")+"/static/;") {
		t.Fatalf("static location missing upstream static proxy_pass: %s", static)
	}
	if !strings.Contains(landing, "proxy_pass "+registry.BaseURL("ledger")+"/;") {
		t.Fatalf("exact landing location changed: %s", landing)
	}
	if !strings.Contains(prefix, "auth_request /_authn;") {
		t.Fatalf("bearer prefix location changed: %s", prefix)
	}
	if !strings.Contains(prm, "proxy_pass "+registry.BaseURL("ledger")+"/.well-known/oauth-protected-resource;") {
		t.Fatalf("PRM bootstrap location changed: %s", prm)
	}
	if !strings.Contains(reemit, "return 429;") || !strings.Contains(reemit, "return 500;") {
		t.Fatalf("authn 500 re-emit location changed: %s", reemit)
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
			t.Fatalf("ledger exited before health: %v\nstdout:\n%s\nstderr:\n%s", err, stdout.String(), stderr.String())
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
	t.Fatalf("ledger never served health at %s: %s\nstdout:\n%s\nstderr:\n%s", url, last, stdout.String(), stderr.String())
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

func composedMux(t *testing.T, mcp http.Handler) *http.ServeMux {
	t.Helper()
	mux := http.NewServeMux()
	mux.Handle("GET /{$}", landingHandler(loadWWW(t), "ledger", "9.9.9-test"))
	mux.Handle("POST /mcp", mcp)
	return mux
}

func readWWWStaticResponse(t *testing.T, path string) *httptest.ResponseRecorder {
	t.Helper()
	rec := httptest.NewRecorder()
	loadWWW(t).Static().ServeHTTP(rec, httptest.NewRequest(http.MethodGet, path, nil))
	return rec
}

func readWWWStatic(t *testing.T, path string) string {
	t.Helper()
	rec := readWWWStaticResponse(t, path)
	if rec.Code != http.StatusOK {
		t.Fatalf("GET %s status = %d, want %d\n%s", path, rec.Code, http.StatusOK, rec.Body.String())
	}
	return rec.Body.String()
}

func htmlHead(t *testing.T, body string) string {
	t.Helper()

	start := strings.Index(body, "<head>")
	end := strings.Index(body, "</head>")
	if start == -1 || end == -1 || end < start {
		t.Fatalf("landing markup missing head: %s", body)
	}
	return body[start:end]
}

func linkTagContaining(t *testing.T, head, needle string) string {
	t.Helper()

	needleAt := strings.Index(head, needle)
	if needleAt == -1 {
		t.Fatalf("landing head missing link with %q: %s", needle, head)
	}
	tagStart := strings.LastIndex(head[:needleAt], "<link")
	tagEnd := strings.Index(head[needleAt:], ">")
	if tagStart == -1 || tagEnd == -1 {
		t.Fatalf("landing head has malformed link for %q: %s", needle, head)
	}
	return head[tagStart : needleAt+tagEnd+1]
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

func copyTree(t *testing.T, src, dst string) {
	t.Helper()
	err := filepath.WalkDir(src, func(path string, entry os.DirEntry, err error) error {
		if err != nil {
			return err
		}
		rel, err := filepath.Rel(src, path)
		if err != nil {
			return err
		}
		target := filepath.Join(dst, rel)
		if entry.IsDir() {
			return os.MkdirAll(target, 0o755)
		}
		info, err := entry.Info()
		if err != nil {
			return err
		}
		b, err := os.ReadFile(path)
		if err != nil {
			return err
		}
		return os.WriteFile(target, b, info.Mode().Perm())
	})
	if err != nil {
		t.Fatalf("copy %s to %s: %v", src, dst, err)
	}
}
