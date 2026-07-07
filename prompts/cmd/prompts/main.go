// Command prompts is the loopback-only domain service behind nginx. It trusts the
// X-Owner-Email / X-Client-Id headers nginx injects after a successful
// auth_request against the dashboard's authorization server, and performs no
// token logic of its own.
//
// The uniform chassis — the fixed subcommands (serve/version/manifest/migrate/
// schema), config-from-env, the migration runner + downgrade guard, the
// loopback HTTP server + PRM + identity gate (health), and graceful
// shutdown — is owned by appkit. main.go declares only prompts' identity (the
// Spec) and wires its domain surface through the Handlers hook: the prompt
// store, per-prompt sandbox tree, async runner, the boot-time crash-recovery
// sweep, the bare MCP surface, and the session-gated human landing page (service
// name + version, Carbon-styled) served ungated in-process at the mount root.
// RESOURCE_ID / AUTH_SERVER are composed
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
	"net/http"
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
	"prompts/internal/prompt"
	"prompts/internal/runner"
	"prompts/internal/sandbox"

	"registry"
)

// consumerID is the stable id prompts presents on every consumer connect
// (event-protocol.md §7.1) — the literal "prompts" across all its upstream loops.
const consumerID = "prompts"

// sources is the resolved upstream producers prompts consumes (A11).
// CONSUMES=cron,crm,ledger,dropbox,scripts,prompts in etc/manifest.env mirrors
// this for the registry. The final "prompts" entry is self-chaining (A12): a
// consumer loop pointed at prompts' OWN /feed (PROMPTS_PROMPTS_FEED_URL defaults
// through the shared registry) so a prompt can fire on another prompt's
// run.succeeded/run.failed.
var sources = []string{"cron", "crm", "ledger", "dropbox", "scripts", "prompts"}

// svcRef carries the prompt service from the Handlers hook (where appkit has
// opened + migrated the DB and built the domain) to the consumer handlers, which
// are built strictly afterward by appkit's Consumers table.
// storeRef is the same hand-off for the producer outbox: the Producer hook
// (which runs AFTER Handlers, once appkit has constructed the outbox) injects it
// onto the store so the runner's terminal write emits the outcome event on the
// same tx (event-triggering decisions §3).
var (
	svcRef   *prompt.Service
	storeRef *prompt.Store
)

func main() {
	appkit.Main(promptsSpec())
}

func promptsSpec() appkit.Spec {
	consumers := make([]appkit.Consumer, 0, len(sources))
	for _, src := range sources {
		src := src
		consumers = append(consumers, appkit.Consumer{
			Source:        src,
			Subscriptions: consume.Subscriptions([]string{src}),
			Handler: func(rt *appkit.Router) consumer.Handler {
				logger := rt.Logger()
				fire := func(ctx context.Context, promptID, s, evType, eventID string, payload []byte) error {
					_, err := svcRef.RunByEvent(ctx, promptID, s, evType, eventID, payload)
					return err
				}
				return consume.Handler(fire, svcRef.PromptsForEvent, src, logger)
			},
		})
	}

	return appkit.Spec{
		App:       "prompts",
		Mount:     "/srv/prompts/",
		Port:      registry.MustPort("prompts"),
		MCP:       true,
		WWW:       true,
		Consumers: consumers,
		// prompts is ALSO an event-plane PRODUCER of two STATIC outcome types:
		// run.succeeded / run.failed, emitted in the SAME tx as a run's
		// terminal-state write. Feed mounts the /feed producer; Events is the static
		// registry (NOT a dynamic Publishes provider — the outcome types are fixed at
		// build time); Consumers emits CONSUMES; Producer injects the outbox onto the
		// store; ManifestExtras round-trips the retention config like every other
		// producer.
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
		// serving), captures the service for the consumer handlers, and mounts the
		// prompts_* MCP surface gated behind nginx-injected identity.
		Handlers: func(r *appkit.Router) error {
			return registerRoutes(r)
		},
	}
}

// registerRoutes wires prompts' domain on appkit's server. It is the seam where
// the chassis (appkit) hands off to the domain: appkit has already resolved
// config, opened, and migrated the shared single-writer DB before calling this.
func registerRoutes(rt *appkit.Router) error {
	site := rt.WWW()
	if site == nil {
		return fmt.Errorf("prompts: no WWW site on router")
	}
	rt.HandleFunc("GET /{$}", func(w http.ResponseWriter, r *http.Request) {
		if err := site.Render(w, "landing.html", landingData{
			Service: rt.Service(),
			Version: rt.Version(),
		}); err != nil {
			http.Error(w, err.Error(), http.StatusInternalServerError)
		}
	})

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

	// PROMPTS_DB_PATH is appkit's state DB path. Sandboxes are durable state
	// beside it; runs are boot-recreated scratch under the generation cache.
	dbPath := config.EnvOr(os.Getenv, "PROMPTS_DB_PATH", "./tmp/prompts.db")
	generationPath := config.EnvOr(os.Getenv, "PROMPTS_GENERATION_PATH", filepath.Join(filepath.Dir(dbPath), "prompts.db.generation"))
	// PROMPTS_MANIFEST_ROOT is the box inventory root the runner reads at run
	// spawn to discover the suite's other loopback MCP services (Surface 2 —
	// in-run suite tools). Defaults to /opt, the on-box layout root.
	manifestRoot := config.EnvOr(os.Getenv, "PROMPTS_MANIFEST_ROOT", "/opt")
	stateDir := filepath.Dir(dbPath)
	sandboxesDir := filepath.Join(stateDir, "sandboxes")
	cacheDir := filepath.Dir(generationPath)
	runsDir := filepath.Join(cacheDir, "runs")
	if err := recreateRunsDir(runsDir); err != nil {
		return err
	}
	sb, err := sandbox.New(sandboxesDir)
	if err != nil {
		return fmt.Errorf("prompts: sandbox: %w", err)
	}

	store := prompt.NewStore(conn)
	run := runner.New(store, sb, runTTL, manifestRoot)
	svc := prompt.NewService(store, sb, runsDir, run)
	// Wire the dropbox loopback content fetcher for the import verb. DROPBOX_BASE_URL
	// is env-only (defaulting through the shared registry), the same
	// loopback-URL-via-env shape notify uses for its feed URLs. Field-injected so
	// NewService stays unchanged.
	dropboxBase := dropboxBaseURL(os.Getenv)
	svc.Fetcher = prompt.NewHTTPFetcher(dropboxBase)
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

type landingData struct {
	Service string
	Version string
}

func dropboxBaseURL(getenv func(string) string) string {
	return config.EnvOr(getenv, "DROPBOX_BASE_URL", registry.BaseURL("dropbox"))
}

func recreateRunsDir(runsDir string) error {
	if runsDir == "" || runsDir == "." || runsDir == string(os.PathSeparator) {
		return fmt.Errorf("prompts: invalid runs dir %q", runsDir)
	}
	if err := os.RemoveAll(runsDir); err != nil {
		return fmt.Errorf("prompts: recreate runs dir: remove %s: %w", runsDir, err)
	}
	if err := os.MkdirAll(runsDir, 0o755); err != nil {
		return fmt.Errorf("prompts: recreate runs dir: mkdir %s: %w", runsDir, err)
	}
	return nil
}
