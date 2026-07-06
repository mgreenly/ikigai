package web

import (
	"net/http"
	"net/http/httptest"
	"os"
	"path/filepath"
	"strings"
	"testing"
)

func writeFile(t *testing.T, path, contents string) {
	t.Helper()
	if err := os.MkdirAll(filepath.Dir(path), 0o755); err != nil {
		t.Fatalf("mkdir %s: %v", filepath.Dir(path), err)
	}
	if err := os.WriteFile(path, []byte(contents), 0o644); err != nil {
		t.Fatalf("write %s: %v", path, err)
	}
}

// R-M0CJ-U84T
func TestLoadAndRenderLandingTemplate(t *testing.T) {
	root := t.TempDir()
	writeFile(t, filepath.Join(root, "landing.html"), `{{.Service}} {{.Version}}`)

	site, err := Load(root)
	if err != nil {
		t.Fatalf("Load: %v", err)
	}

	rec := httptest.NewRecorder()
	if err := site.Render(rec, "landing.html", map[string]string{
		"Service": "crm",
		"Version": "20260706",
	}); err != nil {
		t.Fatalf("Render: %v", err)
	}

	res := rec.Result()
	defer res.Body.Close()
	if res.StatusCode != http.StatusOK {
		t.Fatalf("StatusCode = %d, want 200", res.StatusCode)
	}
	if got := res.Header.Get("Content-Type"); got != "text/html; charset=utf-8" {
		t.Fatalf("Content-Type = %q, want text/html; charset=utf-8", got)
	}
	if got := rec.Body.String(); !strings.Contains(got, "crm") || !strings.Contains(got, "20260706") {
		t.Fatalf("body = %q, want service and version values", got)
	}
}

func TestLoadParsesHTMLAndTmplIntoOneTemplateSet(t *testing.T) {
	root := t.TempDir()
	writeFile(t, filepath.Join(root, "landing.html"), `{{template "card.tmpl" .}}`)
	writeFile(t, filepath.Join(root, "card.tmpl"), `service={{.Service}}`)

	site, err := Load(root)
	if err != nil {
		t.Fatalf("Load: %v", err)
	}

	rec := httptest.NewRecorder()
	if err := site.Render(rec, "landing.html", map[string]string{"Service": "crm"}); err != nil {
		t.Fatalf("Render: %v", err)
	}

	if got := rec.Body.String(); got != "service=crm" {
		t.Fatalf("body = %q, want rendered tmpl partial", got)
	}
}

// R-M1KG-7ZVI
func TestLoadNonexistentRootReturnsPathError(t *testing.T) {
	root := filepath.Join(t.TempDir(), "missing-www")

	site, err := Load(root)
	if err == nil {
		t.Fatalf("Load returned nil error and site %#v, want error", site)
	}
	if site != nil {
		t.Fatalf("site = %#v, want nil", site)
	}
	if !strings.Contains(err.Error(), root) {
		t.Fatalf("error = %q, want it to name %q", err, root)
	}
}

// R-M2SC-LRM7
func TestStaticServesCSSBytesWithContentType(t *testing.T) {
	root := t.TempDir()
	writeFile(t, filepath.Join(root, "static", "tokens.css"), ":root { --accent: #0a7; }\n")

	site, err := Load(root)
	if err != nil {
		t.Fatalf("Load: %v", err)
	}

	rec := httptest.NewRecorder()
	site.Static().ServeHTTP(rec, httptest.NewRequest(http.MethodGet, "/static/tokens.css", nil))

	res := rec.Result()
	defer res.Body.Close()
	if res.StatusCode != http.StatusOK {
		t.Fatalf("StatusCode = %d, want 200", res.StatusCode)
	}
	if got := res.Header.Get("Content-Type"); !strings.HasPrefix(got, "text/css") {
		t.Fatalf("Content-Type = %q, want text/css", got)
	}
	if got := rec.Body.String(); got != ":root { --accent: #0a7; }\n" {
		t.Fatalf("body = %q, want css file bytes", got)
	}
}

// R-M408-ZJCW
func TestStaticServesWoff2WithFontContentType(t *testing.T) {
	root := t.TempDir()
	writeFile(t, filepath.Join(root, "static", "fonts", "x.woff2"), "font-bytes")

	site, err := Load(root)
	if err != nil {
		t.Fatalf("Load: %v", err)
	}

	rec := httptest.NewRecorder()
	site.Static().ServeHTTP(rec, httptest.NewRequest(http.MethodGet, "/static/fonts/x.woff2", nil))

	res := rec.Result()
	defer res.Body.Close()
	if res.StatusCode != http.StatusOK {
		t.Fatalf("StatusCode = %d, want 200", res.StatusCode)
	}
	if got := res.Header.Get("Content-Type"); got != "font/woff2" {
		t.Fatalf("Content-Type = %q, want font/woff2", got)
	}
	if got := rec.Body.String(); got != "font-bytes" {
		t.Fatalf("body = %q, want font file bytes", got)
	}
}

// R-M585-DB3L
func TestStaticDirectoryRequestsReturnNotFound(t *testing.T) {
	root := t.TempDir()
	writeFile(t, filepath.Join(root, "static", "tokens.css"), "body {}\n")
	writeFile(t, filepath.Join(root, "static", "fonts", "x.woff2"), "font-bytes")

	site, err := Load(root)
	if err != nil {
		t.Fatalf("Load: %v", err)
	}

	for _, path := range []string{"/static/", "/static/fonts/"} {
		rec := httptest.NewRecorder()
		site.Static().ServeHTTP(rec, httptest.NewRequest(http.MethodGet, path, nil))
		res := rec.Result()
		_ = res.Body.Close()
		if res.StatusCode != http.StatusNotFound {
			t.Fatalf("%s StatusCode = %d, want 404", path, res.StatusCode)
		}
		if strings.Contains(rec.Body.String(), "tokens.css") || strings.Contains(rec.Body.String(), "x.woff2") {
			t.Fatalf("%s body = %q, want no directory listing", path, rec.Body.String())
		}
	}
}
