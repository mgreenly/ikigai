package web

import (
	"io/fs"
	"net/http"
	"net/http/httptest"
	"strings"
	"testing"
)

func TestLandingHandlerRendersServiceVersionAndHTML(t *testing.T) {
	// R-LAND-3C8K
	req := httptest.NewRequest(http.MethodGet, "/", nil)
	rec := httptest.NewRecorder()

	LandingHandler("notify-test", "v1.2.3").ServeHTTP(rec, req)

	if rec.Code != http.StatusOK {
		t.Fatalf("status = %d, want %d", rec.Code, http.StatusOK)
	}
	if got := rec.Header().Get("Content-Type"); got != "text/html; charset=utf-8" {
		t.Fatalf("content type = %q, want text/html; charset=utf-8", got)
	}
	body := rec.Body.String()
	if !strings.Contains(body, "<h1>notify-test</h1>") {
		t.Fatalf("body does not render service name: %s", body)
	}
	if !strings.Contains(body, "<dd>v1.2.3</dd>") {
		t.Fatalf("body does not render version: %s", body)
	}
}

func TestLandingHandlerEscapesInjectedStrings(t *testing.T) {
	// R-LAND-5D1M
	req := httptest.NewRequest(http.MethodGet, "/", nil)
	rec := httptest.NewRecorder()

	LandingHandler(`<script>alert("service")</script>`, `<b>version</b>`).ServeHTTP(rec, req)

	body := rec.Body.String()
	if strings.Contains(body, `<script>alert("service")</script>`) || strings.Contains(body, `<b>version</b>`) {
		t.Fatalf("landing body rendered unescaped input: %s", body)
	}
	if !strings.Contains(body, "&lt;script&gt;") || !strings.Contains(body, "&lt;b&gt;version&lt;/b&gt;") {
		t.Fatalf("landing body did not contain escaped input: %s", body)
	}
}

func TestLandingHandlerLinksEmbeddedNotifyTokens(t *testing.T) {
	// R-LAND-7E4N
	req := httptest.NewRequest(http.MethodGet, "/", nil)
	rec := httptest.NewRecorder()

	LandingHandler("notify", "dev").ServeHTTP(rec, req)

	body := rec.Body.String()
	if !strings.Contains(body, `href="/srv/notify/static/tokens.css"`) {
		t.Fatalf("landing body does not link notify tokens.css: %s", body)
	}
	if strings.Contains(body, "dashboard") || strings.Contains(body, "http://") || strings.Contains(body, "https://") {
		t.Fatalf("landing body references a non-embedded asset: %s", body)
	}
}

func TestLandingHandlerOnlyServesExactRoot(t *testing.T) {
	// R-LAND-9F6P
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
	if got := rec.Header().Get("Allow"); got != http.MethodGet {
		t.Fatalf("Allow = %q, want %q", got, http.MethodGet)
	}
}

func TestStaticHandlerServesTokensCSS(t *testing.T) {
	// R-ROUT-4G2Q
	req := httptest.NewRequest(http.MethodGet, "/static/tokens.css", nil)
	rec := httptest.NewRecorder()

	StaticHandler().ServeHTTP(rec, req)

	if rec.Code != http.StatusOK {
		t.Fatalf("status = %d, want %d", rec.Code, http.StatusOK)
	}
	if got := rec.Header().Get("Content-Type"); got != "text/css; charset=utf-8" {
		t.Fatalf("content type = %q, want text/css; charset=utf-8", got)
	}
	if !strings.Contains(rec.Body.String(), "Carbon") {
		t.Fatalf("tokens.css body does not look like vendored Carbon tokens")
	}
}

func TestStaticHandlerServesFontsWithWoff2ContentType(t *testing.T) {
	// R-ROUT-6H5R
	for _, path := range []string{
		"/static/fonts/space-grotesk.woff2",
		"/static/fonts/ibm-plex-sans.woff2",
		"/static/fonts/ibm-plex-mono-400.woff2",
		"/static/fonts/ibm-plex-mono-500.woff2",
	} {
		t.Run(path, func(t *testing.T) {
			rec := httptest.NewRecorder()
			StaticHandler().ServeHTTP(rec, httptest.NewRequest(http.MethodGet, path, nil))

			if rec.Code != http.StatusOK {
				t.Fatalf("status = %d, want %d", rec.Code, http.StatusOK)
			}
			if got := rec.Header().Get("Content-Type"); got != "font/woff2" {
				t.Fatalf("content type = %q, want font/woff2", got)
			}
			if rec.Body.Len() == 0 {
				t.Fatal("font response body is empty")
			}
		})
	}
}

func TestStaticHandlerRejectsNonStaticPathsAndMethods(t *testing.T) {
	// R-ROUT-8J7S
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
	// R-ASST-3K9T
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
		want := `/srv/notify/static/fonts/` + font
		if !strings.Contains(css, want) {
			t.Fatalf("tokens.css missing embedded font URL %q", want)
		}
	}
}

func TestEmbeddedStaticAssetsContainVendoredFonts(t *testing.T) {
	// R-ASST-5L2V
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
	// R-ASST-7M4W
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
