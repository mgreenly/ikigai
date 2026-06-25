package web

import (
	"html/template"
	"io/fs"
	"net/http"
	"path/filepath"
	"strings"
)

const mountPrefix = "/srv/notify"

var landingTemplate = template.Must(template.ParseFS(templateFiles, "landing.html"))

// LandingHandler renders notify's human landing page.
func LandingHandler(service, version string) http.Handler {
	data := struct {
		Service   string
		Version   string
		AssetPath string
	}{
		Service:   service,
		Version:   version,
		AssetPath: mountPrefix + "/static/tokens.css",
	}

	return http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		if r.Method != http.MethodGet {
			w.Header().Set("Allow", http.MethodGet)
			http.Error(w, "method not allowed", http.StatusMethodNotAllowed)
			return
		}
		if r.URL.Path != "/" {
			http.NotFound(w, r)
			return
		}

		w.Header().Set("Content-Type", "text/html; charset=utf-8")
		if err := landingTemplate.Execute(w, data); err != nil {
			http.Error(w, "render landing page", http.StatusInternalServerError)
		}
	})
}

// StaticHandler serves the landing page's embedded CSS and font assets.
func StaticHandler() http.Handler {
	staticRoot, err := fs.Sub(staticFiles, "static")
	if err != nil {
		panic(err)
	}
	fileServer := http.FileServerFS(staticRoot)

	return http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		if r.Method != http.MethodGet {
			w.Header().Set("Allow", http.MethodGet)
			http.Error(w, "method not allowed", http.StatusMethodNotAllowed)
			return
		}
		if !strings.HasPrefix(r.URL.Path, "/static/") || strings.HasSuffix(r.URL.Path, "/") {
			http.NotFound(w, r)
			return
		}

		switch filepath.Ext(r.URL.Path) {
		case ".css":
			w.Header().Set("Content-Type", "text/css; charset=utf-8")
		case ".woff2":
			w.Header().Set("Content-Type", "font/woff2")
		}

		http.StripPrefix("/static/", fileServer).ServeHTTP(w, r)
	})
}
