package server

// This file implements the profile page's "live grants" feature: the
// list of the signed-in user's MCP token grants (OAuth chains), kept fresh over
// Server-Sent Events, with per-grant revocation.
//
//	GET  /grants/stream            SSE — emits a "chains" event whenever the
//	                                user's grants change (issue/refresh/revoke).
//	GET  /grants/fragment          the grants-list HTML partial (no full page).
//	POST /grants/{public_id}/revoke web revocation of one grant.
//
// All three authenticate via the web session (NOT bearer, NOT auth_request, NOT
// loopback) — this is the one allowed web-session → MCP-chain cross-action. URLs
// use the chain's public_id, never the internal PK. Same-origin is enforced on
// the state-changing revoke route.

import (
	"errors"
	"fmt"
	"net/http"
	"net/url"
	"time"

	"dashboard/internal/audit"
	"dashboard/internal/oauth"
	"dashboard/internal/session"
)

// sessionOwner resolves the signed-in owner from the request's session cookie.
// It returns ("", false) when no live session backs the request. Unlike the
// index page it does not clear dead cookies — the authed grant routes simply
// refuse, and the next index render handles cleanup. This is the shared
// authentication seam for handleIndex and the three grant handlers.
func (a *app) sessionOwner(r *http.Request) (ownerEmail string, signedIn bool) {
	c, err := r.Cookie(sessionCookieName)
	if err != nil {
		return "", false
	}
	sess, lerr := a.sessions.Lookup(r.Context(), c.Value)
	if lerr != nil {
		if !errors.Is(lerr, session.ErrInvalid) {
			a.logger.Error("grants.session_lookup", "err", lerr)
		}
		return "", false
	}
	return sess.OwnerEmail, true
}

// requireSession resolves the visitor's web session or refuses with 401. On
// not-ok the response has already been written.
func (a *app) requireSession(w http.ResponseWriter, r *http.Request) (string, bool) {
	owner, ok := a.sessionOwner(r)
	if !ok {
		http.Error(w, "sign-in required", http.StatusUnauthorized)
		return "", false
	}
	return owner, true
}

// requireSessionIdentity returns the signed-in session's email and durable
// identity handle. PAT creation carries this already-resolved handle forward
// rather than looking the identity up again.
func (a *app) requireSessionIdentity(w http.ResponseWriter, r *http.Request) (string, string, bool) {
	c, err := r.Cookie(sessionCookieName)
	if err != nil {
		http.Error(w, "sign-in required", http.StatusUnauthorized)
		return "", "", false
	}
	sess, err := a.sessions.Lookup(r.Context(), c.Value)
	if err != nil {
		http.Error(w, "sign-in required", http.StatusUnauthorized)
		return "", "", false
	}
	return sess.OwnerEmail, sess.OwnerID, true
}

// handleGrantsStream is the SSE endpoint. It clears the per-connection write
// deadline (the server pins a 15s WriteTimeout, which would otherwise tear the
// stream down), subscribes to the owner's grant-change notifications, and emits
// a "chains" event on every notify plus a periodic keepalive comment.
func (a *app) handleGrantsStream() http.HandlerFunc {
	return func(w http.ResponseWriter, r *http.Request) {
		owner, ok := a.requireSession(w, r)
		if !ok {
			return
		}
		rc := http.NewResponseController(w)
		// Clear the per-response WriteTimeout deadline so the long-lived stream
		// is not torn down at the server's 15s write ceiling. Best-effort: a
		// ResponseWriter that does not support deadlines (e.g. a test recorder)
		// returns an error we tolerate — the production *http.Server connection
		// does support it.
		_ = rc.SetWriteDeadline(time.Time{})
		w.Header().Set("Content-Type", "text/event-stream")
		w.Header().Set("Cache-Control", "no-store")
		w.Header().Set("Connection", "keep-alive")
		w.WriteHeader(http.StatusOK)
		if err := rc.Flush(); err != nil {
			return
		}

		sub, cancel := a.grantEvents.Subscribe(owner)
		defer cancel()

		const keepalive = 15 * time.Second
		tick := time.NewTicker(keepalive)
		defer tick.Stop()

		// Emit an initial event so the client immediately reflects state.
		fmt.Fprint(w, "event: chains\ndata: {}\n\n")
		_ = rc.Flush()

		ctx := r.Context()
		for {
			select {
			case <-ctx.Done():
				return
			case <-sub:
				fmt.Fprint(w, "event: chains\ndata: {}\n\n")
				if err := rc.Flush(); err != nil {
					return
				}
			case <-tick.C:
				fmt.Fprint(w, ": keepalive\n\n")
				if err := rc.Flush(); err != nil {
					return
				}
			}
		}
	}
}

// handleGrantsFragment renders only the grants-list partial for the signed-in
// user — the SSE client swaps this into #grants-block on every "chains" event.
func (a *app) handleGrantsFragment() http.HandlerFunc {
	return func(w http.ResponseWriter, r *http.Request) {
		owner, ok := a.requireSession(w, r)
		if !ok {
			return
		}
		chains, err := a.oauthTokens.ListChainsByOwner(r.Context(), owner)
		if err != nil {
			a.logger.Error("grants.list", "err", err)
			http.Error(w, "internal error", http.StatusInternalServerError)
			return
		}
		w.Header().Set("Content-Type", "text/html; charset=utf-8")
		w.Header().Set("Cache-Control", "no-store")
		if err := a.tmpl.ExecuteTemplate(w, "grants_block", grantRowsFromChains(chains)); err != nil {
			a.logger.Error("grants.fragment.render", "err", err)
		}
	}
}

// handleGrantRevoke revokes one of the signed-in user's grants by public_id.
// State-changing, so same-origin is enforced. A chain that is missing, not
// owned by the caller, or already revoked is reported indistinguishably as
// not-found. On success it publishes a grant-change notify and redirects to /profile.
func (a *app) handleGrantRevoke() http.HandlerFunc {
	return func(w http.ResponseWriter, r *http.Request) {
		if !sameOrigin(r, a.publicBaseURL) {
			http.Error(w, "cross-origin request rejected", http.StatusForbidden)
			return
		}
		owner, ok := a.requireSession(w, r)
		if !ok {
			return
		}
		publicID := r.PathValue("public_id")
		chain, err := a.oauthTokens.GetChainByPublicID(r.Context(), publicID)
		if err != nil || chain.OwnerEmail != owner || chain.RevokedAt != nil {
			// Deliberately indistinguishable from not-found: a signed-in user
			// must not be able to probe other owners' public_ids.
			http.NotFound(w, r)
			return
		}
		if err := a.oauthTokens.RevokeChain(r.Context(), chain.ID); err != nil {
			a.logger.Error("grants.revoke", "err", err)
			http.Error(w, "internal error", http.StatusInternalServerError)
			return
		}
		_ = a.audit.Write(r.Context(), audit.Event{
			Type: audit.EventChainRevoked, OwnerEmail: chain.OwnerEmail, ClientID: chain.ClientID, ChainID: chain.ID,
			IP: r.RemoteAddr, UserAgent: r.Header.Get("User-Agent"),
			Details: map[string]any{"trigger": "user_revoke"},
		})
		a.grantEvents.Publish(owner)
		http.Redirect(w, r, "/profile", http.StatusSeeOther)
	}
}

// grantRow is the view model for one row in the grants block.
type grantRow struct {
	PublicID    string
	Label       string
	LastUsedISO string
	LastUsedRel string
}

func grantRowsFromChains(chains []oauth.ChainSummary) []grantRow {
	out := make([]grantRow, 0, len(chains))
	now := time.Now().UTC()
	for _, c := range chains {
		out = append(out, grantRow{
			PublicID:    c.PublicID,
			Label:       grantLabel(c.ClientName, c.PublicID),
			LastUsedISO: c.LastUsedAt.UTC().Format(time.RFC3339),
			LastUsedRel: relativeTime(c.LastUsedAt, now),
		})
	}
	return out
}

// grantLabel renders a human label for a grant: the client name (if any) plus a
// short discriminator drawn from the public_id's tail. Not pre-escaped — the
// template renders it through html/template's contextual auto-escaping.
func grantLabel(clientName, publicID string) string {
	disc := publicID
	if len(disc) > 4 {
		disc = disc[len(disc)-4:]
	}
	if clientName != "" {
		return clientName + " · " + disc
	}
	return "client " + disc
}

// relativeTime renders durations like "5 minutes ago".
func relativeTime(then, now time.Time) string {
	if then.IsZero() {
		return "never"
	}
	d := now.Sub(then)
	if d < 0 {
		d = 0
	}
	switch {
	case d < time.Minute:
		return "just now"
	case d < time.Hour:
		return fmt.Sprintf("%d minutes ago", int(d/time.Minute))
	case d < 24*time.Hour:
		return fmt.Sprintf("%d hours ago", int(d/time.Hour))
	default:
		return fmt.Sprintf("%d days ago", int(d/(24*time.Hour)))
	}
}

// sameOrigin checks the request's Origin or (fallback) Referer header matches
// the configured public base URL. A request lacking both is rejected as
// cross-origin.
func sameOrigin(r *http.Request, publicBase string) bool {
	if origin := r.Header.Get("Origin"); origin != "" {
		return origin == publicBase
	}
	if referer := r.Header.Get("Referer"); referer != "" {
		u, err := url.Parse(referer)
		if err != nil {
			return false
		}
		return u.Scheme+"://"+u.Host == publicBase
	}
	return false
}
