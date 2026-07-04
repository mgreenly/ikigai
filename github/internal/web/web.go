package web

import (
	"bytes"
	"html/template"
	"io/fs"
	"net/http"
	"path"
	"strings"
)

var (
	landingTemplate = template.Must(template.ParseFS(assets, "landing.html"))
	staticAssets    = mustSub(assets, "static")
)

type landingData struct {
	Service string
	Version string
}

// LandingHandler returns the human landing page for the GitHub service.
func LandingHandler(service, version string) http.Handler {
	return http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		var body bytes.Buffer
		if err := landingTemplate.Execute(&body, landingData{
			Service: service,
			Version: version,
		}); err != nil {
			http.Error(w, "render landing page", http.StatusInternalServerError)
			return
		}

		w.Header().Set("Content-Type", "text/html; charset=utf-8")
		w.WriteHeader(http.StatusOK)
		_, _ = body.WriteTo(w)
	})
}

// StaticHandler serves the package's embedded CSS and font assets.
func StaticHandler() http.Handler {
	return http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		if r.Method != http.MethodGet && r.Method != http.MethodHead {
			w.Header().Set("Allow", "GET, HEAD")
			http.Error(w, "method not allowed", http.StatusMethodNotAllowed)
			return
		}

		name := staticAssetName(r.URL.Path)
		if name == "" {
			http.NotFound(w, r)
			return
		}

		body, err := fs.ReadFile(staticAssets, name)
		if err != nil {
			http.NotFound(w, r)
			return
		}

		w.Header().Set("Content-Type", staticContentType(name, body))
		w.WriteHeader(http.StatusOK)
		if r.Method == http.MethodGet {
			_, _ = w.Write(body)
		}
	})
}

func mustSub(fsys fs.FS, dir string) fs.FS {
	sub, err := fs.Sub(fsys, dir)
	if err != nil {
		panic(err)
	}
	return sub
}

func staticAssetName(urlPath string) string {
	for _, segment := range strings.Split(urlPath, "/") {
		if segment == ".." {
			return ""
		}
	}
	cleaned := path.Clean("/" + urlPath)
	name := strings.TrimPrefix(cleaned, "/")
	name = strings.TrimPrefix(name, "static/")
	if name == "." || name == "" || strings.HasPrefix(name, "../") || strings.Contains(name, "/../") {
		return ""
	}
	return name
}

func staticContentType(name string, body []byte) string {
	switch {
	case strings.HasSuffix(name, ".css"):
		return "text/css; charset=utf-8"
	case strings.HasSuffix(name, ".woff2"):
		return "font/woff2"
	default:
		return http.DetectContentType(body)
	}
}
