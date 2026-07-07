package main

import (
	"bytes"
	"context"
	"encoding/json"
	"fmt"
	"io"
	"io/fs"
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
		if bytes.HasPrefix(line, []byte("SITES_DB_PATH=")) || bytes.HasPrefix(line, []byte("SITES_GENERATION_PATH=")) {
			t.Fatalf("committed manifest.env contains runtime path line %q", line)
		}
	}
}

// R-8IAN-FB87
func TestManifestLibraryByteEqualsCommittedFile(t *testing.T) {
	spec := sitesSpec()
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
		t.Fatalf("manifest.Emit output != committed etc/manifest.env\n--- emit ---\n%s\n--- committed ---\n%s", got, committed)
	}
}

// R-7K2P-QN4D
func TestSitesSpecUsesRegistryPort(t *testing.T) {
	if got, want := sitesSpec().Port, registry.MustPort("sites"); got != want {
		t.Fatalf("sitesSpec().Port = %d, want registry.MustPort(%q) = %d", got, "sites", want)
	}
}

// R-7L9F-XW3H
func TestGoSourcesUseRegistryForLoopbackPorts(t *testing.T) {
	root := filepath.Join("..", "..")
	standalonePort := regexp.MustCompile(`(^|[^A-Za-z0-9_])3004([^A-Za-z0-9_]|$)`)
	var offenders []string

	err := filepath.WalkDir(root, func(path string, entry fs.DirEntry, err error) error {
		if err != nil {
			return err
		}
		if entry.IsDir() || filepath.Ext(path) != ".go" || strings.HasSuffix(path, "_test.go") {
			return nil
		}

		content, err := os.ReadFile(path)
		if err != nil {
			return err
		}
		text := string(content)
		if strings.Contains(text, "127.0.0.1:30") || standalonePort.MatchString(text) {
			rel, relErr := filepath.Rel(root, path)
			if relErr != nil {
				rel = path
			}
			offenders = append(offenders, rel)
		}
		return nil
	})
	if err != nil {
		t.Fatalf("walk Go sources under module root: %v", err)
	}
	if len(offenders) > 0 {
		t.Fatalf("Go sources still contain hardcoded sites loopback port literals: %s", strings.Join(offenders, ", "))
	}
}

// R-7M4C-BV8J
func TestDropboxRegistryBaseURL(t *testing.T) {
	if got, want := registry.BaseURL("dropbox"), "http://127.0.0.1:3200"; got != want {
		t.Fatalf("registry.BaseURL(%q) = %q, want %q", "dropbox", got, want)
	}
}

// R-7N6R-TZ2Q
func TestGoModRequiresAndReplacesRegistry(t *testing.T) {
	content, err := os.ReadFile(filepath.Join("..", "..", "go.mod"))
	if err != nil {
		t.Fatalf("read go.mod: %v", err)
	}
	text := string(content)
	requireRegistry := regexp.MustCompile(`(?m)^\s*registry\s+v0\.0\.0\s*$`)
	replaceRegistry := regexp.MustCompile(`(?m)^replace\s+registry\s+=>\s+\.\./registry\s*$`)
	if !requireRegistry.MatchString(text) {
		t.Fatalf("go.mod is missing require registry v0.0.0")
	}
	if !replaceRegistry.MatchString(text) {
		t.Fatalf("go.mod is missing replace registry => ../registry")
	}
}

func TestSitesSpecEnablesChassisWWWAndKeepsMCPWiring(t *testing.T) {
	// R-0SF5-VPQF — sites opts into the chassis WWW loader while keeping the MCP surface enabled.
	spec := sitesSpec()
	if !spec.WWW {
		t.Fatal("sitesSpec().WWW = false, want true")
	}
	if !spec.MCP {
		t.Fatal("sitesSpec().MCP = false, want true")
	}

	src, err := os.ReadFile("main.go")
	if err != nil {
		t.Fatal(err)
	}
	main := string(src)
	for _, want := range []string{
		`WWW:        true,`,
		`rt.WWW().Render(w, "landing.html",`,
		`struct{ Service, Version string }{rt.Service(), rt.Version()}`,
		`mirror := sites.NewMirrorClient(base)`,
		`handler, err := mcp.NewHandler(store, layout, baseURL, mirror, rt)`,
		`if err != nil {`,
		`rt.Handle("POST /mcp", rt.RequireIdentity(handler))`,
	} {
		if !strings.Contains(main, want) {
			t.Fatalf("cmd/sites/main.go missing %q", want)
		}
	}
	for _, forbidden := range []string{
		`"sites/internal/web"`,
		`rt.Handle("GET /static/"`,
		`web.LandingHandler`,
		`web.StaticHandler`,
	} {
		if strings.Contains(main, forbidden) {
			t.Fatalf("cmd/sites/main.go still contains %q", forbidden)
		}
	}
}

func TestWWWSiteLoadsRealShareTree(t *testing.T) {
	// R-0TN2-9HH4 — landing and static assets are loaded from share/www, not an embedded internal web package.
	root := wwwRoot(t)
	if strings.Contains(root, "internal/web") {
		t.Fatalf("WWW root %q points at deleted internal web package", root)
	}

	site := loadWWW(t)
	rec := httptest.NewRecorder()
	if err := site.Render(rec, "landing.html", landingData("sites-real", "v1.2.3")); err != nil {
		t.Fatalf("render landing.html from share/www: %v", err)
	}
	if rec.Code != http.StatusOK || !strings.Contains(rec.Body.String(), "<title>sites-real") {
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
	rec := renderLanding(t, "sites", "9.9.9-test")
	body := rec.Body.String()

	// R-LAND-3C9K
	if rec.Code != http.StatusOK {
		t.Fatalf("status = %d, want %d", rec.Code, http.StatusOK)
	}
	// R-LAND-5E2M
	if !strings.Contains(body, "sites") {
		t.Fatalf("body does not contain service name: %q", body)
	}
	// R-LAND-7G4P
	if !strings.Contains(body, "9.9.9-test") {
		t.Fatalf("body does not contain version: %q", body)
	}
	// R-LAND-9J6R
	if got := rec.Header().Get("Content-Type"); got != "text/html; charset=utf-8" {
		t.Fatalf("Content-Type = %q, want text/html; charset=utf-8", got)
	}
}

func TestExactRootRouteDispatchesWithoutShadowingSiblings(t *testing.T) {
	mux := composedMux(t, http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		w.WriteHeader(http.StatusAccepted)
		_, _ = w.Write([]byte("mcp"))
	}))

	root := httptest.NewRecorder()
	mux.ServeHTTP(root, httptest.NewRequest(http.MethodGet, "/", nil))
	rootBody := root.Body.String()
	// R-ROUT-4Q8B
	if root.Code != http.StatusOK || !strings.Contains(rootBody, "sites") || !strings.Contains(rootBody, "1.2.3") {
		t.Fatalf("GET / returned status %d body %q, want landing page", root.Code, rootBody)
	}

	mcp := httptest.NewRecorder()
	mux.ServeHTTP(mcp, httptest.NewRequest(http.MethodPost, "/mcp", nil))
	// R-ROUT-6S1D
	if mcp.Code != http.StatusAccepted || strings.Contains(mcp.Body.String(), "sites") {
		t.Fatalf("POST /mcp returned status %d body %q, want sibling handler", mcp.Code, mcp.Body.String())
	}

	nope := httptest.NewRecorder()
	mux.ServeHTTP(nope, httptest.NewRequest(http.MethodGet, "/nope", nil))
	// R-ROUT-8U3F
	if nope.Code == http.StatusOK || strings.Contains(nope.Body.String(), "sites") {
		t.Fatalf("GET /nope returned status %d body %q, want not found without landing page", nope.Code, nope.Body.String())
	}
}

func TestWWWStaticServesTokensCSS(t *testing.T) {
	rec := readWWWStaticResponse(t, "/static/tokens.css")
	body := rec.Body.String()

	// R-ASST-3H7N
	if rec.Code != http.StatusOK || !strings.Contains(rec.Header().Get("Content-Type"), "text/css") {
		t.Fatalf("GET /static/tokens.css returned status %d Content-Type %q", rec.Code, rec.Header().Get("Content-Type"))
	}

	// R-629P-84O5
	if strings.Contains(body, "font-display: swap") {
		t.Fatalf("tokens.css still uses swap font display: %q", body)
	}
	for _, block := range strings.Split(body, "@font-face") {
		if !strings.Contains(block, "font-family:") {
			continue
		}
		if !strings.Contains(block, "font-display: optional;") {
			t.Fatalf("@font-face block does not use optional font display: %s", block)
		}
	}

	// R-ASST-3H7N
	// R-63HL-LWEU
	if strings.Contains(body, `url('/static/fonts/`) {
		t.Fatalf("tokens.css contains origin-absolute font URLs: %q", body)
	}
	for _, want := range []string{
		`url('fonts/space-grotesk.woff2')`,
		`url('fonts/ibm-plex-sans.woff2')`,
		`url('fonts/ibm-plex-mono-400.woff2')`,
		`url('fonts/ibm-plex-mono-500.woff2')`,
	} {
		if !strings.Contains(body, want) {
			t.Fatalf("tokens.css missing relative font URL %q in: %q", want, body)
		}
	}
	for _, want := range []string{
		"--color-bg:",
		"--space-4:  16px;",
		"--text-display-size:",
		"--text-display-lh:",
		"--text-label-size:",
		"--text-label-weight:",
		"--border-width:",
	} {
		if !strings.Contains(body, want) {
			t.Fatalf("tokens.css missing Carbon landing token %q in: %q", want, body)
		}
	}
}

func TestWWWLandingReferencesOwnStaticPath(t *testing.T) {
	rec := renderLanding(t, "sites", "asset-test")
	body := rec.Body.String()
	head := htmlHead(t, body)

	// R-ASST-5K9Q
	// R-64PH-ZO5J
	if !strings.Contains(head, `href="static/tokens.css"`) {
		t.Fatalf("landing HTML head does not reference document-relative tokens.css: %q", head)
	}
	if strings.Contains(head, `href="/static/tokens.css"`) {
		t.Fatalf("landing HTML head references origin-absolute tokens.css: %q", head)
	}
	if strings.Contains(body, "dashboard") || strings.Contains(body, "://") {
		t.Fatalf("landing HTML references a cross-service or remote asset URL: %q", body)
	}
}

func TestWWWLandingPreloadsSelfHostedFonts(t *testing.T) {
	rec := renderLanding(t, "sites", "font-preload-test")
	body := rec.Body.String()
	head := htmlHead(t, body)
	css := readWWWStatic(t, "/static/tokens.css")

	// R-65XE-DFW8
	for _, font := range []string{
		"space-grotesk.woff2",
		"ibm-plex-sans.woff2",
	} {
		want := `<link rel="preload" as="font" type="font/woff2" crossorigin
        href="static/fonts/` + font + `">`
		if !strings.Contains(head, want) {
			t.Fatalf("landing HTML head missing document-relative preload for %s in: %q", font, head)
		}
		if strings.Contains(head, `href="/static/fonts/`+font+`"`) {
			t.Fatalf("landing HTML head references origin-absolute preload for %s in: %q", font, head)
		}
		if !strings.Contains(css, `url('fonts/`+font+`')`) {
			t.Fatalf("tokens.css does not contain matching @font-face source for %s in: %q", font, css)
		}
	}
}

func TestWWWLandingRendersHomeLinkBeforeMain(t *testing.T) {
	rec := renderLanding(t, "sites", "home-link-test")
	body := rec.Body.String()

	// R-HOME-9S3W
	if !strings.Contains(body, `<main>
    <a class="home" href="/">Home</a>`) {
		t.Fatalf("landing HTML does not render Home link as the first body child inside main: %q", body)
	}
	if !strings.Contains(body, `.home {`) || !strings.Contains(body, `.home:hover,
    .home:focus-visible`) {
		t.Fatalf("landing HTML does not include inline .home style rule: %q", body)
	}
}

func TestLandingTemplateConformsToCronCanonicalWithSitesCopy(t *testing.T) {
	webDir := wwwRoot(t)
	sitesLanding := readFile(t, filepath.Join(webDir, "landing.html"))
	cronLanding := string(readFile(t, filepath.Join(webDir, "..", "..", "..", "cron", "share", "www", "landing.html")))

	want := cronLanding
	for _, replacement := range []struct {
		old string
		new string
	}{
		{`<title>{{.Service}} · cron</title>`, `<title>{{.Service}} · sites</title>`},
		{`<div class="eyebrow">Scheduled event emitter</div>`, `<div class="eyebrow">Static website host</div>`},
		{`<p>Cron keeps named schedules in SQLite and emits typed event-plane messages at minute boundaries.</p>`, `<p>Sites hosts file-backed static websites and serves them through the suite gateway.</p>`},
	} {
		if !strings.Contains(want, replacement.old) {
			t.Fatalf("cron canonical landing template missing %q", replacement.old)
		}
		want = strings.Replace(want, replacement.old, replacement.new, 1)
	}

	if got := string(sitesLanding); got != want {
		t.Fatalf("sites landing template does not match cron canonical template with the sites substitutions")
	}
}

func TestTokensCSSMatchesCronCanonical(t *testing.T) {
	webDir := wwwRoot(t)
	sitesTokens := readFile(t, filepath.Join(webDir, "static", "tokens.css"))
	cronTokens := readFile(t, filepath.Join(webDir, "..", "..", "..", "cron", "share", "www", "static", "tokens.css"))

	if !bytes.Equal(sitesTokens, cronTokens) {
		t.Fatalf("sites tokens.css differs from cron canonical tokens.css")
	}
}

func TestWWWStaticServesFonts(t *testing.T) {
	// R-ASST-7M2S
	for _, font := range []string{
		"space-grotesk.woff2",
		"ibm-plex-sans.woff2",
		"ibm-plex-mono-400.woff2",
		"ibm-plex-mono-500.woff2",
	} {
		rec := readWWWStaticResponse(t, "/static/fonts/"+font)

		if rec.Code != http.StatusOK || rec.Header().Get("Content-Type") != "font/woff2" {
			t.Fatalf("GET %s returned status %d Content-Type %q", font, rec.Code, rec.Header().Get("Content-Type"))
		}
		if rec.Body.Len() == 0 {
			t.Fatalf("GET %s returned an empty body", font)
		}
	}
}

func TestNginxFragmentGatesAndProxiesLandingRoot(t *testing.T) {
	conf := readNginxConfig(t)
	block := nginxLocationBlock(t, conf, "location = /srv/sites/")

	// R-NGNX-3P6T
	if !strings.Contains(conf, "location = /srv/sites/ {") {
		t.Fatalf("nginx fragment is missing exact-match landing root location")
	}
	if strings.Contains(conf, "location /srv/sites/ {") {
		t.Fatalf("nginx fragment contains a catch-all /srv/sites/ prefix location")
	}
	if !strings.Contains(conf, "location /srv/sites/public/ {") || !strings.Contains(conf, "location /srv/sites/private/ {") {
		t.Fatalf("nginx fragment is missing public/private tier prefixes")
	}

	// R-NGNX-5R8V
	if !strings.Contains(block, "auth_request /_session-authn;") {
		t.Fatalf("landing root block does not use dashboard session auth:\n%s", block)
	}
	if strings.Contains(block, "auth_request /_authn;") {
		t.Fatalf("landing root block uses bearer auth instead of session auth:\n%s", block)
	}

	// R-NGNX-7T1X
	if !strings.Contains(block, "proxy_pass http://127.0.0.1:3004/;") {
		t.Fatalf("landing root block does not proxy to the templated upstream root:\n%s", block)
	}
	if strings.Contains(block, "alias ") {
		t.Fatalf("landing root block is disk-backed instead of proxied:\n%s", block)
	}
}

func TestNginxFragmentPreservesExistingLocations(t *testing.T) {
	conf := readNginxConfig(t)

	prm := nginxLocationBlock(t, conf, "location = /srv/sites/.well-known/oauth-protected-resource")
	mcp := nginxLocationBlock(t, conf, "location = /srv/sites/mcp")
	landing := nginxLocationBlock(t, conf, "location = /srv/sites/")
	public := nginxLocationBlock(t, conf, "location /srv/sites/public/")
	private := nginxLocationBlock(t, conf, "location /srv/sites/private/")
	authn500 := nginxLocationBlock(t, conf, "location @sites_authn_500")

	// R-NGNX-9W4Z
	if strings.Contains(prm, "auth_request") {
		t.Fatalf("PRM bootstrap location unexpectedly requires auth:\n%s", prm)
	}
	if !strings.Contains(mcp, "auth_request /_authn;") {
		t.Fatalf("MCP location does not preserve bearer auth_request:\n%s", mcp)
	}
	if strings.Contains(public, "auth_request") {
		t.Fatalf("public static tier unexpectedly requires auth:\n%s", public)
	}
	if !strings.Contains(private, "auth_request /_session-authn;") {
		t.Fatalf("private static tier does not preserve session auth_request:\n%s", private)
	}
	if !strings.Contains(landing, "proxy_pass http://127.0.0.1:3004/;") {
		t.Fatalf("landing root block does not preserve upstream root proxy:\n%s", landing)
	}
	if !strings.Contains(authn500, "return 429;") || !strings.Contains(authn500, "return 500;") {
		t.Fatalf("authn 500 re-emit location does not preserve expected returns:\n%s", authn500)
	}
}

func TestNginxFragmentGatesAndProxiesLandingStaticAssets(t *testing.T) {
	conf := readNginxConfig(t)
	block := nginxLocationBlock(t, conf, "location /srv/sites/static/")

	// R-675A-R7MX
	if !strings.Contains(block, "auth_request /_session-authn;") {
		t.Fatalf("landing static block does not use dashboard session auth:\n%s", block)
	}
	if !strings.Contains(block, "proxy_pass http://127.0.0.1:3004/static/;") {
		t.Fatalf("landing static block does not proxy to the templated static upstream:\n%s", block)
	}
	if strings.Contains(block, "alias ") {
		t.Fatalf("landing static block is disk-backed instead of proxied:\n%s", block)
	}
}

// R-4LKF-FB23
func TestNginxFragmentServesStateWWWPublicAndSessionGatesPrivate(t *testing.T) {
	conf := readNginxConfig(t)
	public := nginxLocationBlock(t, conf, "location /srv/sites/public/")
	private := nginxLocationBlock(t, conf, "location /srv/sites/private/")

	if !strings.Contains(public, "alias /opt/sites/state/www/public/;") {
		t.Fatalf("public tier is not served from state/www/public:\n%s", public)
	}
	if strings.Contains(public, "auth_request") {
		t.Fatalf("public tier unexpectedly requires auth:\n%s", public)
	}
	if !strings.Contains(private, "auth_request /_session-authn;") {
		t.Fatalf("private tier does not use dashboard session auth:\n%s", private)
	}
	if !strings.Contains(private, "alias /opt/sites/state/www/private/;") {
		t.Fatalf("private tier is not served from state/www/private:\n%s", private)
	}
	if strings.Contains(conf, "/opt/sites/www/served/") {
		t.Fatalf("nginx fragment still references legacy /opt/sites/www/served path:\n%s", conf)
	}
}

// R-4LKF-FB23
func TestSitesBootsFromOpsctlLayoutAndServesHealth(t *testing.T) {
	root := t.TempDir()
	appRoot := filepath.Join(root, "sites")
	stateDir := filepath.Join(appRoot, "state")
	cacheDir := filepath.Join(appRoot, "cache")
	wwwDir := filepath.Join(stateDir, "www")
	libexecDir := filepath.Join(appRoot, "libexec")
	binDir := filepath.Join(appRoot, "bin")
	etcDir := filepath.Join(appRoot, "etc")
	shareDir := filepath.Join(appRoot, "share")
	for _, dir := range []string{
		filepath.Join(wwwDir, "public"),
		filepath.Join(wwwDir, "private"),
		filepath.Join(wwwDir, "working"),
		cacheDir,
		libexecDir,
		binDir,
		etcDir,
		shareDir,
	} {
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

	binary := filepath.Join(libexecDir, "sites-"+version)
	build := exec.Command("go", "build", "-o", binary, ".")
	build.Env = os.Environ()
	if out, err := build.CombinedOutput(); err != nil {
		t.Fatalf("go build sites: %v\n%s", err, out)
	}

	run := filepath.Join(binDir, "run")
	if err := os.Symlink("../libexec/sites-"+version, run); err != nil {
		t.Fatalf("symlink bin/run: %v", err)
	}
	if resolved, err := filepath.EvalSymlinks(run); err != nil || resolved != binary {
		t.Fatalf("bin/run resolves to %q err=%v, want %q", resolved, err, binary)
	}

	port := freeTCPPort(t)
	dbPath := filepath.Join(stateDir, "sites.db")
	generationPath := filepath.Join(cacheDir, "sites.db.generation")
	dropbox := httptest.NewServer(http.NotFoundHandler())
	t.Cleanup(dropbox.Close)

	ctx, cancel := context.WithCancel(context.Background())
	defer cancel()

	var stdout, stderr bytes.Buffer
	cmd := exec.CommandContext(ctx, run, "serve")
	cmd.Env = testEnv(map[string]string{
		"IKIGENBA_DOMAIN":  "int.ikigenba.com",
		"IKIGENBA_ROOT":    root,
		"SITES_IP":         "127.0.0.1",
		"SITES_PORT":       fmt.Sprintf("%d", port),
		"DROPBOX_BASE_URL": dropbox.URL,
	}, "SITES_ROOT")
	cmd.Stdout = &stdout
	cmd.Stderr = &stderr
	if err := cmd.Start(); err != nil {
		t.Fatalf("start sites: %v", err)
	}
	done := make(chan error, 1)
	go func() { done <- cmd.Wait() }()
	defer stopProcess(cancel, done)

	doc := waitForHealth(t, port, done, &stdout, &stderr)
	if got := doc["service"]; got != "sites" {
		t.Fatalf("health service = %v, want sites; body=%v", got, doc)
	}
	if got := doc["status"]; got != "ok" {
		t.Fatalf("health status = %v, want ok; body=%v", got, doc)
	}
	if _, err := os.Stat(dbPath); err != nil {
		t.Fatalf("sites did not create DB under state/: %v", err)
	}
	if _, err := os.Stat(generationPath); err != nil {
		t.Fatalf("sites did not create generation sidecar under cache/: %v", err)
	}
	if filepath.Dir(generationPath) != cacheDir {
		t.Fatalf("generation sidecar path %s is not under cache dir %s", generationPath, cacheDir)
	}
	for _, tier := range []string{"public", "private", "working"} {
		if _, err := os.Stat(filepath.Join(wwwDir, tier)); err != nil {
			t.Fatalf("sites www tier %s missing under state/www: %v", tier, err)
		}
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
	mux.Handle("GET /{$}", http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		if r.URL.Path != "/" {
			http.NotFound(w, r)
			return
		}
		_ = loadWWW(t).Render(w, "landing.html", landingData("sites", "1.2.3"))
	}))
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

func readFile(t *testing.T, name string) []byte {
	t.Helper()
	body, err := os.ReadFile(name)
	if err != nil {
		t.Fatal(err)
	}
	return body
}

func htmlHead(t *testing.T, body string) string {
	t.Helper()
	start := strings.Index(body, "<head>")
	end := strings.Index(body, "</head>")
	if start == -1 || end == -1 || end < start {
		t.Fatalf("HTML does not contain a closed head: %q", body)
	}
	return body[start : end+len("</head>")]
}

func readNginxConfig(t *testing.T) string {
	t.Helper()
	body, err := os.ReadFile(filepath.Join("..", "..", "etc", "nginx.conf"))
	if err != nil {
		t.Fatalf("read nginx fragment: %v", err)
	}
	return string(body)
}

func nginxLocationBlock(t *testing.T, conf, prefix string) string {
	t.Helper()
	start := strings.Index(conf, prefix+" {")
	if start == -1 {
		t.Fatalf("nginx fragment is missing %q location", prefix)
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
	t.Fatalf("nginx location %q is not closed", prefix)
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

func manifestExtras(in []appkit.ManifestKV) []manifest.KV {
	out := make([]manifest.KV, 0, len(in))
	for _, kv := range in {
		out = append(out, manifest.KV{Key: kv.Key, Value: kv.Value})
	}
	return out
}

func testEnv(overrides map[string]string, remove ...string) []string {
	env := os.Environ()
	out := make([]string, 0, len(env)+len(overrides))
	removed := make(map[string]bool, len(remove))
	for _, key := range remove {
		removed[key] = true
	}
	for _, kv := range env {
		key, _, _ := strings.Cut(kv, "=")
		if _, ok := overrides[key]; ok || removed[key] {
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
			t.Fatalf("sites exited before health: %v\nstdout:\n%s\nstderr:\n%s", err, stdout.String(), stderr.String())
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
	t.Fatalf("sites never served health at %s: %s\nstdout:\n%s\nstderr:\n%s", url, last, stdout.String(), stderr.String())
	return nil
}

func stopProcess(cancel context.CancelFunc, done <-chan error) {
	cancel()
	select {
	case <-done:
	case <-time.After(time.Second):
	}
}
