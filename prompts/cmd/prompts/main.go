// Command agent is the loopback-only domain service behind nginx. It trusts the
// X-Owner-Email / X-Client-Id headers nginx injects after a successful
// auth_request against the dashboard's authorization server, and performs no
// token logic of its own.
//
// The uniform chassis — the fixed subcommands (serve/version/manifest/migrate/
// backup/restore), config-from-env, the migration runner + downgrade guard, the
// loopback HTTP server + PRM + identity gate (ikigenba_agent_health), and graceful
// shutdown — is owned by appkit. main.go declares only agent's identity (the
// Spec) and wires its domain surface through the Handlers hook: the session
// store, per-session sandbox tree, async runner, the boot-time crash-recovery
// sweep, and the agent_* MCP surface. RESOURCE_ID / AUTH_SERVER are composed
// in-binary by appkit/config from IKIGENBA_DOMAIN + MOUNT (was the deleted
// bin/build run-wrapper's job).
//
// agent is an LLM service: it uses agentkit (the LLM engine + tool surface) for
// the agent loop, kept strictly separate from appkit (the deploy/serve chassis).
// It is neither an event-plane producer nor a consumer — no /feed, no consumer
// loop, no background worker; the async runner is spawned per-run, not a
// long-running task. Its only secret, ANTHROPIC_API_KEY, is read env-only inside
// the runner/session domain at the point of use and never logged (§2.8).
package main

import (
	"context"
	"fmt"
	"os"
	"path/filepath"
	"time"

	"appkit"
	"appkit/config"

	"eventplane/consumer"
	"eventplane/outbox"

	"prompts/internal/consume"
	"prompts/internal/db"
	"prompts/internal/mcp"
	"prompts/internal/runner"
	"prompts/internal/sandbox"
	"prompts/internal/session"
)

// The event-plane upstream agent consumes (cron's /feed) and the stable id it
// presents on every connect (event-protocol.md §7.1). Both are fixed constants —
// agent consumes exactly cron's feed, and its X-Consumer-Id is the literal
// "agent". CONSUMES=cron in etc/manifest.env mirrors cronSource for the registry.
const (
	cronSource = "cron"
	consumerID = "prompts"
)

// svcRef carries the session service from the Handlers hook (where appkit has
// opened + migrated the DB and built the domain) to the consumer Worker, which
// runs strictly afterward. A package-level capture mirrors notify's `rt` capture.
// storeRef is the same hand-off for the producer outbox: the Producer hook
// (which runs AFTER Handlers, once appkit has constructed the outbox) injects it
// onto the store so the runner's terminal write emits the outcome event on the
// same tx (event-triggering decisions §3).
var (
	svcRef   *session.Service
	storeRef *session.Store
)

func main() {
	var rt *appkit.Router

	appkit.Main(appkit.Spec{
		App:        "prompts",
		Mount:      "/srv/prompts/",
		Port:       3004,
		MCP:        true,
		// agent is now also an event-plane CONSUMER: it consumes cron's /feed and
		// fires triggered runs (in-memory fire-and-run, event-triggering decisions
		// §3). CONSUMES=cron mirrors cronSource for the registry.
		Consumes: []string{cronSource},
		// Subscriptions is the LIVE provider the reflection tool reports. agent's
		// one declared in-edge is the cron.* fan-out — the SAME consume.Subscription()
		// the consumer Handler matches against, so the runtime filter and reflection
		// cannot drift.
		Subscriptions: func() []consumer.Subscription {
			return []consumer.Subscription{consume.Subscription()}
		},
		// agent is ALSO an event-plane PRODUCER of two STATIC outcome types
		// (event-triggering decisions §3): run.succeeded / run.failed, emitted in
		// the SAME tx as a run's terminal-state write. Feed mounts the /feed
		// producer; Events is the static registry (NOT a dynamic Publishes provider
		// — the outcome types are fixed at build time, unlike cron's cron.<name>);
		// Producer injects the outbox onto the store; ManifestExtras round-trips the
		// retention config like every other producer.
		Feed:   "/feed",
		Events: session.Events,
		ManifestExtras: []appkit.ManifestKV{
			{Key: "OUTBOX_RETENTION_DAYS", Value: "7"},
			{Key: "OUTBOX_RETENTION_MAX_ROWS", Value: "1000000"},
		},
		Producer: func(ob *outbox.Outbox) error {
			if storeRef == nil {
				return fmt.Errorf("agent: Producer called before Handlers built the Store")
			}
			storeRef.Outbox = ob
			return nil
		},
		Migrations: db.FS,
		// Handlers builds agent's domain over appkit's shared single-writer DB
		// handle, runs the boot-time crash-recovery sweep (after migrate, before
		// serving), captures the Router + service for the consumer worker, and
		// mounts the agent_* MCP surface gated behind nginx-injected identity.
		Handlers: func(r *appkit.Router) error {
			rt = r
			return registerRoutes(r)
		},
		// Workers carries agent's event-plane consumer loop, launched by appkit on
		// the serve context alongside the HTTP server. The consumer Config + the
		// fire-and-run Handler stay app-side — appkit owns the lifecycle, not the
		// event semantics.
		Workers: []func(context.Context) error{
			func(ctx context.Context) error {
				return runConsumer(ctx, rt)
			},
		},
	})
}

// runConsumer is agent's event-plane consumer worker. It drives
// eventplane/consumer.Run over cron's /feed until ctx is cancelled (clean
// shutdown → nil) or a structural fault escapes (→ error, which appkit
// propagates to cancel the server too). The handler is the in-memory
// fire-and-run effect: it never stalls the feed (always returns nil/ErrSkip),
// so cron being down is the only thing the engine retries.
func runConsumer(ctx context.Context, rt *appkit.Router) error {
	logger := rt.Logger()
	// PROMPTS_CRON_FEED_URL is cron's loopback feed. The event plane bypasses
	// nginx, so this is a direct 127.0.0.1 address; the dev fallback is only for
	// `go run`/tests without env (cron's port is 3007).
	feedURL := config.EnvOr(os.Getenv, "PROMPTS_CRON_FEED_URL", "http://127.0.0.1:3007/feed")
	// PROMPTS_CRON_FROM is the first-subscription choice; tail by default so a fresh
	// agent only reacts to cron events fired from now on, not the entire backlog.
	from := config.EnvOr(os.Getenv, "PROMPTS_CRON_FROM", "tail")

	cfg := consumer.Config{
		FeedURL:    feedURL,
		From:       from,
		DB:         rt.DB(),
		Source:     cronSource,
		ConsumerID: consumerID,
		Logger:     logger,
	}
	fire := func(ctx context.Context, sessionID, triggerEvent, scheduledFor string) error {
		_, err := svcRef.RunByID(ctx, sessionID, triggerEvent, scheduledFor)
		return err
	}
	lookup := svcRef.TriggersForEvent
	logger.Info("starting agent cron consumer", "feed_url", feedURL, "from", from)
	if err := consumer.Run(ctx, cfg, consume.Handler(fire, lookup, logger)); err != nil {
		return fmt.Errorf("event-plane consumer: %w", err)
	}
	return nil
}

// registerRoutes wires agent's domain on appkit's server. It is the seam where
// the chassis (appkit) hands off to the domain: appkit has already resolved
// config, opened, and migrated the shared single-writer DB before calling this.
func registerRoutes(rt *appkit.Router) error {
	conn := rt.DB()
	if conn == nil {
		return fmt.Errorf("agent: no DB handle on router")
	}

	// PROMPTS_RUN_TTL bounds each run's wall-clock — the runaway-goroutine backstop
	// (§5.3). Parsed as a Go duration (e.g. "30m", "2h"). Read here at the domain
	// boundary, reusing appkit/config's env helper.
	runTTL, err := config.EnvOrDuration(os.Getenv, "PROMPTS_RUN_TTL", 30*time.Minute)
	if err != nil {
		return err
	}

	// The session data tree (sandboxes + run logs) lives alongside the db file —
	// the same PROMPTS_DB_PATH appkit resolved (default ./tmp/prompts.db; /opt/prompts/
	// data/prompts.db on the box) — mirroring the on-box /opt/prompts/data layout.
	dbPath := config.EnvOr(os.Getenv, "PROMPTS_DB_PATH", "./tmp/prompts.db")
	dataDir := filepath.Join(filepath.Dir(dbPath), "data")
	sb, err := sandbox.New(filepath.Join(dataDir, "sandboxes"))
	if err != nil {
		return fmt.Errorf("agent: sandbox: %w", err)
	}
	runsDir := filepath.Join(dataDir, "runs")

	store := session.NewStore(conn)
	run := runner.New(store, sb, runTTL)
	svc := session.NewService(store, sb, runsDir, run)
	// Capture the service for the consumer Worker and the store for the Producer
	// hook (both run after Handlers; the Producer injects the outbox onto store).
	svcRef = svc
	storeRef = store

	// Crash-recovery sweep (§5.3): runs left 'running' by a previous process are
	// orphaned — mark them failed and return their sessions to idle before
	// serving, so the single-flight gate starts clean. Runs after migrate (appkit
	// migrated the shared conn before calling Handlers) and before the server
	// begins listening.
	if swept, err := run.Recover(context.Background()); err != nil {
		return fmt.Errorf("agent: crash-recovery sweep: %w", err)
	} else if swept > 0 {
		rt.Logger().Warn("crash-recovery: swept orphaned runs", "count", swept)
	}

	rt.Handle("POST /mcp", rt.RequireIdentity(mcp.NewHandler(svc, rt.Version(), rt.Service(), rt.Health())))
	return nil
}
