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
	if !strings.Contains(body, `href="/static/tokens.css"`) {
		t.Errorf("profile page missing Carbon tokens stylesheet:\n%s", body)
	}
	if !strings.Contains(body, `href="/static/app.css"`) {
		t.Errorf("profile page missing app stylesheet:\n%s", body)
	}
	if !strings.Contains(body, `action="/pat"`) {
		t.Errorf("profile page missing PAT form:\n%s", body)
	}
	if !strings.Contains(body, `id="grants-block"`) {
		t.Errorf("profile page missing grants block:\n%s", body)
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

func TestProfileOwnsPATManagementAndIndexOmitsIt(t *testing.T) {
	srv, deps := patTestServer(t)
	const owner = "owner@int.ikigenba.com"
	cookie := mintSession(t, deps, owner)
	publicID := mintPATWithLabel(t, deps, owner, "Codex on laptop")
	headers := map[string]string{"Cookie": cookie.Name + "=" + cookie.Value}

	profile := do(t, srv, "GET", "https://int.ikigenba.com/profile", headers)
	if profile.Code != http.StatusOK {
		t.Fatalf("profile status = %d, want 200", profile.Code)
	}
	profileBody := profile.Body.String()
	// R-DB06-PATM
	if !strings.Contains(profileBody, `action="/pat"`) {
		t.Errorf("profile page missing PAT create form:\n%s", profileBody)
	}
	if !strings.Contains(profileBody, "Codex on laptop") {
		t.Errorf("profile page missing PAT label:\n%s", profileBody)
	}
	if !strings.Contains(profileBody, `/pat/`+publicID+`/revoke`) {
		t.Errorf("profile page missing PAT revoke form for %s:\n%s", publicID, profileBody)
	}

	index := do(t, srv, "GET", "https://int.ikigenba.com/", headers)
	if index.Code != http.StatusOK {
		t.Fatalf("index status = %d, want 200", index.Code)
	}
	indexBody := index.Body.String()
	if strings.Contains(indexBody, `action="/pat"`) {
		t.Errorf("logged-in index still renders PAT create form:\n%s", indexBody)
	}
	if strings.Contains(indexBody, "Codex on laptop") {
		t.Errorf("logged-in index still renders PAT list:\n%s", indexBody)
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
