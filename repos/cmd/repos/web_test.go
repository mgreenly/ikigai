package main

import (
	"bytes"
	"crypto/sha256"
	"fmt"
	"net/http"
	"net/http/httptest"
	"os"
	"path/filepath"
	"strings"
	"testing"

	appweb "appkit/web"
)

func TestAssembledRootRendersCanonicalLanding(t *testing.T) {
	// R-G2WB-O22X
	site := loadReposWWW(t)
	mux := http.NewServeMux()
	mux.Handle("GET /{$}", landingHandler(site, "repos", "v9.8.7-test"))

	response := httptest.NewRecorder()
	mux.ServeHTTP(response, httptest.NewRequest(http.MethodGet, "/", nil))
	if response.Code != http.StatusOK {
		t.Fatalf("GET / status = %d, want %d", response.Code, http.StatusOK)
	}
	if got := response.Header().Get("Content-Type"); got != "text/html; charset=utf-8" {
		t.Fatalf("GET / Content-Type = %q, want text/html; charset=utf-8", got)
	}
	body := response.Body.String()
	for _, want := range []string{"<title>repos · repos</title>", `<h1 id="page-title">repos</h1>`, "<dd>repos</dd>"} {
		if !strings.Contains(body, want) {
			t.Fatalf("rendered landing is missing service name in %q:\n%s", want, body)
		}
	}
	if got := strings.Count(body, "v9.8.7-test"); got != 1 {
		t.Fatalf("rendered version %d times, want once:\n%s", got, body)
	}
	for _, want := range []string{
		`<a class="home" href="/">Home</a>`,
		`<section aria-labelledby="page-title">`,
		`<div class="eyebrow">Repository session runner</div>`,
		`Repos clones repositories, runs agent sessions, and publishes typed session events to the event plane.`,
		`<dl aria-label="Service details">`,
		`<dd><code>POST /mcp</code></dd>`,
		`href="static/tokens.css"`,
		`href="static/fonts/space-grotesk.woff2"`,
		`href="static/fonts/ibm-plex-sans.woff2"`,
	} {
		if !strings.Contains(body, want) {
			t.Fatalf("canonical landing is missing %q:\n%s", want, body)
		}
	}
	for _, forbidden := range []string{`href="/static/`, "https://", "http://", "dashboard"} {
		if strings.Contains(body, forbidden) {
			t.Fatalf("landing contains non-local asset reference %q:\n%s", forbidden, body)
		}
	}

	templateBytes, err := os.ReadFile(filepath.Join("..", "..", "share", "www", "landing.html"))
	if err != nil {
		t.Fatal(err)
	}
	normalized := strings.NewReplacer(
		"{{.Service}} · repos", "{{.Service}} · SERVICE",
		"Repository session runner", "SERVICE EYEBROW",
		"Repos clones repositories, runs agent sessions, and publishes typed session events to the event plane.", "SERVICE LEAD",
		"POST /mcp", "SERVICE API",
	).Replace(string(templateBytes))
	const canonicalLayoutSHA256 = "ee3dcc4bf330be83f6e9ebbee3055f4657d62080c386b9816ee9c5ffb37bbb54"
	if got := fmt.Sprintf("%x", sha256.Sum256([]byte(normalized))); got != canonicalLayoutSHA256 {
		t.Fatalf("landing layout differs from the canonical Carbon template: sha256=%s", got)
	}
}

func TestChassisStaticServesVendoredTokensAndFont(t *testing.T) {
	// R-G448-1TTM
	mux := http.NewServeMux()
	mux.Handle("GET /static/", loadReposWWW(t).Static())
	for _, test := range []struct {
		path        string
		contentType string
		prefix      []byte
	}{
		{path: "/static/tokens.css", contentType: "text/css; charset=utf-8", prefix: []byte("/*")},
		{path: "/static/fonts/space-grotesk.woff2", contentType: "font/woff2", prefix: []byte("wOF2")},
	} {
		t.Run(test.path, func(t *testing.T) {
			response := httptest.NewRecorder()
			mux.ServeHTTP(response, httptest.NewRequest(http.MethodGet, test.path, nil))
			if response.Code != http.StatusOK {
				t.Fatalf("GET %s status = %d, want %d", test.path, response.Code, http.StatusOK)
			}
			if got := response.Header().Get("Content-Type"); got != test.contentType {
				t.Fatalf("GET %s Content-Type = %q, want %q", test.path, got, test.contentType)
			}
			if !bytes.HasPrefix(response.Body.Bytes(), test.prefix) {
				t.Fatalf("GET %s body does not begin with %q", test.path, test.prefix)
			}
		})
	}
}

func loadReposWWW(t *testing.T) *appweb.Site {
	t.Helper()
	site, err := appweb.Load(filepath.Join("..", "..", "share", "www"))
	if err != nil {
		t.Fatal(err)
	}
	return site
}
