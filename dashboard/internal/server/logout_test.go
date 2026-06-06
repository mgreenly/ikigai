package server

import (
	"net/http"
	"testing"
)

// TestLogoutRevokesAndClears: POST /logout with a live session cookie revokes
// the session server-side (revoked_at set), clears the cookie on the browser,
// and redirects to /.
func TestLogoutRevokesAndClears(t *testing.T) {
	srv, database := loginServer(t)
	sess := liveSession(t, srv)

	rec := do(t, srv, "POST", "https://int.ikigenba.com/logout",
		map[string]string{"Cookie": sess.Name + "=" + sess.Value})

	if rec.Code != http.StatusFound {
		t.Fatalf("status = %d, want 302", rec.Code)
	}
	if loc := rec.Header().Get("Location"); loc != "/" {
		t.Errorf("redirect Location = %q, want /", loc)
	}

	var cleared *http.Cookie
	for _, c := range rec.Result().Cookies() {
		if c.Name == sessionCookieName {
			cleared = c
		}
	}
	if cleared == nil {
		t.Fatalf("logout did not clear cookie (no %s Set-Cookie)", sessionCookieName)
	}
	if cleared.MaxAge >= 0 {
		t.Errorf("cleared cookie MaxAge = %d, want negative (delete)", cleared.MaxAge)
	}

	// The session is revoked server-side, not merely forgotten by the browser.
	var revoked int
	if err := database.QueryRow(
		`SELECT COUNT(*) FROM web_sessions WHERE revoked_at IS NOT NULL`).Scan(&revoked); err != nil {
		t.Fatalf("count revoked: %v", err)
	}
	if revoked != 1 {
		t.Errorf("revoked rows = %d, want 1", revoked)
	}
}

// TestLogoutNoCookie: POST /logout with no session cookie still redirects to /
// without error — logout is idempotent and forgiving of an absent session.
func TestLogoutNoCookie(t *testing.T) {
	srv := testServer(t)
	rec := do(t, srv, "POST", "https://int.ikigenba.com/logout", nil)
	if rec.Code != http.StatusFound {
		t.Fatalf("status = %d, want 302", rec.Code)
	}
	if loc := rec.Header().Get("Location"); loc != "/" {
		t.Errorf("redirect Location = %q, want /", loc)
	}
}
