package webhooks

import (
	"context"
	"crypto/hmac"
	"crypto/sha256"
	"database/sql"
	"encoding/base64"
	"encoding/hex"
	"encoding/json"
	"io"
	"log/slog"
	"net/http"
	"net/http/httptest"
	"path/filepath"
	"strings"
	"testing"
	"time"

	chassis "appkit/db"
	"eventplane/outbox"

	"webhooks/internal/db"
)

// newIngressFixture stands up a real temp-file SQLite (never :memory:), migrates
// it, wires a Service with a real *outbox.Outbox over a deterministic clock,
// provisions one webhook through Create, and returns the ingress handler, the raw
// *sql.DB (for asserting real outbox row counts), the live webhook name, and the
// plaintext secret shown once by Create.
func newIngressFixture(t *testing.T) (h http.Handler, conn *sql.DB, name, secret string) {
	h, conn, _, name, secret = newIngressFixtureScheme(t, "bearer")
	return
}

func newIngressFixtureScheme(t *testing.T, scheme string) (h http.Handler, conn *sql.DB, svc *Service, name, secret string) {
	t.Helper()
	dbPath := filepath.Join(t.TempDir(), "webhooks.db")
	conn, err := chassis.Open(dbPath)
	if err != nil {
		t.Fatalf("chassis.Open: %v", err)
	}
	t.Cleanup(func() { conn.Close() })
	migs, err := chassis.LoadMigrations(db.FS, "migrations")
	if err != nil {
		t.Fatalf("chassis.LoadMigrations: %v", err)
	}
	if err := chassis.Migrate(context.Background(), conn, migs); err != nil {
		t.Fatalf("chassis.Migrate: %v", err)
	}
	now := time.Date(2026, 6, 25, 12, 0, 0, 0, time.UTC)
	clk := fixedClock{t: now}
	ob, err := outbox.New(conn, outbox.Options{Source: "webhooks", Registry: Events, Now: clk.Now})
	if err != nil {
		t.Fatalf("outbox.New: %v", err)
	}
	svc = NewService(conn, clk)
	svc.Outbox = ob

	wh, sec, err := svc.Create(context.Background(), "owner@example.com", "deploy-hook", scheme)
	if err != nil {
		t.Fatalf("Create: %v", err)
	}
	log := slog.New(slog.NewTextHandler(io.Discard, nil))
	return NewIngressHandler(svc, log), conn, svc, wh.Name, sec
}

func githubSignature(secret string, body []byte) string {
	mac := hmac.New(sha256.New, []byte(secret))
	_, _ = mac.Write(body)
	return "sha256=" + hex.EncodeToString(mac.Sum(nil))
}

func onlyPayloadMap(t *testing.T, conn *sql.DB) map[string]any {
	t.Helper()
	var raw string
	if err := conn.QueryRow(`SELECT payload FROM outbox`).Scan(&raw); err != nil {
		t.Fatal(err)
	}
	var payload map[string]any
	if err := json.Unmarshal([]byte(raw), &payload); err != nil {
		t.Fatal(err)
	}
	return payload
}

// countOutbox returns the live number of outbox rows read through conn.
func countOutbox(t *testing.T, conn *sql.DB) int {
	t.Helper()
	var n int
	if err := conn.QueryRow(`SELECT count(*) FROM outbox`).Scan(&n); err != nil {
		t.Fatalf("count outbox: %v", err)
	}
	return n
}

// doIngress drives one request through the handler and returns the recorded
// response.
func doIngress(h http.Handler, req *http.Request) *httptest.ResponseRecorder {
	rec := httptest.NewRecorder()
	h.ServeHTTP(rec, req)
	return rec
}

// R-7ISQ-ZZCF — a correct-secret POST with an in-cap body is accepted (202) with
// the exact JSON body, driven over a real temp-file SQLite Service and real outbox.
func TestIngress_CorrectSecretInCapBodyAccepted(t *testing.T) {
	h, conn, name, secret := newIngressFixture(t)

	req := httptest.NewRequest(http.MethodPost, "/in/"+name, strings.NewReader(`{"hello":"world"}`))
	req.Header.Set("Authorization", "Bearer "+secret)
	req.Header.Set("Content-Type", "application/json")
	rec := doIngress(h, req)

	if rec.Code != http.StatusAccepted {
		t.Fatalf("status = %d, want 202; body=%q", rec.Code, rec.Body.String())
	}
	if got := rec.Body.String(); got != `{"status":"accepted"}` {
		t.Fatalf("body = %q, want %q", got, `{"status":"accepted"}`)
	}
	if n := countOutbox(t, conn); n != 1 {
		t.Fatalf("outbox rows = %d, want 1", n)
	}
}

// R-7K0N-DR34 — wrong secret, unknown name, and missing Authorization each return
// a byte-identical 404 (status AND body) and none appends an outbox row.
func TestIngress_AuthFailuresAreByteIdenticalAndAppendNothing(t *testing.T) {
	h, conn, name, secret := newIngressFixture(t)

	wrongSecret := httptest.NewRequest(http.MethodPost, "/in/"+name, strings.NewReader("x"))
	wrongSecret.Header.Set("Authorization", "Bearer "+secret+"-tampered")

	unknownName := httptest.NewRequest(http.MethodPost, "/in/does-not-exist", strings.NewReader("x"))
	unknownName.Header.Set("Authorization", "Bearer "+secret)

	missingAuth := httptest.NewRequest(http.MethodPost, "/in/"+name, strings.NewReader("x"))

	recs := []*httptest.ResponseRecorder{
		doIngress(h, wrongSecret),
		doIngress(h, unknownName),
		doIngress(h, missingAuth),
	}

	for i, rec := range recs {
		if rec.Code != http.StatusNotFound {
			t.Fatalf("case %d: status = %d, want 404", i, rec.Code)
		}
	}
	if b0, b1, b2 := recs[0].Body.String(), recs[1].Body.String(), recs[2].Body.String(); b0 != b1 || b1 != b2 {
		t.Fatalf("404 bodies differ: %q / %q / %q", b0, b1, b2)
	}
	if n := countOutbox(t, conn); n != 0 {
		t.Fatalf("outbox rows = %d, want 0 (no auth failure may record)", n)
	}
}

// R-7L8J-RITT — a correct-secret POST carrying a front-door identity header
// (X-Owner-Email or X-Client-Id) is rejected 404 with no outbox row; the same
// request carrying only X-Forwarded-Proto is accepted 202.
func TestIngress_IdentityHeadersRejectedButForwardedProtoAllowed(t *testing.T) {
	for _, h := range []string{"X-Owner-Email", "X-Client-Id"} {
		t.Run(h, func(t *testing.T) {
			handler, conn, name, secret := newIngressFixture(t)
			req := httptest.NewRequest(http.MethodPost, "/in/"+name, strings.NewReader("x"))
			req.Header.Set("Authorization", "Bearer "+secret)
			switch h {
			case "X-Owner-Email":
				req.Header.Set(h, "attacker@example.com")
			case "X-Client-Id":
				req.Header.Set(h, "some-client")
			}
			rec := doIngress(handler, req)
			if rec.Code != http.StatusNotFound {
				t.Fatalf("status = %d, want 404", rec.Code)
			}
			if n := countOutbox(t, conn); n != 0 {
				t.Fatalf("outbox rows = %d, want 0", n)
			}
		})
	}

	t.Run("X-Forwarded-Proto allowed", func(t *testing.T) {
		handler, conn, name, secret := newIngressFixture(t)
		req := httptest.NewRequest(http.MethodPost, "/in/"+name, strings.NewReader("x"))
		req.Header.Set("Authorization", "Bearer "+secret)
		req.Header.Set("X-Forwarded-Proto", "https")
		rec := doIngress(handler, req)
		if rec.Code != http.StatusAccepted {
			t.Fatalf("status = %d, want 202; body=%q", rec.Code, rec.Body.String())
		}
		if n := countOutbox(t, conn); n != 1 {
			t.Fatalf("outbox rows = %d, want 1", n)
		}
	})
}

// R-7MGG-5AKI — with a correct secret, a body of maxBodyBytes+1 is rejected 413
// with no outbox row; a body of exactly maxBodyBytes is accepted 202.
func TestIngress_BodyCapEnforced(t *testing.T) {
	t.Run("over cap rejected", func(t *testing.T) {
		h, conn, name, secret := newIngressFixture(t)
		body := strings.Repeat("a", maxBodyBytes+1)
		req := httptest.NewRequest(http.MethodPost, "/in/"+name, strings.NewReader(body))
		req.Header.Set("Authorization", "Bearer "+secret)
		rec := doIngress(h, req)
		if rec.Code != http.StatusRequestEntityTooLarge {
			t.Fatalf("status = %d, want 413; body=%q", rec.Code, rec.Body.String())
		}
		if n := countOutbox(t, conn); n != 0 {
			t.Fatalf("outbox rows = %d, want 0", n)
		}
	})

	t.Run("at cap accepted", func(t *testing.T) {
		h, conn, name, secret := newIngressFixture(t)
		body := strings.Repeat("a", maxBodyBytes)
		req := httptest.NewRequest(http.MethodPost, "/in/"+name, strings.NewReader(body))
		req.Header.Set("Authorization", "Bearer "+secret)
		rec := doIngress(h, req)
		if rec.Code != http.StatusAccepted {
			t.Fatalf("status = %d, want 202; body=%q", rec.Code, rec.Body.String())
		}
		if n := countOutbox(t, conn); n != 1 {
			t.Fatalf("outbox rows = %d, want 1", n)
		}
	})
}

// R-7NOC-J2B7 — a non-POST request to /in/<name> returns 405 whether or not the
// name exists, and never consults the store (no outbox row either way).
func TestIngress_NonPostRejectedRegardlessOfName(t *testing.T) {
	h, conn, existing, _ := newIngressFixture(t)

	for _, name := range []string{existing, "does-not-exist"} {
		req := httptest.NewRequest(http.MethodGet, "/in/"+name, nil)
		rec := doIngress(h, req)
		if rec.Code != http.StatusMethodNotAllowed {
			t.Fatalf("GET /in/%s: status = %d, want 405", name, rec.Code)
		}
	}
	if n := countOutbox(t, conn); n != 0 {
		t.Fatalf("outbox rows = %d, want 0", n)
	}
}

// R-GBFM-CG9S — a correct GitHub signature commits the exact raw body and only
// the two explicitly allowlisted GitHub headers before returning 202.
func TestIngressGitHubHMACRecordsAllowlistedHeaders(t *testing.T) {
	h, conn, _, name, secret := newIngressFixtureScheme(t, "github-hmac")
	body := []byte{0, 1, 2, 255, '{', '}'}
	req := httptest.NewRequest(http.MethodPost, "/in/"+name, strings.NewReader(string(body)))
	req.Header.Set("X-Hub-Signature-256", githubSignature(secret, body))
	req.Header.Set("X-GitHub-Event", "push")
	req.Header.Set("X-GitHub-Delivery", "delivery-1")
	req.Header.Set("X-Private-Provider-Header", "must-not-leak")
	req.Header.Set("Content-Type", "application/octet-stream")
	rec := doIngress(h, req)
	if rec.Code != http.StatusAccepted || countOutbox(t, conn) != 1 {
		t.Fatalf("response=%d %q outbox=%d", rec.Code, rec.Body.String(), countOutbox(t, conn))
	}
	p := onlyPayloadMap(t, conn)
	for _, key := range []string{"name", "owner", "received_at", "content_type", "body", "headers"} {
		if _, ok := p[key]; !ok {
			t.Errorf("payload missing %q: %v", key, p)
		}
	}
	if p["name"] != name || p["owner"] != "owner@example.com" || p["content_type"] != "application/octet-stream" || p["body"] != base64.StdEncoding.EncodeToString(body) || p["received_at"] != "2026-06-25T12:00:00Z" {
		t.Fatalf("payload values = %v", p)
	}
	headers, ok := p["headers"].(map[string]any)
	if !ok || len(headers) != 2 || headers["x-github-event"] != "push" || headers["x-github-delivery"] != "delivery-1" {
		t.Fatalf("headers = %v, want exact allowlist", p["headers"])
	}
}

// R-GCNI-Q80H — every GitHub authentication failure is the same 404 and none records.
func TestIngressGitHubHMACFailuresAreUniform(t *testing.T) {
	h, conn, _, name, secret := newIngressFixtureScheme(t, "github-hmac")
	body := []byte("payload")
	cases := []struct{ name, path, signature string }{
		{"missing", "/in/" + name, ""},
		{"missing prefix", "/in/" + name, strings.TrimPrefix(githubSignature(secret, body), "sha256=")},
		{"wrong mac", "/in/" + name, githubSignature(secret+"wrong", body)},
		{"unknown hook", "/in/unknown", githubSignature(secret, body)},
	}
	for _, tc := range cases {
		t.Run(tc.name, func(t *testing.T) {
			req := httptest.NewRequest(http.MethodPost, tc.path, strings.NewReader(string(body)))
			req.Header.Set("X-Hub-Signature-256", tc.signature)
			rec := doIngress(h, req)
			if rec.Code != http.StatusNotFound || rec.Body.String() != notFoundBody {
				t.Fatalf("response = %d %q, want uniform 404", rec.Code, rec.Body.String())
			}
		})
	}
	if countOutbox(t, conn) != 0 {
		t.Fatal("authentication failures recorded events")
	}
}

// R-GDVF-3ZR6 — the body-first GitHub path still binds unauthenticated reads to 1 MiB.
func TestIngressGitHubHMACRejectsOverCapBody(t *testing.T) {
	h, conn, _, name, _ := newIngressFixtureScheme(t, "github-hmac")
	req := httptest.NewRequest(http.MethodPost, "/in/"+name, strings.NewReader(strings.Repeat("x", maxBodyBytes+1)))
	rec := doIngress(h, req)
	if rec.Code != http.StatusRequestEntityTooLarge || countOutbox(t, conn) != 0 {
		t.Fatalf("response=%d outbox=%d, want 413 and zero", rec.Code, countOutbox(t, conn))
	}
}

// R-GA7P-YOJ3 — rotation replaces the retained HMAC key immediately.
func TestIngressGitHubHMACRotateAcceptsNewAndRejectsOld(t *testing.T) {
	h, conn, svc, name, oldSecret := newIngressFixtureScheme(t, "github-hmac")
	newSecret, err := svc.Rotate(context.Background(), "owner@example.com", name)
	if err != nil {
		t.Fatal(err)
	}
	body := []byte("rotated")
	post := func(secret string) *httptest.ResponseRecorder {
		req := httptest.NewRequest(http.MethodPost, "/in/"+name, strings.NewReader(string(body)))
		req.Header.Set("X-Hub-Signature-256", githubSignature(secret, body))
		return doIngress(h, req)
	}
	if rec := post(newSecret); rec.Code != http.StatusAccepted {
		t.Fatalf("new secret response = %d %q", rec.Code, rec.Body.String())
	}
	if rec := post(oldSecret); rec.Code != http.StatusNotFound || rec.Body.String() != notFoundBody {
		t.Fatalf("old secret response = %d %q", rec.Code, rec.Body.String())
	}
	if countOutbox(t, conn) != 1 {
		t.Fatalf("outbox rows = %d, want one", countOutbox(t, conn))
	}
}

type countingReader struct{ reads int }

func (r *countingReader) Read([]byte) (int, error) { r.reads++; return 0, io.ErrUnexpectedEOF }

// R-GF3B-HRHV — bearer auth still precedes body reads, valid payloads retain
// their original shape, and wrong bearer credentials use the uniform 404.
func TestIngressBearerAuthenticationOrderAndPayloadRegression(t *testing.T) {
	h, conn, name, secret := newIngressFixture(t)
	probe := &countingReader{}
	req := httptest.NewRequest(http.MethodPost, "/in/"+name, probe)
	req.Header.Set("Authorization", "Bearer wrong")
	rec := doIngress(h, req)
	if rec.Code != http.StatusNotFound || rec.Body.String() != notFoundBody || probe.reads != 0 {
		t.Fatalf("wrong bearer response=%d %q reads=%d", rec.Code, rec.Body.String(), probe.reads)
	}
	valid := httptest.NewRequest(http.MethodPost, "/in/"+name, strings.NewReader("ok"))
	valid.Header.Set("Authorization", "Bearer "+secret)
	if rec := doIngress(h, valid); rec.Code != http.StatusAccepted {
		t.Fatalf("valid bearer response=%d %q", rec.Code, rec.Body.String())
	}
	if _, exists := onlyPayloadMap(t, conn)["headers"]; exists {
		t.Fatal("bearer payload gained headers")
	}
}
