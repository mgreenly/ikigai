package web

import (
	"os"
	"path/filepath"
	"strings"
	"testing"
)

func TestNginxFragmentRoutesGithubMount(t *testing.T) {
	// R-EYEW-NH1D
	conf := readNginxFragment(t)

	root := nginxLocationBlock(t, conf, `location = /srv/github/ {`)
	for _, want := range []string{
		"auth_request /_session-authn;",
		"proxy_pass http://127.0.0.1:3203/;",
	} {
		if !strings.Contains(root, want) {
			t.Fatalf("bare mount root missing %q:\n%s", want, root)
		}
	}

	prefix := nginxLocationBlock(t, conf, `location /srv/github/ {`)
	for _, want := range []string{
		"auth_request /_authn;",
		"auth_request_set $github_owner",
		"auth_request_set $github_client",
		"error_page 500 = @github_authn_500;",
		"proxy_set_header X-Owner-Email $github_owner;",
		"proxy_set_header X-Client-Id  $github_client;",
		"proxy_pass http://127.0.0.1:3203/;",
	} {
		if !strings.Contains(prefix, want) {
			t.Fatalf("mount prefix missing %q:\n%s", want, prefix)
		}
	}

	for _, want := range []string{
		"location = /srv/github/pr { return 404; }",
		"location = /srv/github/.well-known/oauth-protected-resource {",
		"location /srv/github/static/ {",
		"location @github_authn_500 {",
	} {
		if !strings.Contains(conf, want) {
			t.Fatalf("nginx fragment missing %q", want)
		}
	}
	if strings.Contains(conf, "/srv/github/feed") {
		t.Fatalf("nginx fragment must not contain a github feed block:\n%s", conf)
	}
	if strings.Count(conf, "location = /srv/github/ {") != 1 {
		t.Fatalf("nginx fragment should contain exactly one bare-root exact location")
	}
	if prm := nginxLocationBlock(t, conf, `location = /srv/github/.well-known/oauth-protected-resource {`); strings.Contains(prm, "auth_request") {
		t.Fatalf("PRM bootstrap location should be open, got:\n%s", prm)
	}
	if static := nginxLocationBlock(t, conf, `location /srv/github/static/ {`); !strings.Contains(static, "auth_request /_session-authn;") {
		t.Fatalf("static location should require dashboard session auth:\n%s", static)
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
