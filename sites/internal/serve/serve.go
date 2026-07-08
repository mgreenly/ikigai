package serve

import (
	"errors"
	"mime"
	"net/http"
	"os"
	"path/filepath"
	"strings"

	"sites/internal/files"
)

// Handler serves files under a single visibility root. The request path must be
// under urlPrefix; the remainder is confined under root before any filesystem
// access.
func Handler(root, urlPrefix string) http.Handler {
	return http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		if !strings.HasPrefix(r.URL.Path, urlPrefix) {
			http.NotFound(w, r)
			return
		}

		rel := strings.TrimPrefix(r.URL.Path, urlPrefix)
		if rel == "" {
			rel = "."
		}
		target, err := files.ConfinePath(root, filepath.FromSlash(rel))
		if err != nil {
			http.NotFound(w, r)
			return
		}

		info, err := os.Stat(target)
		if errors.Is(err, os.ErrNotExist) {
			http.NotFound(w, r)
			return
		}
		if err != nil {
			http.NotFound(w, r)
			return
		}

		if info.IsDir() {
			if !strings.HasSuffix(r.URL.Path, "/") {
				http.Redirect(w, r, r.URL.Path+"/", http.StatusMovedPermanently)
				return
			}
			target = filepath.Join(target, "index.html")
			info, err = os.Stat(target)
			if errors.Is(err, os.ErrNotExist) || (err == nil && info.IsDir()) {
				http.NotFound(w, r)
				return
			}
			if err != nil {
				http.NotFound(w, r)
				return
			}
		}

		if !info.Mode().IsRegular() {
			http.NotFound(w, r)
			return
		}
		if contentType := mime.TypeByExtension(filepath.Ext(target)); contentType != "" {
			w.Header().Set("Content-Type", contentType)
		}
		http.ServeFile(w, r, target)
	})
}
