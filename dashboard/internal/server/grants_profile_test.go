package server

import (
	"net/http"
	"strings"
	"testing"
)

func TestProfileRendersGrantManagementBlock(t *testing.T) {
	srv, deps := grantsTestServer(t)
	const owner = "owner@int.ikigenba.com"
	cookie := mintSession(t, deps, owner)
	publicID, clientName := issueChain(t, deps, owner, "Acme Desktop")

	rec := do(t, srv, "GET", "https://int.ikigenba.com/profile",
		map[string]string{"Cookie": cookie.Name + "=" + cookie.Value})
	if rec.Code != http.StatusOK {
		t.Fatalf("profile status = %d, want 200", rec.Code)
	}
	body := rec.Body.String()
	// R-DB09-GRNT
	if !strings.Contains(body, `<div id="grants-block" data-stream="/grants/stream" data-fragment="/grants/fragment">`) {
		t.Errorf("profile page missing live grants block:\n%s", body)
	}
	if !strings.Contains(body, clientName) {
		t.Errorf("profile page missing grant client name %q:\n%s", clientName, body)
	}
	if !strings.Contains(body, `/grants/`+publicID+`/revoke`) {
		t.Errorf("profile page missing grant revoke form for %s:\n%s", publicID, body)
	}
}

func TestGrantRevokeRedirectsBackToProfile(t *testing.T) {
	srv, deps := grantsTestServer(t)
	const owner = "owner@int.ikigenba.com"
	cookie := mintSession(t, deps, owner)
	publicID, clientName := issueChain(t, deps, owner, "Acme Desktop")
	headers := map[string]string{
		"Cookie": cookie.Name + "=" + cookie.Value,
		"Origin": "https://int.ikigenba.com",
	}

	rec := do(t, srv, "POST", "https://int.ikigenba.com/grants/"+publicID+"/revoke", headers)
	// R-DB10-GRVK
	if rec.Code != http.StatusSeeOther {
		t.Fatalf("revoke status = %d, want 303", rec.Code)
	}
	if loc := rec.Header().Get("Location"); loc != "/profile" {
		t.Errorf("Location = %q, want /profile", loc)
	}

	profile := do(t, srv, "GET", "https://int.ikigenba.com/profile",
		map[string]string{"Cookie": cookie.Name + "=" + cookie.Value})
	if strings.Contains(profile.Body.String(), clientName) {
		t.Errorf("revoked grant still appears on profile:\n%s", profile.Body.String())
	}
}

func TestSignedInIndexOmitsGrantManagement(t *testing.T) {
	srv, deps := grantsTestServer(t)
	const owner = "owner@int.ikigenba.com"
	cookie := mintSession(t, deps, owner)
	publicID, clientName := issueChain(t, deps, owner, "Acme Desktop")

	rec := do(t, srv, "GET", "https://int.ikigenba.com/",
		map[string]string{"Cookie": cookie.Name + "=" + cookie.Value})
	if rec.Code != http.StatusOK {
		t.Fatalf("index status = %d, want 200", rec.Code)
	}
	body := rec.Body.String()
	// R-DB11-GRNX
	if strings.Contains(body, `id="grants-block"`) {
		t.Errorf("signed-in index still renders grants block:\n%s", body)
	}
	if strings.Contains(body, clientName) || strings.Contains(body, `/grants/`+publicID+`/revoke`) {
		t.Errorf("signed-in index still renders grant management:\n%s", body)
	}
	if !strings.Contains(body, `href="/profile"`) {
		t.Errorf("signed-in index missing profile link:\n%s", body)
	}
}
