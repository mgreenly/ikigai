package server

import (
	"net/http"
	"strings"
	"testing"

	"dashboard/internal/googleidp"
)

// TestIndexLoggedIn: a request carrying a live session cookie renders the
// identity-aware page — the owner's email and a POST /logout control, and not
// the logged-out sign-in link.
func TestIndexLoggedIn(t *testing.T) {
	srv := testServer(t)
	sess := liveSession(t, srv)

	rec := do(t, srv, "GET", "https://int.ikigenba.com/", map[string]string{"Cookie": sess.Name + "=" + sess.Value})
	if rec.Code != http.StatusOK {
		t.Fatalf("status = %d, want 200", rec.Code)
	}
	body := rec.Body.String()
	if !strings.Contains(body, googleidp.StubIdentity.Email) {
		t.Errorf("logged-in index missing owner email %q:\n%s", googleidp.StubIdentity.Email, body)
	}
	if !strings.Contains(body, `action="/logout"`) {
		t.Errorf("logged-in index missing logout control:\n%s", body)
	}
	if strings.Contains(body, `href="/login"`) {
		t.Errorf("logged-in index still shows sign-in link:\n%s", body)
	}
}

// TestIndexLoggedOut: no cookie renders the logged-out landing — the sign-in
// link, no owner.
func TestIndexLoggedOut(t *testing.T) {
	srv := testServer(t)
	rec := do(t, srv, "GET", "https://int.ikigenba.com/", nil)
	if rec.Code != http.StatusOK {
		t.Fatalf("status = %d, want 200", rec.Code)
	}
	body := rec.Body.String()
	if !strings.Contains(body, `href="/login"`) {
		t.Errorf("logged-out index missing sign-in link:\n%s", body)
	}
	if strings.Contains(body, `action="/logout"`) {
		t.Errorf("logged-out index shows a logout control:\n%s", body)
	}
}

// TestIndexDeadCookie: a present-but-invalid cookie (here, unknown) renders
// logged-out and clears the cookie so the browser stops resending a value that
// can never redeem.
func TestIndexDeadCookie(t *testing.T) {
	srv := testServer(t)
	rec := do(t, srv, "GET", "https://int.ikigenba.com/",
		map[string]string{"Cookie": sessionCookieName + "=bogus-value"})
	if rec.Code != http.StatusOK {
		t.Fatalf("status = %d, want 200", rec.Code)
	}
	if !strings.Contains(rec.Body.String(), `href="/login"`) {
		t.Errorf("dead-cookie index not logged-out:\n%s", rec.Body.String())
	}

	var cleared *http.Cookie
	for _, c := range rec.Result().Cookies() {
		if c.Name == sessionCookieName {
			cleared = c
		}
	}
	if cleared == nil {
		t.Fatalf("dead cookie was not cleared (no %s Set-Cookie)", sessionCookieName)
	}
	if cleared.MaxAge >= 0 {
		t.Errorf("cleared cookie MaxAge = %d, want negative (delete)", cleared.MaxAge)
	}
}
