package webhooks

import (
	"context"
	"database/sql"
	"io"
	"log/slog"
	"net/http"
	"net/http/httptest"
	"path/filepath"
	"strings"
	"testing"
	"time"

	"eventplane/outbox"

	"webhooks/internal/db"
)

// newIngressFixture stands up a real temp-file SQLite (never :memory:), migrates
// it, wires a Service with a real *outbox.Outbox over a deterministic clock,
// provisions one webhook through Create, and returns the ingress handler, the raw
// *sql.DB (for asserting real outbox row counts), the live webhook name, and the
// plaintext secret shown once by Create.
func newIngressFixture(t *testing.T) (h http.Handler, conn *sql.DB, name, secret string) {
	t.Helper()
	dbPath := filepath.Join(t.TempDir(), "webhooks.db")
	conn, err := db.Open(dbPath)
	if err != nil {
		t.Fatalf("db.Open: %v", err)
	}
	t.Cleanup(func() { conn.Close() })
	if err := db.Migrate(context.Background(), conn); err != nil {
		t.Fatalf("db.Migrate: %v", err)
	}
	now := time.Date(2026, 6, 25, 12, 0, 0, 0, time.UTC)
	clk := fixedClock{t: now}
	ob, err := outbox.New(conn, outbox.Options{Source: "webhooks", Registry: Events, Now: clk.Now})
	if err != nil {
		t.Fatalf("outbox.New: %v", err)
	}
	svc := NewService(conn, clk)
	svc.Outbox = ob

	wh, sec, err := svc.Create(context.Background(), "owner@example.com", "deploy-hook")
	if err != nil {
		t.Fatalf("Create: %v", err)
	}
	log := slog.New(slog.NewTextHandler(io.Discard, nil))
	return NewIngressHandler(svc, log), conn, wh.Name, sec
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
