// Command scripts is the loopback-only domain service behind nginx. It trusts
// the X-Owner-Email / X-Client-Id headers nginx injects after a successful
// auth_request against the dashboard's authorization server, and performs no
// token logic of its own.
//
// The uniform chassis — the fixed subcommands (serve/version/manifest/migrate/
// backup/restore), config-from-env, the migration runner + downgrade guard, the
// loopback HTTP server + PRM + identity gate, and graceful shutdown — is owned
// by appkit. main.go declares only scripts' identity (the Spec) and wires its
// domain surface through the Handlers hook: the script store, the python3-exec
// runner, the boot-time crash-recovery sweep, and the ikigenba_scripts_* MCP
// surface. RESOURCE_ID / AUTH_SERVER are composed in-binary by appkit/config
// from IKIGENBA_DOMAIN + MOUNT.
//
// scripts is BOTH an event-plane PRODUCER (it emits scripts.succeeded /
// scripts.failed on its own /feed, in the SAME tx as a run's terminal write) and
// a multi-upstream CONSUMER (one consumer.Run worker per upstream producer:
// cron, crm, ledger, dropbox, prompts). It holds NO secret — no LLM provider,
// open network — so there is no provider API key and no bin/secrets.
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

	"scripts/internal/consume"
	"scripts/internal/db"
	"scripts/internal/mcp"
	"scripts/internal/runner"
	"scripts/internal/script"
)

// consumerID is the stable id scripts presents on every consumer connect
// (event-protocol.md §7.1) — the literal "scripts" across all its upstream
// loops.
const consumerID = "scripts"

// sources is the five resolved upstream producers scripts consumes day-one
// (PLAN.md §A4/§A11). CONSUMES=cron,crm,ledger,dropbox,prompts in
// etc/manifest.env mirrors this for the registry.
//
// TODO(self-chaining, §A12): add "scripts" pointed at the local /feed
// (SCRIPTS_SCRIPTS_FEED_URL default http://127.0.0.1:3009/feed) for
// scripts.succeeded/failed chaining once trivial.
var sources = []string{"cron", "crm", "ledger", "dropbox", "prompts"}

// feedDefaults is each upstream's loopback dev fallback (PLAN.md §A11). The event
// plane bypasses nginx, so these are direct 127.0.0.1 addresses; production
// composes the real SCRIPTS_<SRC>_FEED_URL via the deploy wrapper.
var feedDefaults = map[string]string{
	"cron":    "http://127.0.0.1:3007/feed",
	"crm":     "http://127.0.0.1:3001/feed",
	"ledger":  "http://127.0.0.1:3002/feed",
	"dropbox": "http://127.0.0.1:3005/feed",
	"prompts": "http://127.0.0.1:3004/feed",
}

// svcRef carries the script service from the Handlers hook (where appkit has
// opened + migrated the DB and built the domain) to the consumer Workers, which
// run strictly afterward. storeRef is the same hand-off for the producer outbox:
// the Producer hook (which runs AFTER Handlers) injects it onto the store so the
// runner's terminal write emits the completion event on the same tx.
var (
	svcRef   *script.Service
	storeRef *script.Store
)

func main() {
	var rt *appkit.Router

	// One worker per upstream — the notify multi-cursor pattern. Each closes over
	// its own source so it reads SCRIPTS_<SRC>_FEED_URL with its own cursor.
	workers := make([]func(context.Context) error, 0, len(sources))
	for _, src := range sources {
		src := src
		workers = append(workers, func(ctx context.Context) error {
			return runConsumer(ctx, rt, src)
		})
	}

	appkit.Main(appkit.Spec{
		App:   "scripts",
		Mount: "/srv/scripts/",
		Port:  3009,
		MCP:   true,
		// Multi-upstream CONSUMER: CONSUMES mirrors `sources` for the registry.
		Consumes: sources,
		// Subscriptions is the LIVE provider the reflection tool reports — the
		// SAME consume.Subscriptions(sources) the consumer Handlers match against,
		// so runtime filter and reflection cannot drift.
		Subscriptions: func() []consumer.Subscription {
			return consume.Subscriptions(sources)
		},
		// PRODUCER of two STATIC completion types (scripts.succeeded /
		// scripts.failed), emitted in the SAME tx as a run's terminal write. Feed
		// mounts /feed; Events is the static registry; Producer injects the outbox
		// onto the store; ManifestExtras round-trips retention config.
		Feed:   "/feed",
		Events: script.Events,
		// CONSUMES is emitted by appkit from Spec.Consumes (above), so it is NOT
		// repeated here.
		ManifestExtras: []appkit.ManifestKV{
			{Key: "OUTBOX_RETENTION_DAYS", Value: "7"},
			{Key: "OUTBOX_RETENTION_MAX_ROWS", Value: "1000000"},
		},
		Producer: func(ob *outbox.Outbox) error {
			if storeRef == nil {
				return fmt.Errorf("scripts: Producer called before Handlers built the Store")
			}
			storeRef.Outbox = ob
			return nil
		},
		Migrations: db.FS,
		Handlers: func(r *appkit.Router) error {
			rt = r
			return registerRoutes(r)
		},
		Workers: workers,
	})
}

// runConsumer drives eventplane/consumer.Run over one upstream's /feed until ctx
// is cancelled (clean shutdown → nil) or a structural fault escapes (→ error,
// which appkit propagates). The handler is fire-and-run: it never stalls the
// feed (always returns nil/ErrSkip).
func runConsumer(ctx context.Context, rt *appkit.Router, source string) error {
	logger := rt.Logger()
	feedURL := config.EnvOr(os.Getenv, feedURLEnv(source), feedDefaults[source])
	from := config.EnvOr(os.Getenv, fromEnv(source), "tail")

	cfg := consumer.Config{
		FeedURL:    feedURL,
		From:       from,
		DB:         rt.DB(),
		Source:     source,
		ConsumerID: consumerID,
		Logger:     logger,
	}
	fire := func(ctx context.Context, scriptID, src, evType, eventID string, payload []byte) error {
		return svcRef.RunForEvent(ctx, scriptID, src, evType, eventID, payload)
	}
	lookup := svcRef.ScriptsForEvent
	logger.Info("starting scripts consumer", "source", source, "feed_url", feedURL, "from", from)
	if err := consumer.Run(ctx, cfg, consume.Handler(fire, lookup, source, logger)); err != nil {
		return fmt.Errorf("event-plane consumer (%s): %w", source, err)
	}
	return nil
}

// feedURLEnv / fromEnv build the per-upstream env var names (SCRIPTS_<SRC>_FEED_URL
// / SCRIPTS_<SRC>_FROM).
func feedURLEnv(source string) string {
	return "SCRIPTS_" + strings.ToUpper(source) + "_FEED_URL"
}

func fromEnv(source string) string {
	return "SCRIPTS_" + strings.ToUpper(source) + "_FROM"
}

// registerRoutes wires scripts' domain on appkit's server. appkit has already
// resolved config, opened, and migrated the shared single-writer DB before
// calling this.
func registerRoutes(rt *appkit.Router) error {
	conn := rt.DB()
	if conn == nil {
		return fmt.Errorf("scripts: no DB handle on router")
	}

	// SCRIPTS_RUN_TTL bounds each run's wall-clock — the runaway backstop (§5.2).
	runTTL, err := config.EnvOrDuration(os.Getenv, "SCRIPTS_RUN_TTL", 30*time.Minute)
	if err != nil {
		return err
	}

	// The run data tree lives alongside the db file (the same SCRIPTS_DB_PATH
	// appkit resolved; default ./tmp/scripts.db), mirroring the on-box
	// /opt/scripts/data layout.
	dbPath := config.EnvOr(os.Getenv, "SCRIPTS_DB_PATH", "./tmp/scripts.db")
	dataDir := filepath.Join(filepath.Dir(dbPath), "data")
	runsDir := filepath.Join(dataDir, "runs")

	store := script.NewStore(conn)
	run := runner.New(store, dataDir, runTTL)
	svc := script.NewService(store, runsDir, run)
	// Capture the service for the consumer Workers and the store for the Producer
	// hook (both run after Handlers; the Producer injects the outbox onto store).
	svcRef = svc
	storeRef = store

	// Crash-recovery sweep (§5.2): runs left 'running' by a previous process are
	// orphaned — mark them failed before serving. Run dirs are NOT touched (they
	// persist as history).
	if swept, err := run.Recover(context.Background()); err != nil {
		return fmt.Errorf("scripts: crash-recovery sweep: %w", err)
	} else if swept > 0 {
		rt.Logger().Warn("crash-recovery: swept orphaned runs", "count", swept)
	}

	rt.Handle("POST /mcp", rt.RequireIdentity(mcp.NewHandler(svc, rt.Version(), rt.Service(), rt.Health())))
	return nil
}
