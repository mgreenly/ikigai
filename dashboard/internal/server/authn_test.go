package server

import (
	"context"
	"net/http"
	"net/http/httptest"
	"strings"
	"testing"
	"time"

	"dashboard/internal/oauth"
	"dashboard/internal/pat"
	"dashboard/internal/ratelimit"
)

// mintAccessToken issues a fresh chain + token pair directly through the token
// store (bypassing the OAuth grant flow) so authn tests can control the owner
// email and bound resource. It returns the access-token plaintext.
func mintAccessToken(t *testing.T, d serverDeps, ownerEmail, resource string) string {
	t.Helper()
	ctx := context.Background()
	tx, err := d.db.BeginTx(ctx, nil)
	if err != nil {
		t.Fatalf("BeginTx: %v", err)
	}
	pair, err := d.tokens.IssueChainAndTokens(ctx, tx, "client-abc", ownerEmail, "owner-test", resource)
	if err != nil {
		tx.Rollback()
		t.Fatalf("IssueChainAndTokens: %v", err)
	}
	if err := tx.Commit(); err != nil {
		t.Fatalf("Commit: %v", err)
	}
	return pair.AccessToken
}

// authnServer builds a server over the given deps and the given limiter,
// returning the handler and the deps. A nil limiter uses the harness default.
func authnServer(t *testing.T, d serverDeps, limiter *ratelimit.Limiter) http.Handler {
	t.Helper()
	opts := d.opts()
	if limiter != nil {
		opts.RateLimiter = limiter
	}
	srv, err := New(opts)
	if err != nil {
		t.Fatalf("New: %v", err)
	}
	return srv.Handler
}

// doAuthn issues a POST /internal/authn through h with the given headers and a
// loopback RemoteAddr, returning the recorder.
func doAuthn(h http.Handler, headers map[string]string) *httptest.ResponseRecorder {
	req := httptest.NewRequest("POST", "http://127.0.0.1/internal/authn", nil)
	req.RemoteAddr = "127.0.0.1:54321"
	for k, v := range headers {
		req.Header.Set(k, v)
	}
	rec := httptest.NewRecorder()
	h.ServeHTTP(rec, req)
	return rec
}

const wantPRM = "https://int.ikigenba.com/srv/crm/.well-known/oauth-protected-resource"

func TestAuthnValidToken(t *testing.T) {
	d := newServerDeps(t)
	h := authnServer(t, d, nil)
	tok := mintAccessToken(t, d, "owner@int.ikigenba.com", testResource)

	rec := doAuthn(h, map[string]string{
		"Authorization":  "Bearer " + tok,
		"X-Original-URI": "/srv/crm/mcp",
	})
	if rec.Code != http.StatusOK {
		t.Fatalf("status = %d, want 200; WWW-Authenticate=%q", rec.Code, rec.Header().Get("WWW-Authenticate"))
	}
	if got := rec.Header().Get("X-Owner-Email"); got != "owner@int.ikigenba.com" {
		t.Errorf("X-Owner-Email = %q, want owner@int.ikigenba.com", got)
	}
	if got := rec.Header().Get("X-Client-Id"); got != "client-abc" {
		t.Errorf("X-Client-Id = %q, want client-abc", got)
	}
}

func TestAuthnAcceptsQueryStringOnOriginalURI(t *testing.T) {
	d := newServerDeps(t)
	h := authnServer(t, d, nil)
	tok := mintAccessToken(t, d, "owner@int.ikigenba.com", testResource)

	rec := doAuthn(h, map[string]string{
		"Authorization":  "Bearer " + tok,
		"X-Original-URI": "/srv/crm/mcp?foo=bar",
	})
	if rec.Code != http.StatusOK {
		t.Fatalf("status = %d, want 200", rec.Code)
	}
}

func TestAuthnMissingAuthorization(t *testing.T) {
	d := newServerDeps(t)
	h := authnServer(t, d, nil)

	rec := doAuthn(h, map[string]string{"X-Original-URI": "/srv/crm/mcp"})
	if rec.Code != http.StatusUnauthorized {
		t.Fatalf("status = %d, want 401", rec.Code)
	}
	wa := rec.Header().Get("WWW-Authenticate")
	if !strings.HasPrefix(wa, "Bearer") {
		t.Fatalf("WWW-Authenticate = %q, want Bearer challenge", wa)
	}
	if !strings.Contains(wa, `resource_metadata="`+wantPRM+`"`) {
		t.Errorf("WWW-Authenticate = %q, want resource_metadata=%q", wa, wantPRM)
	}
	if !strings.Contains(wa, `error="invalid_request"`) {
		t.Errorf("WWW-Authenticate = %q, want error=invalid_request", wa)
	}
}

func TestAuthnInvalidToken(t *testing.T) {
	d := newServerDeps(t)
	h := authnServer(t, d, nil)

	rec := doAuthn(h, map[string]string{
		"Authorization":  "Bearer " + oauth.AccessPrefix + "deadbeefdeadbeef",
		"X-Original-URI": "/srv/crm/mcp",
	})
	if rec.Code != http.StatusUnauthorized {
		t.Fatalf("status = %d, want 401", rec.Code)
	}
	wa := rec.Header().Get("WWW-Authenticate")
	if !strings.Contains(wa, `error="invalid_token"`) {
		t.Errorf("WWW-Authenticate = %q, want error=invalid_token", wa)
	}
	if !strings.Contains(wa, `resource_metadata="`+wantPRM+`"`) {
		t.Errorf("WWW-Authenticate = %q, want resource_metadata=%q", wa, wantPRM)
	}
}

func TestAuthnMalformedAuthorizationHeader(t *testing.T) {
	d := newServerDeps(t)
	h := authnServer(t, d, nil)

	// A non-Bearer scheme is a malformed Authorization header → missing_bearer.
	rec := doAuthn(h, map[string]string{
		"Authorization":  "Basic abc123",
		"X-Original-URI": "/srv/crm/mcp",
	})
	if rec.Code != http.StatusUnauthorized {
		t.Fatalf("status = %d, want 401", rec.Code)
	}
	if !strings.Contains(rec.Header().Get("WWW-Authenticate"), `error="invalid_request"`) {
		t.Errorf("WWW-Authenticate = %q, want error=invalid_request", rec.Header().Get("WWW-Authenticate"))
	}
}

func TestAuthnResourceBindingMismatchUnknownService(t *testing.T) {
	d := newServerDeps(t)
	h := authnServer(t, d, nil)
	tok := mintAccessToken(t, d, "owner@int.ikigenba.com", testResource)

	// X-Original-URI addresses a service that has no configured resource.
	rec := doAuthn(h, map[string]string{
		"Authorization":  "Bearer " + tok,
		"X-Original-URI": "/srv/other/mcp",
	})
	if rec.Code != http.StatusUnauthorized {
		t.Fatalf("status = %d, want 401", rec.Code)
	}
	// Unknown service: challenge carries NO resource_metadata.
	if strings.Contains(rec.Header().Get("WWW-Authenticate"), "resource_metadata=") {
		t.Errorf("unknown service must not advertise resource_metadata: %q", rec.Header().Get("WWW-Authenticate"))
	}
}

func TestAuthnMissingOriginalURI(t *testing.T) {
	d := newServerDeps(t)
	h := authnServer(t, d, nil)
	tok := mintAccessToken(t, d, "owner@int.ikigenba.com", testResource)

	rec := doAuthn(h, map[string]string{"Authorization": "Bearer " + tok})
	if rec.Code != http.StatusUnauthorized {
		t.Fatalf("status = %d, want 401", rec.Code)
	}
	if strings.Contains(rec.Header().Get("WWW-Authenticate"), "resource_metadata=") {
		t.Errorf("missing X-Original-URI must not advertise resource_metadata: %q", rec.Header().Get("WWW-Authenticate"))
	}
}

func TestAuthnTokenBoundToDifferentResource(t *testing.T) {
	// Configure two resources; mint a token bound to the second; address the
	// first → the token's resource binding does not match this service.
	d := newServerDeps(t)
	opts := d.opts()
	other := "https://int.ikigenba.com/srv/notes/mcp"
	opts.Resources = []string{testResource, other}
	srv, err := New(opts)
	if err != nil {
		t.Fatalf("New: %v", err)
	}
	tok := mintAccessToken(t, d, "owner@int.ikigenba.com", other)

	rec := doAuthn(srv.Handler, map[string]string{
		"Authorization":  "Bearer " + tok,
		"X-Original-URI": "/srv/crm/mcp",
	})
	if rec.Code != http.StatusUnauthorized {
		t.Fatalf("status = %d, want 401", rec.Code)
	}
	wa := rec.Header().Get("WWW-Authenticate")
	if !strings.Contains(wa, `error="invalid_token"`) {
		t.Errorf("WWW-Authenticate = %q, want error=invalid_token", wa)
	}
	// Challenge is for the addressed service (crm), regardless of token binding.
	if !strings.Contains(wa, `resource_metadata="`+wantPRM+`"`) {
		t.Errorf("WWW-Authenticate = %q, want resource_metadata=%q", wa, wantPRM)
	}
}

func TestAuthnOwnerOutsideWorkspace(t *testing.T) {
	d := newServerDeps(t)
	h := authnServer(t, d, nil)
	// Owner email's domain is outside testWorkspaceDomain ("int.ikigenba.com").
	tok := mintAccessToken(t, d, "owner@evil.example", testResource)

	rec := doAuthn(h, map[string]string{
		"Authorization":  "Bearer " + tok,
		"X-Original-URI": "/srv/crm/mcp",
	})
	if rec.Code != http.StatusUnauthorized {
		t.Fatalf("status = %d, want 401", rec.Code)
	}
	if !strings.Contains(rec.Header().Get("WWW-Authenticate"), `error="invalid_token"`) {
		t.Errorf("WWW-Authenticate = %q, want error=invalid_token", rec.Header().Get("WWW-Authenticate"))
	}
}

func TestAuthnRateLimited(t *testing.T) {
	d := newServerDeps(t)
	// A limit of 1 per minute: the first request passes, the second is over.
	h := authnServer(t, d, ratelimit.New(1, time.Minute))
	tok := mintAccessToken(t, d, "owner@int.ikigenba.com", testResource)
	headers := map[string]string{
		"Authorization":  "Bearer " + tok,
		"X-Original-URI": "/srv/crm/mcp",
	}

	if rec := doAuthn(h, headers); rec.Code != http.StatusOK {
		t.Fatalf("first request status = %d, want 200", rec.Code)
	}
	rec := doAuthn(h, headers)
	if rec.Code != http.StatusTooManyRequests {
		t.Fatalf("second request status = %d, want 429", rec.Code)
	}
	if ra := rec.Header().Get("Retry-After"); ra != "60" {
		t.Errorf("Retry-After = %q, want 60", ra)
	}
}

// secondResource is a second configured resource used by the PAT tests to prove
// a single PAT authenticates across more than one service (cross-service by
// definition — it is bound to no single resource).
const secondResource = "https://int.ikigenba.com/srv/notes/mcp"

const wantPRMNotes = "https://int.ikigenba.com/srv/notes/.well-known/oauth-protected-resource"

// twoResourceServer builds a server configured with both testResource (crm) and
// secondResource (notes), so PAT tests can address two different services.
func twoResourceServer(t *testing.T, d serverDeps, limiter *ratelimit.Limiter) http.Handler {
	t.Helper()
	opts := d.opts()
	opts.Resources = []string{testResource, secondResource}
	if limiter != nil {
		opts.RateLimiter = limiter
	}
	srv, err := New(opts)
	if err != nil {
		t.Fatalf("New: %v", err)
	}
	return srv.Handler
}

// mintPAT creates a PAT directly through the store and returns its plaintext.
func mintPAT(t *testing.T, d serverDeps, ownerEmail string) (plaintext string, p pat.PAT) {
	t.Helper()
	plaintext, p, err := d.pats.Create(context.Background(), ownerEmail, "owner-test", "test pat")
	if err != nil {
		t.Fatalf("pats.Create: %v", err)
	}
	return plaintext, p
}

// TestAuthnPATValidCrossService proves one PAT authenticates against two
// different services, and that the identity headers match ADR §D5 (notably
// X-Client-Id = pat:<public_id> and NO X-Chain-Id).
func TestAuthnPATValidCrossService(t *testing.T) {
	d := newServerDeps(t)
	h := twoResourceServer(t, d, nil)
	tok, p := mintPAT(t, d, "owner@int.ikigenba.com")

	for _, uri := range []string{"/srv/crm/mcp", "/srv/notes/mcp"} {
		rec := doAuthn(h, map[string]string{
			"Authorization":  "Bearer " + tok,
			"X-Original-URI": uri,
		})
		if rec.Code != http.StatusOK {
			t.Fatalf("uri %s: status = %d, want 200; WWW-Authenticate=%q", uri, rec.Code, rec.Header().Get("WWW-Authenticate"))
		}
		if got := rec.Header().Get("X-Owner-Email"); got != "owner@int.ikigenba.com" {
			t.Errorf("uri %s: X-Owner-Email = %q, want owner@int.ikigenba.com", uri, got)
		}
		if got, want := rec.Header().Get("X-Client-Id"), "pat:"+p.PublicID; got != want {
			t.Errorf("uri %s: X-Client-Id = %q, want %q", uri, got, want)
		}
		if got := rec.Header().Get("X-Token-Id"); got != p.ID {
			t.Errorf("uri %s: X-Token-Id = %q, want %q", uri, got, p.ID)
		}
		// ADR §D5: a PAT has no chain — X-Chain-Id must be absent.
		if got := rec.Header().Get("X-Chain-Id"); got != "" {
			t.Errorf("uri %s: X-Chain-Id = %q, want absent for PAT", uri, got)
		}
	}
}

// TestAuthnPATRevoked: a revoked PAT is rejected with a 401 invalid_token,
// carrying the addressed service's resource_metadata.
func TestAuthnPATRevoked(t *testing.T) {
	d := newServerDeps(t)
	h := twoResourceServer(t, d, nil)
	tok, p := mintPAT(t, d, "owner@int.ikigenba.com")
	if err := d.pats.Revoke(context.Background(), p.ID); err != nil {
		t.Fatalf("Revoke: %v", err)
	}

	rec := doAuthn(h, map[string]string{
		"Authorization":  "Bearer " + tok,
		"X-Original-URI": "/srv/crm/mcp",
	})
	if rec.Code != http.StatusUnauthorized {
		t.Fatalf("status = %d, want 401", rec.Code)
	}
	wa := rec.Header().Get("WWW-Authenticate")
	if !strings.Contains(wa, `error="invalid_token"`) {
		t.Errorf("WWW-Authenticate = %q, want error=invalid_token", wa)
	}
	if !strings.Contains(wa, `resource_metadata="`+wantPRM+`"`) {
		t.Errorf("WWW-Authenticate = %q, want resource_metadata=%q", wa, wantPRM)
	}
}

// TestAuthnPATOwnerOutsideWorkspace: the (f) workspace check is retained for
// PATs — an owner outside the configured domain is rejected.
func TestAuthnPATOwnerOutsideWorkspace(t *testing.T) {
	d := newServerDeps(t)
	h := twoResourceServer(t, d, nil)
	tok, _ := mintPAT(t, d, "owner@evil.example")

	rec := doAuthn(h, map[string]string{
		"Authorization":  "Bearer " + tok,
		"X-Original-URI": "/srv/notes/mcp",
	})
	if rec.Code != http.StatusUnauthorized {
		t.Fatalf("status = %d, want 401", rec.Code)
	}
	wa := rec.Header().Get("WWW-Authenticate")
	if !strings.Contains(wa, `error="invalid_token"`) {
		t.Errorf("WWW-Authenticate = %q, want error=invalid_token", wa)
	}
	if !strings.Contains(wa, `resource_metadata="`+wantPRMNotes+`"`) {
		t.Errorf("WWW-Authenticate = %q, want resource_metadata=%q", wa, wantPRMNotes)
	}
}

// TestAuthnPATRateLimited: the (g) rate limit is retained for PATs, keyed on the
// PAT's internal id.
func TestAuthnPATRateLimited(t *testing.T) {
	d := newServerDeps(t)
	h := twoResourceServer(t, d, ratelimit.New(1, time.Minute))
	tok, _ := mintPAT(t, d, "owner@int.ikigenba.com")
	headers := map[string]string{
		"Authorization":  "Bearer " + tok,
		"X-Original-URI": "/srv/crm/mcp",
	}

	if rec := doAuthn(h, headers); rec.Code != http.StatusOK {
		t.Fatalf("first request status = %d, want 200", rec.Code)
	}
	rec := doAuthn(h, headers)
	if rec.Code != http.StatusTooManyRequests {
		t.Fatalf("second request status = %d, want 429", rec.Code)
	}
	if ra := rec.Header().Get("Retry-After"); ra != "60" {
		t.Errorf("Retry-After = %q, want 60", ra)
	}
}

func TestAuthnRejectsNonLoopback(t *testing.T) {
	d := newServerDeps(t)
	h := authnServer(t, d, nil)
	tok := mintAccessToken(t, d, "owner@int.ikigenba.com", testResource)

	req := httptest.NewRequest("POST", "http://127.0.0.1/internal/authn", nil)
	req.RemoteAddr = "203.0.113.7:40000" // non-loopback
	req.Header.Set("Authorization", "Bearer "+tok)
	req.Header.Set("X-Original-URI", "/srv/crm/mcp")
	rec := httptest.NewRecorder()
	h.ServeHTTP(rec, req)

	if rec.Code != http.StatusForbidden {
		t.Fatalf("status = %d, want 403", rec.Code)
	}
}
