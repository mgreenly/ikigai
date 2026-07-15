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

func TestNginxSessionLocationsUseLoginBounce(t *testing.T) {
	// R-42HV-I1HS
	conf := readNginxFragment(t)

	for _, start := range []string{
		`location = /srv/github/ {`,
		`location /srv/github/static/ {`,
	} {
		block := nginxLocationBlock(t, conf, start)
		for _, want := range []string{
			"auth_request /_session-authn;",
			"error_page 401 = @login_bounce;",
		} {
			if !strings.Contains(block, want) {
				t.Errorf("session location %q missing %q:\n%s", start, want, block)
			}
		}
	}
}

func TestNginxDeniesPublicTokenRouteR_GW5W_UJVL(t *testing.T) {
	// R-GW5W-UJVL
	conf := readNginxFragment(t)

	for _, want := range []string{
		"location = /srv/github/pr { return 404; }",
		"location = /srv/github/token { return 404; }",
	} {
		if !strings.Contains(conf, want) {
			t.Fatalf("nginx fragment missing loopback route denial %q", want)
		}
	}
}

func TestNginxBearerLocationDoesNotUseLoginBounce(t *testing.T) {
	// R-43PR-VT8H
	conf := readNginxFragment(t)
	prefix := nginxLocationBlock(t, conf, `location /srv/github/ {`)

	if strings.Contains(prefix, "error_page 401 = @login_bounce;") {
		t.Fatalf("bearer location must preserve its 401 challenge instead of using login bounce:\n%s", prefix)
	}
}

func TestNginxLoginBounceChangePreservesLocationsAndSessionProxies(t *testing.T) {
	// R-44XO-9KZ6
	conf := readNginxFragment(t)

	for _, start := range []string{
		`location = /srv/github/.well-known/oauth-protected-resource {`,
		`location = /srv/github/pr { return 404; }`,
		`location = /srv/github/ {`,
		`location /srv/github/static/ {`,
		`location /srv/github/ {`,
		`location @github_authn_500 {`,
	} {
		if !strings.Contains(conf, start) {
			t.Errorf("nginx fragment missing pre-existing location %q", start)
		}
	}

	sessionLocations := map[string]string{
		`location = /srv/github/ {`:      "proxy_pass http://127.0.0.1:3203/;",
		`location /srv/github/static/ {`: "proxy_pass http://127.0.0.1:3203/static/;",
	}
	for start, proxyPass := range sessionLocations {
		block := nginxLocationBlock(t, conf, start)
		for _, want := range []string{"auth_request /_session-authn;", proxyPass} {
			if !strings.Contains(block, want) {
				t.Errorf("session location %q did not retain %q:\n%s", start, want, block)
			}
		}
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
