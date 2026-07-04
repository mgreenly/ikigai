package web

import (
	"os"
	"strings"
	"testing"

	"registry"
)

// fragmentPath is the notify nginx location fragment, relative to
// notify/internal/web/ (this test's directory).
const fragmentPath = "../../etc/nginx.conf"

func readFragment(t *testing.T) string {
	t.Helper()
	b, err := os.ReadFile(fragmentPath)
	if err != nil {
		t.Fatalf("read %s: %v", fragmentPath, err)
	}
	return string(b)
}

// locationBlock returns the body of a location block (up to its closing brace at
// column 0), or "" if absent.
func locationBlock(frag, marker string) string {
	i := strings.Index(frag, marker)
	if i < 0 {
		return ""
	}
	rest := frag[i+len(marker):]
	if end := strings.Index(rest, "\n}"); end >= 0 {
		return rest[:end]
	}
	return rest
}

func exactMatchBlock(frag string) string {
	return locationBlock(frag, "location = /srv/notify/ {")
}

func TestNginxHasExactMatchLandingLocation(t *testing.T) {
	// R-NGNX-3N6X — the fragment contains an exact-match `location = /srv/notify/ {`
	// block, distinct from the prefix form `location /srv/notify/ {`. Require the
	// `= ` form specifically so the prefix location does not satisfy the assertion.
	frag := readFragment(t)
	if !strings.Contains(frag, "location = /srv/notify/ {") {
		t.Fatal("missing exact-match `location = /srv/notify/ {` block")
	}
	// The exact-match marker must be the `= ` form, not the bare prefix that
	// would also be a substring without the `= `.
	if strings.Index(frag, "location = /srv/notify/ {") == strings.Index(frag, "location /srv/notify/ {") {
		t.Fatal("exact-match and prefix locations must be distinct blocks")
	}
}

func TestNginxExactMatchUsesSessionAuthn(t *testing.T) {
	// R-NGNX-5P8Y — the exact-match block gates the landing root with the session
	// hook `auth_request /_session-authn` and does NOT use the bearer hook
	// `auth_request /_authn`.
	block := exactMatchBlock(readFragment(t))
	if block == "" {
		t.Fatal("exact-match `location = /srv/notify/ {` block not found")
	}
	if !strings.Contains(block, "auth_request /_session-authn") {
		t.Errorf("exact-match block missing `auth_request /_session-authn`:\n%s", block)
	}
	if strings.Contains(block, "auth_request /_authn") {
		t.Errorf("exact-match block must NOT gate landing root with bearer `auth_request /_authn`:\n%s", block)
	}
}

func TestNginxExactMatchProxiesToLoopbackRoot(t *testing.T) {
	// R-NGNX-7Q1Z — the exact-match block proxies to the loopback upstream root
	// with a trailing slash.
	// R-RGDR-4F6Q
	block := exactMatchBlock(readFragment(t))
	if block == "" {
		t.Fatal("exact-match `location = /srv/notify/ {` block not found")
	}
	want := "proxy_pass " + registry.BaseURL("notify") + "/;"
	if !strings.Contains(block, want) {
		t.Errorf("exact-match block missing %q:\n%s", want, block)
	}
}

func TestNginxPreExistingLocationsSurvive(t *testing.T) {
	// R-NGNX-9R3B — the additive edit preserves the bearer-gated prefix location
	// (with `auth_request /_authn`) and the unauthenticated PRM bootstrap.
	frag := readFragment(t)
	if !strings.Contains(frag, "location /srv/notify/ {") {
		t.Error("bearer-gated prefix `location /srv/notify/ {` missing")
	}
	if !strings.Contains(frag, "auth_request /_authn;") {
		t.Error("bearer-gated prefix must still use `auth_request /_authn;`")
	}
	if !strings.Contains(frag, "location = /srv/notify/.well-known/oauth-protected-resource {") {
		t.Error("PRM bootstrap location missing")
	}
}

func TestNginxSessionGatesStaticAssets(t *testing.T) {
	// R-8ONM-1TCP — static assets are session-gated while existing landing, bearer, PRM, and rate-limit locations remain.
	frag := readFragment(t)
	block := locationBlock(frag, "location /srv/notify/static/ {")
	if block == "" {
		t.Fatal("static asset `location /srv/notify/static/ {` block not found")
	}
	wantStatic := "proxy_pass " + registry.BaseURL("notify") + "/static/;"
	for _, want := range []string{
		"auth_request /_session-authn;",
		wantStatic,
		"proxy_set_header Host $host;",
		"proxy_set_header X-Forwarded-Proto $scheme;",
		"proxy_http_version 1.1;",
	} {
		if !strings.Contains(block, want) {
			t.Fatalf("static asset block missing %q:\n%s", want, block)
		}
	}
	for _, want := range []string{
		"location = /srv/notify/ {",
		"location /srv/notify/ {",
		"location = /srv/notify/.well-known/oauth-protected-resource {",
		"location @notify_authn_500 {",
	} {
		if !strings.Contains(frag, want) {
			t.Fatalf("pre-existing nginx location %q missing", want)
		}
	}
}
