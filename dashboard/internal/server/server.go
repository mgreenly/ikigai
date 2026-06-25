// Package server builds the dashboard's HTTP layer: the apex OAuth
// authorization-server endpoints, /internal/authn introspection, the IAM index +
// grants, the install landing, the service inventory, and static assets.
//
// The dashboard is the apex/DEFAULT app: it issues identity, it does not consume
// it, so it owns its WHOLE route table and bypasses appkit/server's PRM +
// identity-gate routes (the Apex bypass, PLAN §B1 map §3 risk 3). appkit owns the
// outer chassis — the loopback *http.Server, graceful shutdown, request-id +
// security-header middleware, the DB handle, and the fixed verbs — so this
// package no longer carries a server bootstrap or a Run loop. Register hands the
// dashboard's route table to appkit via Spec.Handlers; New is retained only so
// the in-package tests can drive the same routes as a standalone *http.Server.
package server

import (
	"database/sql"
	"errors"
	"fmt"
	"html/template"
	"io/fs"
	"log/slog"
	"net/http"
	"time"

	"appkit/server"

	"dashboard/internal/audit"
	"dashboard/internal/googleidp"
	"dashboard/internal/grantevents"
	"dashboard/internal/oauth"
	"dashboard/internal/oauthstate"
	"dashboard/internal/pat"
	"dashboard/internal/ratelimit"
	"dashboard/internal/session"
	"dashboard/ui"
)

// Options configures the dashboard's HTTP layer.
type Options struct {
	Logger          *slog.Logger               // structured logger (required)
	IDPProvider     googleidp.Provider         // Google identity-provider seam (required for login)
	PublicBaseURL   string                     // public origin, e.g. "https://int.ikigenba.com" (for the OAuth redirect URI)
	Handshakes      *oauthstate.HandshakeStore // login-handshake store (required for login)
	WorkspaceDomain string                     // Google Workspace hosted domain federation is restricted to (required for login)
	Sessions        *session.SessionStore      // web-session store (required for login)

	// OAuth authorization-server collaborators (required).
	DB           *sql.DB              // shared database handle (token-exchange transactions)
	OAuthClients *oauth.ClientStore   // DCR client registrations
	OAuthCodes   *oauth.AuthCodeStore // short-lived authorization codes
	OAuthTokens  *oauth.TokenStore    // chains, access tokens, refresh tokens
	PATs         *pat.Store           // personal access tokens (cross-service bearer)
	Audit        *audit.Log           // security-audit log

	// Resources is the configured set of resource identifiers (one or more) the
	// authorization server will mint tokens for. Required.
	Resources []string
	// Admins may introspect any owner's tokens. May be empty.
	Admins []string

	// RateLimiter is the per-token sliding-window limiter the /internal/authn
	// introspection endpoint applies after a token validates. Required.
	RateLimiter *ratelimit.Limiter

	// GrantEvents is the in-process pub/sub that keeps the index page's
	// live-grants block fresh: token issuance/refresh/revocation publish on it,
	// the SSE handler subscribes to it. Required.
	GrantEvents *grantevents.Bus

	// ManifestRoot is the directory under which each service drops its
	// <name>/etc/manifest.env, read by the /services inventory endpoint.
	// Defaults to "/opt" when empty.
	ManifestRoot string
}

// app holds the HTTP layer's dependencies. Handlers are methods on app, so new
// collaborators (sessions, tokens, config) become struct fields rather than
// ever-longer handler parameter lists. It is unexported: the package's public
// surface is New/Register, not the struct.
type app struct {
	logger          *slog.Logger
	tmpl            *template.Template
	static          fs.FS
	idpProvider     googleidp.Provider
	publicBaseURL   string
	handshakes      *oauthstate.HandshakeStore
	workspaceDomain string
	sessions        *session.SessionStore

	db           *sql.DB
	oauthClients *oauth.ClientStore
	oauthCodes   *oauth.AuthCodeStore
	oauthTokens  *oauth.TokenStore
	pats         *pat.Store
	audit        *audit.Log
	resources    []string
	admins       []string
	rateLimiter  *ratelimit.Limiter
	manifestRoot string
	grantEvents  *grantevents.Bus
}

// newApp validates every required dependency at this wiring seam (so a
// misconfigured boot fails loudly here rather than at first request) and parses
// the templates once (a broken template fails startup, not a request). It builds
// the app but does not stand up a server — Register/New mount its routes.
func newApp(opts Options) (*app, error) {
	if opts.Logger == nil {
		return nil, errors.New("server: Logger is required")
	}
	if opts.IDPProvider == nil {
		return nil, errors.New("server: IDPProvider is required")
	}
	if opts.Handshakes == nil {
		return nil, errors.New("server: Handshakes is required")
	}
	if opts.WorkspaceDomain == "" {
		return nil, errors.New("server: WorkspaceDomain is required")
	}
	if opts.Sessions == nil {
		return nil, errors.New("server: Sessions is required")
	}
	if opts.DB == nil {
		return nil, errors.New("server: DB is required")
	}
	if opts.OAuthClients == nil {
		return nil, errors.New("server: OAuthClients is required")
	}
	if opts.OAuthCodes == nil {
		return nil, errors.New("server: OAuthCodes is required")
	}
	if opts.OAuthTokens == nil {
		return nil, errors.New("server: OAuthTokens is required")
	}
	if opts.PATs == nil {
		return nil, errors.New("server: PATs is required")
	}
	if opts.Audit == nil {
		return nil, errors.New("server: Audit is required")
	}
	if len(opts.Resources) == 0 {
		return nil, errors.New("server: at least one Resource is required")
	}
	if opts.RateLimiter == nil {
		return nil, errors.New("server: RateLimiter is required")
	}
	if opts.GrantEvents == nil {
		return nil, errors.New("server: GrantEvents is required")
	}

	manifestRoot := opts.ManifestRoot
	if manifestRoot == "" {
		manifestRoot = "/opt"
	}

	// Parse the index page together with the partials it embeds (the
	// live-grants block, which the /grants/fragment handler also renders
	// stand-alone). Both files share one template set so {{template "grants_block"}}
	// resolves and a broken partial fails startup loudly.
	tmpl, err := template.ParseFS(ui.Files,
		"html/index.html",
		"html/profile.html",
		"html/partials/grants_block.tmpl",
		"html/partials/pat_block.tmpl",
		"html/partials/pat_created.tmpl",
	)
	if err != nil {
		return nil, fmt.Errorf("parse templates: %w", err)
	}

	static, err := fs.Sub(ui.Files, "static")
	if err != nil {
		return nil, fmt.Errorf("static subtree: %w", err)
	}

	return &app{
		logger:          opts.Logger,
		tmpl:            tmpl,
		static:          static,
		idpProvider:     opts.IDPProvider,
		publicBaseURL:   opts.PublicBaseURL,
		handshakes:      opts.Handshakes,
		workspaceDomain: opts.WorkspaceDomain,
		sessions:        opts.Sessions,
		db:              opts.DB,
		oauthClients:    opts.OAuthClients,
		oauthCodes:      opts.OAuthCodes,
		oauthTokens:     opts.OAuthTokens,
		pats:            opts.PATs,
		audit:           opts.Audit,
		resources:       opts.Resources,
		admins:          opts.Admins,
		rateLimiter:     opts.RateLimiter,
		manifestRoot:    manifestRoot,
		grantEvents:     opts.GrantEvents,
	}, nil
}

// Register builds the dashboard's HTTP layer from opts and returns an
// appkit.Spec.Handlers hook that mounts its complete apex route table on appkit's
// server (via the Apex bypass — no PRM, no identity gate). The dashboard issues
// identity, it does not consume it, so it owns the whole table. appkit applies
// the security-header + request-id middleware around it.
func Register(opts Options) (func(*server.Router) error, error) {
	a, err := newApp(opts)
	if err != nil {
		return nil, err
	}
	return func(rt *server.Router) error {
		a.register(rt)
		return nil
	}, nil
}

// New builds a standalone *http.Server over the dashboard's routes (security
// headers + the apex route table). It exists for the in-package tests, which
// drive the route table directly; production goes through Register + appkit. It
// does not start listening.
func New(opts Options) (*http.Server, error) {
	a, err := newApp(opts)
	if err != nil {
		return nil, err
	}
	return &http.Server{
		Addr:              "127.0.0.1:0",
		Handler:           a.routes(),
		ReadHeaderTimeout: 5 * time.Second,
		ReadTimeout:       15 * time.Second,
		WriteTimeout:      15 * time.Second,
		IdleTimeout:       60 * time.Second,
	}, nil
}
