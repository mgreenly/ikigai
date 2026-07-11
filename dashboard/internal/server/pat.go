package server

// This file implements the logged-in index page's "personal access tokens"
// (PAT) feature: owner-minted, cross-service opaque bearer tokens the signed-in
// user creates, copies once, lists, and revokes from the profile page.
//
//	POST /pat                     mint a PAT; renders the show-once confirmation.
//	POST /pat/{public_id}/revoke  web revocation of one PAT.
//
// Both authenticate via the web session (NOT bearer, NOT auth_request) — the
// same web-session seam the grant routes use. URLs use the PAT's public_id,
// never the internal PK. Same-origin is enforced on both state-changing routes,
// mirroring handleGrantRevoke. Unlike grants, PATs have no SSE live-stream: a
// PAT row only changes through an explicit create/revoke the user performs here,
// so the list is rendered inline in handleProfile.

import (
	"bytes"
	"net/http"
	"strings"
	"time"

	"dashboard/internal/audit"
	"dashboard/internal/pat"
)

// maxPATLabelLen bounds the required, user-supplied PAT label (ADR §D8).
const maxPATLabelLen = 48

// handlePATCreate mints a PAT for the signed-in user and renders the show-once
// confirmation directly in the 200 response (no PRG — a redirect would discard
// the one-time plaintext; ADR §D10). Same-origin is enforced first, then the
// session, mirroring handleGrantRevoke. The label is required: TrimSpace'd and
// rejected if empty or over 48 characters.
func (a *app) handlePATCreate() http.HandlerFunc {
	return func(w http.ResponseWriter, r *http.Request) {
		if !sameOrigin(r, a.publicBaseURL) {
			http.Error(w, "cross-origin request rejected", http.StatusForbidden)
			return
		}
		owner, ownerID, ok := a.requireSessionIdentity(w, r)
		if !ok {
			return
		}
		label := strings.TrimSpace(r.FormValue("label"))
		if label == "" || len(label) > maxPATLabelLen {
			http.Error(w, "a label is required (1–48 characters)", http.StatusBadRequest)
			return
		}
		plaintext, p, err := a.pats.Create(r.Context(), owner, ownerID, label)
		if err != nil {
			a.logger.Error("pat.create", "err", err)
			http.Error(w, "internal error", http.StatusInternalServerError)
			return
		}
		_ = a.audit.Write(r.Context(), audit.Event{
			Type: audit.EventPATCreated, OwnerEmail: owner,
			IP: r.RemoteAddr, UserAgent: r.Header.Get("User-Agent"),
			Details: map[string]any{"public_id": p.PublicID, "label": p.Label},
		})

		// Buffer-render the dedicated show-once template so a render failure is a
		// clean 500 rather than a half-written 200 that leaked a useless secret.
		var buf bytes.Buffer
		if err := a.tmpl.ExecuteTemplate(&buf, "pat_created", patCreated{Label: p.Label, Secret: plaintext}); err != nil {
			a.logger.Error("pat.created.render", "err", err)
			http.Error(w, "internal server error", http.StatusInternalServerError)
			return
		}
		w.Header().Set("Content-Type", "text/html; charset=utf-8")
		w.Header().Set("Cache-Control", "no-store")
		w.WriteHeader(http.StatusOK)
		_, _ = w.Write(buf.Bytes())
	}
}

// handlePATRevoke revokes one of the signed-in user's PATs by public_id.
// State-changing, so same-origin is enforced. A PAT that is missing, not owned
// by the caller, or already revoked is reported indistinguishably as not-found
// (a signed-in user must not be able to probe other owners' public_ids). On
// success it redirects to /profile, where the re-rendered list reflects the change.
func (a *app) handlePATRevoke() http.HandlerFunc {
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
		p, err := a.pats.GetByPublicID(r.Context(), publicID)
		if err != nil || p.OwnerEmail != owner || p.RevokedAt != nil {
			http.NotFound(w, r)
			return
		}
		if err := a.pats.Revoke(r.Context(), p.ID); err != nil {
			a.logger.Error("pat.revoke", "err", err)
			http.Error(w, "internal error", http.StatusInternalServerError)
			return
		}
		_ = a.audit.Write(r.Context(), audit.Event{
			Type: audit.EventPATRevoked, OwnerEmail: owner,
			IP: r.RemoteAddr, UserAgent: r.Header.Get("User-Agent"),
			Details: map[string]any{"public_id": p.PublicID},
		})
		http.Redirect(w, r, "/profile", http.StatusSeeOther)
	}
}

// patCreated is the view model for the show-once confirmation template.
type patCreated struct {
	Label  string
	Secret string
}

// patRow is the view model for one row in the PAT list block.
type patRow struct {
	PublicID   string
	Label      string
	CreatedISO string
	CreatedRel string
}

func patRowsFromPATs(pats []pat.PAT) []patRow {
	out := make([]patRow, 0, len(pats))
	now := time.Now().UTC()
	for _, p := range pats {
		out = append(out, patRow{
			PublicID:   p.PublicID,
			Label:      p.Label,
			CreatedISO: p.CreatedAt.UTC().Format(time.RFC3339),
			CreatedRel: relativeTime(p.CreatedAt, now),
		})
	}
	return out
}
