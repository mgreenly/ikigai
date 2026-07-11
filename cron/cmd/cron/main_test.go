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

	"appkit"
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
		if bytes.HasPrefix(line, []byte("CRON_DB_PATH=")) || bytes.HasPrefix(line, []byte("CRON_GENERATION_PATH=")) {
			t.Fatalf("committed manifest.env contains runtime path line %q", line)
		}
	}
}

// R-8IAN-FB87
func TestManifestLibraryByteEqualsCommittedFile(t *testing.T) {
	spec := cronSpec()
	got := manifest.Emit(manifest.Fields{
		App:      spec.App,
		Mount:    spec.Mount,
		Default:  spec.Default,
		Port:     spec.Port,
		MCP:      spec.MCP,
		Feed:     spec.Feed,
		Consumes: spec.Consumes,
		Extras:   manifestExtras(spec.ManifestExtras),
	})
	committed, err := os.ReadFile(filepath.Join("..", "..", "etc", "manifest.env"))
	if err != nil {
		t.Fatalf("read committed manifest.env: %v", err)
	}

	if got != string(committed) {
		// R-LVQ8-CF18
		t.Fatalf("manifest.Emit output != committed etc/manifest.env\n--- emit ---\n%s\n--- committed ---\n%s", got, committed)
	}
}

func TestCronSpecPortComesFromRegistry(t *testing.T) {
	// R-LTAF-KVJU
	if got, want := cronSpec().Port, registry.MustPort("cron"); got != want {
		t.Fatalf("cronSpec().Port = %d, want registry.MustPort(%q) = %d", got, "cron", want)
	}
}

func TestNoHardcodedLoopbackPortLiteralInSource(t *testing.T) {
	moduleRoot := filepath.Join("..", "..")
	self := filepath.Clean(filepath.Join(moduleRoot, "cmd", "cron", "main_test.go"))
	needle := "127.0.0.1:" + "30"

	err := filepath.Walk(moduleRoot, func(path string, info os.FileInfo, err error) error {
		if err != nil {
			return err
		}
		if info.IsDir() || filepath.Ext(path) != ".go" || filepath.Clean(path) == self {
			return nil
		}
		body, err := os.ReadFile(path)
		if err != nil {
			return err
		}
		// R-LUIB-YNAJ
		if bytes.Contains(body, []byte(needle)) {
			t.Fatalf("%s contains hardcoded loopback port prefix %q", path, needle)
		}
		return nil
	})
	if err != nil {
		t.Fatalf("walk Go source under %s: %v", moduleRoot, err)
	}
}

// R-4LKF-FB23
func TestCronBootsFromOpsctlLayoutAndServesHealth(t *testing.T) {
	root := t.TempDir()
	appRoot := filepath.Join(root, "cron")
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
	copyTree(t, wwwRoot(t), filepath.Join(shareVersionDir, "www"))
	selectedManifest, err := os.ReadFile(filepath.Join(etcDir, "current", "manifest.env"))
	if err != nil {
		t.Fatalf("read selected manifest.env: %v", err)
	}
	if !bytes.Equal(selectedManifest, committedManifest) {
		t.Fatalf("selected manifest.env differs from committed authored file\n--- selected ---\n%s\n--- committed ---\n%s", selectedManifest, committedManifest)
	}

	binary := filepath.Join(libexecDir, "cron-"+version)
	build := exec.Command("go", "build", "-o", binary, ".")
	build.Env = os.Environ()
	if out, err := build.CombinedOutput(); err != nil {
		t.Fatalf("go build cron: %v\n%s", err, out)
	}

	run := filepath.Join(binDir, "run")
	if err := os.Symlink("../libexec/cron-"+version, run); err != nil {
		t.Fatalf("symlink bin/run: %v", err)
	}
	if resolved, err := filepath.EvalSymlinks(run); err != nil || resolved != binary {
		t.Fatalf("bin/run resolves to %q err=%v, want %q", resolved, err, binary)
	}

	port := freeTCPPort(t)
	dbPath := filepath.Join(stateDir, "cron.db")
	generationPath := filepath.Join(cacheDir, "cron.db.generation")
	ctx, cancel := context.WithCancel(context.Background())
	defer cancel()

	var stdout, stderr bytes.Buffer
	cmd := exec.CommandContext(ctx, run, "serve")
	cmd.Env = testEnv(map[string]string{
		"IKIGENBA_DOMAIN":           "int.ikigenba.com",
		"IKIGENBA_ROOT":             root,
		"CRON_IP":                   "127.0.0.1",
		"CRON_PORT":                 fmt.Sprintf("%d", port),
		"OUTBOX_RETENTION_DAYS":     "7",
		"OUTBOX_RETENTION_MAX_ROWS": "1000000",
		"CRON_WWW_PATH":             filepath.Join(shareDir, "current", "www"),
	})
	cmd.Stdout = &stdout
	cmd.Stderr = &stderr
	if err := cmd.Start(); err != nil {
		t.Fatalf("start cron: %v", err)
	}
	done := make(chan error, 1)
	go func() { done <- cmd.Wait() }()
	defer stopProcess(cancel, done)

	doc := waitForHealth(t, port, done, &stdout, &stderr)
	if got := doc["service"]; got != "cron" {
		t.Fatalf("health service = %v, want cron; body=%v", got, doc)
	}
	if got := doc["status"]; got != "ok" {
		t.Fatalf("health status = %v, want ok; body=%v", got, doc)
	}
	if _, err := os.Stat(dbPath); err != nil {
		t.Fatalf("cron did not create DB under state/: %v", err)
	}
	if _, err := os.Stat(generationPath); err != nil {
		t.Fatalf("cron did not create generation sidecar under cache/: %v", err)
	}
	if filepath.Dir(generationPath) != cacheDir {
		t.Fatalf("generation sidecar path %s is not under cache dir %s", generationPath, cacheDir)
	}
}

func TestWWWSiteRendersLandingWithInjectedServiceVersion(t *testing.T) {
	// R-LPMQ-FKBR
	rec := httptest.NewRecorder()
	if err := loadWWW(t).Render(rec, "landing.html", landingData("cron-real", "v7.8.9-test")); err != nil {
		t.Fatalf("render landing.html from share/www: %v", err)
	}

	if rec.Code != http.StatusOK {
		t.Fatalf("status = %d, want %d", rec.Code, http.StatusOK)
	}
	if got := rec.Header().Get("Content-Type"); got != "text/html; charset=utf-8" {
		t.Fatalf("Content-Type = %q, want text/html; charset=utf-8", got)
	}
	body := rec.Body.String()
	if !strings.Contains(body, "cron-real") {
		t.Fatalf("landing body does not contain service name %q:\n%s", "cron-real", body)
	}
	if !strings.Contains(body, "v7.8.9-test") {
		t.Fatalf("landing body does not contain injected version %q:\n%s", "v7.8.9-test", body)
	}
}

func TestWWWStaticServesThroughChassisMountAndNoCronStaticHandler(t *testing.T) {
	// R-LQUM-TC2G
	css := readWWWStaticResponse(t, "/static/tokens.css")
	if css.Code != http.StatusOK {
		t.Fatalf("GET /static/tokens.css status = %d, want %d", css.Code, http.StatusOK)
	}
	if got := css.Header().Get("Content-Type"); !strings.Contains(got, "text/css") {
		t.Fatalf("GET /static/tokens.css Content-Type = %q, want text/css", got)
	}

	font := readWWWStaticResponse(t, "/static/fonts/space-grotesk.woff2")
	if font.Code != http.StatusOK {
		t.Fatalf("GET /static/fonts/space-grotesk.woff2 status = %d, want %d", font.Code, http.StatusOK)
	}
	if got := font.Header().Get("Content-Type"); got != "font/woff2" {
		t.Fatalf("GET /static/fonts/space-grotesk.woff2 Content-Type = %q, want font/woff2", got)
	}
	if !bytes.HasPrefix(font.Body.Bytes(), []byte("wOF2")) {
		t.Fatalf("GET /static/fonts/space-grotesk.woff2 body does not start with wOF2")
	}

	src, err := os.ReadFile("main.go")
	if err != nil {
		t.Fatal(err)
	}
	main := string(src)
	for _, forbidden := range []string{
		`rt.Handle("GET /static/"`,
		`web.StaticHandler`,
	} {
		if strings.Contains(main, forbidden) {
			t.Fatalf("cmd/cron/main.go still contains cron-side static handler %q", forbidden)
		}
	}
}

func TestLandingHandlerRendersSharePage(t *testing.T) {
	req := httptest.NewRequest(http.MethodGet, "/", nil)
	rec := httptest.NewRecorder()

	landingHandler(loadWWW(t), "cron-test", "v1.2.3").ServeHTTP(rec, req)

	res := rec.Result()
	body := rec.Body.String()
	// R-LAND-3C9K
	if res.StatusCode != http.StatusOK {
		t.Fatalf("status = %d, want %d", res.StatusCode, http.StatusOK)
	}
	// R-LAND-5E2L
	if !strings.Contains(body, "cron-test") {
		t.Fatalf("landing body does not contain service name %q:\n%s", "cron-test", body)
	}
}

func TestLandingHandlerReflectsInjectedVersion(t *testing.T) {
	req := httptest.NewRequest(http.MethodGet, "/", nil)
	rec := httptest.NewRecorder()

	landingHandler(loadWWW(t), "cron", "9.9.9-test").ServeHTTP(rec, req)

	body := rec.Body.String()
	// R-LAND-7G4M
	if !strings.Contains(body, "9.9.9-test") {
		t.Fatalf("landing body does not contain injected version %q:\n%s", "9.9.9-test", body)
	}
}

func TestLandingHandlerSetsHTMLContentType(t *testing.T) {
	req := httptest.NewRequest(http.MethodGet, "/", nil)
	rec := httptest.NewRecorder()

	landingHandler(loadWWW(t), "cron", "test").ServeHTTP(rec, req)

	// R-LAND-9J6N
	if got := rec.Result().Header.Get("Content-Type"); got != "text/html; charset=utf-8" {
		t.Fatalf("Content-Type = %q, want text/html; charset=utf-8", got)
	}
}

func TestLandingHandlerRendersHomeLinkToApex(t *testing.T) {
	req := httptest.NewRequest(http.MethodGet, "/", nil)
	rec := httptest.NewRecorder()

	landingHandler(loadWWW(t), "cron", "test").ServeHTTP(rec, req)

	body := rec.Body.String()
	// R-HOME-2K4P
	if !strings.Contains(body, `<a class="home" href="/">Home</a>`) {
		t.Fatalf("landing body does not contain the home apex link:\n%s", body)
	}
	if strings.Index(body, `<a class="home" href="/">Home</a>`) < strings.Index(body, "<main>") {
		t.Fatalf("home link does not appear inside main:\n%s", body)
	}
	for _, want := range []string{
		".home {",
		"position: absolute;",
		"top: var(--space-8);",
		"position: relative;",
		".home:hover,",
		".home:focus-visible",
	} {
		if !strings.Contains(body, want) {
			t.Fatalf("landing body does not contain home style %q:\n%s", want, body)
		}
	}
}

func TestLandingRouteRejectsNonRootAndNonGetRequests(t *testing.T) {
	mux := http.NewServeMux()
	mux.Handle("GET /{$}", landingHandler(loadWWW(t), "cron", "test"))

	for _, tc := range []struct {
		name   string
		method string
		path   string
		status int
	}{
		{name: "non root", method: http.MethodGet, path: "/mcp", status: http.StatusNotFound},
		{name: "non get", method: http.MethodPost, path: "/", status: http.StatusMethodNotAllowed},
	} {
		t.Run(tc.name, func(t *testing.T) {
			req := httptest.NewRequest(tc.method, tc.path, nil)
			rec := httptest.NewRecorder()

			mux.ServeHTTP(rec, req)

			if rec.Result().StatusCode != tc.status {
				t.Fatalf("status = %d, want %d", rec.Result().StatusCode, tc.status)
			}
		})
	}
}

func TestLandingTemplateUsesOnlyLocalShareAssets(t *testing.T) {
	req := httptest.NewRequest(http.MethodGet, "/", nil)
	rec := httptest.NewRecorder()

	landingHandler(loadWWW(t), "cron", "test").ServeHTTP(rec, req)

	body := rec.Body.String()
	for _, disallowed := range []string{"https://", "http://", "//fonts.googleapis.com", "dashboard"} {
		// R-ASST-5X9Y
		if strings.Contains(body, disallowed) {
			t.Fatalf("landing body contains runtime external asset reference %q:\n%s", disallowed, body)
		}
	}
	for _, want := range []string{`href="static/tokens.css"`} {
		// R-ASST-5X9Y
		if !strings.Contains(body, want) {
			t.Fatalf("landing body does not contain %q:\n%s", want, body)
		}
	}
}

func TestServeMuxRootRouteIsExactAndUngated(t *testing.T) {
	mux := http.NewServeMux()
	mux.Handle("GET /{$}", landingHandler(loadWWW(t), "cron", "route-version"))
	mux.Handle("POST /mcp", http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		w.Header().Set("X-MCP-Stub", "reached")
		w.WriteHeader(http.StatusNoContent)
	}))

	root := httptest.NewRecorder()
	mux.ServeHTTP(root, httptest.NewRequest(http.MethodGet, "/", nil))
	rootBody := root.Body.String()
	// R-ROUT-2P8Q
	if root.Result().StatusCode != http.StatusOK {
		t.Fatalf("GET / status = %d, want %d", root.Result().StatusCode, http.StatusOK)
	}
	// R-ROUT-2P8Q
	if !strings.Contains(rootBody, "cron") || !strings.Contains(rootBody, "route-version") {
		t.Fatalf("GET / did not dispatch to landing handler:\n%s", rootBody)
	}

	mcp := httptest.NewRecorder()
	mux.ServeHTTP(mcp, httptest.NewRequest(http.MethodPost, "/mcp", nil))
	// R-ROUT-4R1S
	if mcp.Result().StatusCode != http.StatusNoContent {
		t.Fatalf("POST /mcp status = %d, want %d", mcp.Result().StatusCode, http.StatusNoContent)
	}
	// R-ROUT-4R1S
	if got := mcp.Result().Header.Get("X-MCP-Stub"); got != "reached" {
		t.Fatalf("POST /mcp did not reach stub handler: X-MCP-Stub = %q", got)
	}

	other := httptest.NewRecorder()
	mux.ServeHTTP(other, httptest.NewRequest(http.MethodGet, "/nope", nil))
	// R-ROUT-6T3U
	if other.Result().StatusCode == http.StatusOK || strings.Contains(other.Body.String(), "route-version") {
		t.Fatalf("GET /nope returned landing page: status=%d body=\n%s", other.Result().StatusCode, other.Body.String())
	}
}

func TestCompositionRootMountsLandingWithoutIdentityWrapper(t *testing.T) {
	// R-ROUT-2P8Q
	// R-ROUT-6T3U
	source, err := os.ReadFile("main.go")
	if err != nil {
		t.Fatal(err)
	}

	compositionRoot := string(source)
	want := `rt.Handle("GET /{$}", landingHandler(rt.WWW(), rt.Service(), rt.Version()))`
	if !strings.Contains(compositionRoot, want) {
		t.Fatalf("main.go does not mount the landing handler with %q", want)
	}
	if strings.Contains(compositionRoot, `RequireIdentity(landingHandler`) {
		t.Fatal("landing handler is wrapped with RequireIdentity")
	}
	if !strings.Contains(compositionRoot, `rt.Handle("POST /mcp", rt.RequireIdentity(`) {
		t.Fatal("main.go no longer gates POST /mcp with RequireIdentity")
	}
}

func TestStaticAssetsServeShareCarbonFiles(t *testing.T) {
	static := loadWWW(t).Static()

	// R-ASST-3V7W
	css := requestAsset(t, static, "/static/tokens.css", "text/css; charset=utf-8")
	for _, want := range []string{
		"@font-face",
		"url('fonts/space-grotesk.woff2')",
		"url('fonts/ibm-plex-sans.woff2')",
		"url('fonts/ibm-plex-mono-400.woff2')",
		"url('fonts/ibm-plex-mono-500.woff2')",
	} {
		if !strings.Contains(css, want) {
			t.Fatalf("tokens.css does not contain %q:\n%s", want, css)
		}
	}
	for _, disallowed := range []string{"https://", "http://", "fonts.googleapis.com", "dashboard"} {
		// R-ASST-5X9Y
		if strings.Contains(css, disallowed) {
			t.Fatalf("tokens.css contains runtime external asset reference %q:\n%s", disallowed, css)
		}
	}

	for _, path := range []string{
		"/static/fonts/space-grotesk.woff2",
		"/static/fonts/ibm-plex-sans.woff2",
		"/static/fonts/ibm-plex-mono-400.woff2",
		"/static/fonts/ibm-plex-mono-500.woff2",
	} {
		// R-ASST-7Z2A
		body := requestAsset(t, static, path, "font/woff2")
		if !strings.HasPrefix(body, "wOF2") {
			t.Fatalf("%s is not a woff2 payload", path)
		}
	}
}

func TestStaticTokensCSSUsesOptionalFontDisplay(t *testing.T) {
	css := requestAsset(t, loadWWW(t).Static(), "/static/tokens.css", "text/css; charset=utf-8")
	fontFaceCount := strings.Count(css, "@font-face")
	if fontFaceCount != 4 {
		t.Fatalf("tokens.css has %d @font-face blocks, want 4:\n%s", fontFaceCount, css)
	}
	// R-21DE-LOX3
	if got := strings.Count(css, "font-display: optional;"); got != fontFaceCount {
		t.Fatalf("tokens.css has %d optional font-display declarations, want %d:\n%s", got, fontFaceCount, css)
	}
	if strings.Contains(css, "font-display: swap") {
		t.Fatalf("tokens.css still contains font-display: swap:\n%s", css)
	}
}

func TestStaticTokensCSSUsesDocumentRelativeFontSources(t *testing.T) {
	css := requestAsset(t, loadWWW(t).Static(), "/static/tokens.css", "text/css; charset=utf-8")
	if strings.Contains(css, "url('/static/fonts/") {
		t.Fatalf("tokens.css still contains origin-absolute font URLs:\n%s", css)
	}
	for _, want := range []string{
		"url('fonts/space-grotesk.woff2')",
		"url('fonts/ibm-plex-sans.woff2')",
		"url('fonts/ibm-plex-mono-400.woff2')",
		"url('fonts/ibm-plex-mono-500.woff2')",
	} {
		// R-22LA-ZGNS
		if !strings.Contains(css, want) {
			t.Fatalf("tokens.css does not contain %q:\n%s", want, css)
		}
	}
}

func TestLandingHeadUsesDocumentRelativeStylesheet(t *testing.T) {
	req := httptest.NewRequest(http.MethodGet, "/", nil)
	rec := httptest.NewRecorder()

	landingHandler(loadWWW(t), "cron", "test").ServeHTTP(rec, req)

	head := headMarkup(t, rec.Body.String())
	// R-23T7-D8EH
	if !strings.Contains(head, `href="static/tokens.css"`) {
		t.Fatalf("landing head does not link document-relative tokens.css:\n%s", head)
	}
	if strings.Contains(head, `href="/static/tokens.css"`) {
		t.Fatalf("landing head still links origin-absolute tokens.css:\n%s", head)
	}
}

func TestLandingHeadPreloadsDisplayAndBodyFonts(t *testing.T) {
	req := httptest.NewRequest(http.MethodGet, "/", nil)
	rec := httptest.NewRecorder()

	landingHandler(loadWWW(t), "cron", "test").ServeHTTP(rec, req)

	head := headMarkup(t, rec.Body.String())
	css := requestAsset(t, loadWWW(t).Static(), "/static/tokens.css", "text/css; charset=utf-8")
	for _, font := range []string{"space-grotesk.woff2", "ibm-plex-sans.woff2"} {
		href := "static/fonts/" + font
		// R-2513-R056
		link := linkMarkupWithHref(t, head, href)
		for _, want := range []string{`rel="preload"`, `as="font"`, `type="font/woff2"`, "crossorigin"} {
			if !strings.Contains(link, want) {
				t.Fatalf("font preload for %s does not contain %q:\n%s", href, want, link)
			}
		}
		if !strings.Contains(css, "url('fonts/"+font+"')") {
			t.Fatalf("tokens.css does not contain matching @font-face src for %s:\n%s", href, css)
		}
	}
}

func TestNginxLandingLocationIsExactSessionGated(t *testing.T) {
	conf := readNginxConfig(t)

	exact := nginxLocationBlock(t, conf, "location = /srv/cron/ {")
	prefix := nginxLocationBlock(t, conf, "location /srv/cron/ {")

	// R-NGNX-3B6C
	if exact == prefix {
		t.Fatal("exact landing location block was not distinct from bearer prefix block")
	}
	if strings.Contains(exact, "location /srv/cron/ {") {
		t.Fatalf("exact landing block contains prefix location header:\n%s", exact)
	}

	// R-NGNX-5D8E
	if !strings.Contains(exact, "auth_request /_session-authn;") {
		t.Fatalf("exact landing block does not use session auth_request:\n%s", exact)
	}
	if strings.Contains(exact, "auth_request /_authn;") {
		t.Fatalf("exact landing block uses bearer auth_request:\n%s", exact)
	}

	// R-NGNX-7F1G
	// R-LVQ8-CF18
	if !strings.Contains(exact, "proxy_pass "+registry.BaseURL("cron")+"/;") {
		t.Fatalf("exact landing block does not proxy to upstream root with trailing slash:\n%s", exact)
	}
}

func TestNginxPreExistingServiceLocationsSurvive(t *testing.T) {
	conf := readNginxConfig(t)
	prefix := nginxLocationBlock(t, conf, "location /srv/cron/ {")

	// R-NGNX-9H3J
	if !strings.Contains(prefix, "auth_request /_authn;") {
		t.Fatalf("bearer prefix block no longer uses bearer auth_request:\n%s", prefix)
	}
	for _, want := range []string{
		"auth_request_set $cron_owner",
		"auth_request_set $cron_client",
		"error_page 500 = @cron_authn_500;",
	} {
		if !strings.Contains(prefix, want) {
			t.Fatalf("bearer prefix block does not contain %q:\n%s", want, prefix)
		}
	}
	for _, want := range []string{
		"location = /srv/cron/feed { return 404; }",
		"location = /srv/cron/.well-known/oauth-protected-resource {",
		"location @cron_authn_500 {",
	} {
		if !strings.Contains(conf, want) {
			t.Fatalf("nginx config does not contain %q:\n%s", want, conf)
		}
	}
}

func TestNginxStaticLocationIsSessionGatedAndProxiesToStaticHandler(t *testing.T) {
	conf := readNginxConfig(t)
	static := nginxLocationBlock(t, conf, "location /srv/cron/static/ {")

	for _, want := range []string{
		"auth_request /_session-authn;",
		"proxy_pass " + registry.BaseURL("cron") + "/static/;",
		"proxy_set_header Host $host;",
		"proxy_set_header X-Forwarded-Proto $scheme;",
		"proxy_http_version 1.1;",
	} {
		// R-2690-4RVV
		// R-LVQ8-CF18
		if !strings.Contains(static, want) {
			t.Fatalf("static location block does not contain %q:\n%s", want, static)
		}
	}
	if strings.Contains(static, "auth_request /_authn;") {
		t.Fatalf("static location block uses bearer auth_request:\n%s", static)
	}

	for _, header := range []string{
		"location = /srv/cron/ {",
		"location /srv/cron/ {",
		"location = /srv/cron/feed { return 404; }",
		"location = /srv/cron/.well-known/oauth-protected-resource {",
		"location @cron_authn_500 {",
	} {
		// R-2690-4RVV
		if !strings.Contains(conf, header) {
			t.Fatalf("nginx config no longer contains %q:\n%s", header, conf)
		}
	}
}

func TestNginxSessionLocationsUseApexLoginBounce(t *testing.T) {
	conf := readNginxConfig(t)
	landing := nginxLocationBlock(t, conf, "location = /srv/cron/ {")
	assets := nginxLocationBlock(t, conf, "location /srv/cron/static/ {")
	bearer := nginxLocationBlock(t, conf, "location /srv/cron/ {")

	for name, block := range map[string]string{
		"landing": landing,
		"assets":  assets,
	} {
		// R-3V6H-7F1M
		for _, want := range []string{
			"auth_request /_session-authn;",
			"error_page 401 = @login_bounce;",
		} {
			if !strings.Contains(block, want) {
				t.Fatalf("%s session location does not contain %q:\n%s", name, want, block)
			}
		}
	}

	// R-3WED-L6SB
	if strings.Contains(bearer, "error_page 401 = @login_bounce;") {
		t.Fatalf("bearer location must retain its OAuth 401 rather than bounce to login:\n%s", bearer)
	}

	// R-3XM9-YYJ0
	for name, block := range map[string]string{
		"landing": landing,
		"assets":  assets,
	} {
		for _, want := range []string{
			"auth_request /_session-authn;",
			"proxy_pass ",
		} {
			if !strings.Contains(block, want) {
				t.Fatalf("%s session location lost existing directive %q:\n%s", name, want, block)
			}
		}
	}
	for _, header := range []string{
		"location = /srv/cron/.well-known/oauth-protected-resource {",
		"location = /srv/cron/feed { return 404; }",
		"location = /srv/cron/ {",
		"location /srv/cron/static/ {",
		"location /srv/cron/ {",
		"location @cron_authn_500 {",
	} {
		if !strings.Contains(conf, header) {
			t.Fatalf("nginx config no longer contains pre-existing location %q:\n%s", header, conf)
		}
	}
}

func manifestExtras(in []appkit.ManifestKV) []manifest.KV {
	out := make([]manifest.KV, 0, len(in))
	for _, kv := range in {
		out = append(out, manifest.KV{Key: kv.Key, Value: kv.Value})
	}
	return out
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
			t.Fatalf("cron exited before health: %v\nstdout:\n%s\nstderr:\n%s", err, stdout.String(), stderr.String())
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
	t.Fatalf("cron never served health at %s: %s\nstdout:\n%s\nstderr:\n%s", url, last, stdout.String(), stderr.String())
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

func readWWWStaticResponse(t *testing.T, path string) *httptest.ResponseRecorder {
	t.Helper()
	rec := httptest.NewRecorder()
	loadWWW(t).Static().ServeHTTP(rec, httptest.NewRequest(http.MethodGet, path, nil))
	return rec
}

func requestAsset(t *testing.T, h http.Handler, path, contentType string) string {
	t.Helper()

	rec := httptest.NewRecorder()
	h.ServeHTTP(rec, httptest.NewRequest(http.MethodGet, path, nil))

	res := rec.Result()
	body, err := io.ReadAll(res.Body)
	if err != nil {
		t.Fatal(err)
	}
	if res.StatusCode != http.StatusOK {
		t.Fatalf("%s status = %d, want %d", path, res.StatusCode, http.StatusOK)
	}
	if got := res.Header.Get("Content-Type"); got != contentType {
		t.Fatalf("%s Content-Type = %q, want %q", path, got, contentType)
	}
	if len(body) == 0 {
		t.Fatalf("%s returned an empty body", path)
	}
	return string(body)
}

func headMarkup(t *testing.T, body string) string {
	t.Helper()

	start := strings.Index(body, "<head>")
	end := strings.Index(body, "</head>")
	if start < 0 || end < 0 || end <= start {
		t.Fatalf("landing body does not contain a complete head:\n%s", body)
	}
	return body[start : end+len("</head>")]
}

func linkMarkupWithHref(t *testing.T, head, href string) string {
	t.Helper()

	hrefAttr := `href="` + href + `"`
	hrefIndex := strings.Index(head, hrefAttr)
	if hrefIndex < 0 {
		t.Fatalf("landing head does not contain %s:\n%s", hrefAttr, head)
	}
	linkStart := strings.LastIndex(head[:hrefIndex], "<link")
	linkEnd := strings.Index(head[hrefIndex:], ">")
	if linkStart < 0 || linkEnd < 0 {
		t.Fatalf("landing head does not contain a complete link for %s:\n%s", href, head)
	}
	return head[linkStart : hrefIndex+linkEnd+1]
}

func readNginxConfig(t *testing.T) string {
	t.Helper()

	body, err := os.ReadFile(filepath.Join("..", "..", "etc", "nginx.conf"))
	if err != nil {
		t.Fatal(err)
	}
	return string(body)
}

func nginxLocationBlock(t *testing.T, conf, header string) string {
	t.Helper()

	start := strings.Index(conf, header)
	if start < 0 {
		t.Fatalf("nginx config does not contain %q:\n%s", header, conf)
	}

	depth := 0
	for i := start; i < len(conf); i++ {
		switch conf[i] {
		case '{':
			depth++
		case '}':
			depth--
			if depth == 0 {
				return conf[start : i+1]
			}
		}
	}
	t.Fatalf("nginx config contains unterminated block for %q:\n%s", header, conf[start:])
	return ""
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
