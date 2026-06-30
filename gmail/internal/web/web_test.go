package web

import (
	"io"
	"net/http"
	"net/http/httptest"
	"strings"
	"testing"
)

func TestLandingHandlerReturnsHTMLStatusAndContentType(t *testing.T) {
	// R-LAND-3F7K
	rec := httptest.NewRecorder()
	LandingHandler("gmail", "v-test").ServeHTTP(rec, httptest.NewRequest(http.MethodGet, "/", nil))

	if rec.Code != http.StatusOK {
		t.Fatalf("status = %d, want %d", rec.Code, http.StatusOK)
	}
	if got, want := rec.Header().Get("Content-Type"), "text/html; charset=utf-8"; got != want {
		t.Fatalf("Content-Type = %q, want %q", got, want)
	}
	if !strings.Contains(rec.Body.String(), `<h1 id="page-title">gmail</h1>`) {
		t.Fatalf("body did not contain rendered service heading:\n%s", rec.Body.String())
	}
	if !strings.Contains(rec.Body.String(), `<dd><code>POST /mcp</code></dd>`) {
		t.Fatalf("body did not contain MCP API detail:\n%s", rec.Body.String())
	}
}

func TestLandingHandlerEscapesServiceAndVersion(t *testing.T) {
	// R-LAND-5H9M
	rec := httptest.NewRecorder()
	LandingHandler(`gmail<script>`, `v&1`).ServeHTTP(rec, httptest.NewRequest(http.MethodGet, "/", nil))
	body := rec.Body.String()

	if strings.Contains(body, `gmail<script>`) {
		t.Fatalf("body contained unescaped service name:\n%s", body)
	}
	for _, want := range []string{`gmail&lt;script&gt;`, `v&amp;1`} {
		if !strings.Contains(body, want) {
			t.Fatalf("body missing escaped dynamic value %q:\n%s", want, body)
		}
	}
}

func TestLandingHandlerRendersCanonicalGmailCopy(t *testing.T) {
	rec := httptest.NewRecorder()
	LandingHandler("gmail", "v-test").ServeHTTP(rec, httptest.NewRequest(http.MethodGet, "/", nil))
	body := rec.Body.String()

	for _, want := range []string{
		`<title>gmail · gmail</title>`,
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

func TestLandingHandlerRendersHomeLinkFirstInBody(t *testing.T) {
	// R-HOME-7Q9U
	rec := httptest.NewRecorder()
	LandingHandler("gmail", "v-test").ServeHTTP(rec, httptest.NewRequest(http.MethodGet, "/", nil))
	body := rec.Body.String()

	if !strings.Contains(body, `<main>
    <a class="home" href="/">Home</a>`) {
		t.Fatalf("home link was not the first body child inside main:\n%s", body)
	}
	if !strings.Contains(body, `.home {`) {
		t.Fatalf("body missing home style rule:\n%s", body)
	}
	if !strings.Contains(body, `.home:hover,
    .home:focus-visible {
      color: var(--color-text);
    }`) {
		t.Fatalf("body missing home hover/focus style rule:\n%s", body)
	}
	if strings.Contains(body, `>Dashboard</a>`) {
		t.Fatalf("home link used Dashboard copy instead of Home:\n%s", body)
	}
}

func TestLandingTemplateLinksEmbeddedTokens(t *testing.T) {
	// R-LAND-7J2N
	rec := httptest.NewRecorder()
	LandingHandler("gmail", "v-test").ServeHTTP(rec, httptest.NewRequest(http.MethodGet, "/", nil))
	body := rec.Body.String()

	if !strings.Contains(body, `href="static/tokens.css"`) {
		t.Fatalf("landing page did not link embedded tokens.css through the static route:\n%s", body)
	}
	if strings.Contains(body, `href="/static/tokens.css"`) || strings.Contains(body, "dashboard") || strings.Contains(body, "http://") || strings.Contains(body, "https://") {
		t.Fatalf("landing page should not fetch assets from another runtime origin:\n%s", body)
	}
}

func TestLandingHandlerLinksDocumentRelativeTokensInHead(t *testing.T) {
	// R-3ZK3-PRTW
	rec := httptest.NewRecorder()
	LandingHandler("gmail", "v-test").ServeHTTP(rec, httptest.NewRequest(http.MethodGet, "/", nil))
	head := landingHead(t, rec.Body.String())

	if !strings.Contains(head, `href="static/tokens.css"`) {
		t.Fatalf("landing head missing document-relative tokens.css link:\n%s", head)
	}
	if strings.Contains(head, `href="/static/tokens.css"`) {
		t.Fatalf("landing head used origin-absolute tokens.css link:\n%s", head)
	}
}

func TestLandingHandlerPreloadsVendoredFontsInHead(t *testing.T) {
	// R-40S0-3JKL
	rec := httptest.NewRecorder()
	LandingHandler("gmail", "v-test").ServeHTTP(rec, httptest.NewRequest(http.MethodGet, "/", nil))
	head := landingHead(t, rec.Body.String())
	tokens := staticTokensBody(t)

	for _, font := range []string{"space-grotesk.woff2", "ibm-plex-sans.woff2"} {
		href := "static/fonts/" + font
		if !strings.Contains(head, `<link rel="preload" as="font" type="font/woff2" crossorigin href="`+href+`">`) {
			t.Fatalf("landing head missing font preload for %s:\n%s", font, head)
		}
		if !strings.Contains(tokens, `src: url('fonts/`+font+`') format('woff2');`) {
			t.Fatalf("tokens.css missing matching @font-face src for %s:\n%s", font, tokens)
		}
	}
}

func TestLandingHandlerIsPureOverServiceAndVersion(t *testing.T) {
	// R-LAND-9K4P
	first := recordLanding(t, "gmail", "v-test")
	second := recordLanding(t, "gmail", "v-test")
	changed := recordLanding(t, "mailbox", "v-next")

	if first != second {
		t.Fatalf("same inputs rendered different output")
	}
	if first == changed {
		t.Fatalf("different service/version inputs rendered identical output")
	}
}

func TestExactRootPatternMatchesOnlyRoot(t *testing.T) {
	// R-ROUT-4M6Q
	mux := http.NewServeMux()
	mux.Handle("GET /{$}", LandingHandler("gmail", "v-test"))

	root := httptest.NewRecorder()
	mux.ServeHTTP(root, httptest.NewRequest(http.MethodGet, "/", nil))
	if root.Code != http.StatusOK {
		t.Fatalf("GET / status = %d, want %d", root.Code, http.StatusOK)
	}

	subtree := httptest.NewRecorder()
	mux.ServeHTTP(subtree, httptest.NewRequest(http.MethodGet, "/anything", nil))
	if subtree.Code != http.StatusNotFound {
		t.Fatalf("GET /anything status = %d, want %d", subtree.Code, http.StatusNotFound)
	}
}

func TestExactRootPatternDoesNotShadowMCP(t *testing.T) {
	// R-ROUT-6N8R
	mux := http.NewServeMux()
	mux.Handle("GET /{$}", LandingHandler("gmail", "v-test"))
	mux.HandleFunc("POST /mcp", func(w http.ResponseWriter, r *http.Request) {
		w.WriteHeader(http.StatusNoContent)
	})

	rec := httptest.NewRecorder()
	mux.ServeHTTP(rec, httptest.NewRequest(http.MethodPost, "/mcp", nil))
	if rec.Code != http.StatusNoContent {
		t.Fatalf("POST /mcp status = %d, want %d", rec.Code, http.StatusNoContent)
	}
}

func TestStaticRouteServesUnderStaticPrefix(t *testing.T) {
	// R-ROUT-8P1S
	mux := http.NewServeMux()
	mux.Handle("GET /static/", http.StripPrefix("/static/", StaticHandler()))

	rec := httptest.NewRecorder()
	mux.ServeHTTP(rec, httptest.NewRequest(http.MethodGet, "/static/tokens.css", nil))
	if rec.Code != http.StatusOK {
		t.Fatalf("GET /static/tokens.css status = %d, want %d", rec.Code, http.StatusOK)
	}
	if !strings.Contains(rec.Body.String(), "Carbon") {
		t.Fatalf("static route did not return embedded tokens.css")
	}
}

func TestStaticTokensContentType(t *testing.T) {
	// R-ASST-3T5V
	rec := httptest.NewRecorder()
	StaticHandler().ServeHTTP(rec, httptest.NewRequest(http.MethodGet, "/tokens.css", nil))

	if rec.Code != http.StatusOK {
		t.Fatalf("status = %d, want %d", rec.Code, http.StatusOK)
	}
	if got, want := rec.Header().Get("Content-Type"), "text/css; charset=utf-8"; got != want {
		t.Fatalf("Content-Type = %q, want %q", got, want)
	}
	if !strings.Contains(rec.Body.String(), "--font-display") {
		t.Fatalf("tokens.css body did not include expected design tokens")
	}
}

func TestStaticTokensUseOptionalFontDisplay(t *testing.T) {
	// R-3X4A-Y8CI
	body := staticTokensBody(t)
	blocks := strings.Split(body, "@font-face")
	if len(blocks) == 1 {
		t.Fatalf("tokens.css did not contain any @font-face blocks:\n%s", body)
	}
	if strings.Contains(body, "font-display: swap") {
		t.Fatalf("tokens.css still contained font-display: swap:\n%s", body)
	}
	for _, block := range blocks[1:] {
		if !strings.Contains(block, "font-display: optional;") {
			t.Fatalf("@font-face block missing font-display: optional:\n%s", block)
		}
	}
}

func TestStaticFontsContentTypeAndBytes(t *testing.T) {
	// R-ASST-5W7X
	rec := httptest.NewRecorder()
	StaticHandler().ServeHTTP(rec, httptest.NewRequest(http.MethodGet, "/fonts/space-grotesk.woff2", nil))

	if rec.Code != http.StatusOK {
		t.Fatalf("status = %d, want %d", rec.Code, http.StatusOK)
	}
	if got, want := rec.Header().Get("Content-Type"), "font/woff2"; got != want {
		t.Fatalf("Content-Type = %q, want %q", got, want)
	}
	if rec.Body.Len() == 0 {
		t.Fatalf("embedded font response was empty")
	}
}

func TestStaticTokensReferenceVendoredFontsWithoutRemoteOrigins(t *testing.T) {
	// R-ASST-7Y9Z
	body := staticTokensBody(t)

	for _, want := range []string{
		`fonts/space-grotesk.woff2`,
		`fonts/ibm-plex-sans.woff2`,
		`fonts/ibm-plex-mono-400.woff2`,
		`fonts/ibm-plex-mono-500.woff2`,
	} {
		if !strings.Contains(body, want) {
			t.Fatalf("tokens.css missing embedded font path %q:\n%s", want, body)
		}
	}
	for _, forbidden := range []string{"fonts.googleapis.com", "fonts.gstatic.com", "dashboard"} {
		if strings.Contains(body, forbidden) {
			t.Fatalf("tokens.css contained forbidden runtime origin marker %q", forbidden)
		}
	}
}

func TestStaticTokensUseRelativeFontSources(t *testing.T) {
	// R-3YC7-C037
	body := staticTokensBody(t)

	if strings.Contains(body, `url('/static/fonts/`) {
		t.Fatalf("tokens.css contained origin-absolute font URL:\n%s", body)
	}
	for _, want := range []string{
		`src: url('fonts/space-grotesk.woff2') format('woff2');`,
		`src: url('fonts/ibm-plex-sans.woff2') format('woff2');`,
		`src: url('fonts/ibm-plex-mono-400.woff2') format('woff2');`,
		`src: url('fonts/ibm-plex-mono-500.woff2') format('woff2');`,
	} {
		if !strings.Contains(body, want) {
			t.Fatalf("tokens.css missing relative font source %q:\n%s", want, body)
		}
	}
}

func recordLanding(t *testing.T, service, version string) string {
	t.Helper()

	rec := httptest.NewRecorder()
	LandingHandler(service, version).ServeHTTP(rec, httptest.NewRequest(http.MethodGet, "/", nil))
	if rec.Code != http.StatusOK {
		t.Fatalf("status = %d, want %d", rec.Code, http.StatusOK)
	}
	body, err := io.ReadAll(rec.Result().Body)
	if err != nil {
		t.Fatalf("read response body: %v", err)
	}
	return string(body)
}

func landingHead(t *testing.T, body string) string {
	t.Helper()

	start := strings.Index(body, "<head>")
	end := strings.Index(body, "</head>")
	if start == -1 || end == -1 || end < start {
		t.Fatalf("landing page missing complete head:\n%s", body)
	}
	return body[start : end+len("</head>")]
}

func staticTokensBody(t *testing.T) string {
	t.Helper()

	rec := httptest.NewRecorder()
	StaticHandler().ServeHTTP(rec, httptest.NewRequest(http.MethodGet, "/tokens.css", nil))
	if rec.Code != http.StatusOK {
		t.Fatalf("GET /tokens.css status = %d, want %d", rec.Code, http.StatusOK)
	}
	return rec.Body.String()
}
