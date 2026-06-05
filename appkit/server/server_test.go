package server_test

import (
	"encoding/json"
	"io"
	"log/slog"
	"net/http"
	"net/http/httptest"
	"strings"
	"testing"

	"appkit/server"
)

const (
	testResourceID = "https://ai.metaspot.org/srv/ledger/mcp"
	testAuthServer = "https://ai.metaspot.org"
)

func discardLogger() *slog.Logger {
	return slog.New(slog.NewJSONHandler(io.Discard, nil))
}

// newStandardServer builds a path-routed service server whose Register hook
// mounts a gated /mcp echo and an unauthenticated /open route via the Router.
func newStandardServer(t *testing.T) http.Handler {
	t.Helper()
	srv, err := server.New(server.Options{
		Addr:       "127.0.0.1:0",
		Logger:     discardLogger(),
		ResourceID: testResourceID,
		AuthServer: testAuthServer,
		Register: func(rt *server.Router) error {
			rt.Handle("POST /mcp", rt.RequireIdentity(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
				id, ok := server.IdentityFrom(r.Context())
				if !ok {
					t.Error("gated handler reached without identity on context")
				}
				_ = json.NewEncoder(w).Encode(map[string]string{"owner": id.OwnerEmail})
			})))
			rt.HandleFunc("GET /open", func(w http.ResponseWriter, r *http.Request) {
				w.WriteHeader(http.StatusOK)
			})
			return nil
		},
	})
	if err != nil {
		t.Fatalf("New: %v", err)
	}
	return srv.Handler
}

func TestNew_RequiresConfig(t *testing.T) {
	logger := discardLogger()
	cases := []struct {
		name string
		opts server.Options
	}{
		{"no logger", server.Options{ResourceID: testResourceID, AuthServer: testAuthServer}},
		{"no resource", server.Options{Logger: logger, AuthServer: testAuthServer}},
		{"no auth server", server.Options{Logger: logger, ResourceID: testResourceID}},
	}
	for _, tc := range cases {
		t.Run(tc.name, func(t *testing.T) {
			if _, err := server.New(tc.opts); err == nil {
				t.Fatalf("expected error for %s, got nil", tc.name)
			}
		})
	}
}

func TestPRMetadata_Unauthenticated(t *testing.T) {
	h := newStandardServer(t)
	req := httptest.NewRequest(http.MethodGet, "/.well-known/oauth-protected-resource", nil)
	rr := httptest.NewRecorder()
	h.ServeHTTP(rr, req)

	if rr.Code != http.StatusOK {
		t.Fatalf("status = %d, want 200", rr.Code)
	}
	var doc map[string]any
	if err := json.Unmarshal(rr.Body.Bytes(), &doc); err != nil {
		t.Fatalf("decode body: %v", err)
	}
	if doc["resource"] != testResourceID {
		t.Errorf("resource = %v, want %q", doc["resource"], testResourceID)
	}
}

func TestWhoami_WithIdentity(t *testing.T) {
	h := newStandardServer(t)
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
		t.Fatalf("decode: %v", err)
	}
	if doc["owner_email"] != "owner@example.com" {
		t.Errorf("owner_email = %v", doc["owner_email"])
	}
	if doc["client_id"] != "client-abc" {
		t.Errorf("client_id = %v", doc["client_id"])
	}
}

func TestIdentityGate_RejectsWithoutOwnerEmail(t *testing.T) {
	h := newStandardServer(t)
	// X-Client-Id present but X-Owner-Email absent: did not transit nginx.
	req := httptest.NewRequest(http.MethodGet, "/whoami", nil)
	req.Header.Set("X-Client-Id", "client-abc")
	rr := httptest.NewRecorder()
	h.ServeHTTP(rr, req)

	if rr.Code != http.StatusUnauthorized {
		t.Fatalf("status = %d, want 401", rr.Code)
	}
	wa := rr.Header().Get("WWW-Authenticate")
	if wa == "" {
		t.Fatal("missing WWW-Authenticate challenge on 401")
	}
	// The challenge must point at this resource's PRM (resource_metadata), so an
	// MCP client can discover the AS.
	if want := testResourceID + "/.well-known/oauth-protected-resource"; !strings.Contains(wa, want) {
		t.Errorf("WWW-Authenticate = %q, want it to carry resource_metadata %q", wa, want)
	}
}

func TestIdentityGate_AllowsServiceRouteWithHeaders(t *testing.T) {
	h := newStandardServer(t)
	req := httptest.NewRequest(http.MethodPost, "/mcp", nil)
	req.Header.Set("X-Owner-Email", "owner@example.com")
	rr := httptest.NewRecorder()
	h.ServeHTTP(rr, req)

	if rr.Code != http.StatusOK {
		t.Fatalf("gated /mcp status = %d, want 200 with headers", rr.Code)
	}
	var doc map[string]string
	if err := json.Unmarshal(rr.Body.Bytes(), &doc); err != nil {
		t.Fatalf("decode: %v", err)
	}
	if doc["owner"] != "owner@example.com" {
		t.Errorf("gated handler saw owner = %q", doc["owner"])
	}
}

func TestRouter_UnauthenticatedRoute(t *testing.T) {
	h := newStandardServer(t)
	// /open is registered without RequireIdentity, so it answers with no headers.
	req := httptest.NewRequest(http.MethodGet, "/open", nil)
	rr := httptest.NewRecorder()
	h.ServeHTTP(rr, req)
	if rr.Code != http.StatusOK {
		t.Fatalf("/open status = %d, want 200 unauthenticated", rr.Code)
	}
}

func TestApex_BypassesPRMAndOwnsRouteTable(t *testing.T) {
	srv, err := server.New(server.Options{
		Addr:   "127.0.0.1:0",
		Logger: discardLogger(),
		Apex:   true, // dashboard: no ResourceID/AuthServer required
		Register: func(rt *server.Router) error {
			rt.HandleFunc("GET /index", func(w http.ResponseWriter, r *http.Request) {
				_, _ = w.Write([]byte("apex"))
			})
			return nil
		},
	})
	if err != nil {
		t.Fatalf("New apex: %v", err)
	}
	h := srv.Handler

	// The standard PRM route must NOT be mounted in apex mode: appkit added no
	// route, and the apex registered only /index, so the PRM path 404s.
	rr := httptest.NewRecorder()
	h.ServeHTTP(rr, httptest.NewRequest(http.MethodGet, "/.well-known/oauth-protected-resource", nil))
	if rr.Code != http.StatusNotFound {
		t.Errorf("apex PRM status = %d, want 404 (apex owns its own table)", rr.Code)
	}
	// The apex's own route answers.
	rr = httptest.NewRecorder()
	h.ServeHTTP(rr, httptest.NewRequest(http.MethodGet, "/index", nil))
	if rr.Code != http.StatusOK || rr.Body.String() != "apex" {
		t.Errorf("apex /index = (%d, %q), want (200, apex)", rr.Code, rr.Body.String())
	}
}
