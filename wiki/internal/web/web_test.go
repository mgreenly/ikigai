package web

import (
	"io/fs"
	"net/http"
	"net/http/httptest"
	"strings"
	"testing"
)

func TestLandingHandlerServesExactRootHTML(t *testing.T) {
	// R-LAND-PG01
	req := httptest.NewRequest(http.MethodGet, "/", nil)
	rec := httptest.NewRecorder()

	LandingHandler("wiki", "v-test")(rec, req)

	if rec.Code != http.StatusOK {
		t.Fatalf("status = %d, want 200; body=%s", rec.Code, rec.Body.String())
	}
	if got := rec.Header().Get("Content-Type"); got != "text/html; charset=utf-8" {
		t.Fatalf("Content-Type = %q, want text/html; charset=utf-8", got)
	}
	if !strings.Contains(rec.Body.String(), "<!doctype html>") {
		t.Fatalf("body does not look like HTML: %s", rec.Body.String())
	}
}

func TestLandingHandlerRendersInjectedNameAndVersion(t *testing.T) {
	// R-LAND-NMVR
	const service = "wiki-service"
	const version = "v63-carbon-test"
	req := httptest.NewRequest(http.MethodGet, "/", nil)
	rec := httptest.NewRecorder()

	LandingHandler(service, version)(rec, req)

	body := rec.Body.String()
	if !strings.Contains(body, service) {
		t.Fatalf("body does not contain service %q: %s", service, body)
	}
	if !strings.Contains(body, version) {
		t.Fatalf("body does not contain version %q: %s", version, body)
	}
}

func TestLandingHandlerUsesCanonicalWikiLandingCopy(t *testing.T) {
	req := httptest.NewRequest(http.MethodGet, "/", nil)
	rec := httptest.NewRecorder()

	LandingHandler("wiki", "v-test")(rec, req)

	body := rec.Body.String()
	for _, want := range []string{
		"<title>wiki · wiki</title>",
		`<div class="eyebrow">Knowledge base</div>`,
		"Wiki compiles maintained knowledge into searchable pages and answers grounded in source material.",
		`<dd><code>POST /mcp</code></dd>`,
	} {
		if !strings.Contains(body, want) {
			t.Fatalf("body missing %q: %s", want, body)
		}
	}
}

func TestLandingHandlerRendersDashboardHomeLink(t *testing.T) {
	// R-HOME-3U5Y
	req := httptest.NewRequest(http.MethodGet, "/", nil)
	rec := httptest.NewRecorder()

	LandingHandler("wiki", "v-test")(rec, req)

	body := rec.Body.String()
	if rec.Code != http.StatusOK {
		t.Fatalf("status = %d, want 200; body=%s", rec.Code, body)
	}
	for _, want := range []string{
		`<a class="home" href="/">Home</a>`,
		`.home {`,
		`position: absolute;`,
		`top: var(--space-8);`,
		`position: relative;`,
	} {
		if !strings.Contains(body, want) {
			t.Fatalf("body missing %q: %s", want, body)
		}
	}
}

func TestLandingHandlerUsesEmbeddedCarbonAssets(t *testing.T) {
	// R-LAND-CARB
	if _, err := fs.Stat(assets, "static/tokens.css"); err != nil {
		t.Fatalf("embedded tokens.css missing: %v", err)
	}
	fonts, err := fs.Glob(assets, "static/fonts/*.woff2")
	if err != nil {
		t.Fatalf("glob fonts: %v", err)
	}
	if len(fonts) == 0 {
		t.Fatal("embedded woff2 fonts missing")
	}

	pageReq := httptest.NewRequest(http.MethodGet, "/", nil)
	pageRec := httptest.NewRecorder()
	LandingHandler("wiki", "v-test")(pageRec, pageReq)
	if !strings.Contains(pageRec.Body.String(), `/static/tokens.css`) {
		t.Fatalf("landing page does not reference tokens.css: %s", pageRec.Body.String())
	}

	mux := http.NewServeMux()
	mux.Handle("GET /static/", StaticHandler())
	cssReq := httptest.NewRequest(http.MethodGet, "/static/tokens.css", nil)
	cssRec := httptest.NewRecorder()
	mux.ServeHTTP(cssRec, cssReq)
	if cssRec.Code != http.StatusOK {
		t.Fatalf("tokens.css status = %d, want 200; body=%s", cssRec.Code, cssRec.Body.String())
	}
	if got := cssRec.Header().Get("Content-Type"); !strings.HasPrefix(got, "text/css") {
		t.Fatalf("tokens.css Content-Type = %q, want text/css", got)
	}
	if !strings.Contains(cssRec.Body.String(), "--color-accent") {
		t.Fatalf("tokens.css does not contain Carbon token: %s", cssRec.Body.String())
	}
}

func TestExactRootMuxDoesNotServeLandingForOtherPaths(t *testing.T) {
	// R-LAND-ROOT
	mux := http.NewServeMux()
	mux.HandleFunc("GET /{$}", LandingHandler("wiki", "v-test"))

	for _, path := range []string{"/health", "/mcp", "/nope"} {
		req := httptest.NewRequest(http.MethodGet, path, nil)
		rec := httptest.NewRecorder()
		mux.ServeHTTP(rec, req)
		if rec.Code == http.StatusOK && strings.Contains(rec.Body.String(), "Knowledge base") {
			t.Fatalf("%s received landing page unexpectedly: status=%d body=%s", path, rec.Code, rec.Body.String())
		}
	}
}

func TestLandingHandlerIsAvailableWithoutIdentityHeaders(t *testing.T) {
	// R-LAND-UNGT
	req := httptest.NewRequest(http.MethodGet, "/", nil)
	rec := httptest.NewRecorder()

	LandingHandler("wiki", "v-test")(rec, req)

	if rec.Code != http.StatusOK {
		t.Fatalf("status = %d, want 200 without identity headers; body=%s", rec.Code, rec.Body.String())
	}
	if req.Header.Get("X-Owner-Email") != "" || req.Header.Get("Authorization") != "" {
		t.Fatal("test request unexpectedly carried identity or bearer headers")
	}
}
