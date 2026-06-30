package opsctl

import (
	"context"
	"os"
	"strings"
	"testing"
)

// R-VCF3-PLWD
// R-4LKF-FB23
func TestSetupNginxFragmentServesPublicDirectlyAndPrivateBehindAuth(t *testing.T) {
	const app = "svc"
	o, _, l := newSetupTestOpsctl(t, app)

	if err := o.Setup(context.Background(), SetupOptions{
		App:  app,
		Port: 3104,
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
	if !strings.Contains(private, "auth_request /_session-authn;") {
		t.Fatalf("private block is not behind the dashboard session auth_request:\n%s", private)
	}
	if strings.Contains(fragment, "/srv/"+app+"/introspect") {
		t.Fatalf("fragment still contains service-local introspection path:\n%s", fragment)
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
