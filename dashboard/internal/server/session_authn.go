package server

// This file implements /internal/session-authn — the loopback-only
// session-cookie introspection endpoint nginx calls via auth_request to gate the
// `sites` PRIVATE static tier. It is the browser-session sibling of handleAuthn:
// where handleAuthn validates an opaque bearer access token bound to a specific
// service, this validates a `dashboard_session` cookie against the web-session
// store. The authorization model here is intentionally coarse — ANY valid
// logged-in dashboard session may view ANY private site. There is no per-resource
// binding, no rate limit, and no audit, so the handler is deliberately minimal.
//
// nginx reads only the status code and the response headers — never the body.

import "net/http"

// handleSessionAuthn is the auth_request introspection endpoint for the private
// static tier. It mirrors handleAuthn's loopback guard, denial status, and
// identity header (X-Owner-Email), differing only in that it reads a session
// cookie and validates it via the web-session store instead of token
// introspection. On success it returns a bare 200 with X-Owner-Email; on any
// failure (missing/invalid cookie) it returns 401.
func (a *app) handleSessionAuthn() http.HandlerFunc {
	return func(w http.ResponseWriter, r *http.Request) {
		// (a) Loopback guard — identical to handleAuthn. nginx marks this
		// location `internal;`; this is defense in depth. Non-loopback → 403.
		if !remoteIsLoopback(r.RemoteAddr) {
			w.Header().Set("Cache-Control", "no-store")
			w.WriteHeader(http.StatusForbidden)
			return
		}

		// (b) Read the session cookie. Missing → 401.
		c, err := r.Cookie(sessionCookieName)
		if err != nil {
			w.Header().Set("Cache-Control", "no-store")
			w.WriteHeader(http.StatusUnauthorized)
			return
		}

		// (c) Validate the cookie against the web-session store. Any
		// invalid/expired/revoked/unknown session → 401.
		sess, err := a.sessions.Lookup(r.Context(), c.Value)
		if err != nil {
			w.Header().Set("Cache-Control", "no-store")
			w.WriteHeader(http.StatusUnauthorized)
			return
		}

		// (d) Allow: emit the identity header nginx forwards upstream. No
		// resource/workspace/rate-limit headers — this tier is coarse by design.
		w.Header().Set("X-Owner-Email", sess.OwnerEmail)
		w.Header().Set("Cache-Control", "no-store")
		w.WriteHeader(http.StatusOK)
	}
}
