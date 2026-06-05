// Command wiki is the loopback-only domain service behind nginx. It trusts the
// X-Owner-Email / X-Client-Id headers nginx injects after a successful
// auth_request against the dashboard's authorization server, and performs no
// token logic of its own. See appkit/server for the auth contract.
//
// wiki is the proof that the two shared libraries COMPOSE: the uniform deploy/
// serve chassis — the fixed subcommands (serve/version/manifest/migrate/backup/
// restore), config-from-env, the migration runner + downgrade guard, the loopback
// HTTP server + PRM + identity gate, and the serve lifecycle — is owned by appkit;
// the LLM provider + async job-runner that powers ingest/ask/lint is owned by
// agentkit; the dropbox file-lifecycle subscription is owned by eventplane. main.go
// declares wiki's identity (the Spec) and wires its domain surface:
//
//   - Handlers builds the agentkit-backed domain graph (the immutable content
//     store, the BM25 search index, the ingest/ask cores over the agentkit
//     provider + job-runner) over appkit's shared single-writer DB handle, and
//     mounts the wiki_* MCP surface. This is the composition root where the
//     agentkit provider is constructed: its client factory closes over
//     ANTHROPIC_API_KEY read env-only here (presence-checked, never logged — §2.8);
//     an absent key disables the agent verbs without blocking boot.
//   - Workers carries the single event-plane CONSUMER loop (eventplane/consumer.Run
//     over dropbox's /feed). appkit runs it on the serve context alongside the HTTP
//     server (the E2 Workers seam). The consumer feeds the SAME ingest core the MCP
//     verbs use, which SPAWNS the async agentkit integration job and returns
//     immediately — so the agentkit job-runner is per-trigger, not a standing
//     Worker; one Worker (the consumer) matches the pre-appkit behavior exactly.
//
// The HTTP server and the consumer worker share appkit's serve context: a SIGTERM
// cancels both; a STRUCTURAL consumer fault (consumer.Run escaping on a missing
// feed_offset table — a deploy bug, event-protocol.md decision 11) returns from
// the worker, which cancels the serve context and brings the server down too — no
// half-alive HTTP-up / consumer-dead state. A transport fault (dropbox down) is
// retried indefinitely inside the engine and never returns.
package main

import (
	"context"
	"fmt"
	"os"
	"strconv"
	"time"

	"appkit"

	"agentkit/provider"
	"agentkit/provider/anthropic"

	"eventplane/consumer"

	"wiki/internal/ask"
	"wiki/internal/consume"
	"wiki/internal/db"
	"wiki/internal/ingest"
	"wiki/internal/lint"
	"wiki/internal/mcp"
	"wiki/internal/search"
	"wiki/internal/store"
)

// The event-plane upstream wiki consumes and the stable id it presents on every
// connect (event-protocol.md §7.1). Both are fixed constants — wiki consumes
// exactly dropbox's file-lifecycle feed, and its X-Consumer-Id is the literal
// "wiki". CONSUMES=dropbox in etc/manifest.env mirrors upstreamSource for the
// registry.
const (
	upstreamSource = "dropbox"
	consumerID     = "wiki"
)

func main() {
	// ingestCore and rt are captured by the Handlers hook (which appkit runs after
	// it opens + migrates the shared single-writer DB) so the consumer worker can
	// reach the same ingest core, DB handle, and logger. The worker runs strictly
	// after Handlers has built the graph, so the captures are always set by the
	// time it executes (nil ingestCore ⇒ no ANTHROPIC_API_KEY ⇒ consumer disabled).
	var (
		rt         *appkit.Router
		ingestCore *ingest.Core
	)

	appkit.Main(appkit.Spec{
		App:        "wiki",
		Mount:      "/srv/wiki/",
		Port:       3006,
		MCP:        true,
		Consumes:   []string{upstreamSource}, // event-plane consumer → CONSUMES=dropbox
		Migrations: db.FS,
		// Non-secret ingest config the old bin/build wrapper exported, now declared
		// here so `wiki manifest` round-trips it (§1.1). The ANTHROPIC_API_KEY secret
		// is NEVER a ManifestExtra — it flows env-only into the client factory below.
		ManifestExtras: []appkit.ManifestKV{
			{Key: "WIKI_INGEST_MODEL", Value: envOr(os.Getenv, "WIKI_INGEST_MODEL", ingest.DefaultModel)},
			{Key: "WIKI_INGEST_MAX_TOKENS", Value: strconv.Itoa(envOrIntOrDefault(os.Getenv, "WIKI_INGEST_MAX_TOKENS", ingest.DefaultMaxTokens))},
		},
		// Handlers is wiki's composition root: it builds the agentkit-backed domain
		// graph over appkit's shared DB handle (rt.DB()) and mounts the MCP surface.
		// The agentkit provider is constructed here (client factory closing over the
		// env-only ANTHROPIC_API_KEY); an absent key disables the agent verbs but the
		// service still serves wiki_whoami / wiki_search / tools/list.
		Handlers: func(r *appkit.Router) error {
			rt = r
			core, h, err := buildDomain(r)
			if err != nil {
				return err
			}
			ingestCore = core
			rt.Handle("POST /mcp", rt.RequireIdentity(h))
			return nil
		},
		// Workers carries wiki's event-plane consumer loop. appkit launches it on the
		// serve context alongside the HTTP server (the E2 consumer seam). The consumer
		// Config + dropbox→ingest Handler stay app-side — appkit owns the lifecycle,
		// not the event semantics. When ingest is disabled (no key) the consumer is a
		// graceful no-op (nothing to file dropbox events into).
		Workers: []func(context.Context) error{
			func(ctx context.Context) error {
				return runConsumer(ctx, rt, ingestCore)
			},
		},
	})
}

// buildDomain assembles wiki's agentkit-backed domain graph over appkit's shared
// single-writer DB handle and returns the ingest core (shared with the consumer
// worker) plus the MCP handler to mount. It is the composition root where the
// agentkit provider is constructed and the LLM secret is read.
//
// wiki_search is a SYNCHRONOUS BM25 read (no agent, no key), so it is always
// wired. ingest/ask are agentic: their client factories close over the env-only
// ANTHROPIC_API_KEY; when it is absent they are nil and the corresponding verbs
// return a clear "unavailable" tool-error while the rest of the surface stays up.
func buildDomain(rt *appkit.Router) (*ingest.Core, *mcp.Handler, error) {
	cfg, err := resolveDomainCfg(os.Getenv)
	if err != nil {
		return nil, nil, err
	}
	logger := rt.Logger()
	conn := rt.DB()

	// Filesystem content store + BM25 search index (the index file lives under the
	// store's per-collection .search/ slot).
	st, err := store.New(cfg.dataRoot)
	if err != nil {
		return nil, nil, fmt.Errorf("store: %w", err)
	}
	idx := search.NewBM25Index(st.SearchIndexPath)

	var (
		ingester   mcp.Ingester
		ingestCore *ingest.Core
		asker      mcp.Asker
	)

	// The agentkit provider is built here, behind the env-only secret. We
	// presence-check the key (NEVER its value) and degrade loudly when absent: the
	// non-agent surface (wiki_whoami, wiki_search, tools/list) must still come up.
	if cfg.apiKey == "" {
		logger.Warn("ANTHROPIC_API_KEY is not set — ingest and ask are DISABLED (wiki_whoami, wiki_search, and tools/list still work); set it via app-config (box) or .envrc (dev) to enable the agent verbs")
	} else {
		// The agentkit anthropic client factory: it closes over the env-only API key
		// and the ingest model resolved at this boundary. It is only invoked when a
		// job actually runs, so a missing key never blocks boot — but here the key is
		// present, so ingest/ask are live.
		newIngestClient := func() (provider.Client, error) {
			return anthropic.New(cfg.apiKey, cfg.ingestModel)
		}
		core := ingest.New(st, idx, conn, newIngestClient, ingest.Config{
			Model:     cfg.ingestModel,
			MaxTokens: cfg.maxTokens,
			JobTTL:    cfg.jobTTL,
		})
		// Boot-time crash recovery: flip any 'running' rows orphaned by a crash to
		// 'failed' before serving (the agentkit runner's per-spawn nature means this
		// is the one place the whole table is swept; ingest + lint share wiki_jobs).
		if n, err := core.Recover(context.Background()); err != nil {
			return nil, nil, fmt.Errorf("ingest recover: %w", err)
		} else if n > 0 {
			logger.Warn("swept crash-orphaned ingest/lint jobs at boot", "count", n)
		}
		ingester = core
		ingestCore = core

		// Lint maintenance pass: reuses the same agentkit agent/job machinery as
		// ingest, sharing the single-writer DB, the wiki_jobs table, and ingest's
		// per-(owner,collection) flight key. Its trigger (Linter.Lint) is MANUAL and
		// internal for now — cadence DEFERRED — so it is deliberately NOT a public MCP
		// verb. It is constructed so it is callable today and schedulable later. The
		// blank assignment keeps it live in the composition root without an MCP wire.
		_ = lint.New(st, idx, conn, newIngestClient, lint.Config{
			Model:     cfg.lintModel,
			MaxTokens: cfg.lintMaxTokens,
			JobTTL:    cfg.lintJobTTL,
		})

		// Ask synthesis pass: the agentic, async read behind wiki_ask. It reuses the
		// same agentkit agent/job machinery and shares ingest's flight key. Its client
		// factory closes over the ask-specific model so its cost/latency tune
		// independently of ingest; like ingest it needs the env-only key.
		newAskClient := func() (provider.Client, error) {
			return anthropic.New(cfg.apiKey, cfg.askModel)
		}
		asker = ask.New(st, idx, conn, newAskClient, ask.Config{
			Model:     cfg.askModel,
			MaxTokens: cfg.askMaxTokens,
			JobTTL:    cfg.askJobTTL,
		})
	}

	// wiki_search is a SYNCHRONOUS read over the BM25 index — no agent, no key — so
	// the *search.BM25Index is wired directly and stays available even when the key
	// is absent (ingest disabled).
	h := mcp.NewHandler(ingester, idx, asker)

	logger.Info("wiki domain ready",
		"data_root", cfg.dataRoot, "ingest_model", cfg.ingestModel,
		"ingest_enabled", cfg.apiKey != "")
	return ingestCore, h, nil
}

// runConsumer is wiki's event-plane consumer worker. It maps dropbox's
// file-lifecycle events (for the hardcoded wiki/ingest folder) into the SAME async
// ingest core the MCP verbs use, then drives eventplane/consumer.Run over dropbox's
// /feed until ctx is cancelled (clean shutdown → nil) or a structural fault escapes
// (→ error, which appkit propagates to cancel the server too — decision 11).
//
// The consumer runs ONLY when ingest is enabled (ingestCore != nil — no core,
// nothing to file), a feed URL resolved, and a box owner is configured. When it is
// intentionally disabled it must NOT return early: under appkit's Workers seam a
// worker that returns (even nil) cancels the serve context and tears the HTTP
// server down (the decision-11 fault coupling). A *disabled* consumer is not a
// fault, so it blocks on ctx until shutdown and then returns nil — the server stays
// up serving the MCP surface, exactly like a transport-fault-free idle worker
// (appkit workers_test TestWorkers_TransportFaultDoesNotKillServer).
func runConsumer(ctx context.Context, rt *appkit.Router, ingestCore *ingest.Core) error {
	logger := rt.Logger()
	cfg := resolveConsumerCfg(os.Getenv)

	switch {
	case ingestCore == nil:
		logger.Warn("event-plane consumer DISABLED: ingest is off (no ANTHROPIC_API_KEY) — nothing to file dropbox events into")
		return blockUntilDone(ctx)
	case cfg.feedURL == "":
		logger.Warn("event-plane consumer DISABLED: no DROPBOX_FEED_URL")
		return blockUntilDone(ctx)
	case cfg.owner == "":
		logger.Warn("event-plane consumer DISABLED: no WIKI_OWNER (dropbox is single-owner; the box owner must be configured to file wiki/ingest events)")
		return blockUntilDone(ctx)
	}

	consumerCfg := consumer.Config{
		FeedURL:    cfg.feedURL,
		From:       cfg.from,
		DB:         rt.DB(),
		Source:     upstreamSource,
		ConsumerID: consumerID,
		Logger:     logger,
	}
	handler := consume.Handler(consume.Config{
		Owner:    cfg.owner,
		Ingester: ingestCore,
		Logger:   logger,
	})
	logger.Info("starting wiki consumer",
		"feed_url", cfg.feedURL, "from", cfg.from, "owner", cfg.owner)
	if err := consumer.Run(ctx, consumerCfg, handler); err != nil {
		return fmt.Errorf("event-plane consumer: %w", err)
	}
	return nil
}

// domainCfg is wiki's agentic-domain configuration, read once at the composition
// root. The plain config (data root, model ids, cost ceilings, TTLs) carries dev
// fallbacks; apiKey is the deployment SECRET (ANTHROPIC_API_KEY) injected via the
// environment (.envrc locally; app-config in prod) and never read from source,
// composed in, or logged (§2.8).
type domainCfg struct {
	dataRoot string
	apiKey   string // ANTHROPIC_API_KEY — SECRET (presence drives the agent verbs)

	ingestModel string
	maxTokens   int
	jobTTL      time.Duration

	lintModel     string
	lintMaxTokens int
	lintJobTTL    time.Duration

	askModel     string
	askMaxTokens int
	askJobTTL    time.Duration
}

// resolveDomainCfg reads wiki's agentic-domain config from the environment.
// WIKI_INGEST_MODEL / WIKI_INGEST_MAX_TOKENS mirror the ManifestExtras (the
// manifest round-trips the non-secret ingest config). ANTHROPIC_API_KEY is read
// here — its value is never logged; only its presence is observed.
func resolveDomainCfg(getenv func(string) string) (domainCfg, error) {
	maxTokens, err := envOrInt(getenv, "WIKI_INGEST_MAX_TOKENS", ingest.DefaultMaxTokens)
	if err != nil {
		return domainCfg{}, err
	}
	ttlSeconds, err := envOrInt(getenv, "WIKI_INGEST_JOB_TTL_SECONDS", 600)
	if err != nil {
		return domainCfg{}, err
	}
	lintMaxTokens, err := envOrInt(getenv, "WIKI_LINT_MAX_TOKENS", lint.DefaultMaxTokens)
	if err != nil {
		return domainCfg{}, err
	}
	lintTTLSeconds, err := envOrInt(getenv, "WIKI_LINT_JOB_TTL_SECONDS", 600)
	if err != nil {
		return domainCfg{}, err
	}
	askMaxTokens, err := envOrInt(getenv, "WIKI_ASK_MAX_TOKENS", ask.DefaultMaxTokens)
	if err != nil {
		return domainCfg{}, err
	}
	askTTLSeconds, err := envOrInt(getenv, "WIKI_ASK_JOB_TTL_SECONDS", 600)
	if err != nil {
		return domainCfg{}, err
	}
	return domainCfg{
		dataRoot: envOr(getenv, "WIKI_DATA_ROOT", "./tmp/data"),
		apiKey:   getenv("ANTHROPIC_API_KEY"), // SECRET — value never logged

		ingestModel: envOr(getenv, "WIKI_INGEST_MODEL", ingest.DefaultModel),
		maxTokens:   maxTokens,
		jobTTL:      time.Duration(ttlSeconds) * time.Second,

		lintModel:     envOr(getenv, "WIKI_LINT_MODEL", lint.DefaultModel),
		lintMaxTokens: lintMaxTokens,
		lintJobTTL:    time.Duration(lintTTLSeconds) * time.Second,

		askModel:     envOr(getenv, "WIKI_ASK_MODEL", ask.DefaultModel),
		askMaxTokens: askMaxTokens,
		askJobTTL:    time.Duration(askTTLSeconds) * time.Second,
	}, nil
}

// consumerCfg is wiki's event-plane consumer configuration, read at the worker's
// composition root. All non-secret: the feed URL (dropbox's loopback /feed — the
// event plane bypasses nginx), the first-subscription choice, and the configured
// box owner (dropbox is single-owner; its events carry no owner).
type consumerCfg struct {
	feedURL string // dropbox's loopback feed (DROPBOX_FEED_URL)
	from    string // first-subscription choice: tail|earliest (WIKI_CONSUMER_FROM)
	owner   string // box owner every wiki/ingest file is filed under (WIKI_OWNER)
}

// resolveConsumerCfg reads wiki's consumer config from the environment. An empty
// feedURL or owner disables the consumer gracefully (the MCP surface still comes
// up) — the same degrade-don't-crash policy as the ingest-disabled path.
func resolveConsumerCfg(getenv func(string) string) consumerCfg {
	return consumerCfg{
		feedURL: envOr(getenv, "DROPBOX_FEED_URL", "http://127.0.0.1:3005/feed"),
		from:    envOr(getenv, "WIKI_CONSUMER_FROM", "tail"),
		owner:   getenv("WIKI_OWNER"),
	}
}

// blockUntilDone parks a deliberately-disabled worker until the serve context is
// cancelled, then returns nil. A disabled consumer must not RETURN early: appkit's
// Workers seam cancels the server when any worker returns (the decision-11 fault
// coupling), so an early return would take the HTTP surface down. Blocking models
// an idle, transport-fault-free worker — the server stays up; a SIGTERM unwinds it.
func blockUntilDone(ctx context.Context) error {
	<-ctx.Done()
	return nil
}

// envOr returns getenv(key) when non-empty, else def.
func envOr(getenv func(string) string, key, def string) string {
	if v := getenv(key); v != "" {
		return v
	}
	return def
}

// envOrInt returns def when key is unset/empty, the parsed value when it holds a
// valid integer, and an error naming the variable otherwise — a malformed
// override fails loudly rather than silently reverting to def.
func envOrInt(getenv func(string) string, key string, def int) (int, error) {
	v := getenv(key)
	if v == "" {
		return def, nil
	}
	n, err := strconv.Atoi(v)
	if err != nil {
		return 0, fmt.Errorf("%s: invalid integer %q", key, v)
	}
	return n, nil
}

// envOrIntOrDefault is the silent variant used only for the manifest ManifestExtra
// (a malformed WIKI_INGEST_MAX_TOKENS surfaces as a real error on serve/migrate via
// resolveDomainCfg; the manifest emit just falls back to the default so `wiki
// manifest` always renders). Distinct name to avoid masking the error-returning one.
func envOrIntOrDefault(getenv func(string) string, key string, def int) int {
	n, err := envOrInt(getenv, key, def)
	if err != nil {
		return def
	}
	return n
}
