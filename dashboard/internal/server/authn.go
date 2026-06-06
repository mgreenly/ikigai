package server

// This file implements POST /internal/authn — the loopback-only token
// introspection endpoint nginx calls via auth_request on every request to a
// service mounted under this box. It is the requireBearer logic lifted out of
// the per-service request path: validate the opaque access token, check that
// it is bound to the service being addressed, that its owner is inside the
// configured Workspace, and that it is within its per-token rate budget. On
// success it returns 200 plus the identity headers nginx injects upstream
// (X-Owner-Email, X-Client-Id); on failure 401 with an MCP-style
// WWW-Authenticate challenge, or 429 when the token is over budget.
//
// nginx reads only the status code and the response headers — never the body.

import (
	"net"
	"net/http"
	"strconv"
	"strings"
	"time"

	"dashboard/internal/audit"
)

// handleAuthn is the auth_request introspection endpoint. Its checks run in a
// strict order so that an unauthenticated caller can never consume rate budget
// and so the 401 challenge can always advertise the right protected-resource
// metadata URL for the service being addressed.
func (a *app) handleAuthn() http.HandlerFunc {
	return func(w http.ResponseWriter, r *http.Request) {
		// (a) Loopback guard. nginx marks this location `internal;`, so it is
		// unreachable from outside; this is defense in depth in case that
		// directive is ever dropped. An unparseable/empty RemoteAddr is treated
		// as not-loopback and rejected — every legitimate caller (nginx, and
		// httptest, which sets 127.0.0.1:port) presents a parseable loopback
		// host:port.
		if !remoteIsLoopback(r.RemoteAddr) {
			w.Header().Set("Cache-Control", "no-store")
			w.WriteHeader(http.StatusForbidden)
			return
		}

		// (b) Resolve the addressed service's bound resource and PRM URL from
		// the forwarded original request URI, BEFORE any token check, so a 401
		// can carry the correct resource_metadata.
		//
		// Derivation rule (an inference, not spelled out in the platform doc):
		//   - nginx forwards $request_uri as X-Original-URI, e.g.
		//     "/srv/crm/mcp" (possibly with a "?query"). Strip the query.
		//   - The service mount is "/srv/<svc>/". We find the configured
		//     resource (a full URL such as
		//     "https://int.ikigenba.com/srv/crm/mcp") whose URL path begins with
		//     that mount; that full URL is the boundResource.
		//   - The protected-resource-metadata (PRM) URL is boundResource with
		//     its final path segment ("mcp") replaced by
		//     ".well-known/oauth-protected-resource", i.e.
		//     "https://int.ikigenba.com/srv/crm/.well-known/oauth-protected-resource".
		//     It is derived from boundResource (NOT publicBaseURL) so its host
		//     matches the token's resource host in every environment.
		boundResource, ok := a.resourceForOriginalURI(r.Header.Get("X-Original-URI"))
		if !ok {
			// Unknown service: we cannot name a resource_metadata URL.
			writeBearerChallenge(w, http.StatusUnauthorized, bearerChallenge{
				oauthError:  "invalid_request",
				description: "unknown service for original request URI",
			})
			a.auditAuthnDeny(r, audit.Event{Details: map[string]any{"reason": "unknown_service"}})
			return
		}
		prmURL := protectedResourceMetadataURL(boundResource)

		// (c) Extract the bearer token.
		tok, ok := bearerFromHeader(r)
		if !ok {
			writeBearerChallenge(w, http.StatusUnauthorized, bearerChallenge{
				oauthError:       "invalid_request",
				description:      "missing or malformed Authorization header",
				resourceMetadata: prmURL,
			})
			a.auditAuthnDeny(r, audit.Event{Details: map[string]any{"reason": "missing_bearer"}})
			return
		}

		// (d) Validate the access token.
		vt, err := a.oauthTokens.ValidateAccess(r.Context(), tok)
		if err != nil {
			writeBearerChallenge(w, http.StatusUnauthorized, bearerChallenge{
				oauthError:       "invalid_token",
				description:      err.Error(),
				resourceMetadata: prmURL,
			})
			a.auditAuthnDeny(r, audit.Event{Details: map[string]any{"reason": "invalid_token", "detail": err.Error()}})
			return
		}

		// (e) Resource binding: the token must be bound to exactly this service.
		if vt.Chain.Resource != boundResource {
			writeBearerChallenge(w, http.StatusUnauthorized, bearerChallenge{
				oauthError:       "invalid_token",
				description:      "token resource binding does not match this service",
				resourceMetadata: prmURL,
			})
			a.auditAuthnDeny(r, audit.Event{
				OwnerEmail: vt.Chain.OwnerEmail, ClientID: vt.Chain.ClientID, ChainID: vt.Chain.ID,
				Details: map[string]any{"reason": "resource_mismatch", "token_resource": vt.Chain.Resource, "bound_resource": boundResource},
			})
			return
		}

		// (f) Workspace: the owner identity must be inside the configured domain.
		if !ownerInWorkspace(vt.Chain.OwnerEmail, a.workspaceDomain) {
			writeBearerChallenge(w, http.StatusUnauthorized, bearerChallenge{
				oauthError:       "invalid_token",
				description:      "owner identity outside configured workspace",
				resourceMetadata: prmURL,
			})
			a.auditAuthnDeny(r, audit.Event{
				OwnerEmail: vt.Chain.OwnerEmail, ClientID: vt.Chain.ClientID, ChainID: vt.Chain.ID,
				Details: map[string]any{"reason": "workspace_mismatch"},
			})
			return
		}

		// (g) Rate limit — only after auth fully passes, keyed on the
		// server-side token id so unauthenticated calls never consume budget.
		dec := a.rateLimiter.Decide(vt.Token.ID)
		if !dec.Allowed {
			w.Header().Set("Cache-Control", "no-store")
			w.Header().Set("Retry-After", retryAfterSeconds(a.rateLimiter.Window()))
			w.WriteHeader(http.StatusTooManyRequests)
			ip, ua := audit.FromRequest(r)
			_ = a.audit.Write(r.Context(), audit.Event{
				Type:       audit.EventRateLimitHit,
				OwnerEmail: vt.Chain.OwnerEmail, ClientID: vt.Chain.ClientID, ChainID: vt.Chain.ID,
				IP: ip, UserAgent: ua,
				Details: map[string]any{"surface": "authn", "token_id": vt.Token.ID, "window_count": dec.WindowCount},
			})
			return
		}

		// (h) Allow: emit the identity headers nginx forwards upstream.
		w.Header().Set("X-Owner-Email", vt.Chain.OwnerEmail)
		w.Header().Set("X-Client-Id", vt.Chain.ClientID)
		w.Header().Set("X-Chain-Id", vt.Chain.ID)
		w.Header().Set("X-Token-Id", vt.Token.ID)
		w.Header().Set("Cache-Control", "no-store")
		ip, ua := audit.FromRequest(r)
		_ = a.audit.Write(r.Context(), audit.Event{
			Type:       audit.EventAuthnAllow,
			OwnerEmail: vt.Chain.OwnerEmail, ClientID: vt.Chain.ClientID, ChainID: vt.Chain.ID,
			IP: ip, UserAgent: ua,
			Details: map[string]any{"resource": boundResource},
		})
		w.WriteHeader(http.StatusOK)
	}
}

// resourceForOriginalURI maps a forwarded X-Original-URI (nginx's $request_uri)
// to the configured resource of the service it addresses. It returns ok=false
// when the header is missing/malformed or no configured resource is mounted
// under the URI's "/srv/<svc>/" prefix. See the derivation rule documented in
// handleAuthn.
func (a *app) resourceForOriginalURI(originalURI string) (boundResource string, ok bool) {
	if originalURI == "" {
		return "", false
	}
	// Strip any query string; we only key on the path.
	path := originalURI
	if i := strings.IndexByte(path, '?'); i >= 0 {
		path = path[:i]
	}
	// Expect "/srv/<svc>/...". Derive the mount "/srv/<svc>/".
	const prefix = "/srv/"
	if !strings.HasPrefix(path, prefix) {
		return "", false
	}
	rest := path[len(prefix):]
	slash := strings.IndexByte(rest, '/')
	if slash <= 0 {
		// No service segment, or no trailing slash after it.
		return "", false
	}
	svc := rest[:slash]
	mount := prefix + svc + "/" // "/srv/crm/"
	for _, res := range a.resources {
		// res is a full URL such as "https://int.ikigenba.com/srv/crm/mcp".
		// Match on its path component beginning with the mount.
		if p := urlPath(res); strings.HasPrefix(p, mount) {
			return res, true
		}
	}
	return "", false
}

// urlPath returns the path component of a full URL, falling back to the raw
// string if it is not parseable as a URL with a path.
func urlPath(raw string) string {
	// A configured resource is a full origin+path URL; find the path by
	// skipping the scheme://host. Avoid net/url here to keep the match cheap
	// and to treat any leading "scheme://host" uniformly.
	s := raw
	if i := strings.Index(s, "://"); i >= 0 {
		s = s[i+len("://"):]
		if j := strings.IndexByte(s, '/'); j >= 0 {
			return s[j:]
		}
		return "/"
	}
	return s
}

// protectedResourceMetadataURL derives the RFC 9728 protected-resource-metadata
// URL for a bound resource by replacing the resource's final path segment with
// ".well-known/oauth-protected-resource". For
// "https://int.ikigenba.com/srv/crm/mcp" this yields
// "https://int.ikigenba.com/srv/crm/.well-known/oauth-protected-resource".
func protectedResourceMetadataURL(boundResource string) string {
	const wellKnown = ".well-known/oauth-protected-resource"
	if i := strings.LastIndexByte(boundResource, '/'); i >= 0 {
		return boundResource[:i+1] + wellKnown
	}
	return boundResource + "/" + wellKnown
}

// auditAuthnDeny writes an authn.deny audit row, attaching the request's IP and
// User-Agent. The Type is always EventAuthnDeny; callers fill identity/details.
func (a *app) auditAuthnDeny(r *http.Request, e audit.Event) {
	e.Type = audit.EventAuthnDeny
	e.IP, e.UserAgent = audit.FromRequest(r)
	_ = a.audit.Write(r.Context(), e)
}

// bearerChallenge holds the optional fields of a WWW-Authenticate: Bearer
// challenge. Empty fields are omitted.
type bearerChallenge struct {
	oauthError       string // RFC 6750 "error" code, e.g. "invalid_token"
	description      string // human-readable "error_description"
	resourceMetadata string // RFC 9728 "resource_metadata" PRM URL
}

// writeBearerChallenge writes a WWW-Authenticate: Bearer header built
// incrementally from the supplied fields, sets Cache-Control: no-store, and
// writes status with an empty body. nginx reads only the status and headers;
// the body is intentionally empty.
func writeBearerChallenge(w http.ResponseWriter, status int, c bearerChallenge) {
	hdr := "Bearer"
	if c.oauthError != "" {
		hdr += ` error="` + c.oauthError + `"`
	}
	if c.description != "" {
		if strings.HasSuffix(hdr, "Bearer") {
			hdr += " "
		} else {
			hdr += ", "
		}
		hdr += `error_description="` + c.description + `"`
	}
	if c.resourceMetadata != "" {
		if strings.HasSuffix(hdr, "Bearer") {
			hdr += " "
		} else {
			hdr += ", "
		}
		hdr += `resource_metadata="` + c.resourceMetadata + `"`
	}
	w.Header().Set("WWW-Authenticate", hdr)
	w.Header().Set("Cache-Control", "no-store")
	w.WriteHeader(status)
}

// remoteIsLoopback reports whether remoteAddr (host:port form) has a loopback
// host. An empty or unparseable address is treated as not-loopback.
func remoteIsLoopback(remoteAddr string) bool {
	if remoteAddr == "" {
		return false
	}
	host, _, err := net.SplitHostPort(remoteAddr)
	if err != nil {
		// Some callers may present a bare host with no port.
		host = remoteAddr
	}
	ip := net.ParseIP(host)
	if ip == nil {
		return false
	}
	return ip.IsLoopback()
}

// retryAfterSeconds renders a rate-limit window as a whole-second Retry-After
// value, rounding up so it never advertises a shorter wait than the window.
func retryAfterSeconds(window time.Duration) string {
	secs := int64((window + time.Second - 1) / time.Second)
	if secs < 1 {
		secs = 1
	}
	return strconv.FormatInt(secs, 10)
}
