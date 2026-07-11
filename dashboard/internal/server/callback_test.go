package server

import (
	"crypto/sha256"
	"encoding/hex"
	"net/http"
	"net/url"
	"testing"

	"dashboard/internal/googleidp"
)

// startLogin runs the GET /login leg and returns the state the handler sent to
// the IdP and the plaintext binding cookie it set on the browser — the two
// values the callback leg needs to prove it is the same round-trip.
func startLogin(t *testing.T, srv *http.Server) (state string, binding *http.Cookie) {
	t.Helper()
	rec := do(t, srv, "GET", "https://int.ikigenba.com/login", nil)
	if rec.Code != http.StatusFound {
		t.Fatalf("login status = %d, want 302", rec.Code)
	}
	loc, err := url.Parse(rec.Header().Get("Location"))
	if err != nil {
		t.Fatalf("login Location unparseable: %v", err)
	}
	state = loc.Query().Get("state")
	if state == "" {
		t.Fatal("login redirect carries no state")
	}
	for _, c := range rec.Result().Cookies() {
		if c.Name == bindingCookieName {
			binding = c
		}
	}
	if binding == nil {
		t.Fatalf("login set no %s cookie", bindingCookieName)
	}
	return state, binding
}

// liveSession drives the full login -> callback flow and returns the resulting
// dashboard_session cookie, so identity-aware tests can present a genuine,
// store-backed session rather than fabricating one.
func liveSession(t *testing.T, srv *http.Server) *http.Cookie {
	t.Helper()
	state, binding := startLogin(t, srv)
	target := "https://int.ikigenba.com/oauth/google/callback?state=" + url.QueryEscape(state) + "&code=auth-code"
	rec := do(t, srv, "GET", target, map[string]string{"Cookie": binding.Name + "=" + binding.Value})
	if rec.Code != http.StatusFound {
		t.Fatalf("callback status = %d, want 302", rec.Code)
	}
	for _, c := range rec.Result().Cookies() {
		if c.Name == sessionCookieName {
			return c
		}
	}
	t.Fatalf("callback set no %s cookie", sessionCookieName)
	return nil
}

// R-VQY2-GZ3F
// TestCallbackSuccessMintsSession is the end-to-end contract for the callback
// leg: a valid handshake + binding cookie + a verified, in-Workspace identity
// yields a redirect to /, a session cookie on the browser, and exactly one
// web_sessions row whose stored hash is sha256(session cookie) — never the
// plaintext — bound to the identity's email.
func TestCallbackSuccessMintsSession(t *testing.T) {
	// loginServer gates federation on testWorkspaceDomain, which matches the
	// stub's canned StubIdentity.HostedDomain — keep them aligned or the gate
	// rejects the stub identity and this test exercises the wrong path.
	if testWorkspaceDomain != googleidp.StubIdentity.HostedDomain {
		t.Fatalf("test setup drift: workspace domain %q != stub hd %q",
			testWorkspaceDomain, googleidp.StubIdentity.HostedDomain)
	}
	srv, database := loginServer(t)
	state, binding := startLogin(t, srv)

	target := "https://int.ikigenba.com/oauth/google/callback?state=" + url.QueryEscape(state) + "&code=auth-code"
	rec := do(t, srv, "GET", target, map[string]string{"Cookie": binding.Name + "=" + binding.Value})

	if rec.Code != http.StatusFound {
		t.Fatalf("callback status = %d, want 302", rec.Code)
	}
	if loc := rec.Header().Get("Location"); loc != "/" {
		t.Errorf("redirect Location = %q, want /", loc)
	}

	var session *http.Cookie
	for _, c := range rec.Result().Cookies() {
		if c.Name == sessionCookieName {
			session = c
		}
	}
	if session == nil {
		t.Fatalf("callback set no %s cookie", sessionCookieName)
	}
	if session.Value == "" {
		t.Fatal("session cookie has empty value")
	}

	var count int
	if err := database.QueryRow(`SELECT COUNT(*) FROM web_sessions`).Scan(&count); err != nil {
		t.Fatalf("count web_sessions: %v", err)
	}
	if count != 1 {
		t.Fatalf("web_sessions rows = %d, want 1", count)
	}

	var ownerEmail, ownerID, storedHash string
	if err := database.QueryRow(`SELECT owner_email, owner_id, cookie_hash FROM web_sessions`).Scan(&ownerEmail, &ownerID, &storedHash); err != nil {
		t.Fatalf("read web_sessions: %v", err)
	}
	if ownerEmail != googleidp.StubIdentity.Email {
		t.Errorf("owner_email = %q, want %q", ownerEmail, googleidp.StubIdentity.Email)
	}
	var resolvedID string
	if err := database.QueryRow(`SELECT id FROM identities WHERE iss = ? AND sub = ?`, googleidp.StubIdentity.Iss, googleidp.StubIdentity.Sub).Scan(&resolvedID); err != nil {
		t.Fatalf("read resolved identity: %v", err)
	}
	if ownerID != resolvedID {
		t.Errorf("owner_id = %q, want resolved identity handle %q", ownerID, resolvedID)
	}
	sum := sha256.Sum256([]byte(session.Value))
	wantHash := hex.EncodeToString(sum[:])
	if storedHash != wantHash {
		t.Errorf("stored hash = %q, want sha256(session cookie) %q", storedHash, wantHash)
	}
	if storedHash == session.Value {
		t.Error("plaintext session cookie was stored — must store only the hash")
	}
}
