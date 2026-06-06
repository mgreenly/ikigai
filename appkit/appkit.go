// Package appkit is the ikigai app chassis: the uniform half of every service on
// the box. A service's main.go collapses to a single appkit.Main(appkit.Spec{…})
// call. appkit owns the fixed-verb dispatcher, config-from-env, the migration
// runner + downgrade guard, the loopback HTTP server (PRM + identity gate +
// optional /feed producer mount), and manifest.env emit/parse.
//
// appkit is the deploy/serve CHASSIS, not the agentic engine: it knows nothing
// about LLMs, prompts, jobs, or tools — that is agentkit's job, a strictly
// separate sibling library. Neither imports the other (PLAN §1.2, §B1 map §2).
//
// Consumed via a committed `replace appkit => ../appkit` + `require appkit
// v0.0.0`, exactly like eventplane; never tagged (PLAN §1.6).
package appkit

import (
	"context"
	"embed"
	"errors"
	"fmt"
	"io"
	"os"

	"appkit/server"

	"eventplane/consumer"
	"eventplane/outbox"
)

// version / commit are stamped at build time via
// -ldflags "-X appkit.version=… -X appkit.commit=…". They are package-level VARs
// (NOT consts) so the linker injection takes effect (PLAN §F1: -X on a const is
// silently ignored). Defaults make an un-stamped dev build self-identify.
var (
	version = "dev"
	commit  = "none"
)

// ManifestKV is one ordered manifest extra. A slice of these (not a map) keeps
// the manifest emit order deterministic for the byte-compare against the
// committed etc/manifest.env.
type ManifestKV struct {
	Key   string
	Value string
}

// Router is the route-registration seam a service's Handlers hook uses to mount
// its own routes (gated via Router.RequireIdentity, or unauthenticated) on the
// server appkit stands up. Aliased from appkit/server so services depend only on
// the appkit root package.
type Router = server.Router

// Identity is the authenticated caller nginx injects, re-exported so a service's
// gated handlers can read it off the request context.
type Identity = server.Identity

// IdentityFrom returns the caller identity on the context (ok == true behind
// Router.RequireIdentity).
var IdentityFrom = server.IdentityFrom

// Envelope is the single health-envelope builder both transports render through
// (the HTTP /health route and every service's ikigenba_<svc>_health MCP tool),
// re-exported from appkit/server so service MCP packages call appkit.Envelope(…)
// without importing appkit/server directly (DECISIONS §4).
var Envelope = server.Envelope

// BackupReq / RestoreReq carry the resolved paths a Backup/Restore hook needs.
// The default verbs do a minimal consistent SQLite snapshot/restore of DBPath;
// a service overrides Spec.Backup/Spec.Restore for richer behavior (the
// dashboard's cert+S3 snapshot, a producer's generation-epoch re-mint).
type BackupReq struct {
	App            string
	DBPath         string
	GenerationPath string
	Args           []string // extra args after the verb
	Stdout         io.Writer
	Stderr         io.Writer
}

// RestoreReq mirrors BackupReq for the restore verb.
type RestoreReq struct {
	App            string
	DBPath         string
	GenerationPath string
	Args           []string
	Stdout         io.Writer
	Stderr         io.Writer
}

// Spec is the entire contract of an ikigai app — "an ikigai app is exactly this"
// (PLAN §1.1). main.go declares one and passes it to Main. The chassis half is
// appkit's; the domain half plugs in through Handlers / Config / Migrations /
// Backup / Restore.
type Spec struct {
	// App is the service name ("ledger"). Drives manifest APP, the DB filename,
	// the install root, log identity, and the env-var prefix (<APP>_PORT, …).
	App string
	// Mount is the /srv/<app>/ prefix (manifest MOUNT). Composes RESOURCE_ID /
	// AUTH_SERVER from METASPOT_DOMAIN and builds the PRM route. The apex is "/".
	Mount string
	// Default marks the apex/DEFAULT=true app (the dashboard only). Emits DEFAULT
	// and selects the apex server shape (no PRM/identity routes — Handlers owns
	// the whole route table).
	Default bool
	// Port is the loopback port (manifest PORT); the server binds 127.0.0.1:$PORT.
	// Read from <APP>_PORT at serve, defaulted from here.
	Port int
	// MCP marks an MCP surface (manifest MCP=true) — what makes the dashboard
	// include the service in its AS resource list / inventory.
	MCP bool
	// Feed is the producer role (empty = not a producer). Non-empty ("/feed")
	// emits FEED= and mounts the unauthenticated outbox handler over
	// eventplane/outbox at that path.
	Feed string
	// Consumes is the consumer role (nil = not a consumer). Upstream producer
	// names; emits CONSUMES= comma-joined. appkit emits the key only — the
	// consumer loop is wired service-side (PLAN §B1 map §3 risk 2).
	Consumes []string
	// ManifestExtras is ordered non-secret service config the manifest must
	// round-trip beyond the universal/role keys. Ordered ⇒ byte-stable manifest.
	// Secrets NEVER appear here.
	ManifestExtras []ManifestKV
	// Migrations is the app's embedded forward-only migration set
	// (//go:embed migrations/*.sql). appkit's runner applies unapplied higher
	// versions and refuses to start on a version the binary no longer embeds.
	Migrations embed.FS
	// MigrationsDir is the directory within Migrations holding the *.sql
	// (default "migrations").
	MigrationsDir string
	// Handlers registers the service's own routes (/mcp gated, /content, the
	// dashboard's whole apex table) on appkit's server. The real domain surface
	// lives here, untouched by the chassis. May be nil (a server with only the
	// uniform routes).
	Handlers func(*Router) error
	// Config is the service-side composition-root hook: it reads ManifestExtras
	// non-secret config AND the service's secrets (ANTHROPIC_API_KEY, DROPBOX_*,
	// NTFY_*) from env and returns the service's own config object. Keeping the
	// secret-read here honors §2.8 — appkit never reads or logs a secret. May be
	// nil. (Not yet consumed by the dispatcher in B2; the seam is reserved for the
	// service conversions in C1/E.)
	Config func(getenv func(string) string) (any, error)
	// Producer is the producer-outbox injection hook (C1's closed seam). When
	// Spec.Feed != "", appkit constructs the eventplane outbox over the shared
	// single-writer DB handle, starts retention, and mounts the /feed handler; then
	// — AFTER Handlers has built the service's domain Service over rt.DB() — it
	// calls Producer with that *outbox.Outbox so the service injects the outbox
	// into its domain Service and its domain writes Append events on the same
	// transaction. Nil for non-producers (and ignored when Feed == ""). The
	// producer's event payload builders stay app-side (PLAN §B1 map: appkit/feed is
	// orchestration only; the domain owns the payload shape).
	Producer func(ob *outbox.Outbox) error
	// Workers are long-running background tasks appkit runs alongside the HTTP
	// server for the whole serve lifecycle (E2's consumer seam, the mirror of the
	// C1 producer seam). The canonical worker is an event-plane CONSUMER loop
	// (eventplane/consumer.Run over an upstream named in Spec.Consumes), but any
	// background task fits. Each worker is launched in its own goroutine on the
	// serve context, which is the coupling the consumer model requires:
	//   - a SIGTERM / clean shutdown cancels the serve context, so every worker's
	//     ctx is cancelled and they unwind alongside the server;
	//   - a worker that RETURNS (a structural fault — e.g. consumer.Run escaping on a
	//     missing feed_offset table) cancels the serve context too, bringing the HTTP
	//     server down so the process exits non-zero rather than lingering half-alive
	//     (HTTP up / consumer dead — event-protocol.md decision 11);
	//   - a TRANSPORT fault (the upstream producer is unreachable) is retried inside
	//     the worker and never returns, so it never takes the server down.
	// The first non-nil worker error becomes the serve verb's exit error. Workers
	// run only on serve, not on the one-shot verbs (migrate/manifest/…). Nil/empty
	// for services with no background task (crm, ledger). The consumer Config /
	// Handler stay app-side: appkit owns the lifecycle, not the event semantics
	// (PLAN §B1 map §3 risk 2 — appkit emits CONSUMES=, the loop is wired here).
	Workers []func(ctx context.Context) error
	// Health is the optional per-service telemetry reporter. When set, appkit calls
	// it to populate the `details` object of the health envelope on BOTH the HTTP
	// /health route and the ikigenba_<svc>_health MCP tool. Nil → details is {}.
	// The reporter contributes ONLY to details; the required top-level keys
	// (status/version/service) are appkit-owned and reserved (DECISIONS §3).
	Health func(ctx context.Context) (map[string]any, error)
	// Events is the published event-type registry — the static source of truth for
	// the reflection tool (its `publishes` half) AND Append-time validation (a
	// non-empty registry makes the outbox reject any unregistered ev.Type). A
	// service's emittable types are compile-time payload structs, so this is a
	// static value, not a provider. Empty for non-producers.
	Events outbox.Registry
	// Subscriptions is the LIVE provider of what this service currently listens to,
	// called at reflection time (mirrors Spec.Health). It returns a fixed list for
	// a static consumer and the live union for a future dynamic one, so reflection
	// always reports the live in-edges with no redesign. nil for non-consumers.
	Subscriptions func() []consumer.Subscription
	// Backup overrides the backup verb (nil = appkit's default SQLite snapshot).
	Backup func(ctx context.Context, req BackupReq) error
	// Restore overrides the restore verb (nil = appkit's default SQLite restore).
	Restore func(ctx context.Context, req RestoreReq) error
}

func (s Spec) migrationsDir() string {
	if s.MigrationsDir != "" {
		return s.MigrationsDir
	}
	return "migrations"
}

// Main is the single entrypoint each service's main.go calls. It parses os.Args,
// dispatches the fixed verb (default = serve), and exits with the verb's status.
// It never returns to the caller.
func Main(spec Spec) {
	code := dispatch(spec, os.Args[1:], os.Getenv, os.Stdin, os.Stdout, os.Stderr)
	os.Exit(code)
}

// dispatch is Main's testable core: it routes args to the right verb and returns
// a process exit code (0 = ok). It does not call os.Exit, so tests drive it
// directly.
func dispatch(spec Spec, args []string, getenv func(string) string, stdin io.Reader, stdout, stderr io.Writer) int {
	if spec.App == "" {
		fmt.Fprintln(stderr, "appkit: Spec.App is required")
		return 1
	}

	// Global --version flag and the no-arg default both pre-empt the verb switch.
	cmd := ""
	var rest []string
	if len(args) > 0 {
		cmd = args[0]
		rest = args[1:]
	}
	if cmd == "--version" || cmd == "-version" {
		cmd = "version"
		rest = nil
	}

	var err error
	switch cmd {
	case "", "serve":
		// The default no-arg invocation is serve (PLAN §1.1).
		err = runServe(spec, rest, getenv, stdout, stderr)
	case "version":
		fmt.Fprintf(stdout, "%s\n", versionString())
	case "manifest":
		fmt.Fprint(stdout, emitManifest(spec))
	case "migrate":
		err = runMigrate(spec, rest, getenv, stdout, stderr)
	case "schema":
		err = runSchema(spec, rest, getenv, stdout, stderr)
	case "backup":
		err = runBackup(spec, rest, getenv, stdout, stderr)
	case "restore":
		err = runRestore(spec, rest, getenv, stdin, stdout, stderr)
	default:
		fmt.Fprintf(stderr, "%s: unknown command %q (want serve|version|manifest|migrate|schema|backup|restore)\n", spec.App, cmd)
		return 2
	}
	if err != nil {
		if errors.Is(err, flagHelp) {
			return 0
		}
		fmt.Fprintf(stderr, "%s: %v\n", spec.App, err)
		return 1
	}
	return 0
}

// versionString is the self-report: "<version> (<commit>)" — the box cannot lie
// about what is deployed (PLAN §1.6). The commit stamp is whatever bin/ship
// injects via -ldflags -X appkit.commit=…: a short SHA for a clean tag build, or
// "<sha>-dirty" for an ad-hoc build off a dirty source tree (the suffix is part
// of the injected string, so it renders here verbatim — no extra logic needed).
func versionString() string {
	if commit == "" || commit == "none" {
		return version
	}
	return fmt.Sprintf("%s (%s)", version, commit)
}

// flagHelp is the sentinel a verb returns when the user asked for -h/-help, so
// dispatch exits 0 rather than printing it as an error.
var flagHelp = errors.New("flag: help requested")
