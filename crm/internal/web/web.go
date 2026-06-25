package web

import (
	"html/template"
	"net/http"
	"sync"
)

var landingTemplate = sync.OnceValues(func() (*template.Template, error) {
	return template.ParseFS(templateFS, "landing.html")
})

// LandingHandler renders the crm human landing page.
func LandingHandler(service, version string) http.Handler {
	return http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		if r.URL.Path != "/" {
			http.NotFound(w, r)
			return
		}

		tmpl, err := landingTemplate()
		if err != nil {
			http.Error(w, "template error", http.StatusInternalServerError)
			return
		}

		w.Header().Set("Content-Type", "text/html; charset=utf-8")
		w.WriteHeader(http.StatusOK)
		_ = tmpl.Execute(w, struct {
			Service string
			Version string
		}{
			Service: service,
			Version: version,
		})
	})
}

// StaticHandler serves crm's embedded landing assets.
func StaticHandler() http.Handler {
	return http.StripPrefix("/static/", http.FileServer(http.FS(staticFS)))
}
