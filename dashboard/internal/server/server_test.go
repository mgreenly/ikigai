package server

import (
	"database/sql"
	"io"
	"log/slog"
	"net/http"
	"net/http/httptest"
	"path/filepath"
	"strings"
	"testing"
	"time"

	"dashboard/internal/audit"
	"dashboard/internal/db"
	"dashboard/internal/googleidp"
	"dashboard/internal/grantevents"
	"dashboard/internal/identity"
	"dashboard/internal/oauth"
	"dashboard/internal/oauthstate"
	"dashboard/internal/pat"
	"dashboard/internal/ratelimit"
	"dashboard/internal/session"
)

// testWorkspaceDomain is the Workspace federation gate value used across server
// tests — any non-empty domain satisfies New's required-domain guard.
const testWorkspaceDomain = "int.ikigenba.com"

// testResource is a configured resource identifier the authorize/token endpoints
// accept; it is the sole member of Options.Resources in tests.
const testResource = "https://int.ikigenba.com/srv/crm/mcp"

// serverDeps bundles a server's collaborators built over one shared, migrated
// SQLite db, so a full token flow (handshake → callback → authcode → token) sees
// a single consistent store rather than several disjoint ones. opts() expands it
// into a valid Options for New.
type serverDeps struct {
	db         *sql.DB
	handshakes *oauthstate.HandshakeStore
	sessions   *session.SessionStore
	identity   *identity.Store
	clients    *oauth.ClientStore
	codes      *oauth.AuthCodeStore
	tokens     *oauth.TokenStore
	pats       *pat.Store
	audit      *audit.Log
	grants     *grantevents.Bus
}

// newServerDeps opens one migrated SQLite db under the test's temp dir (closed
// when the test ends) and builds every store the server needs over that shared
// handle. TTLs are sane test values; production TTLs are a wiring-time concern.
func newServerDeps(t *testing.T) serverDeps {
	t.Helper()
	database, err := db.Open(filepath.Join(t.TempDir(), "test.db"))
	if err != nil {
		t.Fatalf("db.Open: %v", err)
	}
	t.Cleanup(func() { database.Close() })
	return serverDeps{
		db:         database,
		handshakes: oauthstate.NewHandshakeStore(database, 5*time.Minute),
		sessions:   session.NewSessionStore(database),
		identity:   identity.NewStore(database),
		clients:    oauth.NewClientStore(database),
		codes:      oauth.NewAuthCodeStore(database, 10*time.Minute),
		tokens:     oauth.NewTokenStore(database, 60*time.Minute, 14*24*time.Hour),
		pats:       pat.NewStore(database),
		audit:      audit.New(database),
		grants:     grantevents.New(),
	}
}

// opts expands the deps into a valid Options for New. Individual tests mutate the
// returned value (e.g. nil out one dep) to probe the constructor's guards.
func (d serverDeps) opts() Options {
	return Options{
		Logger:          slog.New(slog.NewTextHandler(io.Discard, nil)),
		IDPProvider:     googleidp.NewStub(),
		PublicBaseURL:   "https://int.ikigenba.com",
		Handshakes:      d.handshakes,
		WorkspaceDomain: testWorkspaceDomain,
		Sessions:        d.sessions,
		Identity:        d.identity,
		DB:              d.db,
		OAuthClients:    d.clients,
		OAuthCodes:      d.codes,
		OAuthTokens:     d.tokens,
		PATs:            d.pats,
		Audit:           d.audit,
		Resources:       []string{testResource},
		RateLimiter:     ratelimit.New(60, 10*time.Second),
		GrantEvents:     d.grants,
	}
}

func testServer(t *testing.T) *http.Server {
	t.Helper()
	srv, err := New(newServerDeps(t).opts())
	if err != nil {
		t.Fatalf("New: %v", err)
	}
	return srv
}

func do(t *testing.T, srv *http.Server, method, target string, hdr map[string]string) *httptest.ResponseRecorder {
	t.Helper()
	req := httptest.NewRequest(method, target, nil)
	for k, v := range hdr {
		req.Header.Set(k, v)
	}
	rec := httptest.NewRecorder()
	srv.Handler.ServeHTTP(rec, req)
	return rec
}

func TestIndex(t *testing.T) {
	srv := testServer(t)
	rec := do(t, srv, "GET", "http://int.ikigenba.com/", nil)

	if rec.Code != http.StatusOK {
		t.Fatalf("status = %d, want 200", rec.Code)
	}
	if ct := rec.Header().Get("Content-Type"); !strings.HasPrefix(ct, "text/html") {
		t.Errorf("Content-Type = %q, want text/html", ct)
	}
	body := rec.Body.String()
	if !strings.Contains(body, "ikigenba") {
		t.Errorf("body missing product name:\n%s", body)
	}
}

func TestIndexEscapesHost(t *testing.T) {
	// r.Host is attacker-influenced; html/template must escape it.
	srv := testServer(t)
	rec := do(t, srv, "GET", "http://evil<script>/", nil)
	if strings.Contains(rec.Body.String(), "<script>") {
		t.Errorf("host was not escaped:\n%s", rec.Body.String())
	}
}

func TestStaticAsset(t *testing.T) {
	srv := testServer(t)
	rec := do(t, srv, "GET", "http://int.ikigenba.com/static/app.css", nil)
	if rec.Code != http.StatusOK {
		t.Fatalf("status = %d, want 200", rec.Code)
	}
	if ct := rec.Header().Get("Content-Type"); !strings.HasPrefix(ct, "text/css") {
		t.Errorf("Content-Type = %q, want text/css", ct)
	}
}

func TestStaticDirListingDisabled(t *testing.T) {
	srv := testServer(t)
	rec := do(t, srv, "GET", "http://int.ikigenba.com/static/", nil)
	if rec.Code != http.StatusNotFound {
		t.Errorf("GET /static/ = %d, want 404 (no autoindex)", rec.Code)
	}
	if strings.Contains(rec.Body.String(), "app.css") {
		t.Errorf("directory listing leaked asset names:\n%s", rec.Body.String())
	}
}

// TestNewRequiresDependencies asserts every required dependency is checked at the
// New seam — each case starts from a valid Options and nils out exactly one dep.
func TestNewRequiresDependencies(t *testing.T) {
	valid := func() Options { return newServerDeps(t).opts() }
	cases := map[string]func(*Options){
		"Logger":          func(o *Options) { o.Logger = nil },
		"IDPProvider":     func(o *Options) { o.IDPProvider = nil },
		"Handshakes":      func(o *Options) { o.Handshakes = nil },
		"WorkspaceDomain": func(o *Options) { o.WorkspaceDomain = "" },
		"Sessions":        func(o *Options) { o.Sessions = nil },
		"DB":              func(o *Options) { o.DB = nil },
		"OAuthClients":    func(o *Options) { o.OAuthClients = nil },
		"OAuthCodes":      func(o *Options) { o.OAuthCodes = nil },
		"OAuthTokens":     func(o *Options) { o.OAuthTokens = nil },
		"PATs":            func(o *Options) { o.PATs = nil },
		"Audit":           func(o *Options) { o.Audit = nil },
		"Resources":       func(o *Options) { o.Resources = nil },
		"RateLimiter":     func(o *Options) { o.RateLimiter = nil },
		"GrantEvents":     func(o *Options) { o.GrantEvents = nil },
	}
	for dep, omit := range cases {
		t.Run(dep, func(t *testing.T) {
			opts := valid()
			omit(&opts)
			if _, err := New(opts); err == nil {
				t.Errorf("New with nil %s: want error, got nil", dep)
			}
		})
	}
}

// TestNewAcceptsManyResources locks the removal of the old runtime 3-resource
// cap: New must accept four or more configured resources.
func TestNewAcceptsManyResources(t *testing.T) {
	opts := newServerDeps(t).opts()
	opts.Resources = []string{"r1", "r2", "r3", "r4"}
	if _, err := New(opts); err != nil {
		t.Fatalf("New with 4 resources: want success, got %v", err)
	}
}

func TestUnknownPath404(t *testing.T) {
	srv := testServer(t)
	rec := do(t, srv, "GET", "http://int.ikigenba.com/nope", nil)
	if rec.Code != http.StatusNotFound {
		t.Errorf("status = %d, want 404", rec.Code)
	}
	// Security headers apply even to 404s (middleware wraps the whole mux).
	if rec.Header().Get("X-Content-Type-Options") != "nosniff" {
		t.Error("nosniff missing on 404")
	}
}

func TestSecurityHeaders(t *testing.T) {
	srv := testServer(t)

	plain := do(t, srv, "GET", "http://int.ikigenba.com/", nil)
	if plain.Header().Get("X-Content-Type-Options") != "nosniff" {
		t.Error("nosniff missing")
	}
	if plain.Header().Get("Cache-Control") != "no-store" {
		t.Errorf("Cache-Control = %q, want no-store", plain.Header().Get("Cache-Control"))
	}
	if plain.Header().Get("Strict-Transport-Security") != "" {
		t.Error("HSTS must not be set on plain HTTP")
	}

	https := do(t, srv, "GET", "http://int.ikigenba.com/", map[string]string{"X-Forwarded-Proto": "https"})
	if https.Header().Get("Strict-Transport-Security") == "" {
		t.Error("HSTS must be set when X-Forwarded-Proto is https")
	}
}

// The graceful-shutdown loop (Run) moved into appkit/server with the conversion
// to the appkit contract — it is tested there (appkit/server.Run), no longer here.
