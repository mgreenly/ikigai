package web

import (
	"io/fs"
	"net/http"
	"net/http/httptest"
	"os"
	"strings"
	"testing"
)

func TestLandingHandlerRootResponse(t *testing.T) {
	// R-LAND-PG01
	req := httptest.NewRequest(http.MethodGet, "/", nil)
	rec := httptest.NewRecorder()

	LandingHandler("prompts", "v11-test")(rec, req)

	if rec.Code != http.StatusOK {
		t.Fatalf("status = %d, want %d", rec.Code, http.StatusOK)
	}
	if got, want := rec.Header().Get("Content-Type"), "text/html; charset=utf-8"; got != want {
		t.Fatalf("content type = %q, want %q", got, want)
	}
}

func TestLandingHandlerRendersInjectedNameAndVersion(t *testing.T) {
	// R-LAND-NMVR
	req := httptest.NewRequest(http.MethodGet, "/", nil)
	rec := httptest.NewRecorder()

	LandingHandler("prompts-canary", "v2035.04.19-phase11")(rec, req)

	body := rec.Body.String()
	for _, want := range []string{"prompts-canary", "v2035.04.19-phase11"} {
		if !strings.Contains(body, want) {
			t.Fatalf("body missing %q:\n%s", want, body)
		}
	}
}

func TestLandingHandlerRendersCanonicalPromptsLanding(t *testing.T) {
	req := httptest.NewRequest(http.MethodGet, "/", nil)
	rec := httptest.NewRecorder()

	LandingHandler("prompts-canary", "v2035.04.19-phase14")(rec, req)

	body := rec.Body.String()
	for _, want := range []string{
		"<title>prompts-canary · prompts</title>",
		`<a class="home" href="/">Home</a>`,
		`<div class="eyebrow">Agent sessions</div>`,
		`<h1 id="page-title">prompts-canary</h1>`,
		"Prompts runs configured agent sessions through MCP tools and records the resulting outputs.",
		`<dl aria-label="Service details">`,
		"<dt>Service</dt>",
		"<dd>prompts-canary</dd>",
		"<dt>Version</dt>",
		`<dd class="version">v2035.04.19-phase14</dd>`,
		"<dt>API</dt>",
		"<dd><code>POST /mcp</code></dd>",
	} {
		if !strings.Contains(body, want) {
			t.Fatalf("body missing %q:\n%s", want, body)
		}
	}
}

func TestLandingHandlerRendersHomeLinkToDashboardApex(t *testing.T) {
	// R-HOME-2T4X
	req := httptest.NewRequest(http.MethodGet, "/", nil)
	rec := httptest.NewRecorder()

	LandingHandler("prompts", "v15-test")(rec, req)

	body := rec.Body.String()
	for _, want := range []string{
		`<a class="home" href="/">Home</a>`,
		".home {",
		"position: absolute;",
		"top: var(--space-8);",
		"position: relative;",
	} {
		if !strings.Contains(body, want) {
			t.Fatalf("body missing %q:\n%s", want, body)
		}
	}
}

func TestLandingAssetsAreEmbeddedAndServed(t *testing.T) {
	// R-LAND-CARB
	if _, err := content.ReadFile("static/tokens.css"); err != nil {
		t.Fatalf("embedded tokens.css missing: %v", err)
	}
	fonts, err := fs.Glob(content, "static/fonts/*.woff2")
	if err != nil {
		t.Fatalf("glob fonts: %v", err)
	}
	if len(fonts) == 0 {
		t.Fatal("embedded woff2 fonts missing")
	}

	pageReq := httptest.NewRequest(http.MethodGet, "/", nil)
	pageRec := httptest.NewRecorder()
	LandingHandler("prompts", "v11")(pageRec, pageReq)
	if !strings.Contains(pageRec.Body.String(), `href="static/tokens.css"`) {
		t.Fatalf("landing page does not reference tokens.css:\n%s", pageRec.Body.String())
	}

	staticReq := httptest.NewRequest(http.MethodGet, "/tokens.css", nil)
	staticRec := httptest.NewRecorder()
	StaticHandler().ServeHTTP(staticRec, staticReq)

	if staticRec.Code != http.StatusOK {
		t.Fatalf("tokens.css status = %d, want %d", staticRec.Code, http.StatusOK)
	}
	if got := staticRec.Header().Get("Content-Type"); !strings.HasPrefix(got, "text/css") {
		t.Fatalf("tokens.css content type = %q, want text/css", got)
	}
}

func TestLandingMuxMatchesExactRootOnly(t *testing.T) {
	// R-LAND-ROOT
	mux := http.NewServeMux()
	mux.HandleFunc("GET /{$}", LandingHandler("prompts-root-only", "v11"))

	for _, path := range []string{"/health", "/mcp", "/nope"} {
		req := httptest.NewRequest(http.MethodGet, path, nil)
		rec := httptest.NewRecorder()

		mux.ServeHTTP(rec, req)

		if rec.Code == http.StatusOK && strings.Contains(rec.Body.String(), "prompts-root-only") {
			t.Fatalf("path %s received landing page", path)
		}
	}
}

func TestLandingIsUngated(t *testing.T) {
	// R-LAND-UNGT
	req := httptest.NewRequest(http.MethodGet, "/", nil)
	rec := httptest.NewRecorder()

	LandingHandler("prompts", "v11")(rec, req)

	if rec.Code != http.StatusOK {
		t.Fatalf("status without identity or bearer = %d, want %d", rec.Code, http.StatusOK)
	}
}

func TestTokensCSSUsesOptionalRelativeFontSources(t *testing.T) {
	staticReq := httptest.NewRequest(http.MethodGet, "/tokens.css", nil)
	staticRec := httptest.NewRecorder()
	StaticHandler().ServeHTTP(staticRec, staticReq)

	if staticRec.Code != http.StatusOK {
		t.Fatalf("tokens.css status = %d, want %d", staticRec.Code, http.StatusOK)
	}
	css := staticRec.Body.String()

	// R-DFKP-IVZU
	if strings.Contains(css, "font-display: swap") {
		t.Fatalf("tokens.css still contains font-display swap:\n%s", css)
	}
	if got, want := strings.Count(css, "@font-face"), strings.Count(css, "font-display: optional"); got != want {
		t.Fatalf("font-display optional count = %d, want one per %d @font-face blocks", want, got)
	}

	// R-DGSL-WNQJ
	if strings.Contains(css, "url('/static/fonts/") {
		t.Fatalf("tokens.css still contains origin-absolute font URLs:\n%s", css)
	}
	for _, want := range []string{
		"url('fonts/space-grotesk.woff2')",
		"url('fonts/ibm-plex-sans.woff2')",
		"url('fonts/ibm-plex-mono-400.woff2')",
		"url('fonts/ibm-plex-mono-500.woff2')",
	} {
		if !strings.Contains(css, want) {
			t.Fatalf("tokens.css missing relative font source %q:\n%s", want, css)
		}
	}
}

func TestLandingHeadUsesRelativeTokensAndPreloadsFonts(t *testing.T) {
	req := httptest.NewRequest(http.MethodGet, "/", nil)
	rec := httptest.NewRecorder()

	LandingHandler("prompts", "v16")(rec, req)

	head := renderedHead(t, rec.Body.String())

	// R-DI0I-AFH8
	if !strings.Contains(head, `href="static/tokens.css"`) {
		t.Fatalf("head missing relative tokens.css link:\n%s", head)
	}
	if strings.Contains(head, `href="/static/tokens.css"`) {
		t.Fatalf("head contains origin-absolute tokens.css link:\n%s", head)
	}

	// R-DJ8E-O77X
	for _, font := range []string{"space-grotesk.woff2", "ibm-plex-sans.woff2"} {
		want := `<link rel="preload" as="font" type="font/woff2" crossorigin href="static/fonts/` + font + `">`
		if !strings.Contains(head, want) {
			t.Fatalf("head missing font preload %q:\n%s", want, head)
		}
	}
}

func TestNginxStaticLocationUsesSessionAuth(t *testing.T) {
	conf, err := os.ReadFile("../../etc/nginx.conf")
	if err != nil {
		t.Fatalf("read nginx conf: %v", err)
	}
	text := string(conf)
	block := nginxLocationBlock(t, text, "location /srv/prompts/static/ {")

	// R-DKGB-1YYM
	for _, want := range []string{
		"auth_request /_session-authn;",
		"proxy_pass http://127.0.0.1:__PORT__/static/;",
	} {
		if !strings.Contains(block, want) {
			t.Fatalf("static location missing %q:\n%s", want, block)
		}
	}
	for _, want := range []string{
		"location = /srv/prompts/.well-known/oauth-protected-resource",
		"location = /srv/prompts/",
		"location = /srv/prompts/feed { return 404; }",
		"location /srv/prompts/ {",
	} {
		if !strings.Contains(text, want) {
			t.Fatalf("nginx conf missing existing location %q:\n%s", want, text)
		}
	}
}

func renderedHead(t *testing.T, html string) string {
	t.Helper()
	start := strings.Index(html, "<head>")
	end := strings.Index(html, "</head>")
	if start < 0 || end < 0 || end < start {
		t.Fatalf("rendered page missing head:\n%s", html)
	}
	return html[start : end+len("</head>")]
}

func nginxLocationBlock(t *testing.T, conf, marker string) string {
	t.Helper()
	start := strings.Index(conf, marker)
	if start < 0 {
		t.Fatalf("nginx conf missing %q:\n%s", marker, conf)
	}
	rest := conf[start:]
	end := strings.Index(rest, "\n}\n")
	if end < 0 {
		t.Fatalf("nginx conf location %q is not closed:\n%s", marker, rest)
	}
	return rest[:end+len("\n}")]
}
