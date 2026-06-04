package server

import (
	"encoding/json"
	"io"
	"log/slog"
	"net/http"
	"net/http/httptest"
	"testing"
)

const (
	testResourceID = "https://ai.metaspot.org/srv/ledger/mcp"
	testAuthServer = "https://ai.metaspot.org"
)

// newTestServer builds a handler over fixed config with a discard logger.
func newTestServer(t *testing.T) http.Handler {
	t.Helper()
	srv, err := New(Options{
		Addr:       "127.0.0.1:0",
		Logger:     slog.New(slog.NewJSONHandler(io.Discard, nil)),
		ResourceID: testResourceID,
		AuthServer: testAuthServer,
		MCP:        http.HandlerFunc(func(http.ResponseWriter, *http.Request) {}),
		Feed:       http.HandlerFunc(func(http.ResponseWriter, *http.Request) {}),
	})
	if err != nil {
		t.Fatalf("New: %v", err)
	}
	return srv.Handler
}

func TestNewRequiresConfig(t *testing.T) {
	logger := slog.New(slog.NewJSONHandler(io.Discard, nil))
	mcpStub := http.HandlerFunc(func(http.ResponseWriter, *http.Request) {})
	feedStub := http.HandlerFunc(func(http.ResponseWriter, *http.Request) {})
	cases := []struct {
		name string
		opts Options
	}{
		{"no logger", Options{ResourceID: testResourceID, AuthServer: testAuthServer, MCP: mcpStub, Feed: feedStub}},
		{"no resource", Options{Logger: logger, AuthServer: testAuthServer, MCP: mcpStub, Feed: feedStub}},
		{"no auth server", Options{Logger: logger, ResourceID: testResourceID, MCP: mcpStub, Feed: feedStub}},
		{"no mcp", Options{Logger: logger, ResourceID: testResourceID, AuthServer: testAuthServer, Feed: feedStub}},
		{"no feed", Options{Logger: logger, ResourceID: testResourceID, AuthServer: testAuthServer, MCP: mcpStub}},
	}
	for _, tc := range cases {
		t.Run(tc.name, func(t *testing.T) {
			if _, err := New(tc.opts); err == nil {
				t.Fatalf("expected error for %s, got nil", tc.name)
			}
		})
	}
}

func TestPRMetadataUnauthenticated(t *testing.T) {
	h := newTestServer(t)

	// No identity headers at all — the PRM route must still answer 200.
	req := httptest.NewRequest(http.MethodGet, "/.well-known/oauth-protected-resource", nil)
	rr := httptest.NewRecorder()
	h.ServeHTTP(rr, req)

	if rr.Code != http.StatusOK {
		t.Fatalf("status = %d, want 200", rr.Code)
	}
	if ct := rr.Header().Get("Content-Type"); ct != "application/json" {
		t.Fatalf("content-type = %q, want application/json", ct)
	}

	var doc map[string]any
	if err := json.Unmarshal(rr.Body.Bytes(), &doc); err != nil {
		t.Fatalf("decode body: %v", err)
	}
	if doc["resource"] != testResourceID {
		t.Errorf("resource = %v, want %q", doc["resource"], testResourceID)
	}
	servers, ok := doc["authorization_servers"].([]any)
	if !ok || len(servers) != 1 || servers[0] != testAuthServer {
		t.Errorf("authorization_servers = %v, want [%q]", doc["authorization_servers"], testAuthServer)
	}
	methods, ok := doc["bearer_methods_supported"].([]any)
	if !ok || len(methods) != 1 || methods[0] != "header" {
		t.Errorf("bearer_methods_supported = %v, want [\"header\"]", doc["bearer_methods_supported"])
	}
}

func TestWhoamiWithIdentity(t *testing.T) {
	h := newTestServer(t)

	req := httptest.NewRequest(http.MethodGet, "/whoami", nil)
	req.Header.Set("X-Owner-Email", "owner@example.com")
	req.Header.Set("X-Client-Id", "client-abc")
	rr := httptest.NewRecorder()
	h.ServeHTTP(rr, req)

	if rr.Code != http.StatusOK {
		t.Fatalf("status = %d, want 200", rr.Code)
	}
	var doc map[string]any
	if err := json.Unmarshal(rr.Body.Bytes(), &doc); err != nil {
		t.Fatalf("decode body: %v", err)
	}
	if doc["owner_email"] != "owner@example.com" {
		t.Errorf("owner_email = %v, want owner@example.com", doc["owner_email"])
	}
	if doc["client_id"] != "client-abc" {
		t.Errorf("client_id = %v, want client-abc", doc["client_id"])
	}
}

func TestWhoamiWithoutOwnerEmailIs401(t *testing.T) {
	h := newTestServer(t)

	// X-Client-Id present but X-Owner-Email absent: did not transit the
	// authenticated front door, so it must be refused.
	req := httptest.NewRequest(http.MethodGet, "/whoami", nil)
	req.Header.Set("X-Client-Id", "client-abc")
	rr := httptest.NewRecorder()
	h.ServeHTTP(rr, req)

	if rr.Code != http.StatusUnauthorized {
		t.Fatalf("status = %d, want 401", rr.Code)
	}
	if wa := rr.Header().Get("WWW-Authenticate"); wa == "" {
		t.Errorf("missing WWW-Authenticate challenge on 401")
	}
}
