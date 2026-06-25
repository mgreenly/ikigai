package server

import (
	"net/http"
	"strings"
	"testing"

	"dashboard/internal/googleidp"
)

func landingServerWithCRM(t *testing.T) *http.Server {
	t.Helper()
	root := t.TempDir()
	writeManifest(t, root, "dashboard", "APP=dashboard\nMOUNT=/\nDEFAULT=true\n")
	writeManifest(t, root, "crm", "APP=crm\nMOUNT=/srv/crm/\nMCP=true\n")

	opts := newServerDeps(t).opts()
	opts.ManifestRoot = root
	srv, err := New(opts)
	if err != nil {
		t.Fatalf("New: %v", err)
	}
	return srv
}

func signedInLanding(t *testing.T, srv *http.Server) string {
	t.Helper()
	sess := liveSession(t, srv)
	rec := do(t, srv, "GET", "https://int.ikigenba.com/", map[string]string{
		"Cookie":            sess.Name + "=" + sess.Value,
		"X-Forwarded-Proto": "https",
	})
	if rec.Code != http.StatusOK {
		t.Fatalf("status = %d, want 200", rec.Code)
	}
	return rec.Body.String()
}

func TestLandingServiceNameLinksToMount(t *testing.T) {
	body := signedInLanding(t, landingServerWithCRM(t))

	// R-DB12-LINK
	if !strings.Contains(body, `<a href="/srv/crm/">ikigenba_crm</a>`) {
		t.Errorf("service name is not linked to the service mount:\n%s", body)
	}
	if !strings.Contains(body, `https://int.ikigenba.com/srv/crm/mcp`) {
		t.Errorf("service row no longer shows raw MCP URL:\n%s", body)
	}
}

func TestLandingOwnerEmailLinksToProfile(t *testing.T) {
	body := signedInLanding(t, landingServerWithCRM(t))

	want := `<a href="/profile">` + googleidp.StubIdentity.Email + `</a>`
	// R-DB13-MAIL
	if !strings.Contains(body, want) {
		t.Errorf("owner email is not linked to profile; want %q in:\n%s", want, body)
	}
}

func TestLandingSignOutRemainsPostLogout(t *testing.T) {
	body := signedInLanding(t, landingServerWithCRM(t))

	// R-DB14-SOUT
	if !strings.Contains(body, `<form method="POST" action="/logout">`) {
		t.Errorf("logged-in landing missing POST /logout form:\n%s", body)
	}
	if !strings.Contains(body, `<button type="submit"`) || !strings.Contains(body, `Sign out`) {
		t.Errorf("logged-in landing missing sign-out submit button:\n%s", body)
	}
}

func TestLandingInstallInstructionsRemain(t *testing.T) {
	body := signedInLanding(t, landingServerWithCRM(t))

	for _, want := range []string{
		`curl -fsSL https://int.ikigenba.com/install/claude | bash`,
		`curl -fsSL https://int.ikigenba.com/install/codex | bash`,
	} {
		// R-DB15-INST
		if !strings.Contains(body, want) {
			t.Errorf("logged-in landing missing install instruction %q:\n%s", want, body)
		}
	}
}
