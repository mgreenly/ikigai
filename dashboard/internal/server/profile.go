package server

import (
	"bytes"
	"errors"
	"net/http"

	"dashboard/internal/session"
)

type profileData struct {
	Host         string
	Scheme       string
	Owner        string
	OwnerInitial string
	Grants       []grantRow
	PATs         []patRow
}

// handleProfile renders the signed-in user's account controls. Unlike the
// identity-aware index, /profile is session-gated: anonymous or dead-session
// requests go back to / without seeing profile content.
func (a *app) handleProfile() http.HandlerFunc {
	return func(w http.ResponseWriter, r *http.Request) {
		owner, ok := a.sessionOwner(r)
		if !ok {
			if c, err := r.Cookie(sessionCookieName); err == nil {
				if _, lerr := a.sessions.Lookup(r.Context(), c.Value); errors.Is(lerr, session.ErrInvalid) {
					clearSessionCookie(w, r)
				}
			}
			http.Redirect(w, r, "/", http.StatusFound)
			return
		}

		data := profileData{
			Host:         r.Host,
			Scheme:       requestScheme(r),
			Owner:        owner,
			OwnerInitial: ownerInitial(owner),
		}

		if chains, err := a.oauthTokens.ListChainsByOwner(r.Context(), owner); err != nil {
			a.logger.Error("profile.list_grants", "err", err)
		} else {
			data.Grants = grantRowsFromChains(chains)
		}

		if pats, err := a.pats.ListByOwner(r.Context(), owner); err != nil {
			a.logger.Error("profile.list_pats", "err", err)
		} else {
			data.PATs = patRowsFromPATs(pats)
		}

		var buf bytes.Buffer
		if err := a.tmpl.ExecuteTemplate(&buf, "profile.html", data); err != nil {
			a.logger.Error("profile.render", "err", err)
			http.Error(w, "internal server error", http.StatusInternalServerError)
			return
		}
		w.Header().Set("Content-Type", "text/html; charset=utf-8")
		w.WriteHeader(http.StatusOK)
		_, _ = w.Write(buf.Bytes())
	}
}
