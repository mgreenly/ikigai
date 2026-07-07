package main

import (
	"net/http"
	"net/http/httptest"
	"os"
	"path/filepath"
	"strings"
	"testing"

	appweb "appkit/web"
)

func TestGmailSpecEnablesChassisWWWAndKeepsMCPWiring(t *testing.T) {
	spec := gmailSpec()

	// R-9LIV-1C1D
	if !spec.WWW {
		t.Fatal("gmailSpec().WWW = false, want true")
	}
	if !spec.MCP {
		t.Fatal("gmailSpec().MCP = false, want true")
	}
	if spec.Feed != "/feed" {
		t.Fatalf("gmailSpec().Feed = %q, want /feed", spec.Feed)
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
		`Workers: []func(context.Context) error{`,
	} {
		if !strings.Contains(main, want) {
			t.Fatalf("cmd/gmail/main.go missing %q", want)
		}
	}

	// R-9MQR-F3S2
	for _, forbidden := range []string{
		`"gmail/internal/web"`,
		`rt.Handle("GET /static/`,
		`web.LandingHandler`,
		`web.StaticHandler`,
	} {
		if strings.Contains(main, forbidden) {
			t.Fatalf("cmd/gmail/main.go still contains %q", forbidden)
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
	if err := site.Render(rec, "landing.html", landingData("gmail-real", "v1.2.3")); err != nil {
		t.Fatalf("render landing.html from share/www: %v", err)
	}
	if rec.Code != http.StatusOK || !strings.Contains(rec.Body.String(), "<title>gmail-real") {
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
	rec := renderLanding(t, `gmail <service>`, `v1&2`)

	// R-LAND-3F7K
	if rec.Code != http.StatusOK {
		t.Fatalf("status = %d, want %d", rec.Code, http.StatusOK)
	}
	if got := rec.Header().Get("Content-Type"); got != "text/html; charset=utf-8" {
		t.Fatalf("content-type = %q, want text/html; charset=utf-8", got)
	}

	body := rec.Body.String()
	// R-LAND-5H9M
	if strings.Contains(body, `gmail <service>`) {
		t.Fatalf("rendered body contained unescaped service name: %s", body)
	}
	if !strings.Contains(body, "gmail &lt;service&gt;") {
		t.Fatalf("rendered body did not contain escaped service name: %s", body)
	}
	if !strings.Contains(body, ">v1&amp;2<") {
		t.Fatalf("rendered body did not contain escaped version: %s", body)
	}
}

func TestWWWSiteRendersCanonicalGmailCopy(t *testing.T) {
	body := renderLanding(t, "gmail", "v-test").Body.String()

	for _, want := range []string{
		`<div class="eyebrow">Gmail connector</div>`,
		`<p>Gmail connects the owner's mailbox to the suite and publishes typed message events to the event plane.</p>`,
		`<dt>Service</dt>`,
		`<dd>gmail</dd>`,
		`<dt>Version</dt>`,
		`<dd class="version">v-test</dd>`,
		`<dt>API</dt>`,
		`<dd><code>POST /mcp</code></dd>`,
	} {
		if !strings.Contains(body, want) {
			t.Fatalf("body missing canonical landing fragment %q:\n%s", want, body)
		}
	}
}

func TestWWWSiteReferencesOnlyDocumentRelativeLocalStaticAssets(t *testing.T) {
	body := renderLanding(t, "gmail", "1.2.3").Body.String()

	// R-LAND-7J2N
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

func TestWWWSiteLandingLinksDocumentRelativeTokensInHead(t *testing.T) {
	head := htmlHead(t, renderLanding(t, "gmail", "1.2.3").Body.String())

	// R-3ZK3-PRTW
	if !strings.Contains(head, `href="static/tokens.css"`) {
		t.Fatalf("landing head missing document-relative tokens.css link:\n%s", head)
	}
	if strings.Contains(head, `href="/static/tokens.css"`) {
		t.Fatalf("landing head used origin-absolute tokens.css link:\n%s", head)
	}
}

func TestWWWSitePreloadsSelfServedFontFiles(t *testing.T) {
	head := htmlHead(t, renderLanding(t, "gmail", "1.2.3").Body.String())
	css := readWWWStatic(t, "/static/tokens.css")

	// R-40S0-3JKL
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

func TestWWWSiteLandingIsPureOverServiceAndVersion(t *testing.T) {
	// R-LAND-9K4P
	first := renderLanding(t, "gmail", "v-test").Body.String()
	second := renderLanding(t, "gmail", "v-test").Body.String()
	changed := renderLanding(t, "mailbox", "v-next").Body.String()

	if first != second {
		t.Fatal("same service/version inputs rendered different output")
	}
	if first == changed {
		t.Fatal("different service/version inputs rendered identical output")
	}
}

func TestWWWSiteLandingIncludesHomeLinkToDashboardApex(t *testing.T) {
	body := renderLanding(t, "gmail", "1.2.3").Body.String()

	// R-HOME-7Q9U
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

func TestExactRootRouteDispatchesToLanding(t *testing.T) {
	rec := httptest.NewRecorder()
	composedMux(t, http.NotFoundHandler()).ServeHTTP(rec, httptest.NewRequest(http.MethodGet, "/", nil))

	// R-ROUT-4M6Q
	if rec.Code != http.StatusOK {
		t.Fatalf("status = %d, want %d", rec.Code, http.StatusOK)
	}
	body := rec.Body.String()
	if !strings.Contains(body, "gmail") || !strings.Contains(body, "9.9.9-test") {
		t.Fatalf("GET / did not reach landing handler: %s", body)
	}
}

func TestExactRootRouteDoesNotCaptureNonRootPaths(t *testing.T) {
	for _, path := range []string{"/health", "/feed", "/.well-known/prm", "/nope"} {
		rec := httptest.NewRecorder()
		composedMux(t, http.NotFoundHandler()).ServeHTTP(rec, httptest.NewRequest(http.MethodGet, path, nil))

		// R-ROUT-6N8R
		if rec.Code != http.StatusNotFound {
			t.Fatalf("GET %s status = %d, want %d", path, rec.Code, http.StatusNotFound)
		}
		if strings.Contains(rec.Body.String(), "Gmail connector") {
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

	// R-ROUT-8P1S
	if rec.Code != http.StatusTeapot || rec.Body.String() != "mcp-stub" {
		t.Fatalf("POST /mcp did not reach stub: code=%d body=%q", rec.Code, rec.Body.String())
	}
	if strings.Contains(rec.Body.String(), "Gmail connector") {
		t.Fatal("POST /mcp was shadowed by the landing page")
	}
}

func TestWWWStaticServesTokensCSSWithContentType(t *testing.T) {
	rec := readWWWStaticResponse(t, "/static/tokens.css")

	// R-ASST-3T5V
	if rec.Code != http.StatusOK {
		t.Fatalf("tokens.css status = %d, want %d", rec.Code, http.StatusOK)
	}
	if got := rec.Header().Get("Content-Type"); got != "text/css; charset=utf-8" {
		t.Fatalf("tokens.css content-type = %q, want text/css; charset=utf-8", got)
	}
	if !strings.Contains(rec.Body.String(), "--font-display") {
		t.Fatalf("tokens.css body did not include expected design tokens: %s", rec.Body.String())
	}
}

func TestWWWStaticKeepsAssetsUnderStaticPath(t *testing.T) {
	for _, path := range []string{"/tokens.css", "/srv/gmail/static/tokens.css", "/static/missing.css"} {
		rec := readWWWStaticResponse(t, path)

		if rec.Code != http.StatusNotFound {
			t.Fatalf("GET %s status = %d, want %d", path, rec.Code, http.StatusNotFound)
		}
	}
}

func TestWWWTokensCSSDefinesSelfHostedFonts(t *testing.T) {
	body := readWWWStatic(t, "/static/tokens.css")

	// R-ASST-7Y9Z
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

	// R-3X4A-Y8CI
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

	// R-3YC7-C037
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

		// R-ASST-5W7X
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
	landingHandler(loadWWW(t), service, version).ServeHTTP(rec, httptest.NewRequest(http.MethodGet, "/", nil))
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
	mux.Handle("GET /{$}", landingHandler(loadWWW(t), "gmail", "9.9.9-test"))
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
