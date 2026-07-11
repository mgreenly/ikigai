package server

import (
	"net/http"
	"net/url"

	"dashboard/internal/audit"
	"dashboard/internal/identity"
	"dashboard/internal/oauth"
	"dashboard/internal/oauthstate"
)

// sessionCookieName carries the plaintext web-session value to the browser. The
// matching hash lives server-side on the web_sessions row; every subsequent
// request presents this cookie and the server re-hashes it to find the session.
const sessionCookieName = "dashboard_session"

// setSessionCookie hands the browser the plaintext session cookie. Secure is
// gated on isHTTPS so it works on plain-http localhost and is Secure on the
// deployed TLS host. It is a session cookie (no Max-Age): the server-side
// absolute and idle ceilings are the authoritative expiry, not the browser.
func setSessionCookie(w http.ResponseWriter, r *http.Request, cookie string) {
	http.SetCookie(w, &http.Cookie{
		Name:     sessionCookieName,
		Value:    cookie,
		Path:     "/",
		HttpOnly: true,
		SameSite: http.SameSiteLaxMode,
		Secure:   isHTTPS(r),
	})
}

// clearSessionCookie expires the session cookie on the browser. It mirrors
// setSessionCookie's attributes (a Set-Cookie only overwrites a prior cookie
// when Name/Path match) but carries an empty value and Max-Age -1, telling the
// browser to delete it immediately. Used on logout and when the index sees a
// dead cookie.
func clearSessionCookie(w http.ResponseWriter, r *http.Request) {
	http.SetCookie(w, &http.Cookie{
		Name:     sessionCookieName,
		Value:    "",
		Path:     "/",
		HttpOnly: true,
		SameSite: http.SameSiteLaxMode,
		Secure:   isHTTPS(r),
		MaxAge:   -1,
	})
}

// handleCallback completes the Google sign-in flow: it consumes the one-time
// state record minted by handleLogin (web origin) or handleAuthorize (MCP
// origin), proving the round-trip returned to the same browser that started it,
// then exchanges the authorization code for tokens. After federation policy
// passes it branches on the consumed handshake's origin: a web origin mints a
// web session; an MCP origin issues an OAuth authorization code and redirects
// back to the originating MCP client.
func (a *app) handleCallback() http.HandlerFunc {
	return func(w http.ResponseWriter, r *http.Request) {
		state := r.URL.Query().Get("state")
		code := r.URL.Query().Get("code")
		cookie, err := r.Cookie(bindingCookieName)
		if err != nil {
			a.logger.Warn("callback.missing_binding_cookie", "err", err)
			a.writeFederationReject(r, "", "missing_binding_cookie")
			http.Error(w, "bad request", http.StatusBadRequest)
			return
		}
		handshake, err := a.handshakes.Consume(r.Context(), state, cookie.Value)
		if err != nil {
			a.logger.Warn("callback.consume_handshake", "err", err)
			a.writeFederationReject(r, "", "state_rejected")
			http.Error(w, "bad request", http.StatusBadRequest)
			return
		}
		redirectURI := a.publicBaseURL + "/oauth/google/callback"
		googleIdentity, err := a.idpProvider.ExchangeCode(r.Context(), code, redirectURI)
		if err != nil {
			a.logger.Warn("callback.exchange_code", "err", err)
			a.writeFederationReject(r, "", "code_exchange_failed")
			http.Error(w, "bad request", http.StatusBadRequest)
			return
		}
		// Federation policy: this app only admits verified identities from the
		// box's own Google Workspace. ExchangeCode proved the token is authentic;
		// the handler decides whether that authentic identity is allowed in.
		if !googleIdentity.EmailVerified {
			a.logger.Warn("callback.email_unverified", "email", googleIdentity.Email)
			a.writeFederationReject(r, googleIdentity.Email, "email_not_verified")
			http.Error(w, "forbidden", http.StatusForbidden)
			return
		}
		if googleIdentity.HostedDomain != a.workspaceDomain {
			a.logger.Warn("callback.wrong_workspace", "email", googleIdentity.Email, "hd", googleIdentity.HostedDomain)
			a.writeFederationReject(r, googleIdentity.Email, "workspace_domain")
			http.Error(w, "forbidden", http.StatusForbidden)
			return
		}
		ownerID, err := a.identity.ResolveOrCreate(r.Context(), identity.Claims{
			Iss: googleIdentity.Iss, Sub: googleIdentity.Sub, Email: googleIdentity.Email,
			Name: googleIdentity.Name, Picture: googleIdentity.Picture,
		})
		if err != nil {
			a.logger.Error("callback.resolve_identity", "err", err)
			http.Error(w, "internal server error", http.StatusInternalServerError)
			return
		}

		switch handshake.Origin {
		case oauthstate.OriginMCP:
			a.callbackMCP(w, r, handshake, googleIdentity.Email, ownerID)
		default:
			a.callbackWeb(w, r, googleIdentity.Email, ownerID)
		}
	}
}

// callbackWeb is the web-origin completion: it mints a web session, sets the
// session cookie, and redirects to the apex. Behavior (status, cookie, redirect)
// matches the pre-OAuth callback exactly.
func (a *app) callbackWeb(w http.ResponseWriter, r *http.Request, email, ownerID string) {
	issued, err := a.sessions.Create(r.Context(), email, ownerID)
	if err != nil {
		a.logger.Warn("callback.create_session", "email", email, "err", err)
		http.Error(w, "internal server error", http.StatusInternalServerError)
		return
	}
	_ = a.audit.Write(r.Context(), audit.Event{
		Type: audit.EventFederationSuccess, OwnerEmail: email,
		IP: r.RemoteAddr, UserAgent: r.Header.Get("User-Agent"),
		Details: map[string]any{"origin": "web"},
	})
	_ = a.audit.Write(r.Context(), audit.Event{
		Type: audit.EventSessionEstablished, OwnerEmail: email,
		IP: r.RemoteAddr, UserAgent: r.Header.Get("User-Agent"),
		Details: map[string]any{"session_id": issued.ID},
	})
	setSessionCookie(w, r, issued.CookieValue)
	http.Redirect(w, r, "/", http.StatusFound)
}

// callbackMCP is the MCP-origin completion: it issues an OAuth authorization
// code bound to the captured MCP request and redirects back to the originating
// MCP client carrying the code and the client's original state.
func (a *app) callbackMCP(w http.ResponseWriter, r *http.Request, handshake oauthstate.Handshake, email, ownerID string) {
	plaintext, _, err := a.oauthCodes.Issue(r.Context(), oauth.IssueParams{
		ClientID:            handshake.MCPClientID,
		OwnerEmail:          email,
		OwnerID:             ownerID,
		CodeChallenge:       handshake.MCPCodeChallenge,
		CodeChallengeMethod: handshake.MCPCodeChallengeMethod,
		RedirectURI:         handshake.MCPRedirectURI,
		Resource:            handshake.MCPResource,
		OriginalState:       handshake.MCPClientState,
	})
	if err != nil {
		a.logger.Error("callback.mcp.issue_code", "err", err)
		http.Error(w, "internal server error", http.StatusInternalServerError)
		return
	}
	_ = a.audit.Write(r.Context(), audit.Event{
		Type: audit.EventFederationSuccess, OwnerEmail: email, ClientID: handshake.MCPClientID,
		IP: r.RemoteAddr, UserAgent: r.Header.Get("User-Agent"),
		Details: map[string]any{"origin": "mcp"},
	})
	_ = a.audit.Write(r.Context(), audit.Event{
		Type: audit.EventAuthcodeIssued, OwnerEmail: email, ClientID: handshake.MCPClientID,
		IP: r.RemoteAddr, UserAgent: r.Header.Get("User-Agent"),
	})
	target, err := url.Parse(handshake.MCPRedirectURI)
	if err != nil {
		a.logger.Error("callback.mcp.bad_redirect", "err", err)
		http.Error(w, "internal server error", http.StatusInternalServerError)
		return
	}
	q := target.Query()
	q.Set("code", plaintext)
	q.Set("state", handshake.MCPClientState)
	target.RawQuery = q.Encode()
	http.Redirect(w, r, target.String(), http.StatusSeeOther)
}

// writeFederationReject records a federation.reject audit event with a reason.
func (a *app) writeFederationReject(r *http.Request, email, reason string) {
	if a.audit == nil {
		return
	}
	_ = a.audit.Write(r.Context(), audit.Event{
		Type: audit.EventFederationReject, OwnerEmail: email,
		IP: r.RemoteAddr, UserAgent: r.Header.Get("User-Agent"),
		Details: map[string]any{"reason": reason},
	})
}
