package web

import (
	"io"
	"net/http"
	"net/http/httptest"
	"os"
	"path/filepath"
	"runtime"
	"strings"
	"testing"
)

func TestLandingHandlerRendersEmbeddedPage(t *testing.T) {
	req := httptest.NewRequest(http.MethodGet, "/", nil)
	rec := httptest.NewRecorder()

	LandingHandler("cron-test", "v1.2.3").ServeHTTP(rec, req)

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

	LandingHandler("cron", "9.9.9-test").ServeHTTP(rec, req)

	body := rec.Body.String()
	// R-LAND-7G4M
	if !strings.Contains(body, "9.9.9-test") {
		t.Fatalf("landing body does not contain injected version %q:\n%s", "9.9.9-test", body)
	}
}

func TestLandingHandlerSetsHTMLContentType(t *testing.T) {
	req := httptest.NewRequest(http.MethodGet, "/", nil)
	rec := httptest.NewRecorder()

	LandingHandler("cron", "test").ServeHTTP(rec, req)

	// R-LAND-9J6N
	if got := rec.Result().Header.Get("Content-Type"); got != "text/html; charset=utf-8" {
		t.Fatalf("Content-Type = %q, want text/html; charset=utf-8", got)
	}
}

func TestLandingHandlerRendersHomeLinkToApex(t *testing.T) {
	req := httptest.NewRequest(http.MethodGet, "/", nil)
	rec := httptest.NewRecorder()

	LandingHandler("cron", "test").ServeHTTP(rec, req)

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

func TestLandingHandlerRejectsNonRootAndNonGetRequests(t *testing.T) {
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

			LandingHandler("cron", "test").ServeHTTP(rec, req)

			if rec.Result().StatusCode != tc.status {
				t.Fatalf("status = %d, want %d", rec.Result().StatusCode, tc.status)
			}
		})
	}
}

func TestLandingTemplateUsesOnlyEmbeddedLocalAssets(t *testing.T) {
	req := httptest.NewRequest(http.MethodGet, "/", nil)
	rec := httptest.NewRecorder()

	LandingHandler("cron", "test").ServeHTTP(rec, req)

	body := rec.Body.String()
	for _, disallowed := range []string{"https://", "http://", "//fonts.googleapis.com", "dashboard"} {
		// R-ASST-5X9Y
		if strings.Contains(body, disallowed) {
			t.Fatalf("landing body contains runtime external asset reference %q:\n%s", disallowed, body)
		}
	}
	for _, want := range []string{`href="/static/tokens.css"`, "/static/"} {
		// R-ASST-5X9Y
		if !strings.Contains(body, want) {
			t.Fatalf("landing body does not contain %q:\n%s", want, body)
		}
	}
}

func TestServeMuxRootRouteIsExactAndUngated(t *testing.T) {
	mux := http.NewServeMux()
	mux.Handle("GET /{$}", LandingHandler("cron", "route-version"))
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
	_, file, _, ok := runtime.Caller(0)
	if !ok {
		t.Fatal("runtime.Caller failed")
	}
	source, err := os.ReadFile(filepath.Join(filepath.Dir(file), "..", "..", "cmd", "cron", "main.go"))
	if err != nil {
		t.Fatal(err)
	}

	main := string(source)
	want := `rt.Handle("GET /{$}", web.LandingHandler(rt.Service(), rt.Version()))`
	if !strings.Contains(main, want) {
		t.Fatalf("main.go does not mount the landing handler with %q", want)
	}
	if strings.Contains(main, `RequireIdentity(web.LandingHandler`) {
		t.Fatal("landing handler is wrapped with RequireIdentity")
	}
	if !strings.Contains(main, `rt.Handle("POST /mcp", rt.RequireIdentity(`) {
		t.Fatal("main.go no longer gates POST /mcp with RequireIdentity")
	}
}

func TestStaticAssetsServeEmbeddedCarbonFiles(t *testing.T) {
	mux := http.NewServeMux()
	mux.Handle("GET /static/", StaticHandler())

	// R-ASST-3V7W
	css := requestAsset(t, mux, "/static/tokens.css", "text/css; charset=utf-8")
	for _, want := range []string{
		"@font-face",
		"url('/static/fonts/space-grotesk.woff2')",
		"url('/static/fonts/ibm-plex-sans.woff2')",
		"url('/static/fonts/ibm-plex-mono-400.woff2')",
		"url('/static/fonts/ibm-plex-mono-500.woff2')",
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
		body := requestAsset(t, mux, path, "font/woff2")
		if !strings.HasPrefix(body, "wOF2") {
			t.Fatalf("%s is not a woff2 payload", path)
		}
	}
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
