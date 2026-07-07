package main

import (
	"bytes"
	"io"
	"net/http"
	"net/http/httptest"
	"os"
	"path/filepath"
	"runtime"
	"strings"
	"testing"

	"appkit/web"
)

const webhooksDescription = "Webhooks keeps named inbound endpoints in SQLite and emits typed event-plane messages when callbacks arrive."

func TestLoadedShareWWWRendersLandingWithServiceAndVersion(t *testing.T) {
	site := loadShareWWW(t)
	rec := httptest.NewRecorder()

	if err := site.Render(rec, "landing.html",
		struct{ Service, Version string }{Service: "webhooks", Version: "v11-test"}); err != nil {
		t.Fatal(err)
	}

	body := rec.Body.String()
	// R-0GVM-EPJ9
	if !strings.Contains(body, "webhooks") {
		t.Fatalf("rendered landing body does not contain service name:\n%s", body)
	}
	if !strings.Contains(body, "v11-test") {
		t.Fatalf("rendered landing body does not contain exact version string:\n%s", body)
	}
}

func TestChassisStaticMountServesTokensAndFonts(t *testing.T) {
	site := loadShareWWW(t)
	mux := http.NewServeMux()
	mux.Handle("GET /static/", site.Static())

	// R-0I3I-SH9Y
	css := requestAsset(t, mux, "/static/tokens.css", "text/css")
	if strings.TrimSpace(string(css)) == "" {
		t.Fatal("tokens.css body is empty")
	}

	font := requestAsset(t, mux, "/static/fonts/space-grotesk.woff2", "font/woff2")
	if !bytes.HasPrefix(font, []byte("wOF2")) {
		t.Fatalf("space-grotesk.woff2 does not look like a WOFF2 payload")
	}

	mainGo, err := os.ReadFile("main.go")
	if err != nil {
		t.Fatal(err)
	}
	if bytes.Contains(mainGo, []byte(`rt.Handle("GET /static/`)) {
		t.Fatalf("main.go still registers a webhooks-side static route:\n%s", mainGo)
	}
}

func TestLandingHandlerRendersRootWithServiceVersionAndHTMLContentType(t *testing.T) {
	site := loadShareWWW(t)
	req := httptest.NewRequest(http.MethodGet, "/", nil)
	rec := httptest.NewRecorder()

	landingHandler(site, "webhooks", "v11-handler").ServeHTTP(rec, req)

	res := rec.Result()
	body := rec.Body.String()
	// R-TMJH-V1NP
	if res.StatusCode != http.StatusOK {
		t.Fatalf("status = %d, want %d", res.StatusCode, http.StatusOK)
	}
	if got := res.Header.Get("Content-Type"); got != "text/html; charset=utf-8" {
		t.Fatalf("Content-Type = %q, want text/html; charset=utf-8", got)
	}
	if !strings.Contains(body, "webhooks") {
		t.Fatalf("landing body does not contain service name %q:\n%s", "webhooks", body)
	}
	if !strings.Contains(body, "v11-handler") {
		t.Fatalf("landing body does not contain injected version %q:\n%s", "v11-handler", body)
	}
}

func TestExactRootLandingRouteDoesNotMatchNonRootPath(t *testing.T) {
	site := loadShareWWW(t)
	mux := http.NewServeMux()
	mux.Handle("GET /{$}", landingHandler(site, "webhooks", "test"))

	rec := httptest.NewRecorder()
	mux.ServeHTTP(rec, httptest.NewRequest(http.MethodGet, "/nope", nil))

	// R-TNRE-8TEE
	if rec.Result().StatusCode != http.StatusNotFound {
		t.Fatalf("status = %d, want %d", rec.Result().StatusCode, http.StatusNotFound)
	}
	if strings.Contains(rec.Body.String(), "webhooks") {
		t.Fatalf("non-root request returned landing body:\n%s", rec.Body.String())
	}
}

func TestShareWWWStaticHandlerServesTokensAndFonts(t *testing.T) {
	site := loadShareWWW(t)
	mux := http.NewServeMux()
	mux.Handle("GET /static/", site.Static())

	// R-TOZA-ML53
	css := requestAsset(t, mux, "/static/tokens.css", "text/css")
	if strings.TrimSpace(string(css)) == "" {
		t.Fatal("tokens.css body is empty")
	}

	font := requestAsset(t, mux, "/static/fonts/ibm-plex-sans.woff2", "font/woff2")
	if !bytes.HasPrefix(font, []byte("wOF2")) {
		t.Fatalf("ibm-plex-sans.woff2 does not look like a WOFF2 payload")
	}
}

func TestShippedLandingHTMLKeepsWebhooksContentAndStructure(t *testing.T) {
	body := readShareFile(t, "landing.html")

	// R-TQ77-0CVS
	for _, want := range []string{
		`<title>{{.Service}} · webhooks</title>`,
		`href="static/tokens.css"`,
		`<a class="home" href="/">Home</a>`,
		`<div class="eyebrow">Inbound Webhooks</div>`,
		`<h1 id="page-title">{{.Service}}</h1>`,
		`<p>` + webhooksDescription + `</p>`,
		`<dl aria-label="Service details">`,
		`<dt>Service</dt>`,
		`<dd>{{.Service}}</dd>`,
		`<dt>Version</dt>`,
		`<dd class="version">{{.Version}}</dd>`,
		`<dt>API</dt>`,
		`<dd><code>POST /mcp</code></dd>`,
	} {
		if !strings.Contains(body, want) {
			t.Fatalf("landing.html does not contain %q:\n%s", want, body)
		}
	}
}

func TestShippedAssetsUseOptionalLocalFontsAndPreloads(t *testing.T) {
	css := readShareFile(t, "static", "tokens.css")
	head := headMarkup(t, readShareFile(t, "landing.html"))

	// R-TRF3-E4MH
	fontFaceCount := strings.Count(css, "@font-face")
	if fontFaceCount != 4 {
		t.Fatalf("tokens.css has %d @font-face blocks, want 4:\n%s", fontFaceCount, css)
	}
	if got := strings.Count(css, "font-display: optional;"); got != fontFaceCount {
		t.Fatalf("tokens.css has %d optional font-display declarations, want %d:\n%s", got, fontFaceCount, css)
	}
	for _, disallowed := range []string{"https://", "http://", "fonts.googleapis.com", `url('/`, `url("/`} {
		if strings.Contains(css, disallowed) {
			t.Fatalf("tokens.css contains non-local font source %q:\n%s", disallowed, css)
		}
	}
	for _, want := range []string{
		`url('fonts/space-grotesk.woff2')`,
		`url('fonts/ibm-plex-sans.woff2')`,
		`url('fonts/ibm-plex-mono-400.woff2')`,
		`url('fonts/ibm-plex-mono-500.woff2')`,
	} {
		if !strings.Contains(css, want) {
			t.Fatalf("tokens.css does not contain relative font source %q:\n%s", want, css)
		}
	}

	for _, href := range []string{
		"static/fonts/space-grotesk.woff2",
		"static/fonts/ibm-plex-sans.woff2",
	} {
		link := linkMarkupWithHref(t, head, href)
		for _, want := range []string{`rel="preload"`, `as="font"`, `type="font/woff2"`, "crossorigin"} {
			if !strings.Contains(link, want) {
				t.Fatalf("font preload for %s does not contain %q:\n%s", href, want, link)
			}
		}
	}
	if got := strings.Count(head, `rel="preload"`); got != 2 {
		t.Fatalf("head has %d preload links, want 2:\n%s", got, head)
	}
}

func TestServeMuxLandingAndStaticDoNotShadowExistingRoutes(t *testing.T) {
	site := loadShareWWW(t)
	mux := http.NewServeMux()
	mux.Handle("GET /{$}", landingHandler(site, "webhooks", "route-test"))
	mux.Handle("GET /static/", site.Static())
	mux.Handle("POST /mcp", markerHandler("mcp"))
	mux.Handle("/in/", markerHandler("ingress"))
	mux.Handle("GET /feed", markerHandler("feed"))
	mux.Handle("GET /.well-known/oauth-protected-resource", markerHandler("prm"))

	for _, tc := range []struct {
		name   string
		method string
		path   string
		marker string
	}{
		{name: "mcp", method: http.MethodPost, path: "/mcp", marker: "mcp"},
		{name: "ingress", method: http.MethodPost, path: "/in/deploy", marker: "ingress"},
		{name: "feed", method: http.MethodGet, path: "/feed", marker: "feed"},
		{name: "prm", method: http.MethodGet, path: "/.well-known/oauth-protected-resource", marker: "prm"},
	} {
		t.Run(tc.name, func(t *testing.T) {
			rec := httptest.NewRecorder()
			mux.ServeHTTP(rec, httptest.NewRequest(tc.method, tc.path, nil))

			if rec.Result().StatusCode != http.StatusNoContent {
				t.Fatalf("%s %s status = %d, want %d", tc.method, tc.path, rec.Result().StatusCode, http.StatusNoContent)
			}
			if got := rec.Result().Header.Get("X-Test-Route"); got != tc.marker {
				t.Fatalf("%s %s reached marker %q, want %q", tc.method, tc.path, got, tc.marker)
			}
		})
	}
}

func requestAsset(t *testing.T, h http.Handler, path, contentType string) []byte {
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
	if got := res.Header.Get("Content-Type"); strings.TrimSpace(strings.Split(got, ";")[0]) != contentType {
		t.Fatalf("%s Content-Type = %q, want %q", path, got, contentType)
	}
	if len(body) == 0 {
		t.Fatalf("%s returned an empty body", path)
	}
	return body
}

func loadShareWWW(t *testing.T) *web.Site {
	t.Helper()

	site, err := web.Load(shareWWWRoot(t))
	if err != nil {
		t.Fatal(err)
	}
	return site
}

func readShareFile(t *testing.T, elem ...string) string {
	t.Helper()

	path := filepath.Join(append([]string{shareWWWRoot(t)}, elem...)...)
	body, err := os.ReadFile(path)
	if err != nil {
		t.Fatal(err)
	}
	return string(body)
}

func shareWWWRoot(t *testing.T) string {
	t.Helper()

	_, file, _, ok := runtime.Caller(0)
	if !ok {
		t.Fatal("runtime.Caller failed")
	}
	return filepath.Clean(filepath.Join(filepath.Dir(file), "..", "..", "share", "www"))
}

func markerHandler(marker string) http.Handler {
	return http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		w.Header().Set("X-Test-Route", marker)
		w.WriteHeader(http.StatusNoContent)
	})
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
