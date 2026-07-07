package main

import (
	"os"
	"path/filepath"
	"regexp"
	"strings"
	"testing"

	"registry"
)

func TestNginxLandingLocationIsExactSessionGatedRoot(t *testing.T) {
	conf := readNginxConfig(t)
	landing := nginxLocationBlock(t, conf, "location = /srv/gmail/ {")
	prefix := nginxLocationBlock(t, conf, "location /srv/gmail/ {")

	// R-NGNX-3B6C
	if landing == prefix {
		t.Fatal("exact landing location resolved to the bearer-gated prefix block")
	}
	if !strings.Contains(landing, "auth_request /_session-authn;") {
		t.Fatalf("landing location missing session auth_request: %s", landing)
	}
	if strings.Contains(landing, "auth_request /_authn;") {
		t.Fatalf("landing location uses bearer auth_request: %s", landing)
	}

	// R-NGNX-5D8E
	for _, want := range []string{
		"auth_request_set $gmail_session_owner $upstream_http_x_owner_email;",
		"proxy_set_header X-Owner-Email $gmail_session_owner;",
	} {
		if !strings.Contains(landing, want) {
			t.Fatalf("landing location missing %q: %s", want, landing)
		}
	}

	// R-NGNX-7F1G
	if !strings.Contains(landing, "proxy_pass "+registry.BaseURL("gmail")+"/;") {
		t.Fatalf("landing location does not proxy to upstream root with trailing slash: %s", landing)
	}
	for _, want := range []string{
		"proxy_set_header Host $host;",
		"proxy_set_header X-Forwarded-Proto $scheme;",
		"proxy_http_version 1.1;",
	} {
		if !strings.Contains(landing, want) {
			t.Fatalf("landing location missing %q: %s", want, landing)
		}
	}
}

func TestNginxFragmentRetainsBearerAndBootstrapLocations(t *testing.T) {
	conf := readNginxConfig(t)

	prefix := nginxLocationBlock(t, conf, "location /srv/gmail/ {")
	reemit := nginxLocationBlock(t, conf, "location @gmail_authn_500 {")
	prm := nginxLocationBlock(t, conf, "location = /srv/gmail/.well-known/oauth-protected-resource {")

	// R-NGNX-9H3J
	if !strings.Contains(prefix, "auth_request /_authn;") {
		t.Fatalf("prefix location missing bearer auth_request: %s", prefix)
	}
	if !strings.Contains(reemit, "return 429;") || !strings.Contains(reemit, "return 500;") {
		t.Fatalf("authn 500 re-emit location missing expected returns: %s", reemit)
	}
	if !strings.Contains(prm, "proxy_pass "+registry.BaseURL("gmail")+"/.well-known/oauth-protected-resource;") {
		t.Fatalf("PRM bootstrap location missing expected proxy_pass: %s", prm)
	}
	for _, want := range []string{
		"location = /srv/gmail/feed { return 404; }",
		"location = /srv/gmail/ {",
		"location /srv/gmail/static/ {",
	} {
		if !strings.Contains(conf, want) {
			t.Fatalf("nginx fragment missing required location %q", want)
		}
	}
	if regexp.MustCompile(`(?m)^\s*(server|listen)\b`).MatchString(conf) {
		t.Fatalf("nginx fragment must remain a location fragment, not a vhost:\n%s", conf)
	}
	if strings.Count(conf, "location = /srv/gmail/ {") != 1 {
		t.Fatalf("nginx fragment should contain exactly one bare-root exact-match location")
	}
}

func TestNginxStaticLocationIsSessionGated(t *testing.T) {
	conf := readNginxConfig(t)

	landing := nginxLocationBlock(t, conf, "location = /srv/gmail/ {")
	static := nginxLocationBlock(t, conf, "location /srv/gmail/static/ {")
	prefix := nginxLocationBlock(t, conf, "location /srv/gmail/ {")
	reemit := nginxLocationBlock(t, conf, "location @gmail_authn_500 {")
	prm := nginxLocationBlock(t, conf, "location = /srv/gmail/.well-known/oauth-protected-resource {")

	// R-41ZW-HBBA
	if !strings.Contains(static, "auth_request /_session-authn;") {
		t.Fatalf("static location missing session auth_request: %s", static)
	}
	if !strings.Contains(static, "proxy_pass "+registry.BaseURL("gmail")+"/static/;") {
		t.Fatalf("static location missing upstream static proxy_pass: %s", static)
	}
	if !strings.Contains(landing, "proxy_pass "+registry.BaseURL("gmail")+"/;") {
		t.Fatalf("exact landing location changed: %s", landing)
	}
	if !strings.Contains(prefix, "auth_request /_authn;") {
		t.Fatalf("bearer prefix location changed: %s", prefix)
	}
	if !strings.Contains(prm, "proxy_pass "+registry.BaseURL("gmail")+"/.well-known/oauth-protected-resource;") {
		t.Fatalf("PRM bootstrap location changed: %s", prm)
	}
	if !strings.Contains(reemit, "return 429;") || !strings.Contains(reemit, "return 500;") {
		t.Fatalf("authn 500 re-emit location changed: %s", reemit)
	}
	if strings.Count(conf, "location /srv/gmail/static/ {") != 1 {
		t.Fatalf("nginx fragment should contain exactly one static location:\n%s", conf)
	}
}

func readNginxConfig(t *testing.T) string {
	t.Helper()
	src, err := os.ReadFile(filepath.Join("..", "..", "etc", "nginx.conf"))
	if err != nil {
		t.Fatal(err)
	}
	return string(src)
}

func nginxLocationBlock(t *testing.T, conf, opener string) string {
	t.Helper()
	start := strings.Index(conf, opener)
	if start == -1 {
		t.Fatalf("nginx config missing %q", opener)
	}
	bodyStart := start + len(opener)
	endRel := strings.Index(conf[bodyStart:], "\n}")
	if endRel == -1 {
		t.Fatalf("nginx config location %q has no closing brace", opener)
	}
	return conf[start : bodyStart+endRel+len("\n}")]
}
