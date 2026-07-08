package serve

import (
	"net/http"
	"net/http/httptest"
	"os"
	"path/filepath"
	"strings"
	"testing"
)

func writeFile(t *testing.T, path, body string) {
	t.Helper()
	if err := os.MkdirAll(filepath.Dir(path), 0o755); err != nil {
		t.Fatalf("mkdir %s: %v", filepath.Dir(path), err)
	}
	if err := os.WriteFile(path, []byte(body), 0o644); err != nil {
		t.Fatalf("write %s: %v", path, err)
	}
}

func serveRequest(root, target string) *httptest.ResponseRecorder {
	rec := httptest.NewRecorder()
	Handler(root, "/public/").ServeHTTP(rec, httptest.NewRequest(http.MethodGet, target, nil))
	return rec
}

// R-QZX3-2WYW
func TestHandlerServesRegularFileWithExtensionContentType(t *testing.T) {
	root := t.TempDir()
	writeFile(t, filepath.Join(root, "blog", "style.css"), "body{color:red}")

	rec := serveRequest(root, "/public/blog/style.css")

	if rec.Code != http.StatusOK {
		t.Fatalf("status = %d, want %d; body=%q", rec.Code, http.StatusOK, rec.Body.String())
	}
	if got := rec.Body.String(); got != "body{color:red}" {
		t.Fatalf("body = %q, want file bytes", got)
	}
	if got := rec.Header().Get("Content-Type"); !strings.HasPrefix(got, "text/css") {
		t.Fatalf("Content-Type = %q, want mime type for .css", got)
	}
}

// R-R14Z-GOPL
func TestHandlerServesDirectoryIndex(t *testing.T) {
	root := t.TempDir()
	writeFile(t, filepath.Join(root, "blog", "index.html"), "<h1>Blog</h1>")

	rec := serveRequest(root, "/public/blog/")

	if rec.Code != http.StatusOK {
		t.Fatalf("status = %d, want %d; body=%q", rec.Code, http.StatusOK, rec.Body.String())
	}
	if got := rec.Body.String(); got != "<h1>Blog</h1>" {
		t.Fatalf("body = %q, want index bytes", got)
	}
}

// R-R2CV-UGGA
func TestHandlerReturnsNotFoundForDirectoryWithoutIndex(t *testing.T) {
	root := t.TempDir()
	writeFile(t, filepath.Join(root, "blog", "post.html"), "post")

	rec := serveRequest(root, "/public/blog/")

	if rec.Code != http.StatusNotFound {
		t.Fatalf("status = %d, want %d; body=%q", rec.Code, http.StatusNotFound, rec.Body.String())
	}
	if strings.Contains(rec.Body.String(), "post.html") {
		t.Fatalf("directory listing leaked file name in body %q", rec.Body.String())
	}
}

// R-R3KS-886Z
func TestHandlerReturnsNotFoundForEscapingPaths(t *testing.T) {
	root := t.TempDir()
	outside := filepath.Join(t.TempDir(), "secret.css")
	writeFile(t, outside, "secret")

	for _, target := range []string{
		"/public/../" + filepath.Base(outside),
		"/public/" + filepath.ToSlash(outside),
	} {
		rec := serveRequest(root, target)
		if rec.Code != http.StatusNotFound {
			t.Fatalf("%s status = %d, want %d; body=%q", target, rec.Code, http.StatusNotFound, rec.Body.String())
		}
		if strings.Contains(rec.Body.String(), "secret") {
			t.Fatalf("%s served outside content: %q", target, rec.Body.String())
		}
	}
}

// R-R4SO-LZXO
func TestHandlerReturnsNotFoundForMissingPath(t *testing.T) {
	root := t.TempDir()

	rec := serveRequest(root, "/public/missing.html")

	if rec.Code != http.StatusNotFound {
		t.Fatalf("status = %d, want %d; body=%q", rec.Code, http.StatusNotFound, rec.Body.String())
	}
}

// R-R60K-ZROD
func TestHandlerRedirectsExistingDirectoryToTrailingSlash(t *testing.T) {
	root := t.TempDir()
	writeFile(t, filepath.Join(root, "blog", "index.html"), "blog")

	rec := serveRequest(root, "/public/blog")

	if rec.Code != http.StatusMovedPermanently {
		t.Fatalf("status = %d, want %d; body=%q", rec.Code, http.StatusMovedPermanently, rec.Body.String())
	}
	if got, want := rec.Header().Get("Location"), "/public/blog/"; got != want {
		t.Fatalf("Location = %q, want %q", got, want)
	}
}
