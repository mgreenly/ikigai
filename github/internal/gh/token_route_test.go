package gh

import (
	"context"
	"crypto/rsa"
	"encoding/json"
	"log/slog"
	"net/http"
	"net/http/httptest"
	"strings"
	"sync"
	"testing"
	"time"

	"appkit/server"
)

func TestTokenHandlerAssembledRouterJSONGuardAndFailureR_GTQ4_30E7(t *testing.T) {
	// R-GTQ4-30E7
	key := mustRSAKey(t)
	var calls int
	httpClient := stubClient(func(req *http.Request) (*http.Response, error) {
		calls++
		switch req.URL.Path {
		case "/orgs/acme/installation":
			return jsonResponse(http.StatusOK, `{"id":42}`), nil
		case "/app/installations/42/access_tokens":
			return jsonResponse(http.StatusCreated, `{"token":"route-token","expires_at":"2026-07-04T12:10:00Z"}`), nil
		default:
			t.Fatalf("unexpected path %s", req.URL.Path)
			return nil, nil
		}
	})
	c := newTokenRouteClient(key, httpClient)
	handler := newTokenRouteServer(t, c)

	forwarded := httptest.NewRequest(http.MethodGet, "/token", nil)
	forwarded.Header.Set("X-Forwarded-Proto", "https")
	rr := httptest.NewRecorder()
	handler.ServeHTTP(rr, forwarded)
	if rr.Code != http.StatusNotFound || rr.Body.String() != "404 page not found\n" || calls != 0 {
		t.Fatalf("forwarded request = %d %q with %d GitHub calls; want bare 404 before handler", rr.Code, rr.Body.String(), calls)
	}

	rr = httptest.NewRecorder()
	handler.ServeHTTP(rr, httptest.NewRequest(http.MethodGet, "/token", nil))
	if rr.Code != http.StatusOK || rr.Header().Get("Content-Type") != "application/json" {
		t.Fatalf("loopback response = %d, Content-Type %q, body %q", rr.Code, rr.Header().Get("Content-Type"), rr.Body.String())
	}
	var body map[string]string
	if err := json.Unmarshal(rr.Body.Bytes(), &body); err != nil {
		t.Fatalf("decode token response: %v", err)
	}
	if len(body) != 2 || body["token"] != "route-token" || body["expires_at"] != "2026-07-04T12:10:00Z" {
		t.Fatalf("token JSON = %#v, want exactly token and expires_at", body)
	}

	failureMaterial := "must-not-escape"
	failingHTTP := stubClient(func(req *http.Request) (*http.Response, error) {
		if req.URL.Path == "/orgs/acme/installation" {
			return jsonResponse(http.StatusOK, `{"id":42}`), nil
		}
		return jsonResponse(http.StatusUnauthorized, `{"message":"`+failureMaterial+`"}`), nil
	})
	rr = httptest.NewRecorder()
	newTokenRouteServer(t, newTokenRouteClient(key, failingHTTP)).ServeHTTP(rr, httptest.NewRequest(http.MethodGet, "/token", nil))
	if rr.Code != http.StatusBadGateway || strings.Contains(rr.Body.String(), failureMaterial) || strings.Contains(rr.Body.String(), "route-token") {
		t.Fatalf("failure response = %d %q, want generic 502 without token material", rr.Code, rr.Body.String())
	}
}

func TestTokenOperationsNeverLogOrExposeTokenR_GUY0_GS4W(t *testing.T) {
	// R-GUY0-GS4W
	key := mustRSAKey(t)
	secret := "installation-secret-never-log"
	capture := &tokenLogCapture{}
	previous := slog.Default()
	slog.SetDefault(slog.New(capture))
	t.Cleanup(func() { slog.SetDefault(previous) })

	successHTTP := stubClient(func(req *http.Request) (*http.Response, error) {
		if req.URL.Path == "/orgs/acme/installation" {
			return jsonResponse(http.StatusOK, `{"id":42}`), nil
		}
		return jsonResponse(http.StatusCreated, `{"token":"`+secret+`","expires_at":"2026-07-04T12:10:00Z"}`), nil
	})
	success := newTokenRouteClient(key, successHTTP)
	if token, _, err := success.Token(context.Background()); err != nil || token != secret {
		t.Fatalf("direct Token() = %q, %v", token, err)
	}
	rr := httptest.NewRecorder()
	newTokenRouteServer(t, success).ServeHTTP(rr, httptest.NewRequest(http.MethodGet, "/token", nil))
	if rr.Code != http.StatusOK {
		t.Fatalf("successful route status = %d", rr.Code)
	}

	failingHTTP := stubClient(func(req *http.Request) (*http.Response, error) {
		if req.URL.Path == "/orgs/acme/installation" {
			return jsonResponse(http.StatusOK, `{"id":42}`), nil
		}
		return jsonResponse(http.StatusUnauthorized, `{"message":"`+secret+`"}`), nil
	})
	rr = httptest.NewRecorder()
	newTokenRouteServer(t, newTokenRouteClient(key, failingHTTP)).ServeHTTP(rr, httptest.NewRequest(http.MethodGet, "/token", nil))
	if rr.Code != http.StatusBadGateway {
		t.Fatalf("failing route status = %d, want 502", rr.Code)
	}
	if strings.Contains(rr.Body.String(), secret) {
		t.Fatalf("502 body exposed token material: %q", rr.Body.String())
	}
	if logs := capture.String(); strings.Contains(logs, secret) {
		t.Fatalf("logs exposed token material: %q", logs)
	}
}

func newTokenRouteClient(key *rsa.PrivateKey, httpClient *http.Client) *Client {
	return &Client{org: "acme", http: httpClient, ts: &tokenSource{
		appID: "12345", org: "acme", signer: key, httpClient: httpClient,
		now: func() time.Time { return time.Date(2026, 7, 4, 12, 0, 0, 0, time.UTC) },
	}}
}

func newTokenRouteServer(t *testing.T, c *Client) http.Handler {
	t.Helper()
	srv, err := server.New(server.Options{
		Addr: "127.0.0.1:0", Logger: slog.Default(), Apex: true, Version: "test", Service: "github",
		Register: func(rt *server.Router) error {
			rt.HandleLoopback("GET /token", c.TokenHandler())
			return nil
		},
	})
	if err != nil {
		t.Fatal(err)
	}
	return srv.Handler
}

type tokenLogCapture struct {
	mu    sync.Mutex
	lines []string
}

func (h *tokenLogCapture) Enabled(context.Context, slog.Level) bool { return true }
func (h *tokenLogCapture) Handle(_ context.Context, record slog.Record) error {
	h.mu.Lock()
	defer h.mu.Unlock()
	line := record.Message
	record.Attrs(func(attr slog.Attr) bool {
		line += " " + attr.String()
		return true
	})
	h.lines = append(h.lines, line)
	return nil
}
func (h *tokenLogCapture) WithAttrs([]slog.Attr) slog.Handler { return h }
func (h *tokenLogCapture) WithGroup(string) slog.Handler      { return h }
func (h *tokenLogCapture) String() string {
	h.mu.Lock()
	defer h.mu.Unlock()
	return strings.Join(h.lines, "\n")
}
