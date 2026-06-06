package server

import (
	"encoding/json"
	"net/http"
)

// writeJSON encodes v as the response body with the given status and a JSON
// content type. An encode failure is logged-by-omission: headers are already
// committed, so there is nothing further to do but stop writing.
func writeJSON(w http.ResponseWriter, status int, v any) {
	w.Header().Set("Content-Type", "application/json")
	w.WriteHeader(status)
	_ = json.NewEncoder(w).Encode(v)
}

// handlePRMetadata serves the RFC 9728 protected-resource metadata document.
// This is the one UNAUTHENTICATED route: a client (or MCP host) fetches it to
// learn which authorization server mints tokens for this resource. nginx strips
// the /srv/<app>/ prefix, so the bare path is
// /.well-known/oauth-protected-resource. `resource` is byte-equal to the
// configured canonical resource id.
func (a *appHandler) handlePRMetadata() http.HandlerFunc {
	return func(w http.ResponseWriter, r *http.Request) {
		writeJSON(w, http.StatusOK, map[string]any{
			"resource":                 a.resourceID,
			"authorization_servers":    []string{a.authServer},
			"bearer_methods_supported": []string{"header"},
		})
	}
}

// Envelope is the fixed health envelope rendered by BOTH the HTTP /health route
// and every service's ikigenba_<svc>_health MCP tool, so the two cannot diverge
// (DECISIONS §4). Required top-level keys are appkit-owned and identical for
// every service; per-service telemetry is namespaced under details, which is
// ALWAYS present (empty {} when a service supplies no reporter) so consumers
// never branch on it. Required keys are reserved — a reporter contributes only
// to details.
func Envelope(version, service string, details map[string]any) map[string]any {
	if details == nil {
		details = map[string]any{}
	}
	return map[string]any{
		"status":  "ok",
		"version": version,
		"service": service,
		"details": details,
	}
}

// handleHealth is the ungated liveness route (DECISIONS §5): a 200 OK is the
// dashboard's "service up" signal, and it survives an auth outage because it
// joins PRM and /feed as an unauthenticated route. Body: {status,version,
// service,details} — NO identity. Renders through the shared server.Envelope so
// it cannot drift from the MCP health tool.
func (a *appHandler) handleHealth() http.HandlerFunc {
	return func(w http.ResponseWriter, r *http.Request) {
		details := map[string]any{}
		if a.health != nil {
			d, err := a.health(r.Context())
			if err != nil {
				// a reporter failure is a degraded signal, not a dead one: still
				// 200 (liveness), surface the failure inside details.
				details = map[string]any{"error": err.Error()}
			} else if d != nil {
				details = d
			}
		}
		writeJSON(w, http.StatusOK, Envelope(a.version, a.service, details))
	}
}
