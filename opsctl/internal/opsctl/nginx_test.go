package opsctl

import (
	"context"
	"fmt"
	"os"
	"strings"
	"testing"
)

// R-VCF3-PLWD
func TestSetupNginxFragmentServesPublicDirectlyAndPrivateBehindAuth(t *testing.T) {
	const app = "svc"
	o, _, l := newSetupTestOpsctl(t, app)
	fragmentSource := fmt.Sprintf(`
location /srv/%[1]s/public/ {
    alias %[2]s/;
}

location /srv/%[1]s/private/ {
    auth_request /srv/%[1]s/introspect;
    alias %[3]s/;
    proxy_pass http://127.0.0.1:__PORT__;
}
`, app, l.WWWPublicDir(), l.WWWPrivateDir())

	if err := o.Setup(context.Background(), SetupOptions{
		App:      app,
		Port:     3104,
		Fragment: fragmentSource,
	}); err != nil {
		t.Fatalf("setup: %v", err)
	}

	b, err := os.ReadFile(l.FragmentPath())
	if err != nil {
		t.Fatalf("read fragment: %v", err)
	}
	fragment := string(b)
	public := locationBlock(t, fragment, "/srv/"+app+"/public/")
	private := locationBlock(t, fragment, "/srv/"+app+"/private/")

	if !strings.Contains(public, "alias "+l.WWWPublicDir()+"/;") {
		t.Fatalf("public block does not serve %s directly:\n%s", l.WWWPublicDir(), public)
	}
	if strings.Contains(public, "auth_request") {
		t.Fatalf("public block unexpectedly has auth_request:\n%s", public)
	}
	if !strings.Contains(private, "alias "+l.WWWPrivateDir()+"/;") {
		t.Fatalf("private block does not serve %s:\n%s", l.WWWPrivateDir(), private)
	}
	if !strings.Contains(private, "auth_request /srv/"+app+"/introspect;") {
		t.Fatalf("private block is not behind the service introspection auth_request:\n%s", private)
	}
	if !strings.Contains(private, "proxy_pass http://127.0.0.1:3104;") {
		t.Fatalf("private block did not receive port substitution:\n%s", private)
	}
}

func locationBlock(t *testing.T, fragment, path string) string {
	t.Helper()
	startToken := "location " + path + " {"
	start := strings.Index(fragment, startToken)
	if start < 0 {
		t.Fatalf("fragment missing %q:\n%s", startToken, fragment)
	}
	rest := fragment[start:]
	end := strings.Index(rest, "\n}")
	if end < 0 {
		t.Fatalf("fragment block %q is unterminated:\n%s", startToken, fragment)
	}
	return rest[:end+2]
}
