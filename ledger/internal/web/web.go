package web

import (
	"html/template"
	"net/http"
	"strings"
)

type landingData struct {
	Service string
	Version string
}

// LandingHandler returns the exact-root landing page and its embedded assets.
func LandingHandler(service, version string) http.Handler {
	tpl := template.Must(template.ParseFS(templateFS, "landing.html"))
	static := http.FileServerFS(staticFS)

	return http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		if r.Method != http.MethodGet {
			http.NotFound(w, r)
			return
		}
		if strings.HasPrefix(r.URL.Path, "/static/") {
			static.ServeHTTP(w, r)
			return
		}
		if r.URL.Path != "/" {
			http.NotFound(w, r)
			return
		}

		w.Header().Set("Content-Type", "text/html; charset=utf-8")
		w.WriteHeader(http.StatusOK)
		_ = tpl.Execute(w, landingData{Service: service, Version: version})
	})
}
