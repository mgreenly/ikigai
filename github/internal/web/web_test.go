package web

import (
	"io"
	"net/http"
	"net/http/httptest"
	"os"
	"strings"
	"testing"
)

func TestLandingHandlerRendersHTMLWithServiceAndVersion(t *testing.T) {
	// R-EVZ3-VXJZ
	req := httptest.NewRequest(http.MethodGet, "/", nil)
	rec := httptest.NewRecorder()

	LandingHandler("github-test", "v9.8.7").ServeHTTP(rec, req)

	if rec.Code != http.StatusOK {
		t.Fatalf("status = %d, want %d", rec.Code, http.StatusOK)
	}
	if got := rec.Header().Get("Content-Type"); got != "text/html; charset=utf-8" {
		t.Fatalf("Content-Type = %q, want text/html; charset=utf-8", got)
	}

	body := rec.Body.String()
	if count := strings.Count(body, "github-test"); count != 3 {
		t.Fatalf("service name count = %d, want 3 in title, heading, and details\n%s", count, body)
	}
	if count := strings.Count(body, "v9.8.7"); count != 1 {
		t.Fatalf("version count = %d, want 1\n%s", count, body)
	}
}

func TestLandingHandlerLinksOnlyAppLocalStaticAssets(t *testing.T) {
	// R-XSOU-THYE
	req := httptest.NewRequest(http.MethodGet, "/", nil)
	rec := httptest.NewRecorder()

	LandingHandler("github", "dev").ServeHTTP(rec, req)

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

func TestLandingHandlerPreloadsSelfServedFontFiles(t *testing.T) {
	// R-XTWR-79P3
	req := httptest.NewRequest(http.MethodGet, "/", nil)
	rec := httptest.NewRecorder()

	LandingHandler("github", "dev").ServeHTTP(rec, req)

	head := htmlHead(t, rec.Body.String())
	for _, font := range []string{"space-grotesk.woff2", "ibm-plex-sans.woff2"} {
		preload := `<link rel="preload" as="font" type="font/woff2" crossorigin href="static/fonts/` + font + `">`
		if !strings.Contains(head, preload) {
			t.Fatalf("landing head missing font preload %q:\n%s", preload, head)
		}

		req := httptest.NewRequest(http.MethodGet, "/static/tokens.css", nil)
		rec := httptest.NewRecorder()
		StaticHandler().ServeHTTP(rec, req)
		if !strings.Contains(rec.Body.String(), `url('fonts/`+font+`')`) {
			t.Fatalf("tokens.css does not use matching self-served URL for %s:\n%s", font, rec.Body.String())
		}
	}
}

func TestLandingHandlerUsesCanonicalServiceLayout(t *testing.T) {
	// R-7NJI-UTHM
	req := httptest.NewRequest(http.MethodGet, "/", nil)
	rec := httptest.NewRecorder()

	LandingHandler("github", "dev").ServeHTTP(rec, req)

	body := rec.Body.String()
	for _, want := range []string{
		`<main>`,
		`<section aria-labelledby="page-title">`,
		`<div class="eyebrow">GitHub connector</div>`,
		`<h1 id="page-title">github</h1>`,
		`Github connects the suite to GitHub through one shared GitHub App and exposes repository, pull request, and issue actions as MCP tools.`,
		`<dl aria-label="Service details">`,
		`<dd><code>POST /mcp</code></dd>`,
		`class="version"`,
	} {
		if !strings.Contains(body, want) {
			t.Fatalf("landing HTML missing %q:\n%s", want, body)
		}
	}
}

func TestLandingHandlerRendersHomeLinkToDashboardApex(t *testing.T) {
	// R-7PZB-MCZ0
	req := httptest.NewRequest(http.MethodGet, "/", nil)
	rec := httptest.NewRecorder()

	LandingHandler("github", "dev").ServeHTTP(rec, req)

	body := rec.Body.String()
	for _, want := range []string{
		`<a class="home" href="/">Home</a>`,
		`.home {`,
		`position: absolute;`,
		`top: var(--space-8);`,
		`position: relative;`,
		`.home:hover,`,
		`.home:focus-visible {`,
		`color: var(--color-text);`,
	} {
		if !strings.Contains(body, want) {
			t.Fatalf("landing HTML missing home link content %q:\n%s", want, body)
		}
	}
}

func TestStaticHandlerServesTokensAndFonts(t *testing.T) {
	// R-EX70-9PAO
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
	// R-XV4N-L1FS
	req := httptest.NewRequest(http.MethodGet, "/static/tokens.css", nil)
	rec := httptest.NewRecorder()

	StaticHandler().ServeHTTP(rec, req)

	body := rec.Body.String()
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

func TestTokensCSSUsesOptionalFontDisplayForEveryFontFace(t *testing.T) {
	// R-XWCJ-YT6H
	req := httptest.NewRequest(http.MethodGet, "/static/tokens.css", nil)
	rec := httptest.NewRecorder()

	StaticHandler().ServeHTTP(rec, req)

	body := rec.Body.String()
	if strings.Contains(body, "font-display: swap") {
		t.Fatalf("tokens.css still contains font-display swap:\n%s", body)
	}
	if faces, optional := strings.Count(body, "@font-face"), strings.Count(body, "font-display: optional"); optional != faces {
		t.Fatalf("font-display optional count = %d, want one for each of %d @font-face blocks:\n%s", optional, faces, body)
	}
}

func TestExactRootRouteDoesNotShadowMCPOrUnknownPaths(t *testing.T) {
	// R-XXKG-CKX6
	mux := http.NewServeMux()
	mux.Handle("GET /{$}", LandingHandler("github", "dev"))
	mux.HandleFunc("GET /health", func(w http.ResponseWriter, r *http.Request) {
		_, _ = io.WriteString(w, "health")
	})
	mux.HandleFunc("GET /.well-known/prm", func(w http.ResponseWriter, r *http.Request) {
		_, _ = io.WriteString(w, "prm")
	})
	mux.HandleFunc("POST /mcp", func(w http.ResponseWriter, r *http.Request) {
		_, _ = io.WriteString(w, "mcp")
	})

	root := httptest.NewRecorder()
	mux.ServeHTTP(root, httptest.NewRequest(http.MethodGet, "/", nil))
	if root.Code != http.StatusOK || !strings.Contains(root.Body.String(), `<h1 id="page-title">github</h1>`) {
		t.Fatalf("root did not dispatch landing handler: status=%d body=%q", root.Code, root.Body.String())
	}

	mcp := httptest.NewRecorder()
	mux.ServeHTTP(mcp, httptest.NewRequest(http.MethodPost, "/mcp", nil))
	if mcp.Code != http.StatusOK || mcp.Body.String() != "mcp" {
		t.Fatalf("POST /mcp = status %d body %q, want stub handler", mcp.Code, mcp.Body.String())
	}
	if strings.Contains(mcp.Body.String(), `<h1 id="page-title">github</h1>`) {
		t.Fatalf("POST /mcp returned landing page: status=%d body=%q", mcp.Code, mcp.Body.String())
	}

	for _, tc := range []struct {
		path string
		body string
	}{
		{path: "/health", body: "health"},
		{path: "/.well-known/prm", body: "prm"},
	} {
		t.Run(tc.path, func(t *testing.T) {
			rec := httptest.NewRecorder()
			mux.ServeHTTP(rec, httptest.NewRequest(http.MethodGet, tc.path, nil))

			if rec.Code != http.StatusOK || rec.Body.String() != tc.body {
				t.Fatalf("GET %s = status %d body %q, want stub handler body %q", tc.path, rec.Code, rec.Body.String(), tc.body)
			}
			if strings.Contains(rec.Body.String(), `<h1 id="page-title">github</h1>`) {
				t.Fatalf("GET %s returned landing page: status=%d body=%q", tc.path, rec.Code, rec.Body.String())
			}
		})
	}

	nope := httptest.NewRecorder()
	mux.ServeHTTP(nope, httptest.NewRequest(http.MethodGet, "/nope", nil))
	if nope.Code == http.StatusOK && strings.Contains(nope.Body.String(), `<h1 id="page-title">github</h1>`) {
		t.Fatalf("GET /nope returned landing page: status=%d body=%q", nope.Code, nope.Body.String())
	}
	if nope.Code != http.StatusNotFound {
		t.Fatalf("GET /nope status = %d, want %d", nope.Code, http.StatusNotFound)
	}
}

func TestCompositionRootMountsLandingUngatedAndKeepsMCPWiring(t *testing.T) {
	// R-XYSC-QCNV
	src, err := os.ReadFile("../githubapp/spec.go")
	if err != nil {
		t.Fatal(err)
	}
	spec := string(src)

	for _, want := range []string{
		`rt.Handle("GET /{$}", web.LandingHandler(rt.Service(), rt.Version()))`,
		`rt.Handle("POST /mcp", rt.RequireIdentity(`,
		`mcp.NewHandler(client, rt.Version(), rt.Service(), health, rt.Logger()))`,
	} {
		if !strings.Contains(spec, want) {
			t.Fatalf("internal/githubapp/spec.go missing %q", want)
		}
	}
	for _, forbidden := range []string{
		`rt.Events()`,
		`rt.Subscriptions()`,
		`Producer`,
	} {
		if strings.Contains(spec, forbidden) {
			t.Fatalf("internal/githubapp/spec.go contains producer wiring %q", forbidden)
		}
	}

	landingLine := lineContaining(t, spec, `web.LandingHandler`)
	if strings.Contains(landingLine, "RequireIdentity") {
		t.Fatalf("landing route is identity-gated: %s", landingLine)
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
