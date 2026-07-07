package web

import (
	"os"
	"path/filepath"
	"regexp"
	"registry"
	"strings"
	"testing"
)

func TestNginxLandingLocationIsExactMatch(t *testing.T) {
	// R-NGNX-3B6C
	conf := readNginxFragment(t)

	exact := nginxLocationBlock(t, conf, `location = /srv/gmail/ {`)
	if exact == nginxLocationBlock(t, conf, `location /srv/gmail/ {`) {
		t.Fatalf("exact-match landing location was not distinct from prefix location")
	}
	if !strings.Contains(exact, "auth_request /_session-authn;") {
		t.Fatalf("exact-match landing location did not require dashboard session auth:\n%s", exact)
	}
}

func TestNginxLandingLocationForwardsSessionOwner(t *testing.T) {
	// R-NGNX-5D8E
	block := nginxLocationBlock(t, readNginxFragment(t), `location = /srv/gmail/ {`)

	for _, want := range []string{
		"auth_request_set $gmail_session_owner $upstream_http_x_owner_email;",
		"proxy_set_header X-Owner-Email $gmail_session_owner;",
	} {
		if !strings.Contains(block, want) {
			t.Fatalf("landing location missing %q:\n%s", want, block)
		}
	}
	if strings.Contains(block, "/_authn") {
		t.Fatalf("landing location used token auth instead of session auth:\n%s", block)
	}
}

func TestNginxLandingLocationProxiesBareRootWithLiteralPort(t *testing.T) {
	// R-NGNX-7F1G
	// R-9SU9-BYHJ
	block := nginxLocationBlock(t, readNginxFragment(t), `location = /srv/gmail/ {`)

	for _, want := range []string{
		"proxy_pass " + registry.BaseURL("gmail") + "/;",
		"proxy_set_header Host $host;",
		"proxy_set_header X-Forwarded-Proto $scheme;",
		"proxy_http_version 1.1;",
	} {
		if !strings.Contains(block, want) {
			t.Fatalf("landing location missing %q:\n%s", want, block)
		}
	}
}

func TestNginxLandingLocationCoexistsWithExistingFragment(t *testing.T) {
	// R-NGNX-9H3J
	conf := readNginxFragment(t)

	for _, want := range []string{
		"location = /srv/gmail/.well-known/oauth-protected-resource",
		"location = /srv/gmail/feed { return 404; }",
		"location /srv/gmail/ {",
		"location @gmail_authn_500 {",
	} {
		if !strings.Contains(conf, want) {
			t.Fatalf("nginx fragment missing required existing location %q", want)
		}
	}

	if regexp.MustCompile(`(?m)^\s*(server|listen)\b`).MatchString(conf) {
		t.Fatalf("nginx fragment must remain a location fragment, not a vhost:\n%s", conf)
	}
	if strings.Count(conf, "location = /srv/gmail/ {") != 1 {
		t.Fatalf("nginx fragment should contain exactly one bare-root exact-match location")
	}
}

func TestNginxStaticLocationUsesSessionAuthAndStaticProxy(t *testing.T) {
	// R-41ZW-HBBA
	// R-9SU9-BYHJ
	conf := readNginxFragment(t)
	block := nginxLocationBlock(t, conf, `location /srv/gmail/static/ {`)

	for _, want := range []string{
		"auth_request /_session-authn;",
		"proxy_pass " + registry.BaseURL("gmail") + "/static/;",
		"proxy_set_header Host $host;",
		"proxy_set_header X-Forwarded-Proto $scheme;",
		"proxy_http_version 1.1;",
	} {
		if !strings.Contains(block, want) {
			t.Fatalf("static location missing %q:\n%s", want, block)
		}
	}

	for _, want := range []string{
		"location = /srv/gmail/ {",
		"location /srv/gmail/ {",
		"location = /srv/gmail/feed { return 404; }",
		"location = /srv/gmail/.well-known/oauth-protected-resource",
	} {
		if !strings.Contains(conf, want) {
			t.Fatalf("nginx fragment no longer contained required location %q", want)
		}
	}
	if strings.Count(conf, "location /srv/gmail/static/ {") != 1 {
		t.Fatalf("nginx fragment should contain exactly one static location:\n%s", conf)
	}
}

func readNginxFragment(t *testing.T) string {
	t.Helper()

	body, err := os.ReadFile(filepath.Join("..", "..", "etc", "nginx.conf"))
	if err != nil {
		t.Fatalf("read nginx fragment: %v", err)
	}
	return string(body)
}

func nginxLocationBlock(t *testing.T, conf, start string) string {
	t.Helper()

	offset := strings.Index(conf, start)
	if offset == -1 {
		t.Fatalf("nginx fragment missing location %q", start)
	}
	remaining := conf[offset:]
	end := strings.Index(remaining, "\n}")
	if end == -1 {
		t.Fatalf("nginx location %q was not closed:\n%s", start, remaining)
	}
	return remaining[:end+len("\n}")]
}
