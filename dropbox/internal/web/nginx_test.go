package web

import (
	"os"
	"path/filepath"
	"strings"
	"testing"
)

func TestNginxLandingLocationIsExactMatch(t *testing.T) {
	conf := readNginxConfig(t)
	landing := nginxLocationBlock(t, conf, "location = /srv/dropbox/ {")
	prefix := nginxLocationBlock(t, conf, "location /srv/dropbox/ {")

	// R-NGNX-2P4Q
	if landing == prefix || !strings.HasPrefix(landing, "location = /srv/dropbox/ {") || !strings.HasPrefix(prefix, "location /srv/dropbox/ {") {
		t.Fatalf("landing exact-match block is not distinct from prefix block:\nlanding:\n%s\nprefix:\n%s", landing, prefix)
	}
}

func TestNginxLandingLocationUsesSessionAuth(t *testing.T) {
	conf := readNginxConfig(t)
	landing := nginxLocationBlock(t, conf, "location = /srv/dropbox/ {")

	// R-NGNX-4R6S
	if !strings.Contains(landing, "auth_request /_session-authn;") || strings.Contains(landing, "auth_request /_authn;") {
		t.Fatalf("landing block auth_request is not session-gated only:\n%s", landing)
	}
	if !strings.Contains(landing, "auth_request_set $dropbox_session_owner $upstream_http_x_owner_email;") ||
		!strings.Contains(landing, "proxy_set_header X-Owner-Email $dropbox_session_owner;") {
		t.Fatalf("landing block does not propagate session owner identity:\n%s", landing)
	}
}

func TestNginxLandingLocationProxiesToUpstreamRoot(t *testing.T) {
	conf := readNginxConfig(t)
	landing := nginxLocationBlock(t, conf, "location = /srv/dropbox/ {")

	// R-NGNX-6T8U
	if !strings.Contains(landing, "proxy_pass http://127.0.0.1:__PORT__/;") {
		t.Fatalf("landing block does not proxy to loopback upstream root with trailing slash:\n%s", landing)
	}
}

func TestNginxPreExistingLocationsRemain(t *testing.T) {
	conf := readNginxConfig(t)
	prefix := nginxLocationBlock(t, conf, "location /srv/dropbox/ {")

	// R-NGNX-8V1W
	for _, want := range []string{
		"location = /srv/dropbox/.well-known/oauth-protected-resource {",
		"location = /srv/dropbox/content {",
		"location @dropbox_authn_500 {",
	} {
		if !strings.Contains(conf, want) {
			t.Fatalf("nginx config missing pre-existing location %q", want)
		}
	}
	if !strings.Contains(conf, "location = /srv/dropbox/content {\n    return 404;\n}") {
		t.Fatalf("nginx config content defence-in-depth location does not return 404")
	}
	if !strings.Contains(prefix, "auth_request /_authn;") {
		t.Fatalf("bearer-gated prefix block is missing auth_request /_authn:\n%s", prefix)
	}
}

func readNginxConfig(t *testing.T) string {
	t.Helper()

	bytes, err := os.ReadFile(filepath.Join("..", "..", "etc", "nginx.conf"))
	if err != nil {
		t.Fatalf("read nginx.conf: %v", err)
	}
	return string(bytes)
}

func nginxLocationBlock(t *testing.T, conf, start string) string {
	t.Helper()

	i := strings.Index(conf, start)
	if i < 0 {
		t.Fatalf("nginx config missing %q", start)
	}
	conf = conf[i:]

	depth := 0
	for i, r := range conf {
		switch r {
		case '{':
			depth++
		case '}':
			depth--
			if depth == 0 {
				return conf[:i+1]
			}
		}
	}
	t.Fatalf("nginx location %q is not closed", start)
	return ""
}
