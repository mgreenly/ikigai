package server

import (
	"net/http"
	"net/url"
	"strings"

	"appkit/inventory"
)

// Href is the landing-page service link target. serviceRow already carries the
// raw MCP URL for display; derive the mount path by removing the trailing mcp
// segment so the template links the service name to the service root.
func (r serviceRow) Href() string {
	u, err := url.Parse(r.URL)
	if err != nil {
		return ""
	}
	return strings.TrimSuffix(u.Path, "mcp")
}

// inventoryService is one entry in the /services response: the service's name,
// its mount path, and the full MCP resource URL self-templated from the request.
type inventoryService struct {
	Name     string `json:"name"`
	Mount    string `json:"mount"`
	Resource string `json:"resource"`
}

// handleInventory serves GET /services: a public listing of the box's
// MCP-exposing services so the suite plugin's connect skill can wire up each MCP.
// The resource URL is self-templated from the request (scheme from
// X-Forwarded-Proto, host from r.Host) since the same binary serves any account's
// apex. An empty inventory is a 200 with an empty list, not a 404.
func (a *app) handleInventory() http.HandlerFunc {
	return func(w http.ResponseWriter, r *http.Request) {
		svcs, err := inventory.Read(a.manifestRoot)
		if err != nil {
			a.logger.Error("inventory.read", "err", err)
			http.Error(w, "internal server error", http.StatusInternalServerError)
			return
		}
		scheme := requestScheme(r)
		host := r.Host

		out := make([]inventoryService, 0, len(svcs))
		for _, s := range svcs {
			out = append(out, inventoryService{
				Name:     s.Name,
				Mount:    s.Mount,
				Resource: mcpResourceURL(scheme, host, s.Mount),
			})
		}
		writeJSON(w, http.StatusOK, map[string]any{"services": out})
	}
}
