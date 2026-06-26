package web

import (
	"net/http"
	"net/http/httptest"
	"strings"
	"testing"
)

func TestLandingHandlerRendersServiceVersionAndContentType(t *testing.T) {
	rec := httptest.NewRecorder()
	LandingHandler("dropbox", "9.9.9-test").ServeHTTP(rec, httptest.NewRequest(http.MethodGet, "/", nil))
	body := rec.Body.String()

	// R-LAND-3C9X
	if rec.Code != http.StatusOK {
		t.Fatalf("status = %d, want %d", rec.Code, http.StatusOK)
	}
	// R-LAND-5E2Y
	if !strings.Contains(body, "dropbox") {
		t.Fatalf("body does not contain service name: %q", body)
	}
	// R-LAND-7G4Z
	if !strings.Contains(body, "9.9.9-test") {
		t.Fatalf("body does not contain version: %q", body)
	}
	// R-LAND-9J6A
	if got := rec.Header().Get("Content-Type"); got != "text/html; charset=utf-8" {
		t.Fatalf("Content-Type = %q, want text/html; charset=utf-8", got)
	}
}

func TestExactRootRouteDispatchesWithoutShadowingSiblings(t *testing.T) {
	mux := http.NewServeMux()
	mux.Handle("GET /{$}", LandingHandler("dropbox", "1.2.3"))
	mux.HandleFunc("POST /mcp", func(w http.ResponseWriter, r *http.Request) {
		w.WriteHeader(http.StatusAccepted)
		_, _ = w.Write([]byte("mcp"))
	})
	mux.HandleFunc("GET /health", func(w http.ResponseWriter, r *http.Request) {
		w.WriteHeader(http.StatusNoContent)
	})

	root := httptest.NewRecorder()
	mux.ServeHTTP(root, httptest.NewRequest(http.MethodGet, "/", nil))
	rootBody := root.Body.String()
	// R-ROUT-2B5C
	if root.Code != http.StatusOK || !strings.Contains(rootBody, "dropbox") || !strings.Contains(rootBody, "1.2.3") {
		t.Fatalf("GET / returned status %d body %q, want landing page", root.Code, rootBody)
	}

	mcp := httptest.NewRecorder()
	mux.ServeHTTP(mcp, httptest.NewRequest(http.MethodPost, "/mcp", nil))
	// R-ROUT-4D7E
	if mcp.Code != http.StatusAccepted || strings.Contains(mcp.Body.String(), "dropbox") {
		t.Fatalf("POST /mcp returned status %d body %q, want sibling handler", mcp.Code, mcp.Body.String())
	}

	health := httptest.NewRecorder()
	mux.ServeHTTP(health, httptest.NewRequest(http.MethodGet, "/health", nil))
	// R-ROUT-6F9G
	if health.Code != http.StatusNoContent || strings.Contains(health.Body.String(), "dropbox") {
		t.Fatalf("GET /health returned status %d body %q, want sibling handler", health.Code, health.Body.String())
	}
}

func TestStaticHandlerServesTokensCSS(t *testing.T) {
	mux := http.NewServeMux()
	mux.Handle("GET /static/", StaticHandler())

	rec := httptest.NewRecorder()
	mux.ServeHTTP(rec, httptest.NewRequest(http.MethodGet, "/static/tokens.css", nil))
	body := rec.Body.String()

	// R-ASST-3H6J
	if rec.Code != http.StatusOK || rec.Header().Get("Content-Type") != "text/css; charset=utf-8" {
		t.Fatalf("GET /static/tokens.css returned status %d Content-Type %q", rec.Code, rec.Header().Get("Content-Type"))
	}
	if !strings.Contains(body, `url('/static/fonts/space-grotesk.woff2')`) {
		t.Fatalf("tokens.css does not point at embedded service font path: %q", body)
	}
	for _, want := range []string{
		"--font-display: 'Space Grotesk'",
		"--font-body:    'IBM Plex Sans'",
		"--font-mono:    'IBM Plex Mono'",
		"--text-display-size:   56px;",
		"--text-label-size:     12px;",
		"--color-surface:",
	} {
		if !strings.Contains(body, want) {
			t.Fatalf("tokens.css missing Carbon landing token %q in: %q", want, body)
		}
	}
}

func TestLandingHTMLReferencesOwnEmbeddedStaticPath(t *testing.T) {
	rec := httptest.NewRecorder()
	LandingHandler("dropbox", "asset-test").ServeHTTP(rec, httptest.NewRequest(http.MethodGet, "/", nil))
	body := rec.Body.String()

	// R-ASST-5K8L
	if !strings.Contains(body, `/static/tokens.css`) {
		t.Fatalf("landing HTML does not reference embedded static path: %q", body)
	}
	if strings.Contains(body, "/srv/") || strings.Contains(body, "dashboard") || strings.Contains(body, "://") {
		t.Fatalf("landing HTML references a cross-service or remote asset URL: %q", body)
	}
}

func TestStaticHandlerServesEmbeddedFonts(t *testing.T) {
	mux := http.NewServeMux()
	mux.Handle("GET /static/", StaticHandler())

	// R-ASST-7M1N
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
