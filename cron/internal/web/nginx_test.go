package web

import (
	"os"
	"path/filepath"
	"runtime"
	"strings"
	"testing"
)

func TestNginxLandingLocationIsExactSessionGated(t *testing.T) {
	conf := readNginxConfig(t)

	exact := nginxLocationBlock(t, conf, "location = /srv/cron/ {")
	prefix := nginxLocationBlock(t, conf, "location /srv/cron/ {")

	// R-NGNX-3B6C
	if exact == prefix {
		t.Fatal("exact landing location block was not distinct from bearer prefix block")
	}
	if strings.Contains(exact, "location /srv/cron/ {") {
		t.Fatalf("exact landing block contains prefix location header:\n%s", exact)
	}

	// R-NGNX-5D8E
	if !strings.Contains(exact, "auth_request /_session-authn;") {
		t.Fatalf("exact landing block does not use session auth_request:\n%s", exact)
	}
	if strings.Contains(exact, "auth_request /_authn;") {
		t.Fatalf("exact landing block uses bearer auth_request:\n%s", exact)
	}

	// R-NGNX-7F1G
	if !strings.Contains(exact, "proxy_pass http://127.0.0.1:__PORT__/;") {
		t.Fatalf("exact landing block does not proxy to upstream root with trailing slash:\n%s", exact)
	}
}

func TestNginxPreExistingServiceLocationsSurvive(t *testing.T) {
	conf := readNginxConfig(t)
	prefix := nginxLocationBlock(t, conf, "location /srv/cron/ {")

	// R-NGNX-9H3J
	if !strings.Contains(prefix, "auth_request /_authn;") {
		t.Fatalf("bearer prefix block no longer uses bearer auth_request:\n%s", prefix)
	}
	for _, want := range []string{
		"auth_request_set $cron_owner",
		"auth_request_set $cron_client",
		"error_page 500 = @cron_authn_500;",
	} {
		if !strings.Contains(prefix, want) {
			t.Fatalf("bearer prefix block does not contain %q:\n%s", want, prefix)
		}
	}
	for _, want := range []string{
		"location = /srv/cron/feed { return 404; }",
		"location = /srv/cron/.well-known/oauth-protected-resource {",
		"location @cron_authn_500 {",
	} {
		if !strings.Contains(conf, want) {
			t.Fatalf("nginx config does not contain %q:\n%s", want, conf)
		}
	}
}

func readNginxConfig(t *testing.T) string {
	t.Helper()

	_, file, _, ok := runtime.Caller(0)
	if !ok {
		t.Fatal("runtime.Caller failed")
	}
	body, err := os.ReadFile(filepath.Join(filepath.Dir(file), "..", "..", "etc", "nginx.conf"))
	if err != nil {
		t.Fatal(err)
	}
	return string(body)
}

func nginxLocationBlock(t *testing.T, conf, header string) string {
	t.Helper()

	start := strings.Index(conf, header)
	if start < 0 {
		t.Fatalf("nginx config does not contain %q:\n%s", header, conf)
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
	t.Fatalf("nginx config contains unterminated block for %q:\n%s", header, conf[start:])
	return ""
}
