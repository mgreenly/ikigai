// Command scripts is the loopback-only domain service behind nginx. It trusts
// the X-Owner-Email / X-Client-Id headers nginx injects after a successful
// auth_request against the dashboard's authorization server, and performs no
// token logic of its own.
//
// The uniform chassis — the fixed subcommands (serve/version/manifest/migrate/
// schema), config-from-env, the migration runner + downgrade guard, the
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
	"net/http"
	"os"
	"path/filepath"
	"time"

	"appkit"
	"appkit/config"

	"eventplane/consumer"
	"eventplane/outbox"

	"registry"

	"scripts/internal/consume"
	"scripts/internal/db"
	"scripts/internal/mcp"
	"scripts/internal/runner"
	"scripts/internal/script"
)

// svcRef carries the script service from the Handlers hook (where appkit has
// opened + migrated the DB and built the domain) to the consumer handler
// factories, which run strictly afterward. storeRef is the same hand-off for the
// producer outbox: the Producer hook (which runs AFTER Handlers) injects it onto
// the store so the runner's terminal write emits the completion event on the
// same tx.
var (
	svcRef   *script.Service
	storeRef *script.Store
)

func main() {
	appkit.Main(scriptsSpec())
}

func scriptsSpec() appkit.Spec {
	return appkit.Spec{
		App:   "scripts",
		Mount: "/srv/scripts/",
		Port:  registry.MustPort("scripts"),
		MCP:   true,
		Consumers: []appkit.Consumer{
			scriptsConsumerEntry("cron"),
			scriptsConsumerEntry("crm"),
			scriptsConsumerEntry("ledger"),
			scriptsConsumerEntry("dropbox"),
			scriptsConsumerEntry("prompts"),
		},
		// PRODUCER of two STATIC completion types (scripts.succeeded /
		// scripts.failed), emitted in the SAME tx as a run's terminal write. Feed
		// mounts /feed; Events is the static registry; Producer injects the outbox
		// onto the store; ManifestExtras round-trips retention config.
		Feed:   "/feed",
		Events: script.Events,
		WWW:    true,
		Health: scriptsHealth,
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
		Handlers:   registerRoutes,
	}
}

func scriptsConsumerEntry(source string) appkit.Consumer {
	var entry appkit.Consumer
	entry.Source = source
	entry.Subscriptions = consume.Subscriptions([]string{source})
	entry.Handler = scriptsConsumer(source)
	return entry
}

// scriptsConsumer is the per-upstream handler factory. The chassis calls it with
// the finished Router AFTER Handlers (and Producer) have run, so svcRef — the
// domain Service that registerRoutes built — is populated. Each loop fans that
// upstream's events to matching scripts (consume.Handler is unchanged).
func scriptsConsumer(source string) func(*appkit.Router) consumer.Handler {
	return func(rt *appkit.Router) consumer.Handler {
		return consume.Handler(svcRef.RunForEvent, svcRef.ScriptsForEvent, source, rt.Logger())
	}
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

	// State is durable; runs are rebuildable execution trees. appkit has already
	// resolved the DB and generation paths; re-resolve the same env contract here
	// so runs can live under the service-owned cache directory.
	cfg, err := config.Resolve("scripts", "/srv/scripts/", registry.MustPort("scripts"), os.Getenv)
	if err != nil {
		return err
	}
	rootDir := scriptsRuntimeRoot(cfg.GenerationPath)
	runsDir := filepath.Join(rootDir, "runs")
	if err := recreateRunsDir(runsDir); err != nil {
		return err
	}

	store := script.NewStore(conn)
	run := runner.New(store, rootDir, runTTL)
	svc := script.NewService(store, runsDir, run)
	// Wire the dropbox loopback client for the import verb. DROPBOX_BASE_URL is
	// env-only (the loopback-URL-via-env shape notify uses for *_FEED_URL); the
	// default works for the standard on-box loopback layout. Field-injected after
	// NewService so existing construction stays untouched.
	dropboxBase := config.EnvOr(os.Getenv, "DROPBOX_BASE_URL", registry.BaseURL("dropbox"))
	svc.Fetcher = script.NewHTTPFetcher(dropboxBase)
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

	rt.Handle("GET /{$}", http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		_ = rt.WWW().Render(w, "landing.html", struct{ Service, Version string }{rt.Service(), rt.Version()})
	}))
	handler, err := mcp.NewHandler(svc, rt)
	if err != nil {
		return err
	}
	rt.Handle("POST /mcp", rt.RequireIdentity(handler))
	return nil
}

func scriptsHealth(ctx context.Context) (map[string]any, error) {
	return map[string]any{
		"python_version": ">=3.11",
		"bash_version":   ">=5.0",
		"network":        true,
		"packages":       "stdlib",
	}, nil
}

func scriptsRuntimeRoot(generationPath string) string {
	cacheDir := filepath.Dir(generationPath)
	if cacheDir == "" {
		return "."
	}
	return cacheDir
}

func recreateRunsDir(runsDir string) error {
	if runsDir == "" || runsDir == "." || runsDir == string(os.PathSeparator) {
		return fmt.Errorf("scripts: invalid runs dir %q", runsDir)
	}
	if err := os.RemoveAll(runsDir); err != nil {
		return fmt.Errorf("scripts: recreate runs dir: remove %s: %w", runsDir, err)
	}
	if err := os.MkdirAll(runsDir, 0o700); err != nil {
		return fmt.Errorf("scripts: recreate runs dir: mkdir %s: %w", runsDir, err)
	}
	return nil
}
