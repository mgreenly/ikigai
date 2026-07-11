package server

// This file implements the dashboard's OAuth Authorization Server HTTP surface:
//
//	GET  /.well-known/oauth-authorization-server   AS metadata
//	POST /oauth/register                           dynamic client registration
//	GET  /oauth/authorize                          MCP-origin authorization
//	POST /oauth/token                              authorization_code + refresh_token
//	POST /oauth/introspect                         RFC 7662 (caller-bearer gated)
//	POST /oauth/revoke                             RFC 7009 (always 200)
//
// The dashboard is the suite's only authorization server; an external IdP
// (Google) authenticates the human, and this app mints its own opaque tokens.
// /oauth/authorize captures the MCP client's request, federates the user through
// the existing Google login leg (see callback.go), then resumes by issuing an
// authorization code. Handlers are methods on *app, reading the data-layer
// stores wired in server.go.

import (
	"context"
	"crypto/sha256"
	"database/sql"
	"encoding/base64"
	"encoding/json"
	"errors"
	"net/http"
	"net/url"
	"strings"
	"time"
	"unicode/utf8"

	"dashboard/internal/audit"
	"dashboard/internal/oauth"
	"dashboard/internal/oauthstate"
)

// ── metadata document ──────────────────────────────────────────────────────

// handleASMetadata serves the RFC 8414 authorization-server metadata document.
// The dashboard serves only AS metadata; there is no protected-resource-metadata
// endpoint here.
func (a *app) handleASMetadata() http.HandlerFunc {
	return func(w http.ResponseWriter, r *http.Request) {
		doc := map[string]any{
			"issuer":                                a.publicBaseURL,
			"authorization_endpoint":                a.publicBaseURL + "/oauth/authorize",
			"token_endpoint":                        a.publicBaseURL + "/oauth/token",
			"registration_endpoint":                 a.publicBaseURL + "/oauth/register",
			"introspection_endpoint":                a.publicBaseURL + "/oauth/introspect",
			"revocation_endpoint":                   a.publicBaseURL + "/oauth/revoke",
			"response_types_supported":              []string{"code"},
			"grant_types_supported":                 []string{"authorization_code", "refresh_token"},
			"code_challenge_methods_supported":      []string{"S256"},
			"token_endpoint_auth_methods_supported": []string{"none"},
		}
		writeJSON(w, http.StatusOK, doc)
	}
}

// ── dynamic client registration ────────────────────────────────────────────

type dcrRequest struct {
	RedirectURIs            []string `json:"redirect_uris"`
	ClientName              string   `json:"client_name,omitempty"`
	TokenEndpointAuthMethod string   `json:"token_endpoint_auth_method,omitempty"`
}

type dcrResponse struct {
	ClientID                string   `json:"client_id"`
	ClientName              string   `json:"client_name,omitempty"`
	RedirectURIs            []string `json:"redirect_uris"`
	TokenEndpointAuthMethod string   `json:"token_endpoint_auth_method"`
}

// handleDCRRegister implements RFC 7591 open dynamic client registration. Only
// public clients (token_endpoint_auth_method=none) are admitted; every redirect
// URI must be an absolute http(s) URL with a non-empty host and no fragment.
func (a *app) handleDCRRegister() http.HandlerFunc {
	return func(w http.ResponseWriter, r *http.Request) {
		var body dcrRequest
		if err := json.NewDecoder(r.Body).Decode(&body); err != nil {
			writeOAuthError(w, http.StatusBadRequest, "invalid_client_metadata", "malformed request body")
			a.writeAuditRejection(r, audit.EventDCRReject, "", "", map[string]any{"reason": "malformed_body"})
			return
		}
		if body.TokenEndpointAuthMethod != "" && body.TokenEndpointAuthMethod != "none" {
			writeOAuthError(w, http.StatusBadRequest, "invalid_client_metadata", "token_endpoint_auth_method must be 'none'")
			a.writeAuditRejection(r, audit.EventDCRReject, "", "", map[string]any{"reason": "auth_method"})
			return
		}
		if len(body.RedirectURIs) == 0 {
			writeOAuthError(w, http.StatusBadRequest, "invalid_redirect_uri", "redirect_uris must contain at least one entry")
			a.writeAuditRejection(r, audit.EventDCRReject, "", "", map[string]any{"reason": "no_redirect_uris"})
			return
		}
		for _, u := range body.RedirectURIs {
			if !isValidRedirectURI(u) {
				writeOAuthError(w, http.StatusBadRequest, "invalid_redirect_uri", "redirect_uri "+u+" is not an absolute http(s) URL with non-empty host and no fragment")
				a.writeAuditRejection(r, audit.EventDCRReject, "", "", map[string]any{"reason": "bad_redirect_uri", "uri": u})
				return
			}
		}
		name := strings.TrimSpace(body.ClientName)
		if !validClientName(name) {
			writeOAuthError(w, http.StatusBadRequest, "invalid_client_metadata", "client_name invalid")
			a.writeAuditRejection(r, audit.EventDCRReject, "", "", map[string]any{"reason": "bad_client_name"})
			return
		}
		c, err := a.oauthClients.Register(r.Context(), name, body.RedirectURIs)
		if err != nil {
			a.logger.Error("dcr.register", "err", err)
			writeOAuthError(w, http.StatusInternalServerError, "server_error", "could not register client")
			return
		}
		_ = a.audit.Write(r.Context(), audit.Event{
			Type:      audit.EventDCRSuccess,
			ClientID:  c.ClientID,
			IP:        r.RemoteAddr,
			UserAgent: r.Header.Get("User-Agent"),
			Details:   map[string]any{"client_name": name, "redirect_uris": body.RedirectURIs},
		})
		writeJSON(w, http.StatusCreated, dcrResponse{
			ClientID:                c.ClientID,
			ClientName:              c.ClientName,
			RedirectURIs:            c.RedirectURIs,
			TokenEndpointAuthMethod: "none",
		})
	}
}

// ── authorize ──────────────────────────────────────────────────────────────

// handleAuthorize is the MCP-origin authorization endpoint. It validates the
// client's request (PKCE S256, registered redirect_uri, a configured resource),
// captures it in an MCP-origin handshake bound to the browser, and redirects to
// Google to federate the human. The callback (see callback.go) resumes the
// captured request by issuing an authorization code.
func (a *app) handleAuthorize() http.HandlerFunc {
	return func(w http.ResponseWriter, r *http.Request) {
		q := r.URL.Query()
		clientID := q.Get("client_id")
		responseType := q.Get("response_type")
		redirectURI := q.Get("redirect_uri")
		codeChallenge := q.Get("code_challenge")
		codeChallengeMethod := q.Get("code_challenge_method")
		clientState := q.Get("state")
		resource := q.Get("resource")

		if responseType != "code" {
			writeOAuthError(w, http.StatusBadRequest, "unsupported_response_type", "response_type must be 'code'")
			return
		}
		if codeChallenge == "" {
			writeOAuthError(w, http.StatusBadRequest, "invalid_request", "code_challenge required")
			return
		}
		if codeChallengeMethod != "S256" {
			writeOAuthError(w, http.StatusBadRequest, "invalid_request", "code_challenge_method must be 'S256'")
			return
		}
		// Multi-resource: resource is required and must be one of the configured
		// set. The bound code carries it forward onto the issued chain.
		if resource == "" {
			writeOAuthError(w, http.StatusBadRequest, "invalid_target", "resource is required")
			return
		}
		if !containsString(a.resources, resource) {
			writeOAuthError(w, http.StatusBadRequest, "invalid_target", "unknown resource")
			return
		}
		client, err := a.oauthClients.Get(r.Context(), clientID)
		if err != nil {
			writeOAuthError(w, http.StatusBadRequest, "invalid_client", "unknown client_id")
			return
		}
		if !containsString(client.RedirectURIs, redirectURI) {
			writeOAuthError(w, http.StatusBadRequest, "invalid_request", "redirect_uri does not match any registered URI")
			return
		}

		handshake, cookie, err := a.handshakes.CreateMCP(r.Context(), oauthstate.MCPContext{
			ClientID:            clientID,
			RedirectURI:         redirectURI,
			CodeChallenge:       codeChallenge,
			CodeChallengeMethod: codeChallengeMethod,
			ClientState:         clientState,
			Resource:            resource,
		})
		if err != nil {
			a.logger.Error("authorize.create_handshake", "err", err)
			writeOAuthError(w, http.StatusInternalServerError, "server_error", "could not create state")
			return
		}
		setBindingCookie(w, r, cookie)
		http.Redirect(w, r, a.idpProvider.AuthorizeURL(handshake.ID, a.publicBaseURL+"/oauth/google/callback"), http.StatusSeeOther)
	}
}

// ── token endpoint ─────────────────────────────────────────────────────────

type tokenResponse struct {
	AccessToken  string `json:"access_token"`
	TokenType    string `json:"token_type"`
	ExpiresIn    int    `json:"expires_in"`
	RefreshToken string `json:"refresh_token"`
	Scope        string `json:"scope,omitempty"`
}

// handleToken dispatches the token endpoint on grant_type. Only
// authorization_code and refresh_token are supported.
func (a *app) handleToken() http.HandlerFunc {
	return func(w http.ResponseWriter, r *http.Request) {
		w.Header().Set("Cache-Control", "no-store")
		w.Header().Set("Pragma", "no-cache")

		if err := r.ParseForm(); err != nil {
			writeOAuthError(w, http.StatusBadRequest, "invalid_request", "could not parse form")
			return
		}
		switch r.PostForm.Get("grant_type") {
		case "authorization_code":
			a.handleTokenAuthCode(w, r)
		case "refresh_token":
			a.handleTokenRefresh(w, r)
		default:
			writeOAuthError(w, http.StatusBadRequest, "unsupported_grant_type", "grant_type must be 'authorization_code' or 'refresh_token'")
		}
	}
}

// handleTokenAuthCode redeems an authorization code for a fresh token chain.
// Reuse of an already-redeemed code cascade-revokes the chain it produced. The
// bound code's resource is authoritative: a mismatching resource form param is
// rejected, but set membership is not re-checked here (it was at authorize time).
func (a *app) handleTokenAuthCode(w http.ResponseWriter, r *http.Request) {
	code := r.PostForm.Get("code")
	clientID := r.PostForm.Get("client_id")
	redirectURI := r.PostForm.Get("redirect_uri")
	verifier := r.PostForm.Get("code_verifier")
	resource := r.PostForm.Get("resource")
	if code == "" || clientID == "" || redirectURI == "" || verifier == "" {
		writeOAuthError(w, http.StatusBadRequest, "invalid_request", "missing required parameter")
		return
	}
	tx, err := a.db.BeginTx(r.Context(), nil)
	if err != nil {
		writeOAuthError(w, http.StatusInternalServerError, "server_error", "tx begin")
		return
	}
	defer tx.Rollback()

	ac, err := a.oauthCodes.LookupTx(r.Context(), tx, code)
	if err != nil {
		writeOAuthError(w, http.StatusBadRequest, "invalid_grant", "authorization code not found")
		a.auditAfterRollback(r.Context(), tx, audit.Event{Type: audit.EventTokenReject, ClientID: clientID, IP: r.RemoteAddr, UserAgent: r.Header.Get("User-Agent"), Details: map[string]any{"reason": "code_not_found"}})
		return
	}
	// A second presentation of a redeemed code cascade-revokes its chain.
	if ac.UsedAt != nil {
		if ac.ChainID != nil {
			_ = a.oauthTokens.RevokeChainTx(r.Context(), tx, *ac.ChainID)
			_ = a.audit.WriteTx(r.Context(), tx, audit.Event{
				Type:     audit.EventReuseDetected,
				ClientID: clientID, ChainID: *ac.ChainID, IP: r.RemoteAddr, UserAgent: r.Header.Get("User-Agent"),
				Details: map[string]any{"reason": "authcode_reuse"},
			})
			_ = tx.Commit()
		}
		writeOAuthError(w, http.StatusBadRequest, "invalid_grant", "authorization code already used")
		return
	}
	now := time.Now().UTC()
	if !now.Before(ac.ExpiresAt) {
		writeOAuthError(w, http.StatusBadRequest, "invalid_grant", "authorization code expired")
		return
	}
	if ac.ClientID != clientID {
		writeOAuthError(w, http.StatusBadRequest, "invalid_grant", "client_id does not match bound code")
		a.auditAfterRollback(r.Context(), tx, audit.Event{Type: audit.EventTokenReject, ClientID: clientID, IP: r.RemoteAddr, UserAgent: r.Header.Get("User-Agent"), Details: map[string]any{"reason": "client_mismatch"}})
		return
	}
	if ac.RedirectURI != redirectURI {
		writeOAuthError(w, http.StatusBadRequest, "invalid_grant", "redirect_uri does not match bound code")
		a.auditAfterRollback(r.Context(), tx, audit.Event{Type: audit.EventTokenReject, ClientID: clientID, IP: r.RemoteAddr, UserAgent: r.Header.Get("User-Agent"), Details: map[string]any{"reason": "redirect_mismatch"}})
		return
	}
	if resource != "" && resource != ac.Resource {
		writeOAuthError(w, http.StatusBadRequest, "invalid_target", "resource does not match bound code")
		a.auditAfterRollback(r.Context(), tx, audit.Event{Type: audit.EventTokenReject, ClientID: clientID, IP: r.RemoteAddr, UserAgent: r.Header.Get("User-Agent"), Details: map[string]any{"reason": "resource_mismatch"}})
		return
	}
	if !pkceS256Matches(verifier, ac.CodeChallenge) {
		writeOAuthError(w, http.StatusBadRequest, "invalid_grant", "PKCE verifier does not match challenge")
		a.auditAfterRollback(r.Context(), tx, audit.Event{Type: audit.EventTokenReject, ClientID: clientID, IP: r.RemoteAddr, UserAgent: r.Header.Get("User-Agent"), Details: map[string]any{"reason": "pkce_mismatch"}})
		return
	}
	pair, err := a.oauthTokens.IssueChainAndTokens(r.Context(), tx, ac.ClientID, ac.OwnerEmail, ac.OwnerID, ac.Resource)
	if err != nil {
		a.logger.Error("token.issue", "err", err)
		writeOAuthError(w, http.StatusInternalServerError, "server_error", "could not issue tokens")
		return
	}
	if err := a.oauthCodes.MarkUsed(r.Context(), tx, ac.ID, pair.ChainID); err != nil {
		a.logger.Error("token.mark_used", "err", err)
		writeOAuthError(w, http.StatusInternalServerError, "server_error", "could not finalize code")
		return
	}
	_ = a.audit.WriteTx(r.Context(), tx, audit.Event{
		Type: audit.EventTokenIssued, OwnerEmail: ac.OwnerEmail, ClientID: ac.ClientID, ChainID: pair.ChainID,
		IP: r.RemoteAddr, UserAgent: r.Header.Get("User-Agent"),
	})
	if err := tx.Commit(); err != nil {
		a.logger.Error("token.commit", "err", err)
		writeOAuthError(w, http.StatusInternalServerError, "server_error", "commit")
		return
	}
	_ = a.oauthClients.TouchLastUsed(r.Context(), ac.ClientID)
	// A new grant was born — wake the owner's live index.
	a.grantEvents.Publish(ac.OwnerEmail)

	writeJSON(w, http.StatusOK, tokenResponse{
		AccessToken:  pair.AccessToken,
		TokenType:    "Bearer",
		ExpiresIn:    int(a.oauthTokens.AccessTTL.Seconds()),
		RefreshToken: pair.RefreshToken,
	})
}

// handleTokenRefresh rotates a refresh token. Reuse of an already-used (or a
// revoked) refresh token cascade-revokes the whole chain. The owner's workspace
// membership is rechecked on every rotation.
func (a *app) handleTokenRefresh(w http.ResponseWriter, r *http.Request) {
	refresh := r.PostForm.Get("refresh_token")
	clientID := r.PostForm.Get("client_id")
	if refresh == "" || clientID == "" {
		writeOAuthError(w, http.StatusBadRequest, "invalid_request", "missing required parameter")
		return
	}
	tx, err := a.db.BeginTx(r.Context(), nil)
	if err != nil {
		writeOAuthError(w, http.StatusInternalServerError, "server_error", "tx begin")
		return
	}
	defer tx.Rollback()

	tok, chain, err := a.oauthTokens.LookupRefreshTx(r.Context(), tx, refresh)
	if errors.Is(err, oauth.ErrBadPrefix) || errors.Is(err, oauth.ErrNotFound) {
		writeOAuthError(w, http.StatusBadRequest, "invalid_grant", "refresh token not found")
		a.auditAfterRollback(r.Context(), tx, audit.Event{Type: audit.EventTokenReject, ClientID: clientID, IP: r.RemoteAddr, UserAgent: r.Header.Get("User-Agent"), Details: map[string]any{"reason": "refresh_not_found"}})
		return
	}
	if err != nil {
		writeOAuthError(w, http.StatusInternalServerError, "server_error", "lookup")
		return
	}
	// Client must match the bound chain; a mismatch does not consume the token.
	if chain.ClientID != clientID {
		writeOAuthError(w, http.StatusBadRequest, "invalid_grant", "client_id mismatch")
		return
	}
	// Reuse cascade: an already-used or revoked token revokes the whole chain.
	if tok.UsedAt != nil || tok.RevokedAt != nil || chain.RevokedAt != nil {
		_ = a.oauthTokens.RevokeChainTx(r.Context(), tx, chain.ID)
		_ = a.audit.WriteTx(r.Context(), tx, audit.Event{
			Type: audit.EventReuseDetected, OwnerEmail: chain.OwnerEmail, ClientID: chain.ClientID, ChainID: chain.ID,
			IP: r.RemoteAddr, UserAgent: r.Header.Get("User-Agent"),
			Details: map[string]any{"reason": "refresh_reuse"},
		})
		_ = tx.Commit()
		writeOAuthError(w, http.StatusBadRequest, "invalid_grant", "refresh token already used or revoked")
		return
	}
	now := time.Now().UTC()
	if !now.Before(tok.ExpiresAt) {
		writeOAuthError(w, http.StatusBadRequest, "invalid_grant", "refresh token expired")
		return
	}
	if !ownerInWorkspace(chain.OwnerEmail, a.workspaceDomain) {
		writeOAuthError(w, http.StatusBadRequest, "invalid_grant", "owner outside configured workspace")
		a.auditAfterRollback(r.Context(), tx, audit.Event{Type: audit.EventTokenReject, OwnerEmail: chain.OwnerEmail, ClientID: clientID, ChainID: chain.ID, IP: r.RemoteAddr, UserAgent: r.Header.Get("User-Agent"), Details: map[string]any{"reason": "workspace_mismatch"}})
		return
	}
	if err := a.oauthTokens.MarkRefreshUsed(r.Context(), tx, tok.ID); err != nil {
		writeOAuthError(w, http.StatusInternalServerError, "server_error", "mark used")
		return
	}
	pair, err := a.oauthTokens.IssueSuccessorTokensTx(r.Context(), tx, chain.ID)
	if err != nil {
		writeOAuthError(w, http.StatusInternalServerError, "server_error", "issue successor")
		return
	}
	_ = a.audit.WriteTx(r.Context(), tx, audit.Event{
		Type: audit.EventTokenRefreshed, OwnerEmail: chain.OwnerEmail, ClientID: chain.ClientID, ChainID: chain.ID,
		IP: r.RemoteAddr, UserAgent: r.Header.Get("User-Agent"),
	})
	if err := tx.Commit(); err != nil {
		writeOAuthError(w, http.StatusInternalServerError, "server_error", "commit")
		return
	}
	_ = a.oauthClients.TouchLastUsed(r.Context(), clientID)
	// A rotation refreshes the grant's last-used time — wake the owner's index.
	a.grantEvents.Publish(chain.OwnerEmail)
	writeJSON(w, http.StatusOK, tokenResponse{
		AccessToken:  pair.AccessToken,
		TokenType:    "Bearer",
		ExpiresIn:    int(a.oauthTokens.AccessTTL.Seconds()),
		RefreshToken: pair.RefreshToken,
	})
}

// ── introspect / revoke ────────────────────────────────────────────────────

type introspectResponse struct {
	Active   bool   `json:"active"`
	ClientID string `json:"client_id,omitempty"`
	Username string `json:"username,omitempty"`
	Exp      int64  `json:"exp,omitempty"`
	Iat      int64  `json:"iat,omitempty"`
	Resource string `json:"resource,omitempty"`
}

// handleIntrospect implements RFC 7662, gated on a caller-bearer access token.
// The caller may introspect a token only if they are an admin or own the same
// chain; otherwise the token is reported inactive.
func (a *app) handleIntrospect() http.HandlerFunc {
	return func(w http.ResponseWriter, r *http.Request) {
		w.Header().Set("Cache-Control", "no-store")
		callerToken, ok := bearerFromHeader(r)
		if !ok {
			writeOAuthError(w, http.StatusUnauthorized, "invalid_request", "missing bearer")
			return
		}
		caller, err := a.oauthTokens.ValidateAccess(r.Context(), callerToken)
		if err != nil || !ownerInWorkspace(caller.Chain.OwnerEmail, a.workspaceDomain) {
			writeOAuthError(w, http.StatusUnauthorized, "invalid_token", "caller token not valid")
			return
		}
		if err := r.ParseForm(); err != nil {
			writeJSON(w, http.StatusOK, introspectResponse{Active: false})
			return
		}
		body := r.PostForm.Get("token")
		if body == "" {
			writeJSON(w, http.StatusOK, introspectResponse{Active: false})
			return
		}
		if !strings.HasPrefix(body, oauth.AccessPrefix) {
			writeJSON(w, http.StatusOK, introspectResponse{Active: false})
			return
		}
		bodyTok, bodyErr := a.oauthTokens.ValidateAccess(r.Context(), body)
		if bodyErr != nil {
			writeJSON(w, http.StatusOK, introspectResponse{Active: false})
			return
		}
		isAdmin := containsString(a.admins, caller.Chain.OwnerEmail)
		if !isAdmin && bodyTok.Chain.OwnerEmail != caller.Chain.OwnerEmail {
			writeJSON(w, http.StatusOK, introspectResponse{Active: false})
			return
		}
		if !ownerInWorkspace(bodyTok.Chain.OwnerEmail, a.workspaceDomain) {
			writeJSON(w, http.StatusOK, introspectResponse{Active: false})
			return
		}
		writeJSON(w, http.StatusOK, introspectResponse{
			Active:   true,
			ClientID: bodyTok.Chain.ClientID,
			Username: bodyTok.Chain.OwnerEmail,
			Exp:      bodyTok.Token.ExpiresAt.Unix(),
			Iat:      bodyTok.Token.IssuedAt.Unix(),
			Resource: bodyTok.Chain.Resource,
		})
	}
}

// handleRevoke implements RFC 7009: it revokes the chain behind the presented
// access or refresh token and always returns 200, regardless of whether the
// token was known.
func (a *app) handleRevoke() http.HandlerFunc {
	return func(w http.ResponseWriter, r *http.Request) {
		w.Header().Set("Cache-Control", "no-store")
		if err := r.ParseForm(); err != nil {
			writeOAuthError(w, http.StatusBadRequest, "invalid_request", "could not parse form")
			return
		}
		tokenStr := r.PostForm.Get("token")
		if tokenStr == "" {
			w.WriteHeader(http.StatusOK)
			return
		}
		var chainID, owner, client string
		switch {
		case strings.HasPrefix(tokenStr, oauth.AccessPrefix):
			if vt, err := a.oauthTokens.ValidateAccess(r.Context(), tokenStr); err == nil {
				chainID = vt.Chain.ID
				owner = vt.Chain.OwnerEmail
				client = vt.Chain.ClientID
			}
		case strings.HasPrefix(tokenStr, oauth.RefreshPrefix):
			if tx, err := a.db.BeginTx(r.Context(), nil); err == nil {
				_, chain, lerr := a.oauthTokens.LookupRefreshTx(r.Context(), tx, tokenStr)
				tx.Rollback()
				if lerr == nil {
					chainID = chain.ID
					owner = chain.OwnerEmail
					client = chain.ClientID
				}
			}
		}
		if chainID != "" {
			if err := a.oauthTokens.RevokeChain(r.Context(), chainID); err != nil {
				writeOAuthError(w, http.StatusInternalServerError, "server_error", "revoke failed")
				return
			}
			_ = a.audit.Write(r.Context(), audit.Event{
				Type: audit.EventChainRevoked, OwnerEmail: owner, ClientID: client, ChainID: chainID,
				IP: r.RemoteAddr, UserAgent: r.Header.Get("User-Agent"),
				Details: map[string]any{"trigger": "explicit_revoke"},
			})
			// The grant is gone — wake the owner's live index.
			a.grantEvents.Publish(owner)
		}
		w.WriteHeader(http.StatusOK)
	}
}

// ── helpers ────────────────────────────────────────────────────────────────

// writeJSON encodes v as a JSON response body with the given status.
func writeJSON(w http.ResponseWriter, status int, v any) {
	w.Header().Set("Content-Type", "application/json; charset=utf-8")
	w.WriteHeader(status)
	_ = json.NewEncoder(w).Encode(v)
}

// writeOAuthError writes an RFC 6749 error object with the given status.
func writeOAuthError(w http.ResponseWriter, status int, code, desc string) {
	w.Header().Set("Cache-Control", "no-store")
	w.Header().Set("Content-Type", "application/json; charset=utf-8")
	w.WriteHeader(status)
	_ = json.NewEncoder(w).Encode(map[string]string{
		"error":             code,
		"error_description": desc,
	})
}

// pkceS256Matches reports whether base64url(sha256(verifier)) equals challenge.
func pkceS256Matches(verifier, challenge string) bool {
	sum := sha256.Sum256([]byte(verifier))
	got := base64.RawURLEncoding.EncodeToString(sum[:])
	return got == challenge
}

// containsString reports whether want is in list.
func containsString(list []string, want string) bool {
	for _, s := range list {
		if s == want {
			return true
		}
	}
	return false
}

// ownerInWorkspace reports whether email's domain matches domain (case-insensitive).
func ownerInWorkspace(email, domain string) bool {
	at := strings.LastIndexByte(email, '@')
	if at < 0 {
		return false
	}
	return strings.EqualFold(email[at+1:], domain)
}

// bearerFromHeader extracts a non-empty Bearer token from the Authorization header.
func bearerFromHeader(r *http.Request) (string, bool) {
	h := r.Header.Get("Authorization")
	if h == "" {
		return "", false
	}
	const p = "Bearer "
	if !strings.HasPrefix(h, p) {
		return "", false
	}
	v := strings.TrimSpace(h[len(p):])
	if v == "" {
		return "", false
	}
	return v, true
}

// isValidRedirectURI reports whether s is an absolute http(s) URL with a
// non-empty host and no fragment.
func isValidRedirectURI(s string) bool {
	u, err := url.Parse(s)
	if err != nil {
		return false
	}
	if u.Scheme != "http" && u.Scheme != "https" {
		return false
	}
	if u.Host == "" {
		return false
	}
	if u.Fragment != "" {
		return false
	}
	return u.IsAbs()
}

// validClientName bounds an optional client name: at most 80 runes, no control
// characters. An empty name is valid (treated as unset).
func validClientName(name string) bool {
	if name == "" {
		return true
	}
	if utf8.RuneCountInString(name) > 80 {
		return false
	}
	for _, r := range name {
		if r < 0x20 || r == 0x7f {
			return false
		}
	}
	return true
}

// auditAfterRollback records a token-grant rejection that was detected while a
// transaction held the (single) SQLite connection. The rejection path performed
// no writes, so it first rolls tx back to free the connection — then writes the
// audit row on a fresh one. Writing through the still-open tx would either be
// undone by the deferred rollback (losing the audit) or, via the non-tx
// audit.Write, deadlock waiting for a connection the tx still holds.
func (a *app) auditAfterRollback(ctx context.Context, tx *sql.Tx, e audit.Event) {
	_ = tx.Rollback()
	_ = a.audit.Write(ctx, e)
}

// writeAuditRejection writes a rejection audit event with the request's IP and
// User-Agent attached.
func (a *app) writeAuditRejection(r *http.Request, t audit.EventType, owner, client string, details map[string]any) {
	if a.audit == nil {
		return
	}
	_ = a.audit.Write(r.Context(), audit.Event{
		Type: t, OwnerEmail: owner, ClientID: client,
		IP: r.RemoteAddr, UserAgent: r.Header.Get("User-Agent"),
		Details: details,
	})
}
