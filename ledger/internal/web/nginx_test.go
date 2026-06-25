package web

import (
	"os"
	"path/filepath"
	"strings"
	"testing"
)

func readNginxFragment(t *testing.T) string {
	t.Helper()

	body, err := os.ReadFile(filepath.Join("..", "..", "etc", "nginx.conf"))
	if err != nil {
		t.Fatalf("read nginx fragment: %v", err)
	}
	return string(body)
}

func nginxLocationBlock(t *testing.T, conf, header string) string {
	t.Helper()

	start := strings.Index(conf, header+" {")
	if start == -1 {
		t.Fatalf("nginx fragment missing %q block", header)
	}

	block := conf[start:]
	depth := 0
	for i, r := range block {
		switch r {
		case '{':
			depth++
		case '}':
			depth--
			if depth == 0 {
				return block[:i+1]
			}
		}
	}
	t.Fatalf("nginx fragment has unterminated %q block", header)
	return ""
}

func TestNginxLandingLocationIsExactSessionGatedRoot(t *testing.T) {
	conf := readNginxFragment(t)
	landing := nginxLocationBlock(t, conf, "location = /srv/ledger/")
	prefix := nginxLocationBlock(t, conf, "location /srv/ledger/")

	// R-NGNX-2B4C
	if landing == prefix {
		t.Fatal("exact landing location resolved to the bearer-gated prefix block")
	}

	// R-NGNX-4D6E
	if !strings.Contains(landing, "auth_request /_session-authn;") {
		t.Fatalf("landing location missing session auth_request: %s", landing)
	}
	if strings.Contains(landing, "auth_request /_authn;") {
		t.Fatalf("landing location uses bearer auth_request: %s", landing)
	}

	// R-NGNX-6F8G
	if !strings.Contains(landing, "proxy_pass http://127.0.0.1:__PORT__/;") {
		t.Fatalf("landing location does not proxy to upstream root with trailing slash: %s", landing)
	}
}

func TestNginxFragmentRetainsBearerAndBootstrapLocations(t *testing.T) {
	conf := readNginxFragment(t)

	prefix := nginxLocationBlock(t, conf, "location /srv/ledger/")
	reemit := nginxLocationBlock(t, conf, "location @ledger_authn_500")
	prm := nginxLocationBlock(t, conf, "location = /srv/ledger/.well-known/oauth-protected-resource")

	// R-NGNX-8H1J
	if !strings.Contains(prefix, "auth_request /_authn;") {
		t.Fatalf("prefix location missing bearer auth_request: %s", prefix)
	}
	if !strings.Contains(reemit, "return 429;") || !strings.Contains(reemit, "return 500;") {
		t.Fatalf("authn 500 re-emit location missing expected returns: %s", reemit)
	}
	if !strings.Contains(prm, "proxy_pass http://127.0.0.1:__PORT__/.well-known/oauth-protected-resource;") {
		t.Fatalf("PRM bootstrap location missing expected proxy_pass: %s", prm)
	}
}
