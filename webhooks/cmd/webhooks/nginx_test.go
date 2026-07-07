package main

import (
	"os"
	"strings"
	"testing"

	"registry"
)

func TestNginxLandingLocationIsSessionGatedExactMatch(t *testing.T) {
	conf := readNginxConfig(t)
	block := nginxLocationBlock(t, conf, "location = /srv/webhooks/ {")
	catchAll := "location /srv/webhooks/ { return 404; }"

	// R-TTUW-5O3V
	sessionOwner := sessionOwnerVariable(t, block)
	if !strings.Contains(conf, catchAll) {
		t.Fatalf("nginx config does not contain distinct catch-all %q", catchAll)
	}
	if strings.Contains(block, catchAll) || strings.Contains(block, "return 404;") {
		t.Fatalf("exact landing location was not distinct from catch-all:\n%s", block)
	}
	if !strings.Contains(block, "auth_request /_session-authn;") {
		t.Fatalf("landing location does not use session auth:\n%s", block)
	}
	if strings.Contains(block, "auth_request /_authn;") {
		t.Fatalf("landing location uses bearer auth instead of session auth:\n%s", block)
	}
	if !strings.Contains(block, "proxy_set_header X-Owner-Email "+sessionOwner+";") {
		t.Fatalf("landing location does not forward captured session owner %s:\n%s", sessionOwner, block)
	}
	// R-0FNQ-0XSK
	if !strings.Contains(block, "proxy_pass "+registry.BaseURL("webhooks")+"/;") {
		t.Fatalf("landing location does not proxy to upstream root with trailing slash:\n%s", block)
	}
}

func TestNginxStaticLocationIsSessionGatedAndStripsMount(t *testing.T) {
	conf := readNginxConfig(t)
	block := nginxLocationBlock(t, conf, "location /srv/webhooks/static/ {")

	// R-TV2S-JFUK
	if !strings.Contains(block, "auth_request /_session-authn;") {
		t.Fatalf("static location does not use session auth:\n%s", block)
	}
	if strings.Contains(block, "auth_request /_authn;") {
		t.Fatalf("static location uses bearer auth instead of session auth:\n%s", block)
	}
	// R-0FNQ-0XSK
	if !strings.Contains(block, "proxy_pass "+registry.BaseURL("webhooks")+"/static/;") {
		t.Fatalf("static location does not proxy to upstream static with trailing slash:\n%s", block)
	}
}

func TestNginxPriorTiersRemainIntact(t *testing.T) {
	conf := readNginxConfig(t)
	mcp := nginxLocationBlock(t, conf, "location = /srv/webhooks/mcp {")

	// R-TWAO-X7L9
	if !strings.Contains(conf, "location /srv/webhooks/ { return 404; }") {
		t.Fatalf("nginx config no longer contains verbatim catch-all shield")
	}
	if !strings.Contains(mcp, "auth_request /_authn;") {
		t.Fatalf("mcp location no longer uses bearer auth:\n%s", mcp)
	}
	if !strings.Contains(conf, "location = /srv/webhooks/feed { return 404; }") {
		t.Fatalf("nginx config no longer contains verbatim feed shield")
	}
}

func readNginxConfig(t *testing.T) string {
	t.Helper()

	body, err := os.ReadFile("../../etc/nginx.conf")
	if err != nil {
		t.Fatal(err)
	}
	return string(body)
}

func nginxLocationBlock(t *testing.T, conf, opener string) string {
	t.Helper()

	start := strings.Index(conf, opener)
	if start < 0 {
		t.Fatalf("nginx config does not contain location opener %q", opener)
	}
	bodyStart := start + len(opener)
	relEnd := strings.Index(conf[bodyStart:], "\n}")
	if relEnd < 0 {
		t.Fatalf("nginx location %q does not have a matching closing brace", opener)
	}
	end := bodyStart + relEnd + len("\n}")
	return conf[start:end]
}

func sessionOwnerVariable(t *testing.T, block string) string {
	t.Helper()

	for _, line := range strings.Split(block, "\n") {
		line = strings.TrimSpace(line)
		if !strings.HasPrefix(line, "auth_request_set ") || !strings.HasSuffix(line, " $upstream_http_x_owner_email;") {
			continue
		}
		fields := strings.Fields(line)
		if len(fields) != 3 {
			t.Fatalf("session owner auth_request_set has unexpected shape %q", line)
		}
		return fields[1]
	}
	t.Fatalf("landing location does not capture upstream owner email:\n%s", block)
	return ""
}
