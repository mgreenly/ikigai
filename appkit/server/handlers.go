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

// handleWhoami is the <app>_whoami proof the dashboard's connect skill uses to
// verify the full auth chain end to end. It is behind requireIdentityHeaders, so
// the identity is always present on the context. No side effects.
func (a *appHandler) handleWhoami() http.HandlerFunc {
	return func(w http.ResponseWriter, r *http.Request) {
		id, _ := IdentityFrom(r.Context())
		writeJSON(w, http.StatusOK, map[string]any{
			"owner_email": id.OwnerEmail,
			"client_id":   id.ClientID,
		})
	}
}
