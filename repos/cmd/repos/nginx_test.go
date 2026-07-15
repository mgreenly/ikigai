package main

import (
	"os"
	"path/filepath"
	"strings"
	"testing"

	"registry"
)

func TestNginxFragmentEnforcesPublicRouteTiers(t *testing.T) {
	// R-G1OF-AAC8
	contents, err := os.ReadFile(filepath.Join("..", "..", "etc", "nginx.conf"))
	if err != nil {
		t.Fatal(err)
	}
	conf := string(contents)
	upstream := registry.BaseURL("repos")
	if upstream != "http://127.0.0.1:3007" {
		t.Fatalf("repos registry base URL = %q, want loopback port 3007", upstream)
	}

	prm := nginxBlock(t, conf, "location = /srv/repos/.well-known/oauth-protected-resource {")
	if strings.Contains(prm, "auth_request") {
		t.Fatalf("PRM bootstrap is authentication-gated:\n%s", prm)
	}
	if !strings.Contains(prm, "proxy_pass "+upstream+"/.well-known/oauth-protected-resource;") {
		t.Fatalf("PRM bootstrap does not proxy to the repos upstream:\n%s", prm)
	}
	if !strings.Contains(conf, "location = /srv/repos/feed { return 404; }") {
		t.Fatalf("nginx fragment does not deny the public feed route:\n%s", conf)
	}

	landing := nginxBlock(t, conf, "location = /srv/repos/ {")
	assets := nginxBlock(t, conf, "location /srv/repos/static/ {")
	for name, tier := range map[string]struct {
		block     string
		proxyPass string
	}{
		"landing":       {block: landing, proxyPass: "proxy_pass " + upstream + "/;"},
		"static assets": {block: assets, proxyPass: "proxy_pass " + upstream + "/static/;"},
	} {
		t.Run(name, func(t *testing.T) {
			for _, directive := range []string{
				"auth_request /_session-authn;",
				"error_page 401 = @login_bounce;",
				tier.proxyPass,
			} {
				if !strings.Contains(tier.block, directive) {
					t.Fatalf("session tier missing %q:\n%s", directive, tier.block)
				}
			}
			if strings.Contains(tier.block, "auth_request /_authn;") {
				t.Fatalf("session tier uses bearer authentication:\n%s", tier.block)
			}
		})
	}

	bearer := nginxBlock(t, conf, "location /srv/repos/ {")
	for _, directive := range []string{
		"auth_request /_authn;",
		"auth_request_set $repos_owner  $upstream_http_x_owner_email;",
		"auth_request_set $repos_client $upstream_http_x_client_id;",
		"proxy_set_header X-Owner-Email $repos_owner;",
		"proxy_set_header X-Client-Id  $repos_client;",
		"error_page 500 = @repos_authn_500;",
		"proxy_pass " + upstream + "/;",
	} {
		if !strings.Contains(bearer, directive) {
			t.Fatalf("bearer tier missing %q:\n%s", directive, bearer)
		}
	}
	if strings.Contains(bearer, "error_page 401 = @login_bounce;") {
		t.Fatalf("bearer tier redirects OAuth clients to browser login:\n%s", bearer)
	}
	recovery := nginxBlock(t, conf, "location @repos_authn_500 {")
	for _, directive := range []string{"if ($authn_status = 429)", "add_header Retry-After $authn_retry always;", "return 429;", "return 500;"} {
		if !strings.Contains(recovery, directive) {
			t.Fatalf("429 recovery tier missing %q:\n%s", directive, recovery)
		}
	}
	for _, forbidden := range []string{"server {", "listen ", "ssl_certificate"} {
		if strings.Contains(conf, forbidden) {
			t.Fatalf("location fragment contains vhost directive %q", forbidden)
		}
	}
}

func nginxBlock(t *testing.T, conf, header string) string {
	t.Helper()
	start := strings.Index(conf, header)
	if start < 0 {
		t.Fatalf("nginx fragment does not contain %q:\n%s", header, conf)
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
	t.Fatalf("nginx fragment contains unterminated block %q", header)
	return ""
}
