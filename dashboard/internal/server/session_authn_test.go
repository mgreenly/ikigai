package server

import (
	"context"
	"net/http"
	"net/http/httptest"
	"testing"
)

// doSessionAuthn issues a request to /internal/session-authn through h with the
// given Cookie header value (empty = no cookie) and a loopback RemoteAddr,
// returning the recorder. It mirrors authn_test.go's doAuthn harness.
func doSessionAuthn(h http.Handler, cookie string) *httptest.ResponseRecorder {
	req := httptest.NewRequest("GET", "http://127.0.0.1/internal/session-authn", nil)
	req.RemoteAddr = "127.0.0.1:54321"
	if cookie != "" {
		req.Header.Set("Cookie", sessionCookieName+"="+cookie)
	}
	rec := httptest.NewRecorder()
	h.ServeHTTP(rec, req)
	return rec
}

// liveSessionCookie mints a real, store-backed web session for ownerEmail
// directly through the session store and returns its plaintext cookie value.
func liveSessionCookie(t *testing.T, d serverDeps, ownerEmail string) string {
	t.Helper()
	issued, err := d.sessions.Create(context.Background(), ownerEmail)
	if err != nil {
		t.Fatalf("sessions.Create: %v", err)
	}
	return issued.CookieValue
}

func TestSessionAuthnValidCookie(t *testing.T) {
	d := newServerDeps(t)
	h := authnServer(t, d, nil)
	cookie := liveSessionCookie(t, d, "owner@int.ikigenba.com")

	rec := doSessionAuthn(h, cookie)
	if rec.Code != http.StatusOK {
		t.Fatalf("status = %d, want 200", rec.Code)
	}
	if got := rec.Header().Get("X-Owner-Email"); got != "owner@int.ikigenba.com" {
		t.Errorf("X-Owner-Email = %q, want owner@int.ikigenba.com", got)
	}
}

func TestSessionAuthnMissingCookie(t *testing.T) {
	d := newServerDeps(t)
	h := authnServer(t, d, nil)

	rec := doSessionAuthn(h, "")
	if rec.Code != http.StatusUnauthorized {
		t.Fatalf("status = %d, want 401", rec.Code)
	}
	if got := rec.Header().Get("X-Owner-Email"); got != "" {
		t.Errorf("X-Owner-Email = %q, want empty on denial", got)
	}
}

func TestSessionAuthnInvalidCookie(t *testing.T) {
	d := newServerDeps(t)
	h := authnServer(t, d, nil)

	rec := doSessionAuthn(h, "not-a-real-session-value")
	if rec.Code != http.StatusUnauthorized {
		t.Fatalf("status = %d, want 401", rec.Code)
	}
}

func TestSessionAuthnRejectsNonLoopback(t *testing.T) {
	d := newServerDeps(t)
	h := authnServer(t, d, nil)
	cookie := liveSessionCookie(t, d, "owner@int.ikigenba.com")

	req := httptest.NewRequest("GET", "http://127.0.0.1/internal/session-authn", nil)
	req.RemoteAddr = "203.0.113.7:40000" // non-loopback
	req.Header.Set("Cookie", sessionCookieName+"="+cookie)
	rec := httptest.NewRecorder()
	h.ServeHTTP(rec, req)

	if rec.Code != http.StatusForbidden {
		t.Fatalf("status = %d, want 403 (match handleAuthn non-loopback denial)", rec.Code)
	}
}
