// Command prompts is the loopback-only domain service behind nginx. It trusts the
// X-Owner-Email / X-Client-Id headers nginx injects after a successful
// auth_request against the dashboard's authorization server, and performs no
// token logic of its own.
//
// The uniform chassis — the fixed subcommands (serve/version/manifest/migrate/
// backup/restore), config-from-env, the migration runner + downgrade guard, the
// loopback HTTP server + PRM + identity gate (health), and graceful
// shutdown — is owned by appkit. main.go declares only prompts' identity (the
// Spec) and wires its domain surface through the Handlers hook: the prompt
// store, per-prompt sandbox tree, async runner, the boot-time crash-recovery
// sweep, and the bare MCP surface. RESOURCE_ID / AUTH_SERVER are composed
// in-binary by appkit/config from IKIGENBA_DOMAIN + MOUNT (was the deleted
// bin/build run-wrapper's job).
//
// prompts is an LLM service: it uses agentkit (the LLM engine + tool surface) for
// the agent loop, kept strictly separate from appkit (the deploy/serve chassis).
// It is neither an event-plane producer nor a consumer — no /feed, no consumer
// loop, no background worker; the async runner is spawned per-run, not a
// long-running task. Its only secret, ANTHROPIC_API_KEY, is read env-only inside
// the runner/prompt domain at the point of use and never logged (§2.8).
package main

import (
	"context"
	"fmt"
	"os"
	"path/filepath"
	"strings"
	"time"

	"appkit"
	"appkit/config"

	"eventplane/consumer"
	"eventplane/outbox"

	"prompts/internal/consume"
	"prompts/internal/db"
	"prompts/internal/mcp"
	"prompts/internal/prompt"
	"prompts/internal/runner"
	"prompts/internal/sandbox"
)

// consumerID is the stable id prompts presents on every consumer connect
// (event-protocol.md §7.1) — the literal "prompts" across all its upstream loops.
const consumerID = "prompts"

// sources is the resolved upstream producers prompts consumes (A11).
// CONSUMES=cron,crm,ledger,dropbox,scripts,prompts in etc/manifest.env mirrors
// this for the registry. The final "prompts" entry is self-chaining (A12): a
// consumer loop pointed at prompts' OWN /feed (PROMPTS_PROMPTS_FEED_URL default
// http://127.0.0.1:3004/feed) so a prompt can fire on another prompt's
// run.succeeded/run.failed.
var sources = []string{"cron", "crm", "ledger", "dropbox", "scripts", "prompts"}

// feedDefaults is each upstream's loopback dev fallback (A11). The event plane
// bypasses nginx, so these are direct 127.0.0.1 addresses; production composes
// the real PROMPTS_<SRC>_FEED_URL via env.
var feedDefaults = map[string]string{
	"cron":    "http://127.0.0.1:3007/feed",
	"crm":     "http://127.0.0.1:3001/feed",
	"ledger":  "http://127.0.0.1:3002/feed",
	"dropbox": "http://127.0.0.1:3005/feed",
	"scripts": "http://127.0.0.1:3009/feed",
	"prompts": "http://127.0.0.1:3004/feed", // self-chaining: prompts' OWN /feed (A12)
}

// svcRef carries the prompt service from the Handlers hook (where appkit has
// opened + migrated the DB and built the domain) to the consumer Worker, which
// runs strictly afterward. A package-level capture mirrors notify's `rt` capture.
// storeRef is the same hand-off for the producer outbox: the Producer hook
// (which runs AFTER Handlers, once appkit has constructed the outbox) injects it
// onto the store so the runner's terminal write emits the outcome event on the
// same tx (event-triggering decisions §3).
var (
	svcRef   *prompt.Service
	storeRef *prompt.Store
)

func main() {
	var rt *appkit.Router

	// One worker per upstream — the notify multi-cursor pattern. Each closes over
	// its own source so it reads PROMPTS_<SRC>_FEED_URL with its own cursor.
	workers := make([]func(context.Context) error, 0, len(sources))
	for _, src := range sources {
		src := src
		workers = append(workers, func(ctx context.Context) error {
			return runConsumer(ctx, rt, src)
		})
	}

	appkit.Main(appkit.Spec{
		App:   "prompts",
		Mount: "/srv/prompts/",
		Port:  3004,
		MCP:   true,
		// Multi-upstream CONSUMER: CONSUMES mirrors `sources` for the registry.
		Consumes: sources,
		// Subscriptions is the LIVE provider the reflection tool reports — the SAME
		// consume.Subscriptions(sources) the consumer Handlers match against, so the
		// runtime filter and reflection cannot drift.
		Subscriptions: func() []consumer.Subscription {
			return consume.Subscriptions(sources)
		},
		// prompts is ALSO an event-plane PRODUCER of two STATIC outcome types:
		// run.succeeded / run.failed, emitted in the SAME tx as a run's
		// terminal-state write. Feed mounts the /feed producer; Events is the static
		// registry (NOT a dynamic Publishes provider — the outcome types are fixed at
		// build time); Producer injects the outbox onto the store; ManifestExtras
		// round-trips the retention config like every other producer. CONSUMES is
		// emitted by appkit from Spec.Consumes (above), so it is NOT repeated here.
		Feed:   "/feed",
		Events: prompt.Events,
		ManifestExtras: []appkit.ManifestKV{
			{Key: "OUTBOX_RETENTION_DAYS", Value: "7"},
			{Key: "OUTBOX_RETENTION_MAX_ROWS", Value: "1000000"},
		},
		Producer: func(ob *outbox.Outbox) error {
			if storeRef == nil {
				return fmt.Errorf("prompts: Producer called before Handlers built the Store")
			}
			storeRef.Outbox = ob
			return nil
		},
		Migrations: db.FS,
		// Handlers builds prompts' domain over appkit's shared single-writer DB
		// handle, runs the boot-time crash-recovery sweep (after migrate, before
		// serving), captures the Router + service for the consumer workers, and
		// mounts the prompts_* MCP surface gated behind nginx-injected identity.
		Handlers: func(r *appkit.Router) error {
			rt = r
			return registerRoutes(r)
		},
		// Workers carries prompts' event-plane consumer loops (one per upstream),
		// launched by appkit on the serve context alongside the HTTP server. The
		// consumer Config + the fire-and-run Handler stay app-side — appkit owns the
		// lifecycle, not the event semantics.
		Workers: workers,
	})
}

// runConsumer drives eventplane/consumer.Run over one upstream's /feed until ctx
// is cancelled (clean shutdown → nil) or a structural fault escapes (→ error,
// which appkit propagates to cancel the server too). The handler is the
// fire-and-run effect: it never stalls the feed (always returns nil/ErrSkip), so
// an upstream being down is the only thing the engine retries.
func runConsumer(ctx context.Context, rt *appkit.Router, source string) error {
	logger := rt.Logger()
	feedURL := config.EnvOr(os.Getenv, feedURLEnv(source), feedDefaults[source])
	// PROMPTS_<SRC>_FROM is the first-subscription choice; tail by default so a
	// fresh prompts only reacts to events fired from now on, not the whole backlog.
	from := config.EnvOr(os.Getenv, fromEnv(source), "tail")

	cfg := consumer.Config{
		FeedURL:    feedURL,
		From:       from,
		DB:         rt.DB(),
		Source:     source,
		ConsumerID: consumerID,
		Logger:     logger,
	}
	fire := func(ctx context.Context, promptID, src, evType, eventID string, payload []byte) error {
		_, err := svcRef.RunByEvent(ctx, promptID, src, evType, eventID, payload)
		return err
	}
	lookup := svcRef.PromptsForEvent
	logger.Info("starting prompts consumer", "source", source, "feed_url", feedURL, "from", from)
	if err := consumer.Run(ctx, cfg, consume.Handler(fire, lookup, source, logger)); err != nil {
		return fmt.Errorf("event-plane consumer (%s): %w", source, err)
	}
	return nil
}

// feedURLEnv / fromEnv build the per-upstream env var names
// (PROMPTS_<SRC>_FEED_URL / PROMPTS_<SRC>_FROM).
func feedURLEnv(source string) string {
	return "PROMPTS_" + strings.ToUpper(source) + "_FEED_URL"
}

func fromEnv(source string) string {
	return "PROMPTS_" + strings.ToUpper(source) + "_FROM"
}

// registerRoutes wires prompts' domain on appkit's server. It is the seam where
// the chassis (appkit) hands off to the domain: appkit has already resolved
// config, opened, and migrated the shared single-writer DB before calling this.
func registerRoutes(rt *appkit.Router) error {
	conn := rt.DB()
	if conn == nil {
		return fmt.Errorf("prompts: no DB handle on router")
	}

	// PROMPTS_RUN_TTL bounds each run's wall-clock — the runaway-goroutine backstop
	// (§5.3). Parsed as a Go duration (e.g. "30m", "2h"). Read here at the domain
	// boundary, reusing appkit/config's env helper.
	runTTL, err := config.EnvOrDuration(os.Getenv, "PROMPTS_RUN_TTL", 30*time.Minute)
	if err != nil {
		return err
	}

	// The run data tree (data/runs/<run_id>/, each holding output.jsonl + a
	// per-run sandbox/) lives alongside the db file — the same PROMPTS_DB_PATH
	// appkit resolved (default ./tmp/prompts.db; /opt/prompts/data/prompts.db on
	// the box) — mirroring the on-box /opt/prompts/data layout.
	dbPath := config.EnvOr(os.Getenv, "PROMPTS_DB_PATH", "./tmp/prompts.db")
	// PROMPTS_MANIFEST_ROOT is the box inventory root the runner reads at run
	// spawn to discover the suite's other loopback MCP services (Surface 2 —
	// in-run suite tools). Defaults to /opt, the on-box layout root.
	manifestRoot := config.EnvOr(os.Getenv, "PROMPTS_MANIFEST_ROOT", "/opt")
	dataDir := filepath.Join(filepath.Dir(dbPath), "data")
	// The run directory is the on-disk unit (data/runs/<run_id>/), holding
	// both the run's output.jsonl and its per-run sandbox/ workspace. The
	// sandbox Manager is rooted at runsDir and keyed by run_id.
	runsDir := filepath.Join(dataDir, "runs")
	sb, err := sandbox.New(runsDir)
	if err != nil {
		return fmt.Errorf("prompts: sandbox: %w", err)
	}

	store := prompt.NewStore(conn)
	run := runner.New(store, sb, runTTL, manifestRoot)
	svc := prompt.NewService(store, sb, runsDir, run)
	// Capture the service for the consumer Worker and the store for the Producer
	// hook (both run after Handlers; the Producer injects the outbox onto store).
	svcRef = svc
	storeRef = store

	// Crash-recovery sweep: runs left 'running' by a previous process are
	// orphaned — mark them failed before serving (touches RUNS only; prompts
	// have no status). Runs after migrate (appkit migrated the shared conn
	// before calling Handlers) and before the server begins listening.
	if swept, err := run.Recover(context.Background()); err != nil {
		return fmt.Errorf("prompts: crash-recovery sweep: %w", err)
	} else if swept > 0 {
		rt.Logger().Warn("crash-recovery: swept orphaned runs", "count", swept)
	}

	rt.Handle("POST /mcp", rt.RequireIdentity(mcp.NewHandler(svc, rt.Version(), rt.Service(), rt.Health())))
	return nil
}
