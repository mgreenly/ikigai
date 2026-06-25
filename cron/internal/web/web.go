package web

import (
	"html/template"
	"io/fs"
	"net/http"
)

type landingData struct {
	Service string
	Version string
}

var landingTemplate = template.Must(template.ParseFS(content, "landing.html"))

// LandingHandler serves cron's human landing page.
func LandingHandler(service, version string) http.Handler {
	data := landingData{
		Service: service,
		Version: version,
	}

	return http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		if r.URL.Path != "/" {
			http.NotFound(w, r)
			return
		}
		if r.Method != http.MethodGet {
			w.Header().Set("Allow", http.MethodGet)
			http.Error(w, "method not allowed", http.StatusMethodNotAllowed)
			return
		}

		w.Header().Set("Content-Type", "text/html; charset=utf-8")
		w.WriteHeader(http.StatusOK)
		if err := landingTemplate.Execute(w, data); err != nil {
			panic(err)
		}
	})
}

// StaticHandler serves the embedded Carbon assets used by the landing page.
func StaticHandler() http.Handler {
	staticFS, err := fs.Sub(content, "static")
	if err != nil {
		panic(err)
	}
	return http.StripPrefix("/static/", http.FileServerFS(staticFS))
}
