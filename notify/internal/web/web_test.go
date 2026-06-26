package web

import (
	"io/fs"
	"net/http"
	"net/http/httptest"
	"strings"
	"testing"
)

func TestLandingRootReturns200(t *testing.T) {
	// R-LAND-3C8K — construct web.LandingHandler, drive GET / via httptest, assert 200.
	rec := httptest.NewRecorder()
	LandingHandler("notify", "v1.2.3").ServeHTTP(rec, httptest.NewRequest(http.MethodGet, "/", nil))

	if rec.Code != http.StatusOK {
		t.Fatalf("status = %d, want %d", rec.Code, http.StatusOK)
	}
}

func TestLandingBodyContainsServiceName(t *testing.T) {
	// R-LAND-5D1M — rendered body contains the service name passed in.
	rec := httptest.NewRecorder()
	LandingHandler("notify", "v1.2.3").ServeHTTP(rec, httptest.NewRequest(http.MethodGet, "/", nil))

	if !strings.Contains(rec.Body.String(), "notify") {
		t.Fatalf("body does not render service name: %s", rec.Body.String())
	}
}

func TestLandingBodyContainsInjectedVersion(t *testing.T) {
	// R-LAND-7E4N — a distinctive injected version appears verbatim (not a const).
	rec := httptest.NewRecorder()
	LandingHandler("notify", "9.9.9-test").ServeHTTP(rec, httptest.NewRequest(http.MethodGet, "/", nil))

	if !strings.Contains(rec.Body.String(), "9.9.9-test") {
		t.Fatalf("body does not render injected version verbatim: %s", rec.Body.String())
	}
}

func TestLandingContentTypeIsHTML(t *testing.T) {
	// R-LAND-9F6P — landing response Content-Type is text/html; charset=utf-8.
	rec := httptest.NewRecorder()
	LandingHandler("notify", "v1.2.3").ServeHTTP(rec, httptest.NewRequest(http.MethodGet, "/", nil))

	if got := rec.Header().Get("Content-Type"); got != "text/html; charset=utf-8" {
		t.Fatalf("content type = %q, want text/html; charset=utf-8", got)
	}
}

// composedMux mirrors how cmd/notify/main.go wires the landing handler beside
// the MCP mount: exact-match GET /{$} for the root, POST /mcp for the API.
func composedMux(mcp http.Handler) *http.ServeMux {
	mux := http.NewServeMux()
	mux.Handle("GET /{$}", LandingHandler("notify", "9.9.9-test"))
	mux.Handle("POST /mcp", mcp)
	return mux
}

func TestRouteExactRootDispatchesToLanding(t *testing.T) {
	// R-ROUT-4G2Q — registered at GET /{$} on a ServeMux, GET / dispatches to landing.
	rec := httptest.NewRecorder()
	composedMux(http.NotFoundHandler()).ServeHTTP(rec, httptest.NewRequest(http.MethodGet, "/", nil))

	if rec.Code != http.StatusOK {
		t.Fatalf("status = %d, want %d", rec.Code, http.StatusOK)
	}
	body := rec.Body.String()
	if !strings.Contains(body, "notify") || !strings.Contains(body, "9.9.9-test") {
		t.Fatalf("GET / did not reach landing handler: %s", body)
	}
}

func TestRouteRootDoesNotShadowMCP(t *testing.T) {
	// R-ROUT-6H5R — {$} does not shadow a sibling POST /mcp; the stub is reached.
	stub := http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		w.WriteHeader(http.StatusTeapot)
		_, _ = w.Write([]byte("mcp-stub"))
	})

	rec := httptest.NewRecorder()
	composedMux(stub).ServeHTTP(rec, httptest.NewRequest(http.MethodPost, "/mcp", nil))

	if rec.Code != http.StatusTeapot || rec.Body.String() != "mcp-stub" {
		t.Fatalf("POST /mcp did not reach stub: code=%d body=%q", rec.Code, rec.Body.String())
	}
	if strings.Contains(rec.Body.String(), "notify") {
		t.Fatalf("POST /mcp was shadowed by the landing page")
	}
}

func TestRouteNonRootNotCapturedByExactMatch(t *testing.T) {
	// R-ROUT-8J7S — an unregistered non-root path is not captured by {$} (exact-match, not subtree).
	rec := httptest.NewRecorder()
	composedMux(http.NotFoundHandler()).ServeHTTP(rec, httptest.NewRequest(http.MethodGet, "/nope", nil))

	if rec.Code != http.StatusNotFound {
		t.Fatalf("GET /nope status = %d, want %d", rec.Code, http.StatusNotFound)
	}
	if strings.Contains(rec.Body.String(), "notify") {
		t.Fatalf("GET /nope returned the landing page — {$} matched as a subtree")
	}
}

func TestStaticTokensCSSServedWithCSSContentType(t *testing.T) {
	// R-ASST-3K9T — GET /static/tokens.css through the handler is 200 with a CSS content type.
	rec := httptest.NewRecorder()
	StaticHandler().ServeHTTP(rec, httptest.NewRequest(http.MethodGet, "/static/tokens.css", nil))

	if rec.Code != http.StatusOK {
		t.Fatalf("status = %d, want %d", rec.Code, http.StatusOK)
	}
	if got := rec.Header().Get("Content-Type"); !strings.Contains(got, "text/css") {
		t.Fatalf("content type = %q, want text/css", got)
	}
}

func TestLandingReferencesOnlyLocalStaticAssets(t *testing.T) {
	// R-ASST-5L2V — rendered HTML references notify's own /static/ asset path and no cross-service/dashboard URL.
	rec := httptest.NewRecorder()
	LandingHandler("notify", "v1.2.3").ServeHTTP(rec, httptest.NewRequest(http.MethodGet, "/", nil))

	body := rec.Body.String()
	if !strings.Contains(body, "/static/tokens.css") {
		t.Fatalf("landing body does not reference its own /static/ asset path: %s", body)
	}
	for _, forbidden := range []string{"http://", "https://", "dashboard"} {
		if strings.Contains(body, forbidden) {
			t.Fatalf("landing body references a cross-service/external asset URL %q: %s", forbidden, body)
		}
	}
}

func TestStaticFontServedWithWoff2ContentType(t *testing.T) {
	// R-ASST-7M4W — GET /static/fonts/space-grotesk.woff2 is 200 with a font/woff2 content type.
	rec := httptest.NewRecorder()
	StaticHandler().ServeHTTP(rec, httptest.NewRequest(http.MethodGet, "/static/fonts/space-grotesk.woff2", nil))

	if rec.Code != http.StatusOK {
		t.Fatalf("status = %d, want %d", rec.Code, http.StatusOK)
	}
	if got := rec.Header().Get("Content-Type"); got != "font/woff2" {
		t.Fatalf("content type = %q, want font/woff2", got)
	}
}

// --- supporting (untagged) coverage of the implementation's contracts ---

func TestLandingEscapesInjectedStrings(t *testing.T) {
	rec := httptest.NewRecorder()
	LandingHandler(`<script>alert("x")</script>`, `<b>v</b>`).ServeHTTP(rec, httptest.NewRequest(http.MethodGet, "/", nil))

	body := rec.Body.String()
	if strings.Contains(body, `<script>alert("x")</script>`) || strings.Contains(body, `<b>v</b>`) {
		t.Fatalf("landing body rendered unescaped input: %s", body)
	}
}

func TestLandingOnlyServesExactRoot(t *testing.T) {
	handler := LandingHandler("notify", "dev")

	rec := httptest.NewRecorder()
	handler.ServeHTTP(rec, httptest.NewRequest(http.MethodGet, "/mcp", nil))
	if rec.Code != http.StatusNotFound {
		t.Fatalf("/mcp status = %d, want %d", rec.Code, http.StatusNotFound)
	}

	rec = httptest.NewRecorder()
	handler.ServeHTTP(rec, httptest.NewRequest(http.MethodPost, "/", nil))
	if rec.Code != http.StatusMethodNotAllowed {
		t.Fatalf("POST / status = %d, want %d", rec.Code, http.StatusMethodNotAllowed)
	}
}

func TestStaticHandlerRejectsNonStaticPathsAndMethods(t *testing.T) {
	handler := StaticHandler()

	rec := httptest.NewRecorder()
	handler.ServeHTTP(rec, httptest.NewRequest(http.MethodGet, "/mcp", nil))
	if rec.Code != http.StatusNotFound {
		t.Fatalf("/mcp status = %d, want %d", rec.Code, http.StatusNotFound)
	}

	rec = httptest.NewRecorder()
	handler.ServeHTTP(rec, httptest.NewRequest(http.MethodPost, "/static/tokens.css", nil))
	if rec.Code != http.StatusMethodNotAllowed {
		t.Fatalf("POST /static/tokens.css status = %d, want %d", rec.Code, http.StatusMethodNotAllowed)
	}
}

func TestTokensCSSUsesEmbeddedNotifyFontURLs(t *testing.T) {
	b, err := staticFiles.ReadFile("static/tokens.css")
	if err != nil {
		t.Fatal(err)
	}
	css := string(b)
	for _, font := range []string{
		"space-grotesk.woff2",
		"ibm-plex-sans.woff2",
		"ibm-plex-mono-400.woff2",
		"ibm-plex-mono-500.woff2",
	} {
		want := "/srv/notify/static/fonts/" + font
		if !strings.Contains(css, want) {
			t.Fatalf("tokens.css missing embedded font URL %q", want)
		}
	}
}

func TestEmbeddedStaticAssetsContainVendoredFonts(t *testing.T) {
	for _, path := range []string{
		"static/fonts/space-grotesk.woff2",
		"static/fonts/ibm-plex-sans.woff2",
		"static/fonts/ibm-plex-mono-400.woff2",
		"static/fonts/ibm-plex-mono-500.woff2",
	} {
		t.Run(path, func(t *testing.T) {
			info, err := fs.Stat(staticFiles, path)
			if err != nil {
				t.Fatal(err)
			}
			if info.Size() == 0 {
				t.Fatalf("%s is empty", path)
			}
		})
	}
}

func TestEmbeddedAssetsDoNotReferenceExternalRuntimeOrigins(t *testing.T) {
	for _, path := range []string{"landing.html", "static/tokens.css"} {
		t.Run(path, func(t *testing.T) {
			var (
				b   []byte
				err error
			)
			if path == "landing.html" {
				b, err = templateFiles.ReadFile(path)
			} else {
				b, err = staticFiles.ReadFile(path)
			}
			if err != nil {
				t.Fatal(err)
			}
			body := string(b)
			for _, forbidden := range []string{"http://", "https://", "dashboard", "fonts.googleapis.com", "fonts.gstatic.com"} {
				if strings.Contains(body, forbidden) {
					t.Fatalf("%s references forbidden runtime origin %q", path, forbidden)
				}
			}
		})
	}
}
