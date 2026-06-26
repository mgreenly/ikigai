package web

import (
	"io"
	"net/http"
	"net/http/httptest"
	"strings"
	"testing"
)

func TestLandingHandlerReturnsHTMLPage(t *testing.T) {
	res := httptest.NewRecorder()
	LandingHandler("ledger", "1.2.3").ServeHTTP(res, httptest.NewRequest(http.MethodGet, "/", nil))

	// R-LAND-3C9D
	if res.Code != http.StatusOK {
		t.Fatalf("status = %d, want %d", res.Code, http.StatusOK)
	}
	if got := res.Header().Get("Content-Type"); got != "text/html; charset=utf-8" {
		t.Fatalf("content-type = %q, want text/html; charset=utf-8", got)
	}
}

func TestLandingHandlerEscapesServiceAndVersion(t *testing.T) {
	res := httptest.NewRecorder()
	LandingHandler(`ledger <service>`, `v1&2`).ServeHTTP(res, httptest.NewRequest(http.MethodGet, "/", nil))
	body := res.Body.String()

	// R-LAND-5E1F
	if !strings.Contains(body, "ledger &lt;service&gt;") {
		t.Fatalf("rendered body did not contain escaped service name: %s", body)
	}
	if !strings.Contains(body, ">v1&amp;2<") {
		t.Fatalf("rendered body did not contain escaped version: %s", body)
	}
}

func TestLandingMarkupLinksEmbeddedTokens(t *testing.T) {
	res := httptest.NewRecorder()
	LandingHandler("ledger", "1.2.3").ServeHTTP(res, httptest.NewRequest(http.MethodGet, "/", nil))
	body := res.Body.String()

	// R-LAND-7G2H
	if !strings.Contains(body, `href="/static/tokens.css"`) {
		t.Fatalf("landing markup does not link embedded tokens.css: %s", body)
	}
	if strings.Contains(body, "fonts.googleapis.com") || strings.Contains(body, "dashboard") {
		t.Fatalf("landing markup contains external or dashboard asset reference: %s", body)
	}
}

func TestLandingMarkupIncludesHomeLink(t *testing.T) {
	res := httptest.NewRecorder()
	LandingHandler("ledger", "1.2.3").ServeHTTP(res, httptest.NewRequest(http.MethodGet, "/", nil))
	body := res.Body.String()

	// R-HOME-4M6R
	for _, want := range []string{
		`<a class="home" href="/">Home</a>`,
		".home {",
		"position: absolute",
		"top: var(--space-8)",
		"position: relative",
		".home:hover,",
		".home:focus-visible",
	} {
		if !strings.Contains(body, want) {
			t.Fatalf("landing markup missing Home link content %q in body: %s", want, body)
		}
	}
	if strings.Contains(body, ">Dashboard</a>") {
		t.Fatalf("landing markup used Dashboard link text instead of Home: %s", body)
	}
}

func TestLandingMarkupAppliesCarbonTypeScale(t *testing.T) {
	res := httptest.NewRecorder()
	LandingHandler("ledger", "1.2.3").ServeHTTP(res, httptest.NewRequest(http.MethodGet, "/", nil))
	body := res.Body.String()

	// R-LAND-9J4K
	for _, want := range []string{
		"width: min(100% - 32px, 960px)",
		"font-family: var(--font-display)",
		"font-size: clamp(40px, 8vw, var(--text-display-size))",
		"line-height: var(--text-display-lh)",
		"font-family: var(--font-mono)",
		"font-size: var(--text-label-size)",
		"<code>POST /mcp</code>",
	} {
		if !strings.Contains(body, want) {
			t.Fatalf("landing markup missing Carbon styling %q in body: %s", want, body)
		}
	}
}

func TestLandingHandlerIsExactRoot(t *testing.T) {
	handler := LandingHandler("ledger", "1.2.3")
	for _, path := range []string{"/mcp", "/health", "/feed", "/.well-known/prm"} {
		res := httptest.NewRecorder()
		handler.ServeHTTP(res, httptest.NewRequest(http.MethodGet, path, nil))

		// R-ROUT-2M6N
		if res.Code != http.StatusNotFound {
			t.Fatalf("GET %s status = %d, want %d", path, res.Code, http.StatusNotFound)
		}
	}
}

func TestLandingHandlerServesStaticAssets(t *testing.T) {
	handler := LandingHandler("ledger", "1.2.3")

	res := httptest.NewRecorder()
	handler.ServeHTTP(res, httptest.NewRequest(http.MethodGet, "/static/tokens.css", nil))

	// R-ROUT-4P8Q
	if res.Code != http.StatusOK {
		t.Fatalf("tokens.css status = %d, want %d", res.Code, http.StatusOK)
	}
	if got := res.Header().Get("Content-Type"); got != "text/css; charset=utf-8" {
		t.Fatalf("tokens.css content-type = %q, want text/css; charset=utf-8", got)
	}
}

func TestLandingHandlerKeepsStaticUnderStaticPath(t *testing.T) {
	handler := LandingHandler("ledger", "1.2.3")
	for _, path := range []string{"/tokens.css", "/srv/ledger/static/tokens.css", "/static/missing.css"} {
		res := httptest.NewRecorder()
		handler.ServeHTTP(res, httptest.NewRequest(http.MethodGet, path, nil))

		// R-ROUT-6R1S
		if res.Code != http.StatusNotFound {
			t.Fatalf("GET %s status = %d, want %d", path, res.Code, http.StatusNotFound)
		}
	}
}

func TestTokensCSSDefinesSelfHostedFonts(t *testing.T) {
	res := httptest.NewRecorder()
	LandingHandler("ledger", "1.2.3").ServeHTTP(res, httptest.NewRequest(http.MethodGet, "/static/tokens.css", nil))
	body := res.Body.String()

	// R-ASST-3T7V
	for _, want := range []string{
		"@font-face",
		"font-family: 'Space Grotesk'",
		"font-family: 'IBM Plex Sans'",
		"font-family: 'IBM Plex Mono'",
		`url('/static/fonts/space-grotesk.woff2')`,
		`url('/static/fonts/ibm-plex-mono-500.woff2')`,
	} {
		if !strings.Contains(body, want) {
			t.Fatalf("tokens.css missing %q", want)
		}
	}
	if strings.Contains(body, "@import") || strings.Contains(body, "fonts.googleapis.com") {
		t.Fatalf("tokens.css contains external font loading: %s", body)
	}
}

func TestEmbeddedFontsAreRealWoff2Bytes(t *testing.T) {
	handler := LandingHandler("ledger", "1.2.3")
	for _, path := range []string{
		"/static/fonts/space-grotesk.woff2",
		"/static/fonts/ibm-plex-sans.woff2",
		"/static/fonts/ibm-plex-mono-400.woff2",
		"/static/fonts/ibm-plex-mono-500.woff2",
	} {
		res := httptest.NewRecorder()
		handler.ServeHTTP(res, httptest.NewRequest(http.MethodGet, path, nil))
		body, err := io.ReadAll(res.Result().Body)
		if err != nil {
			t.Fatalf("read %s body: %v", path, err)
		}

		// R-ASST-5W9X
		if res.Code != http.StatusOK {
			t.Fatalf("GET %s status = %d, want %d", path, res.Code, http.StatusOK)
		}
		if got := res.Header().Get("Content-Type"); got != "font/woff2" {
			t.Fatalf("GET %s content-type = %q, want font/woff2", path, got)
		}
		if len(body) < 1024 {
			t.Fatalf("GET %s body length = %d, want real font bytes", path, len(body))
		}
	}
}

func TestTokensCSSContainsCarbonNeutralPalette(t *testing.T) {
	res := httptest.NewRecorder()
	LandingHandler("ledger", "1.2.3").ServeHTTP(res, httptest.NewRequest(http.MethodGet, "/static/tokens.css", nil))
	body := res.Body.String()

	// R-ASST-7Y2Z
	for _, want := range []string{
		"Carbon — Design Tokens",
		"--layout-max-width: 1120px",
		"--text-display-size:   56px",
		"--text-label-size:     12px",
		"--color-bg:            #FFFFFF",
		"--color-text:          #09090B",
	} {
		if !strings.Contains(body, want) {
			t.Fatalf("tokens.css missing Carbon token %q", want)
		}
	}
}
