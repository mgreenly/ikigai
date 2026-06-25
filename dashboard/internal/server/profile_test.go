package server

import (
	"net/http"
	"strings"
	"testing"

	"dashboard/internal/googleidp"
)

func TestProfileRouteRendersSignedInPage(t *testing.T) {
	srv := testServer(t)
	sess := liveSession(t, srv)

	rec := do(t, srv, "GET", "https://int.ikigenba.com/profile",
		map[string]string{"Cookie": sess.Name + "=" + sess.Value})

	// R-DB01-PG3A
	if rec.Code != http.StatusOK {
		t.Fatalf("status = %d, want 200", rec.Code)
	}
	if ct := rec.Header().Get("Content-Type"); !strings.HasPrefix(ct, "text/html") {
		t.Errorf("Content-Type = %q, want text/html", ct)
	}
	body := rec.Body.String()
	if !strings.Contains(body, "<h1>Profile</h1>") {
		t.Errorf("profile page missing heading:\n%s", body)
	}
	if !strings.Contains(body, `action="/pat"`) {
		t.Errorf("profile page missing PAT form:\n%s", body)
	}
	if !strings.Contains(body, `id="grants-block"`) {
		t.Errorf("profile page missing grants block:\n%s", body)
	}
}

func TestIndexLoggedOutKeepsLandingOnly(t *testing.T) {
	srv := testServer(t)

	rec := do(t, srv, "GET", "https://int.ikigenba.com/", nil)

	if rec.Code != http.StatusOK {
		t.Fatalf("status = %d, want 200", rec.Code)
	}
	body := rec.Body.String()
	// R-DB02-LND7
	if !strings.Contains(body, `href="/login"`) {
		t.Errorf("logged-out landing missing sign-in link:\n%s", body)
	}
	if strings.Contains(body, googleidp.StubIdentity.Email) {
		t.Errorf("logged-out landing leaked owner email:\n%s", body)
	}
	if strings.Contains(body, `action="/pat"`) {
		t.Errorf("logged-out landing exposed PAT form:\n%s", body)
	}
	if strings.Contains(body, `id="grants-block"`) {
		t.Errorf("logged-out landing exposed grants block:\n%s", body)
	}
	if strings.Contains(body, `href="/profile"`) {
		t.Errorf("logged-out landing exposed profile control:\n%s", body)
	}
}

func TestProfileUsesLiveSessionOwner(t *testing.T) {
	srv := testServer(t)
	sess := liveSession(t, srv)

	rec := do(t, srv, "GET", "https://int.ikigenba.com/profile",
		map[string]string{"Cookie": sess.Name + "=" + sess.Value})

	if rec.Code != http.StatusOK {
		t.Fatalf("status = %d, want 200", rec.Code)
	}
	body := rec.Body.String()
	// R-DB03-PRF9
	if !strings.Contains(body, googleidp.StubIdentity.Email) {
		t.Errorf("profile page missing session owner %q:\n%s", googleidp.StubIdentity.Email, body)
	}
	if strings.Contains(body, `href="/login"`) {
		t.Errorf("signed-in profile still shows login link:\n%s", body)
	}
}

func TestProfileRedirectsSignedOutToIndex(t *testing.T) {
	srv := testServer(t)

	rec := do(t, srv, "GET", "https://int.ikigenba.com/profile", nil)

	// R-DB04-GATE
	if rec.Code != http.StatusFound {
		t.Fatalf("status = %d, want 302", rec.Code)
	}
	if loc := rec.Header().Get("Location"); loc != "/" {
		t.Errorf("Location = %q, want /", loc)
	}
	if strings.Contains(rec.Body.String(), "<h1>Profile</h1>") {
		t.Errorf("signed-out redirect rendered profile content:\n%s", rec.Body.String())
	}
}

func TestProfileClearsDeadSessionCookie(t *testing.T) {
	srv := testServer(t)

	rec := do(t, srv, "GET", "https://int.ikigenba.com/profile",
		map[string]string{"Cookie": sessionCookieName + "=bogus-value"})

	if rec.Code != http.StatusFound {
		t.Fatalf("status = %d, want 302", rec.Code)
	}
	if loc := rec.Header().Get("Location"); loc != "/" {
		t.Errorf("Location = %q, want /", loc)
	}

	var cleared *http.Cookie
	for _, c := range rec.Result().Cookies() {
		if c.Name == sessionCookieName {
			cleared = c
		}
	}
	// R-DB05-SESS
	if cleared == nil {
		t.Fatalf("dead session cookie was not cleared (no %s Set-Cookie)", sessionCookieName)
	}
	if cleared.MaxAge >= 0 {
		t.Errorf("cleared cookie MaxAge = %d, want negative (delete)", cleared.MaxAge)
	}
}
