package web

import (
	"io/fs"
	"net/http"
	"net/http/httptest"
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
	if !strings.Contains(pageRec.Body.String(), `/static/tokens.css`) {
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
