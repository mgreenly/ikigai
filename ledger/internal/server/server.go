// Package server builds and runs the ledger service's HTTP server: routing, the
// unauthenticated RFC 9728 protected-resource metadata document, the identity
// gate, the whoami proof handler, security headers, and graceful shutdown.
//
// ledger is a loopback-only domain service. nginx (owned by the dashboard)
// terminates TLS, introspects every request via auth_request against the
// dashboard's authorization server, strips the /srv/ledger/ prefix, and injects
// X-Owner-Email / X-Client-Id authoritatively. ledger trusts those headers and
// does NO token logic of its own — see requireIdentityHeaders.
package server

import (
	"context"
	"errors"
	"fmt"
	"log/slog"
	"net/http"
	"time"

	"ledger/internal/logging"
)

// shutdownTimeout bounds how long Run waits for in-flight requests to finish
// before forcing the server down.
const shutdownTimeout = 10 * time.Second

// Options configures the HTTP server.
type Options struct {
	Addr       string       // listen address, e.g. "127.0.0.1:3002"
	Logger     *slog.Logger // structured logger (required)
	ResourceID string       // this service's canonical resource id (required)
	AuthServer string       // the dashboard authorization-server base URL (required)
	MCP        http.Handler // the JSON-RPC MCP handler mounted at POST /mcp (required)
	Feed       http.Handler // the event-plane SSE handler mounted at GET /feed (required)
}

// app holds the HTTP layer's dependencies. Handlers are methods on app so new
// collaborators become struct fields rather than ever-longer parameter lists.
// It is unexported: the package's public surface is New/Run, not the struct.
type app struct {
	logger     *slog.Logger
	resourceID string
	authServer string
	mcp        http.Handler
	feed       http.Handler
}

// New builds the HTTP server with its routes, security headers, and pinned
// timeouts. It validates required config at this wiring seam so a misconfigured
// boot fails loudly here rather than at first request. It does not start listening.
func New(opts Options) (*http.Server, error) {
	if opts.Logger == nil {
		return nil, errors.New("server: Logger is required")
	}
	if opts.ResourceID == "" {
		return nil, errors.New("server: ResourceID is required")
	}
	if opts.AuthServer == "" {
		return nil, errors.New("server: AuthServer is required")
	}
	if opts.MCP == nil {
		return nil, errors.New("server: MCP handler is required")
	}
	if opts.Feed == nil {
		return nil, errors.New("server: Feed handler is required")
	}

	a := &app{
		logger:     opts.Logger,
		resourceID: opts.ResourceID,
		authServer: opts.AuthServer,
		mcp:        opts.MCP,
		feed:       opts.Feed,
	}

	srv := &http.Server{
		Addr:              opts.Addr,
		Handler:           a.routes(),
		ReadHeaderTimeout: 5 * time.Second,
		ReadTimeout:       15 * time.Second,
		WriteTimeout:      15 * time.Second,
		IdleTimeout:       60 * time.Second,
	}
	return srv, nil
}

// routes is the ledger service's complete URL surface. nginx strips the
// /srv/ledger/ prefix, so internal paths are bare. The PRM well-known document is
// the only route NOT behind requireIdentityHeaders — it must be reachable
// unauthenticated so a client can discover the authorization server.
func (a *app) routes() http.Handler {
	mux := http.NewServeMux()

	// Unauthenticated: RFC 9728 protected-resource metadata.
	mux.Handle("GET /.well-known/oauth-protected-resource", a.handlePRMetadata())

	// Authenticated: the ledger_whoami proof. requireIdentityHeaders rejects any
	// request that did not arrive through nginx's authenticated front door.
	mux.Handle("GET /whoami", a.requireIdentityHeaders(a.handleWhoami()))

	// Authenticated: the JSON-RPC MCP endpoint and the ledger_* tool surface.
	mux.Handle("POST /mcp", a.requireIdentityHeaders(a.mcp))

	// Event plane (event-protocol.md §2): the SSE feed is UNAUTHENTICATED and
	// loopback-only, deliberately NOT behind requireIdentityHeaders — one box is
	// one owner, so there is no second principal. It is kept off the public proxy
	// by nginx (an exact-match 404 on /srv/ledger/feed) and the handler itself
	// rejects any request bearing nginx identity headers.
	mux.Handle("GET /feed", a.feed)

	return securityHeaders(logging.RequestIDMiddleware(a.logger, mux))
}

// Run starts srv and blocks until ctx is cancelled, then shuts it down
// gracefully within shutdownTimeout. A clean shutdown returns nil; a listen
// failure returns that error.
func Run(ctx context.Context, srv *http.Server, logger *slog.Logger) error {
	errCh := make(chan error, 1)
	go func() {
		errCh <- srv.ListenAndServe()
	}()

	select {
	case err := <-errCh:
		// Server stopped on its own before any shutdown signal.
		if errors.Is(err, http.ErrServerClosed) {
			return nil
		}
		return err
	case <-ctx.Done():
		logger.Info("shutdown initiated")
		shutdownCtx, cancel := context.WithTimeout(context.Background(), shutdownTimeout)
		defer cancel()
		if err := srv.Shutdown(shutdownCtx); err != nil {
			return fmt.Errorf("graceful shutdown: %w", err)
		}
		logger.Info("shutdown complete")
		return nil
	}
}
