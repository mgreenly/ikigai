package server

import (
	"context"
	"net/http"
	"net/http/httptest"
	"net/url"
	"strings"
	"testing"
)

// doForm runs a POST with a url-encoded form body and the given headers,
// mirroring `do` (which sends a nil body). The PAT create handler reads a form
// value, so its tests need a body the plain `do` helper cannot supply.
func doForm(t *testing.T, srv *http.Server, target string, form url.Values, hdr map[string]string) *httptest.ResponseRecorder {
	t.Helper()
	req := httptest.NewRequest("POST", target, strings.NewReader(form.Encode()))
	req.Header.Set("Content-Type", "application/x-www-form-urlencoded")
	for k, v := range hdr {
		req.Header.Set(k, v)
	}
	rec := httptest.NewRecorder()
	srv.Handler.ServeHTTP(rec, req)
	return rec
}

// patTestServer builds a real handler over one migrated SQLite db plus its deps,
// mirroring grantsTestServer.
func patTestServer(t *testing.T) (*http.Server, serverDeps) {
	t.Helper()
	deps := newServerDeps(t)
	srv, err := New(deps.opts())
	if err != nil {
		t.Fatalf("New: %v", err)
	}
	return srv, deps
}

// TestPATCreateShowsSecretOnce: a same-origin, signed-in create renders the
// show-once confirmation containing the plaintext secret exactly once, and the
// stored token is the hash (the plaintext is not in the database list view).
func TestPATCreateShowsSecretOnce(t *testing.T) {
	srv, deps := patTestServer(t)
	const owner = "owner@int.ikigenba.com"
	cookie := mintSession(t, deps, owner)

	rec := doForm(t, srv, "https://int.ikigenba.com/pat",
		url.Values{"label": {"Codex on laptop"}},
		map[string]string{
			"Cookie": cookie.Name + "=" + cookie.Value,
			"Origin": "https://int.ikigenba.com",
		})
	if rec.Code != http.StatusOK {
		t.Fatalf("create status = %d, want 200\n%s", rec.Code, rec.Body.String())
	}
	body := rec.Body.String()
	if !strings.Contains(body, "Codex on laptop") {
		t.Errorf("confirmation missing label:\n%s", body)
	}
	// Extract the secret from the store and assert it appears exactly once in the
	// rendered page and carries the PAT prefix.
	pats, err := deps.pats.ListByOwner(context.Background(), owner)
	if err != nil {
		t.Fatalf("ListByOwner: %v", err)
	}
	if len(pats) != 1 {
		t.Fatalf("got %d PATs, want 1", len(pats))
	}
	// The plaintext is only available from the response; assert a ms_pat_ token
	// appears once in the body.
	if got := strings.Count(body, "ms_pat_"); got != 1 {
		t.Errorf("ms_pat_ secret appears %d times in confirmation, want 1:\n%s", got, body)
	}
	// R-DB08-PATX
	if !strings.Contains(body, `href="/profile" class="btn btn-secondary">Done</a>`) {
		t.Errorf("confirmation does not link back to profile:\n%s", body)
	}
	if strings.Contains(body, `href="/" class="btn btn-secondary">Done</a>`) {
		t.Errorf("confirmation still links back to index:\n%s", body)
	}
}

// TestPATCreateEmptyLabel: an empty (whitespace) label is rejected 400 and no
// PAT is minted.
func TestPATCreateEmptyLabel(t *testing.T) {
	srv, deps := patTestServer(t)
	const owner = "owner@int.ikigenba.com"
	cookie := mintSession(t, deps, owner)

	rec := doForm(t, srv, "https://int.ikigenba.com/pat",
		url.Values{"label": {"   "}},
		map[string]string{
			"Cookie": cookie.Name + "=" + cookie.Value,
			"Origin": "https://int.ikigenba.com",
		})
	if rec.Code != http.StatusBadRequest {
		t.Fatalf("empty-label status = %d, want 400", rec.Code)
	}
	pats, _ := deps.pats.ListByOwner(context.Background(), owner)
	if len(pats) != 0 {
		t.Errorf("a PAT was minted for an empty label")
	}
}

// TestPATCreateOverLongLabel: a label over 48 characters is rejected 400.
func TestPATCreateOverLongLabel(t *testing.T) {
	srv, deps := patTestServer(t)
	const owner = "owner@int.ikigenba.com"
	cookie := mintSession(t, deps, owner)

	rec := doForm(t, srv, "https://int.ikigenba.com/pat",
		url.Values{"label": {strings.Repeat("x", 49)}},
		map[string]string{
			"Cookie": cookie.Name + "=" + cookie.Value,
			"Origin": "https://int.ikigenba.com",
		})
	if rec.Code != http.StatusBadRequest {
		t.Fatalf("over-long-label status = %d, want 400", rec.Code)
	}
}

// TestPATCreateCrossOrigin: a POST without a matching Origin is rejected 403 and
// no PAT is minted.
func TestPATCreateCrossOrigin(t *testing.T) {
	srv, deps := patTestServer(t)
	const owner = "owner@int.ikigenba.com"
	cookie := mintSession(t, deps, owner)

	rec := doForm(t, srv, "https://int.ikigenba.com/pat",
		url.Values{"label": {"Codex"}},
		map[string]string{
			"Cookie": cookie.Name + "=" + cookie.Value,
			"Origin": "https://evil.example",
		})
	if rec.Code != http.StatusForbidden {
		t.Fatalf("cross-origin create status = %d, want 403", rec.Code)
	}
	pats, _ := deps.pats.ListByOwner(context.Background(), owner)
	if len(pats) != 0 {
		t.Errorf("a PAT was minted on a cross-origin request")
	}
}

// TestPATCreateUnauthenticated: a same-origin POST without a session is 401.
func TestPATCreateUnauthenticated(t *testing.T) {
	srv, _ := patTestServer(t)
	rec := doForm(t, srv, "https://int.ikigenba.com/pat",
		url.Values{"label": {"Codex"}},
		map[string]string{"Origin": "https://int.ikigenba.com"})
	if rec.Code != http.StatusUnauthorized {
		t.Fatalf("unauthenticated create status = %d, want 401", rec.Code)
	}
}

// mintPATWithLabel creates a PAT for owner directly against the store and
// returns its public_id (so revoke tests can target it). It wraps the
// store.Create with a chosen label; the authn-test mintPAT uses a fixed label.
func mintPATWithLabel(t *testing.T, deps serverDeps, owner, label string) string {
	t.Helper()
	_, p, err := deps.pats.Create(context.Background(), owner, "owner-test", label)
	if err != nil {
		t.Fatalf("pats.Create: %v", err)
	}
	return p.PublicID
}

// TestPATRevoke: a same-origin POST revokes the PAT, redirects back to the
// profile page, and removes it from the owner's active list.
func TestPATRevoke(t *testing.T) {
	srv, deps := patTestServer(t)
	const owner = "owner@int.ikigenba.com"
	cookie := mintSession(t, deps, owner)
	publicID := mintPATWithLabel(t, deps, owner, "Codex on laptop")

	rec := do(t, srv, "POST", "https://int.ikigenba.com/pat/"+publicID+"/revoke",
		map[string]string{
			"Cookie": cookie.Name + "=" + cookie.Value,
			"Origin": "https://int.ikigenba.com",
		})
	// R-DB07-PATR
	if rec.Code != http.StatusSeeOther {
		t.Fatalf("revoke status = %d, want 303", rec.Code)
	}
	if loc := rec.Header().Get("Location"); loc != "/profile" {
		t.Errorf("Location = %q, want /profile", loc)
	}
	pats, _ := deps.pats.ListByOwner(context.Background(), owner)
	if len(pats) != 0 {
		t.Errorf("revoked PAT still listed")
	}
}

// TestPATRevokeOtherOwner: revoking a PAT owned by someone else is
// indistinguishable from not-found (404) and does not revoke it.
func TestPATRevokeOtherOwner(t *testing.T) {
	srv, deps := patTestServer(t)
	cookie := mintSession(t, deps, "attacker@int.ikigenba.com")
	publicID := mintPATWithLabel(t, deps, "victim@int.ikigenba.com", "Victim PAT")

	rec := do(t, srv, "POST", "https://int.ikigenba.com/pat/"+publicID+"/revoke",
		map[string]string{
			"Cookie": cookie.Name + "=" + cookie.Value,
			"Origin": "https://int.ikigenba.com",
		})
	if rec.Code != http.StatusNotFound {
		t.Fatalf("foreign-PAT revoke status = %d, want 404", rec.Code)
	}
	pats, _ := deps.pats.ListByOwner(context.Background(), "victim@int.ikigenba.com")
	if len(pats) != 1 {
		t.Errorf("victim's PAT was revoked by another owner")
	}
}

// TestPATRevokeAlreadyRevoked: revoking an already-revoked PAT is 404
// (indistinguishable from not-found).
func TestPATRevokeAlreadyRevoked(t *testing.T) {
	srv, deps := patTestServer(t)
	const owner = "owner@int.ikigenba.com"
	cookie := mintSession(t, deps, owner)
	publicID := mintPATWithLabel(t, deps, owner, "Codex")

	// First revoke succeeds.
	rec := do(t, srv, "POST", "https://int.ikigenba.com/pat/"+publicID+"/revoke",
		map[string]string{
			"Cookie": cookie.Name + "=" + cookie.Value,
			"Origin": "https://int.ikigenba.com",
		})
	if rec.Code != http.StatusSeeOther {
		t.Fatalf("first revoke status = %d, want 303", rec.Code)
	}
	// Second revoke is now indistinguishable from not-found.
	rec = do(t, srv, "POST", "https://int.ikigenba.com/pat/"+publicID+"/revoke",
		map[string]string{
			"Cookie": cookie.Name + "=" + cookie.Value,
			"Origin": "https://int.ikigenba.com",
		})
	if rec.Code != http.StatusNotFound {
		t.Fatalf("second revoke status = %d, want 404", rec.Code)
	}
}

// TestIndexOmitsPATManagement: the signed-in index no longer renders PAT
// management, even when the owner has an active token.
func TestIndexOmitsPATManagement(t *testing.T) {
	srv, deps := patTestServer(t)
	const owner = "owner@int.ikigenba.com"
	cookie := mintSession(t, deps, owner)
	mintPATWithLabel(t, deps, owner, "Codex on laptop")

	rec := do(t, srv, "GET", "https://int.ikigenba.com/",
		map[string]string{"Cookie": cookie.Name + "=" + cookie.Value})
	if rec.Code != http.StatusOK {
		t.Fatalf("index status = %d, want 200", rec.Code)
	}
	body := rec.Body.String()
	if strings.Contains(body, `action="/pat"`) {
		t.Errorf("index still renders PAT create form:\n%s", body)
	}
	if strings.Contains(body, "Codex on laptop") {
		t.Errorf("index still renders PAT list:\n%s", body)
	}
}

// R-VTDV-8IKT
func TestPATCreateStampsOwnerIDFromSession(t *testing.T) {
	srv, deps := patTestServer(t)
	cookie := mintSession(t, deps, "owner@int.ikigenba.com")
	var sessionOwnerID string
	if err := deps.db.QueryRow(`SELECT owner_id FROM web_sessions WHERE cookie_hash IS NOT NULL`).Scan(&sessionOwnerID); err != nil {
		t.Fatalf("read session owner_id: %v", err)
	}
	rec := doForm(t, srv, "https://int.ikigenba.com/pat", url.Values{"label": {"session handle"}}, map[string]string{
		"Cookie": cookie.Name + "=" + cookie.Value,
		"Origin": "https://int.ikigenba.com",
	})
	if rec.Code != http.StatusOK {
		t.Fatalf("create status = %d, want 200", rec.Code)
	}
	var ownerEmail, ownerID string
	if err := deps.db.QueryRow(`SELECT owner_email, owner_id FROM personal_tokens`).Scan(&ownerEmail, &ownerID); err != nil {
		t.Fatalf("read PAT owner: %v", err)
	}
	if ownerEmail != "owner@int.ikigenba.com" {
		t.Errorf("owner_email = %q, want unchanged email", ownerEmail)
	}
	if ownerID != sessionOwnerID {
		t.Errorf("owner_id = %q, want session owner_id %q", ownerID, sessionOwnerID)
	}
}
