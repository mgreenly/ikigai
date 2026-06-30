package web

import (
	"os"
	"strings"
	"testing"
)

func TestNginxFragmentGatesAndProxiesLandingRoot(t *testing.T) {
	conf := readNginxFragment(t)
	block := nginxLocationBlock(t, conf, "location = /srv/sites/")

	// R-NGNX-3P6T
	if !strings.Contains(conf, "location = /srv/sites/ {") {
		t.Fatalf("nginx fragment is missing exact-match landing root location")
	}
	if strings.Contains(conf, "location /srv/sites/ {") {
		t.Fatalf("nginx fragment contains a catch-all /srv/sites/ prefix location")
	}
	if !strings.Contains(conf, "location /srv/sites/public/ {") || !strings.Contains(conf, "location /srv/sites/private/ {") {
		t.Fatalf("nginx fragment is missing public/private tier prefixes")
	}

	// R-NGNX-5R8V
	if !strings.Contains(block, "auth_request /_session-authn;") {
		t.Fatalf("landing root block does not use dashboard session auth:\n%s", block)
	}
	if strings.Contains(block, "auth_request /_authn;") {
		t.Fatalf("landing root block uses bearer auth instead of session auth:\n%s", block)
	}

	// R-NGNX-7T1X
	if !strings.Contains(block, "proxy_pass http://127.0.0.1:__PORT__/;") {
		t.Fatalf("landing root block does not proxy to the templated upstream root:\n%s", block)
	}
	if strings.Contains(block, "alias ") {
		t.Fatalf("landing root block is disk-backed instead of proxied:\n%s", block)
	}
}

func TestNginxFragmentPreservesExistingLocations(t *testing.T) {
	conf := readNginxFragment(t)

	prm := nginxLocationBlock(t, conf, "location = /srv/sites/.well-known/oauth-protected-resource")
	mcp := nginxLocationBlock(t, conf, "location = /srv/sites/mcp")
	public := nginxLocationBlock(t, conf, "location /srv/sites/public/")
	private := nginxLocationBlock(t, conf, "location /srv/sites/private/")
	authn500 := nginxLocationBlock(t, conf, "location @sites_authn_500")

	// R-NGNX-9W4Z
	if strings.Contains(prm, "auth_request") {
		t.Fatalf("PRM bootstrap location unexpectedly requires auth:\n%s", prm)
	}
	if !strings.Contains(mcp, "auth_request /_authn;") {
		t.Fatalf("MCP location does not preserve bearer auth_request:\n%s", mcp)
	}
	if strings.Contains(public, "auth_request") {
		t.Fatalf("public static tier unexpectedly requires auth:\n%s", public)
	}
	if !strings.Contains(private, "auth_request /_session-authn;") {
		t.Fatalf("private static tier does not preserve session auth_request:\n%s", private)
	}
	if !strings.Contains(authn500, "return 429;") || !strings.Contains(authn500, "return 500;") {
		t.Fatalf("authn 500 re-emit location does not preserve expected returns:\n%s", authn500)
	}
}

// R-4LKF-FB23
func TestNginxFragmentServesStateWWWPublicAndSessionGatesPrivate(t *testing.T) {
	conf := readNginxFragment(t)
	public := nginxLocationBlock(t, conf, "location /srv/sites/public/")
	private := nginxLocationBlock(t, conf, "location /srv/sites/private/")

	if !strings.Contains(public, "alias /opt/sites/state/www/public/;") {
		t.Fatalf("public tier is not served from state/www/public:\n%s", public)
	}
	if strings.Contains(public, "auth_request") {
		t.Fatalf("public tier unexpectedly requires auth:\n%s", public)
	}
	if !strings.Contains(private, "auth_request /_session-authn;") {
		t.Fatalf("private tier does not use dashboard session auth:\n%s", private)
	}
	if !strings.Contains(private, "alias /opt/sites/state/www/private/;") {
		t.Fatalf("private tier is not served from state/www/private:\n%s", private)
	}
	if strings.Contains(conf, "/opt/sites/www/served/") {
		t.Fatalf("nginx fragment still references legacy /opt/sites/www/served path:\n%s", conf)
	}
}

func readNginxFragment(t *testing.T) string {
	t.Helper()

	body, err := os.ReadFile("../../etc/nginx.conf")
	if err != nil {
		t.Fatalf("read nginx fragment: %v", err)
	}
	return string(body)
}

func nginxLocationBlock(t *testing.T, conf, prefix string) string {
	t.Helper()

	start := strings.Index(conf, prefix+" {")
	if start == -1 {
		t.Fatalf("nginx fragment is missing %q location", prefix)
	}

	depth := 0
	for i := start; i < len(conf); i++ {
		switch conf[i] {
		case '{':
			depth++
		case '}':
			depth--
			if depth == 0 {
				return conf[start : i+1]
			}
		}
	}
	t.Fatalf("nginx location %q is not closed", prefix)
	return ""
}
