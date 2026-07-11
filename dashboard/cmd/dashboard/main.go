// Command dashboard is the apex/DEFAULT app of the ikigenba suite: the OAuth
// authorization server, IAM index, push, install landing, and service inventory.
// It is the box's trust boundary — it ISSUES identity (it does not consume it),
// so unlike the path-routed services it has no PRM document and no identity gate;
// it owns its WHOLE apex route table through appkit's Apex bypass (Spec.Default).
//
// The uniform chassis — the fixed subcommands (serve/version/migrate/schema),
// config-from-env, the migration runner + downgrade guard, the
// loopback HTTP server, graceful shutdown, and request-id/security middleware —
// is owned by appkit. main.go declares only the dashboard's identity (the Spec)
// and wires its domain surface (the apex AS/IAM/install/inventory route table,
// and its migrations) through the Spec hooks.
//
// The AS resource list is DERIVED at startup from the per-service manifests under
// DASHBOARD_MANIFEST_ROOT (/opt on the box) — registering a new MCP service is a
// dashboard restart, never an env edit. DASHBOARD_RESOURCES is dead and gone.
package main

import (
	"context"
	"encoding/json"
	"fmt"
	"net/http"
	"net/url"
	"os"
	"strings"
	"time"

	"appkit"
	"appkit/config"
	"appkit/inventory"

	"dashboard/internal/audit"
	"dashboard/internal/db"
	"dashboard/internal/googleidp"
	"dashboard/internal/grantevents"
	"dashboard/internal/identity"
	"dashboard/internal/oauth"
	"dashboard/internal/oauthstate"
	"dashboard/internal/pat"
	"dashboard/internal/ratelimit"
	"dashboard/internal/server"
	"dashboard/internal/session"
	"dashboard/internal/telemetry"
)

func main() {
	var rt *appkit.Router
	store := telemetry.NewStore()
	manifestRoot := config.EnvOr(os.Getenv, "DASHBOARD_MANIFEST_ROOT", "/opt")
	appkit.Main(appkit.Spec{
		App:        "dashboard",
		Mount:      "/",  // apex
		Default:    true, // DEFAULT=true → Apex bypass: no PRM, no identity gate
		Port:       3000,
		MCP:        false, // the AS is not itself an MCP resource; it omits MCP so inventory never self-lists
		Migrations: db.FS,
		// Handlers builds the dashboard's whole apex route table over appkit's
		// shared, migrated DB handle and mounts it via the Apex bypass.
		Handlers: func(r *appkit.Router) error {
			rt = r
			return registerRoutes(rt, store, manifestRoot)
		},
		Workers: []func(context.Context) error{
			func(ctx context.Context) error {
				if rt == nil {
					return fmt.Errorf("dashboard: routes not registered before telemetry worker started")
				}
				return telemetry.Run(ctx, store, telemetry.Config{ManifestRoot: manifestRoot}, rt.Logger())
			},
		},
	})
}

// registerRoutes is the Spec.Handlers hook. It reads the dashboard's env at the
// composition root (creds, public origin, manifest root, admins, rate limits),
// derives the AS resource list from the on-box service manifests, builds the
// OAuth/IAM collaborators over appkit's shared DB handle, and returns the apex
// route table for appkit to mount (Apex bypass — no PRM/identity gate).
//
// A missing required secret or an empty derived resource list is a hard boot
// failure here (an AS with no resources can mint no token) — appkit propagates
// the returned error and exits non-zero before listening.
func registerRoutes(rt *appkit.Router, telemetryStore *telemetry.Store, manifestRoot string) error {
	getenv := os.Getenv
	logger := rt.Logger()

	registerHealth(rt)

	conn := rt.DB()
	if conn == nil {
		return fmt.Errorf("dashboard: no DB handle on router")
	}

	// Google credentials are env-only, never flags: a client secret on a --flag
	// would be visible in ps output and shell history. Required at the boundary so
	// a missing secret fails loudly here rather than as a downstream Google 400.
	// CLIENT_SECRET isn't consumed until the code exchange, but login can't work
	// without it, so its presence is required now too. Presence-checked only — the
	// value is never read into a log (PLAN §2.8).
	if err := requireEnv(getenv, "GOOGLE_CLIENT_ID", "GOOGLE_CLIENT_SECRET", "GOOGLE_WORKSPACE_DOMAIN"); err != nil {
		return err
	}
	creds := googleidp.Credentials{
		ClientID:        getenv("GOOGLE_CLIENT_ID"),
		ClientSecret:    getenv("GOOGLE_CLIENT_SECRET"),
		WorkspaceDomain: getenv("GOOGLE_WORKSPACE_DOMAIN"),
	}

	// publicBaseURL is the exact origin Google redirects back to and that the later
	// code-exchange must resend verbatim; it must match the redirect URI registered
	// in the Google Cloud console. On the box bin-side this was composed from
	// IKIGENBA_DOMAIN by the deleted run-wrapper; appkit's config composes the same
	// AUTH_SERVER origin in-binary, so default to it and let DASHBOARD_PUBLIC_BASE_URL
	// override for local dev.
	cfg, err := config.Resolve("dashboard", "/", 3000, getenv)
	if err != nil {
		return err
	}
	publicBaseURL := config.EnvOr(getenv, "DASHBOARD_PUBLIC_BASE_URL", cfg.AuthServer)

	// DASHBOARD_ADMINS is an optional comma-separated set of owner emails permitted
	// to introspect any chain.
	admins := splitList(getenv("DASHBOARD_ADMINS"))

	// manifestRoot is the directory under which each service drops its
	// etc/manifest.env (/opt on the box). The AS resource list is DERIVED from these
	// manifests at startup, so registering a new MCP service is just a dashboard
	// restart — no env edit + redeploy footgun.
	if manifestRoot == "" {
		manifestRoot = config.EnvOr(getenv, "DASHBOARD_MANIFEST_ROOT", "/opt")
	}

	// Per-token introspection rate limit applied by POST /internal/authn.
	authnRateLimit, err := config.EnvOrInt(getenv, "DASHBOARD_AUTHN_RATE_LIMIT", 120)
	if err != nil {
		return err
	}
	authnRateWindow, err := config.EnvOrDuration(getenv, "DASHBOARD_AUTHN_RATE_WINDOW", 10*time.Second)
	if err != nil {
		return err
	}

	// Derive the AS resource list from the on-box service manifests at startup, via
	// the same inventory package the runtime /services endpoint uses. Each MCP
	// service's resource ID is <scheme>://<host><mount>mcp, built from the public
	// base URL's origin so it is byte-identical to the IDs nginx fronts.
	resources, err := deriveResources(manifestRoot, publicBaseURL)
	if err != nil {
		return err
	}
	if len(resources) == 0 {
		// An authorization server with no resources can bind no token to any
		// service — a hard misconfiguration, not a degraded mode. Fail loudly here
		// rather than start an AS that rejects every authorize.
		return fmt.Errorf("no MCP services found under manifest root %q: the authorization server has no resources to mint tokens for", manifestRoot)
	}
	logger.Info("derived AS resources from manifests", "manifest_root", manifestRoot, "count", len(resources))

	// Build the login + OAuth authorization-server collaborators over appkit's one
	// shared, migrated DB handle. Token lifetimes follow the prior crm.bak
	// deployment: short-lived access tokens, long-lived rotating refresh tokens,
	// briefly-valid authorization codes.
	handshakes := oauthstate.NewHandshakeStore(conn, 5*time.Minute)
	sessions := session.NewSessionStore(conn)
	identities := identity.NewStore(conn)
	oauthClients := oauth.NewClientStore(conn)
	oauthCodes := oauth.NewAuthCodeStore(conn, 2*time.Minute)
	oauthTokens := oauth.NewTokenStore(conn, 30*time.Minute, 30*24*time.Hour)
	pats := pat.NewStore(conn)
	auditLog := audit.New(conn)

	regHook, err := server.Register(server.Options{
		Logger:          logger,
		IDPProvider:     googleidp.New(creds),
		PublicBaseURL:   publicBaseURL,
		Handshakes:      handshakes,
		WorkspaceDomain: creds.WorkspaceDomain,
		Sessions:        sessions,
		Identity:        identities,
		DB:              conn,
		OAuthClients:    oauthClients,
		OAuthCodes:      oauthCodes,
		OAuthTokens:     oauthTokens,
		PATs:            pats,
		Audit:           auditLog,
		Resources:       resources,
		ManifestRoot:    manifestRoot,
		Telemetry:       telemetryStore,
		Admins:          admins,
		RateLimiter:     ratelimit.New(authnRateLimit, authnRateWindow),
		GrantEvents:     grantevents.New(),
	})
	if err != nil {
		return err
	}
	return regHook(rt)
}

// registerHealth preserves appkit's standard /health surface for dashboard's
// apex/default router. Default apps bypass appkit's standard route table, so the
// dashboard composition root mounts the same envelope explicitly.
func registerHealth(rt *appkit.Router) {
	rt.HandleFunc("GET /health", func(w http.ResponseWriter, r *http.Request) {
		details := map[string]any{}
		if health := rt.Health(); health != nil {
			var err error
			details, err = health(r.Context())
			if err != nil {
				http.Error(w, err.Error(), http.StatusInternalServerError)
				return
			}
		}
		w.Header().Set("Content-Type", "application/json")
		if err := json.NewEncoder(w).Encode(appkit.Envelope(rt.Version(), rt.Service(), details)); err != nil {
			rt.Logger().Error("write health response", "err", err)
		}
	})
}

// requireEnv returns an error naming every listed variable that is unset or
// empty, reporting them all at once so a misconfigured boot surfaces its full
// list of missing secrets in a single message. It checks presence only — it
// never reads or echoes a value.
func requireEnv(getenv func(string) string, names ...string) error {
	var missing []string
	for _, name := range names {
		if getenv(name) == "" {
			missing = append(missing, name)
		}
	}
	if len(missing) > 0 {
		return fmt.Errorf("missing required environment variables: %s", strings.Join(missing, ", "))
	}
	return nil
}

// deriveResources reads the per-service manifests under manifestRoot via the
// inventory package and returns one MCP resource identifier per MCP service,
// built as <scheme>://<host><mount>mcp from the public base URL's origin. The
// list is sorted by service name (inventory.Read already sorts). A glob-level
// failure or an unparseable publicBaseURL is fatal — the resource IDs must bind
// to live tokens, so a misread root must not silently yield an empty AS.
func deriveResources(manifestRoot, publicBaseURL string) ([]string, error) {
	svcs, err := inventory.Read(manifestRoot)
	if err != nil {
		return nil, fmt.Errorf("reading service manifests under %q: %w", manifestRoot, err)
	}
	base, err := url.Parse(publicBaseURL)
	if err != nil {
		return nil, fmt.Errorf("parsing DASHBOARD_PUBLIC_BASE_URL %q: %w", publicBaseURL, err)
	}
	var resources []string
	for _, s := range svcs {
		// Mount carries its own leading+trailing slash (e.g. "/srv/crm/"), so "mcp"
		// appends directly — matches mcpResourceURL semantics exactly.
		resources = append(resources, base.Scheme+"://"+base.Host+s.Mount+"mcp")
	}
	return resources, nil
}

// splitList parses a comma-separated environment value into a slice, trimming
// surrounding whitespace from each element and dropping empties. An empty or
// all-separator input yields a nil slice.
func splitList(s string) []string {
	var out []string
	for _, part := range strings.Split(s, ",") {
		if p := strings.TrimSpace(part); p != "" {
			out = append(out, p)
		}
	}
	return out
}
