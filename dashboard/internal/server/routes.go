package server

import "net/http"

// muxer is the route-registration seam register writes to. Both *http.ServeMux
// (the standalone test server) and *appkit/server.Router (the Apex bypass in
// production) satisfy it with the identical Handle(pattern, handler) signature,
// so one register method serves both without coupling this package's full route
// table to either concrete type.
type muxer interface {
	Handle(pattern string, h http.Handler)
}

// register mounts the dashboard's complete URL surface on mux. It is the one
// place that answers "what does this server expose?". It does NOT wrap the mux
// with security headers or request-id middleware: appkit's server applies those
// uniformly (the dashboard runs as the apex app behind appkit's Apex bypass, so
// appkit owns the outer chain). New() (the standalone server the tests drive)
// re-applies securityHeaders around the same mux.
func (a *app) register(mux muxer) {
	mux.Handle("GET /{$}", a.handleIndex())
	mux.Handle("GET /login", a.handleLogin())
	mux.Handle("GET /oauth/google/callback", a.handleCallback())
	mux.Handle("POST /logout", a.handleLogout())
	mux.Handle("GET /services", a.handleInventory())
	mux.Handle("GET /install/claude", a.handleInstall(claudeAgent))
	mux.Handle("GET /install/codex", a.handleInstall(codexAgent))
	mux.Handle("GET /static/", a.staticHandler())

	// Live-grants block on the logged-in index: session-authenticated (not
	// bearer / not auth_request / not loopback). SSE stream, the HTML fragment
	// the stream client swaps in, and per-grant web revocation.
	mux.Handle("GET /grants/stream", a.handleGrantsStream())
	mux.Handle("GET /grants/fragment", a.handleGrantsFragment())
	mux.Handle("POST /grants/{public_id}/revoke", a.handleGrantRevoke())

	// Personal access tokens on the logged-in index: session-authenticated,
	// same-origin-enforced. Create renders the show-once secret directly (no
	// PRG); revoke is POST→303→/. No SSE — a PAT row only changes through these
	// explicit actions, so the list renders inline in handleIndex (ADR §D9).
	mux.Handle("POST /pat", a.handlePATCreate())
	mux.Handle("POST /pat/{public_id}/revoke", a.handlePATRevoke())

	// OAuth authorization-server surface.
	mux.Handle("GET /.well-known/oauth-authorization-server", a.handleASMetadata())
	mux.Handle("POST /oauth/register", a.handleDCRRegister())
	mux.Handle("GET /oauth/authorize", a.handleAuthorize())
	mux.Handle("POST /oauth/token", a.handleToken())
	mux.Handle("POST /oauth/introspect", a.handleIntrospect())
	mux.Handle("POST /oauth/revoke", a.handleRevoke())

	// Loopback-only token introspection nginx calls via auth_request on every
	// service request. The nginx location is marked `internal;`; the handler
	// re-checks loopback as defense in depth. Registered with no method
	// constraint: nginx's auth_request subrequest mirrors the original request
	// method (GET for a GET to the service, POST for a POST, ...), so pinning a
	// single verb here would 405 every mismatch — which auth_request then turns
	// into a 500. Introspection is method-independent, so accept any method.
	mux.Handle("/internal/authn", a.handleAuthn())

	// Loopback-only session-cookie introspection nginx calls via auth_request to
	// gate the `sites` PRIVATE static tier. Sibling of /internal/authn but
	// session-based: any valid logged-in dashboard browser session may view any
	// private site. Same `internal;`/loopback posture and method-independence as
	// /internal/authn (no method constraint).
	mux.Handle("/internal/session-authn", a.handleSessionAuthn())
}

// routes is the standalone handler the in-package tests drive via New(): the
// registered mux wrapped in security headers (appkit applies the same wrap in
// production via its Apex bypass).
func (a *app) routes() http.Handler {
	mux := http.NewServeMux()
	a.register(mux)
	return securityHeaders(mux)
}
