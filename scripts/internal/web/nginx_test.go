package web

import (
	"os"
	"regexp"
	"strings"
	"testing"
)

// readNginxConf loads the on-disk nginx fragment relative to this test.
func readNginxConf(t *testing.T) string {
	t.Helper()
	b, err := os.ReadFile("../../etc/nginx.conf")
	if err != nil {
		t.Fatalf("read nginx.conf: %v", err)
	}
	return string(b)
}

// exactMatchLanding matches the exact-match landing location opener
// `location = /srv/scripts/ {` while NOT matching the prefix form
// `location /srv/scripts/ {` (the `= ` is mandatory).
var exactMatchLanding = regexp.MustCompile(`(?m)^location = /srv/scripts/ \{`)

// prefixGated matches the bearer-gated prefix opener `location /srv/scripts/ {`
// (no `= `).
var prefixGated = regexp.MustCompile(`(?m)^location /srv/scripts/ \{`)

// staticLocation matches the session-gated static assets prefix.
var staticLocation = regexp.MustCompile(`(?m)^location /srv/scripts/static/ \{`)

func TestNginxExactMatchLandingLocationPresent(t *testing.T) {
	// R-NGNX-2A5Q — the file contains an exact-match `location = /srv/scripts/ {`
	// block, distinct from the prefix `location /srv/scripts/ {`.
	conf := readNginxConf(t)
	if !exactMatchLanding.MatchString(conf) {
		t.Fatalf("expected exact-match `location = /srv/scripts/ {` block, not found")
	}
	if !prefixGated.MatchString(conf) {
		t.Fatalf("expected the prefix `location /srv/scripts/ {` block to also be present")
	}
	// The two must be genuinely distinct openers, not the same line counted twice.
	if exactMatchLanding.FindString(conf) == prefixGated.FindString(conf) {
		t.Fatalf("exact-match and prefix openers must differ (got identical matches)")
	}
}

// landingBlock returns the body of the exact-match `location = /srv/scripts/ {`
// block (from its opening brace to the matching closing brace at column 0).
func landingBlock(t *testing.T, conf string) string {
	t.Helper()
	loc := exactMatchLanding.FindStringIndex(conf)
	if loc == nil {
		t.Fatalf("exact-match landing block not found")
	}
	rest := conf[loc[0]:]
	end := strings.Index(rest, "\n}")
	if end < 0 {
		t.Fatalf("could not find end of exact-match landing block")
	}
	return rest[:end+2]
}

// staticBlock returns the body of the `location /srv/scripts/static/ {` block.
func staticBlock(t *testing.T, conf string) string {
	t.Helper()
	loc := staticLocation.FindStringIndex(conf)
	if loc == nil {
		t.Fatalf("static assets block not found")
	}
	rest := conf[loc[0]:]
	end := strings.Index(rest, "\n}")
	if end < 0 {
		t.Fatalf("could not find end of static assets block")
	}
	return rest[:end+2]
}

func TestNginxLandingGatedBySessionAuthnNotBearer(t *testing.T) {
	// R-NGNX-4B7R — the exact-match landing block uses `auth_request /_session-authn`
	// (cookie/session validator) and does NOT gate the landing root with the bearer
	// `auth_request /_authn`.
	conf := readNginxConf(t)
	block := landingBlock(t, conf)
	if !strings.Contains(block, "auth_request /_session-authn;") {
		t.Fatalf("landing block must gate with `auth_request /_session-authn;`, got:\n%s", block)
	}
	if strings.Contains(block, "auth_request /_authn;") {
		t.Fatalf("landing block must NOT gate with the bearer `auth_request /_authn;`, got:\n%s", block)
	}
}

func TestNginxLandingProxiesToLoopbackRoot(t *testing.T) {
	// R-NGNX-6C9S — the exact-match landing block proxies to the loopback upstream
	// root with a trailing slash, and keeps __PORT__ templated (no hardcoded 3009).
	conf := readNginxConf(t)
	block := landingBlock(t, conf)
	if !strings.Contains(block, "proxy_pass http://127.0.0.1:__PORT__/;") {
		t.Fatalf("landing block must proxy_pass to `http://127.0.0.1:__PORT__/;`, got:\n%s", block)
	}
	if strings.Contains(block, "127.0.0.1:3009") {
		t.Fatalf("landing block must keep __PORT__ templated, not hardcode 3009, got:\n%s", block)
	}
}

func TestNginxPreExistingLocationsSurvive(t *testing.T) {
	// R-NGNX-8D1T — the pre-existing locations survive: the bearer-gated prefix
	// with `auth_request /_authn`, the feed 404, and the PRM bootstrap.
	conf := readNginxConf(t)

	if !prefixGated.MatchString(conf) {
		t.Fatalf("bearer-gated prefix `location /srv/scripts/ {` must remain present")
	}
	if !strings.Contains(conf, "auth_request /_authn;") {
		t.Fatalf("bearer-gated prefix must retain `auth_request /_authn;`")
	}
	if !strings.Contains(conf, "location = /srv/scripts/feed { return 404; }") {
		t.Fatalf("feed 404 location must remain present")
	}
	if !strings.Contains(conf, "location = /srv/scripts/.well-known/oauth-protected-resource {") {
		t.Fatalf("PRM bootstrap location must remain present")
	}
}

func TestNginxStaticAssetsLocationSessionGated(t *testing.T) {
	// R-MBDE-270D
	conf := readNginxConf(t)
	block := staticBlock(t, conf)

	for _, want := range []string{
		"auth_request /_session-authn;",
		"proxy_pass http://127.0.0.1:__PORT__/static/;",
		"proxy_set_header Host $host;",
		"proxy_set_header X-Forwarded-Proto $scheme;",
		"proxy_http_version 1.1;",
	} {
		if !strings.Contains(block, want) {
			t.Fatalf("static assets block missing %q:\n%s", want, block)
		}
	}
	if strings.Contains(block, "auth_request /_authn;") {
		t.Fatalf("static assets block must be session-gated, not bearer-gated:\n%s", block)
	}
	if !exactMatchLanding.MatchString(conf) {
		t.Fatalf("exact landing location must remain present")
	}
	if !prefixGated.MatchString(conf) || !strings.Contains(conf, "error_page 500 = @prompts_authn_500;") {
		t.Fatalf("bearer-gated prefix and rate-limit handling must remain present")
	}
	if !strings.Contains(conf, "location = /srv/scripts/feed { return 404; }") {
		t.Fatalf("feed denial location must remain present")
	}
	if !strings.Contains(conf, "location = /srv/scripts/.well-known/oauth-protected-resource {") {
		t.Fatalf("PRM bootstrap location must remain present")
	}
}
