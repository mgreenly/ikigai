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
	// R-LAND-7Q3D
	// R-LAND-3T9H
	// R-LAND-9R5F
	// R-LAND-1S7G
	req := httptest.NewRequest(http.MethodGet, "/", nil)
	rec := httptest.NewRecorder()

	LandingHandler("scripts-test", "v9.8.7").ServeHTTP(rec, req)

	if rec.Code != http.StatusOK {
		t.Fatalf("status = %d, want %d", rec.Code, http.StatusOK)
	}
	if got := rec.Header().Get("Content-Type"); got != "text/html; charset=utf-8" {
		t.Fatalf("Content-Type = %q, want text/html; charset=utf-8", got)
	}

	body := rec.Body.String()
	if count := strings.Count(body, "scripts-test"); count != 3 {
		t.Fatalf("service name count = %d, want 3 in title, heading, and service detail\n%s", count, body)
	}
	if count := strings.Count(body, "v9.8.7"); count != 1 {
		t.Fatalf("version count = %d, want 1\n%s", count, body)
	}

	other := httptest.NewRecorder()
	LandingHandler("other-service", "build-123").ServeHTTP(other, req)
	normalizedBody := strings.ReplaceAll(strings.ReplaceAll(body, "scripts-test", "{{service}}"), "v9.8.7", "{{version}}")
	normalizedOther := strings.ReplaceAll(strings.ReplaceAll(other.Body.String(), "other-service", "{{service}}"), "build-123", "{{version}}")
	if normalizedBody != normalizedOther {
		t.Fatalf("landing HTML changed beyond service/version substitutions:\n%s\n---\n%s", normalizedBody, normalizedOther)
	}
}

func TestLandingHandlerLinksOnlyAppLocalStaticAssets(t *testing.T) {
	// R-ASST-7Y1N
	req := httptest.NewRequest(http.MethodGet, "/", nil)
	rec := httptest.NewRecorder()

	LandingHandler("scripts", "dev").ServeHTTP(rec, req)

	body := rec.Body.String()
	if !strings.Contains(body, `href="/static/tokens.css"`) {
		t.Fatalf("landing HTML did not link local tokens.css:\n%s", body)
	}
	for _, forbidden := range []string{"dashboard", "/srv/dashboard", "https://", "http://"} {
		if strings.Contains(body, forbidden) {
			t.Fatalf("landing HTML contains forbidden cross-service asset reference %q:\n%s", forbidden, body)
		}
	}
}

func TestLandingHandlerUsesCronCanonicalStructureForScripts(t *testing.T) {
	req := httptest.NewRequest(http.MethodGet, "/", nil)
	rec := httptest.NewRecorder()

	LandingHandler("scripts", "dev").ServeHTTP(rec, req)

	body := rec.Body.String()
	for _, want := range []string{
		`<title>scripts · scripts</title>`,
		`<link rel="stylesheet" href="/static/tokens.css">`,
		`<a class="home" href="/">Home</a>`,
		`<section aria-labelledby="page-title">`,
		`<div class="eyebrow">Script runner</div>`,
		`<h1 id="page-title">scripts</h1>`,
		`Scripts runs deterministic Python scripts wired to suite events and publishes completion events back to the event plane.`,
		`<dl aria-label="Service details">`,
		`<dt>API</dt>`,
		`<dd><code>POST /mcp</code></dd>`,
		`class="version"`,
	} {
		if !strings.Contains(body, want) {
			t.Fatalf("landing HTML missing %q:\n%s", want, body)
		}
	}
	for _, forbidden := range []string{`class="shell"`, `class="card"`, `Scheduled event emitter`, `minute boundaries`} {
		if strings.Contains(body, forbidden) {
			t.Fatalf("landing HTML contains stale markup %q:\n%s", forbidden, body)
		}
	}
}

func TestStaticHandlerServesTokensAndFonts(t *testing.T) {
	// R-ASST-5X8M
	// R-ASST-9Z3P
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
	req := httptest.NewRequest(http.MethodGet, "/static/tokens.css", nil)
	rec := httptest.NewRecorder()

	StaticHandler().ServeHTTP(rec, req)

	body := rec.Body.String()
	for _, want := range []string{
		`@font-face`,
		`url('/static/fonts/space-grotesk.woff2')`,
		`url('/static/fonts/ibm-plex-sans.woff2')`,
		`url('/static/fonts/ibm-plex-mono-400.woff2')`,
		`url('/static/fonts/ibm-plex-mono-500.woff2')`,
		`font-family: 'Space Grotesk'`,
		`font-family: 'IBM Plex Mono'`,
	} {
		if !strings.Contains(body, want) {
			t.Fatalf("tokens.css missing %q:\n%s", want, body)
		}
	}
}

func TestExactRootRouteDoesNotShadowExistingPaths(t *testing.T) {
	// R-ROUT-8U2J
	// R-ROUT-1V4K
	// R-ROUT-3W6L
	mux := http.NewServeMux()
	mux.Handle("GET /{$}", LandingHandler("scripts", "dev"))
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
	if root.Code != http.StatusOK || !strings.Contains(root.Body.String(), `<h1 id="page-title">scripts</h1>`) {
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
			if strings.Contains(rec.Body.String(), `<h1 id="page-title">scripts</h1>`) {
				t.Fatalf("GET %s returned landing page: status=%d body=%q", tc.path, rec.Code, rec.Body.String())
			}
		})
	}

	nope := httptest.NewRecorder()
	mux.ServeHTTP(nope, httptest.NewRequest(http.MethodGet, "/nope", nil))
	if nope.Code == http.StatusOK && strings.Contains(nope.Body.String(), `<h1 id="page-title">scripts</h1>`) {
		t.Fatalf("GET /nope returned landing page: status=%d body=%q", nope.Code, nope.Body.String())
	}
	if nope.Code != http.StatusNotFound {
		t.Fatalf("GET /nope status = %d, want %d", nope.Code, http.StatusNotFound)
	}
}

func TestCompositionRootMountsLandingUngatedAndKeepsMCPWiring(t *testing.T) {
	src, err := os.ReadFile("../../cmd/scripts/main.go")
	if err != nil {
		t.Fatal(err)
	}
	main := string(src)

	for _, want := range []string{
		`rt.Handle("GET /{$}", web.LandingHandler(rt.Service(), rt.Version()))`,
		`rt.Handle("GET /static/", web.StaticHandler())`,
		`rt.Handle("POST /mcp", rt.RequireIdentity(`,
		`mcp.NewHandler(svc, rt.Version(), rt.Service(), rt.Health())`,
	} {
		if !strings.Contains(main, want) {
			t.Fatalf("cmd/scripts/main.go missing %q", want)
		}
	}

	landingLine := lineContaining(t, main, `web.LandingHandler`)
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
