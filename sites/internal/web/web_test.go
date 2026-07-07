package web

import (
	"bytes"
	"net/http"
	"net/http/httptest"
	"os"
	"path/filepath"
	"runtime"
	"strings"
	"testing"
)

func TestLandingHandlerRendersServiceVersionAndContentType(t *testing.T) {
	rec := httptest.NewRecorder()
	LandingHandler("sites", "9.9.9-test").ServeHTTP(rec, httptest.NewRequest(http.MethodGet, "/", nil))
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
	mux := http.NewServeMux()
	mux.Handle("GET /{$}", LandingHandler("sites", "1.2.3"))
	mux.HandleFunc("POST /mcp", func(w http.ResponseWriter, r *http.Request) {
		w.WriteHeader(http.StatusAccepted)
		_, _ = w.Write([]byte("mcp"))
	})

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

func TestStaticHandlerServesTokensCSS(t *testing.T) {
	mux := http.NewServeMux()
	mux.Handle("GET /static/", StaticHandler())

	rec := httptest.NewRecorder()
	mux.ServeHTTP(rec, httptest.NewRequest(http.MethodGet, "/static/tokens.css", nil))
	body := rec.Body.String()

	// R-ASST-3H7N
	if rec.Code != http.StatusOK || !strings.HasPrefix(rec.Header().Get("Content-Type"), "text/css") {
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

func TestLandingHTMLReferencesOwnEmbeddedStaticPath(t *testing.T) {
	rec := httptest.NewRecorder()
	LandingHandler("sites", "asset-test").ServeHTTP(rec, httptest.NewRequest(http.MethodGet, "/", nil))
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

func TestLandingHTMLPreloadsSelfHostedFonts(t *testing.T) {
	rec := httptest.NewRecorder()
	LandingHandler("sites", "font-preload-test").ServeHTTP(rec, httptest.NewRequest(http.MethodGet, "/", nil))
	body := rec.Body.String()
	head := htmlHead(t, body)

	static := httptest.NewRecorder()
	StaticHandler().ServeHTTP(static, httptest.NewRequest(http.MethodGet, "/static/tokens.css", nil))
	css := static.Body.String()

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

func TestLandingHandlerRendersHomeLinkBeforeMain(t *testing.T) {
	rec := httptest.NewRecorder()
	LandingHandler("sites", "home-link-test").ServeHTTP(rec, httptest.NewRequest(http.MethodGet, "/", nil))
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
	webDir := currentWebDir(t)
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
	webDir := currentWebDir(t)
	sitesTokens := readFile(t, filepath.Join(webDir, "static", "tokens.css"))
	cronTokens := readFile(t, filepath.Join(webDir, "..", "..", "..", "cron", "share", "www", "static", "tokens.css"))

	if !bytes.Equal(sitesTokens, cronTokens) {
		t.Fatalf("sites tokens.css differs from cron canonical tokens.css")
	}
}

func TestStaticHandlerServesEmbeddedFonts(t *testing.T) {
	mux := http.NewServeMux()
	mux.Handle("GET /static/", StaticHandler())

	// R-ASST-7M2S
	for _, font := range []string{
		"space-grotesk.woff2",
		"ibm-plex-sans.woff2",
		"ibm-plex-mono-400.woff2",
		"ibm-plex-mono-500.woff2",
	} {
		rec := httptest.NewRecorder()
		mux.ServeHTTP(rec, httptest.NewRequest(http.MethodGet, "/static/fonts/"+font, nil))

		if rec.Code != http.StatusOK || rec.Header().Get("Content-Type") != "font/woff2" {
			t.Fatalf("GET %s returned status %d Content-Type %q", font, rec.Code, rec.Header().Get("Content-Type"))
		}
		if rec.Body.Len() == 0 {
			t.Fatalf("GET %s returned an empty body", font)
		}
	}
}

func currentWebDir(t *testing.T) string {
	t.Helper()

	_, file, _, ok := runtime.Caller(0)
	if !ok {
		t.Fatal("runtime.Caller failed")
	}
	return filepath.Dir(file)
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
