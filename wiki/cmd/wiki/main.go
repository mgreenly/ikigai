// Command wiki is the loopback-only domain service behind nginx. It trusts the
// X-Owner-Email / X-Client-Id headers nginx injects after a successful
// auth_request against the dashboard's authorization server, and performs no
// token logic of its own. See internal/server for the auth contract.
//
// This is the scaffold wiki service (Task 2.1): it boots the chassis (config,
// db + migrations, logging, server) and exposes a single MCP tool, wiki_whoami,
// the end-to-end auth proof. The database connection is opened and migrated but
// not yet read by any tool — it is the wired seam where real wiki domain logic
// (the agentic ingest core, the search store) attaches in later phases. wiki is
// NOT an event-plane producer in Phase 1, so there is no outbox / /feed wiring.
package main

import (
	"context"
	"errors"
	"flag"
	"fmt"
	"io"
	"net"
	"os"
	"os/signal"
	"strconv"
	"syscall"
	"time"

	"agentkit/provider"
	"agentkit/provider/anthropic"

	"eventplane/consumer"

	"wiki/internal/ask"
	"wiki/internal/consume"
	"wiki/internal/db"
	"wiki/internal/ingest"
	"wiki/internal/lint"
	"wiki/internal/logging"
	"wiki/internal/mcp"
	"wiki/internal/search"
	"wiki/internal/server"
	"wiki/internal/store"
)

// The event-plane upstream wiki consumes and the stable id it presents on every
// connect (event-protocol.md §7.1). Both are fixed constants — wiki consumes
// exactly dropbox's file-lifecycle feed, and its X-Consumer-Id is the literal
// "wiki". CONSUMES=dropbox in etc/manifest.env mirrors this for the registry.
const (
	upstreamSource = "dropbox"
	consumerID     = "wiki"
)

// version is the product version, overridden at build time via -ldflags.
var version = "dev"

func main() {
	if err := run(os.Args[1:], os.Getenv, os.Stdout, os.Stderr); err != nil {
		fmt.Fprintln(os.Stderr, "wiki:", err)
		os.Exit(1)
	}
}

func run(args []string, getenv func(string) string, stdout, stderr io.Writer) error {
	portDef, err := envOrInt(getenv, "WIKI_PORT", 3006)
	if err != nil {
		return err
	}

	fs := flag.NewFlagSet("wiki", flag.ContinueOnError)
	fs.SetOutput(stderr)
	showVersion := fs.Bool("version", false, "print version and exit")
	// Bind 127.0.0.1 by default and in production: nginx is the only ingress
	// and sets identity headers authoritatively. Binding a public interface
	// would let anyone connect directly and spoof X-Owner-Email — a security
	// defect. The flag exists only so tests/local runs can override deliberately.
	ip := fs.String("ip", envOr(getenv, "WIKI_IP", "127.0.0.1"), "listen address — keep loopback (env: WIKI_IP)")
	port := fs.Int("port", portDef, "listen port (env: WIKI_PORT)")
	logLevel := fs.String("log-level", envOr(getenv, "WIKI_LOG_LEVEL", "info"), "log level: debug|info|warn|error (env: WIKI_LOG_LEVEL)")
	if err := fs.Parse(args); err != nil {
		if errors.Is(err, flag.ErrHelp) {
			return nil
		}
		return err
	}
	if *showVersion {
		fmt.Fprintln(stdout, version)
		return nil
	}

	// WIKI_RESOURCE_ID is this service's canonical resource identifier (must be
	// byte-equal to the `resource` in the PRM doc and the dashboard's token
	// binding). WIKI_AUTH_SERVER is the dashboard authorization-server base URL
	// advertised to clients. Both have local-dev defaults; we resolve them here
	// at the boundary so nothing deeper reads the environment.
	resourceID := envOr(getenv, "WIKI_RESOURCE_ID", "http://localhost:8080/srv/wiki/mcp")
	authServer := envOr(getenv, "WIKI_AUTH_SERVER", "http://localhost:8080")
	// WIKI_DB_PATH is the SQLite database file. db.Open pins SetMaxOpenConns(1)
	// for single-writer discipline; we resolve the path here at the boundary.
	dbPath := envOr(getenv, "WIKI_DB_PATH", "./tmp/wiki.db")

	// Ingest config (PLAN Decision 3 + Task 4.1). All read here at the boundary
	// and threaded down; the inner ingest package is env-free.
	//   WIKI_DATA_ROOT       — filesystem content store root (raw/ + page tree).
	//   ANTHROPIC_API_KEY    — the ingest agent's credential (via SSM app-config
	//                          on the box, .envrc in dev). Presence-checked, but
	//                          its ABSENCE only disables ingest — the service still
	//                          boots and serves wiki_whoami + tools/list.
	//   WIKI_INGEST_MODEL    — ingest model (default claude-sonnet-4-6).
	//   WIKI_INGEST_MAX_TOKENS — per-job output-token / cost ceiling.
	//   WIKI_INGEST_JOB_TTL_SECONDS — per-run wall-clock TTL (0 = no deadline).
	dataRoot := envOr(getenv, "WIKI_DATA_ROOT", "./tmp/data")
	apiKey := getenv("ANTHROPIC_API_KEY")
	ingestModel := envOr(getenv, "WIKI_INGEST_MODEL", ingest.DefaultModel)
	maxTokens, err := envOrInt(getenv, "WIKI_INGEST_MAX_TOKENS", ingest.DefaultMaxTokens)
	if err != nil {
		return err
	}
	ttlSeconds, err := envOrInt(getenv, "WIKI_INGEST_JOB_TTL_SECONDS", 600)
	if err != nil {
		return err
	}

	// Lint config (Task 5.2). Lint reuses the ingest agent/job machinery; its
	// model + cost ceiling + TTL are separate config knobs (PLAN Decision 3) so the
	// maintenance pass can be tuned independently of ingest, but default to the
	// same values. The lint trigger (Linter.Lint) is MANUAL/internal for now — the
	// cadence (manual/scheduled/post-ingest) is DEFERRED per GOALS, so lint is NOT
	// on the public MCP surface (the five Task-5.1 verbs are unchanged).
	lintModel := envOr(getenv, "WIKI_LINT_MODEL", lint.DefaultModel)
	lintMaxTokens, err := envOrInt(getenv, "WIKI_LINT_MAX_TOKENS", lint.DefaultMaxTokens)
	if err != nil {
		return err
	}
	lintTTLSeconds, err := envOrInt(getenv, "WIKI_LINT_JOB_TTL_SECONDS", 600)
	if err != nil {
		return err
	}

	// Ask config (Task 6.2). wiki_ask is the agentic synthesis read: it reuses the
	// ingest agent/job machinery but with a navigation-first toolset (read+glob+grep
	// plus write for the synthesis page only). Its model + cost ceiling + TTL are
	// separate config knobs (PLAN Decision 3) so the synthesis pass can be tuned
	// independently of ingest/lint, but default to the same values. Like ingest it
	// needs ANTHROPIC_API_KEY — its absence only disables wiki_ask.
	askModel := envOr(getenv, "WIKI_ASK_MODEL", ask.DefaultModel)
	askMaxTokens, err := envOrInt(getenv, "WIKI_ASK_MAX_TOKENS", ask.DefaultMaxTokens)
	if err != nil {
		return err
	}
	askTTLSeconds, err := envOrInt(getenv, "WIKI_ASK_JOB_TTL_SECONDS", 600)
	if err != nil {
		return err
	}

	// Event-plane consumer config (Task 6.1). wiki is a notify-shaped consumer of
	// dropbox's file-lifecycle feed for the hardcoded wiki/ingest folder.
	//   DROPBOX_FEED_URL — dropbox's loopback /feed (the upstream-named key,
	//                      event-protocol.md §3). On the box the bin/build wrapper
	//                      resolves it BY NAME via `registry feed-url dropbox`; the
	//                      dev default below is for `go run`/tests without env. An
	//                      empty value DISABLES the consumer (graceful — the MCP
	//                      surface still comes up).
	//   WIKI_OWNER       — the box owner every wiki/ingest file is filed under.
	//                      Dropbox is single-owner and its events carry no owner, so
	//                      the owner is service config, not derived from the event.
	//                      Empty also disables the consumer (it cannot file without
	//                      an owner).
	//   WIKI_CONSUMER_FROM — first-subscription choice (tail|earliest); tail by
	//                      default so a fresh wiki does not re-ingest the whole
	//                      folder backlog on first boot.
	feedURL := envOr(getenv, "DROPBOX_FEED_URL", "http://127.0.0.1:3005/feed")
	consumerOwner := getenv("WIKI_OWNER")
	consumerFrom := envOr(getenv, "WIKI_CONSUMER_FROM", "tail")

	cfg := serveConfig{
		ip:            *ip,
		port:          *port,
		logLevel:      *logLevel,
		resourceID:    resourceID,
		authServer:    authServer,
		dbPath:        dbPath,
		dataRoot:      dataRoot,
		apiKey:        apiKey,
		ingestModel:   ingestModel,
		maxTokens:     maxTokens,
		jobTTL:        time.Duration(ttlSeconds) * time.Second,
		lintModel:     lintModel,
		lintMaxTokens: lintMaxTokens,
		lintJobTTL:    time.Duration(lintTTLSeconds) * time.Second,
		askModel:      askModel,
		askMaxTokens:  askMaxTokens,
		askJobTTL:     time.Duration(askTTLSeconds) * time.Second,
		feedURL:       feedURL,
		consumerOwner: consumerOwner,
		consumerFrom:  consumerFrom,
	}
	return serve(cfg, stdout)
}

// serveConfig is the resolved composition-root configuration. main reads it from
// the environment at the boundary; serve assembles the dependency graph from it.
type serveConfig struct {
	ip          string
	port        int
	logLevel    string
	resourceID  string
	authServer  string
	dbPath        string
	dataRoot      string
	apiKey        string
	ingestModel   string
	maxTokens     int
	jobTTL        time.Duration
	lintModel     string
	lintMaxTokens int
	lintJobTTL    time.Duration
	askModel      string
	askMaxTokens  int
	askJobTTL     time.Duration

	// Event-plane consumer (Task 6.1). feedURL is dropbox's loopback /feed;
	// consumerOwner is the box owner every wiki/ingest file is filed under;
	// consumerFrom is the first-subscription choice. An empty feedURL or
	// consumerOwner disables the consumer (graceful).
	feedURL       string
	consumerOwner string
	consumerFrom  string
}

// serve runs the long-running HTTP server until interrupted. It assembles the
// dependency graph (store, search index, db + job store, the anthropic client
// factory, the ingest core) from cfg and injects the ingest core into the MCP
// handler. The inner packages never read the environment — config flows as args.
func serve(cfg serveConfig, stdout io.Writer) error {
	level, err := logging.ParseLevel(cfg.logLevel)
	if err != nil {
		return err
	}
	logger := logging.New(level, stdout)

	sigCtx, stop := signal.NotifyContext(context.Background(), os.Interrupt, syscall.SIGTERM)
	defer stop()
	// A cancelable child so a STRUCTURAL consumer fault (a missing feed_offset
	// table — a deploy bug) can tear the server down too, mirroring notify: no
	// half-alive (HTTP up / consumer dead) state (event-protocol.md decision 11).
	ctx, cancel := context.WithCancel(sigCtx)
	defer cancel()

	conn, err := db.Open(cfg.dbPath)
	if err != nil {
		return err
	}
	defer conn.Close()
	if err := db.Migrate(ctx, conn); err != nil {
		return fmt.Errorf("migrate: %w", err)
	}

	// Filesystem content store + BM25 search index (the index file lives under the
	// store's per-collection .search/ slot).
	st, err := store.New(cfg.dataRoot)
	if err != nil {
		return fmt.Errorf("store: %w", err)
	}
	idx := search.NewBM25Index(st.SearchIndexPath)
	defer idx.Close()

	// Ingest core. The anthropic client factory closes over the API key + model
	// resolved at this boundary; it is only invoked when a job actually runs, so a
	// missing key disables ingest without blocking boot. We presence-check here and
	// log a clear warning so the operator knows ingest is off (mirrors the suite's
	// fail-loud-at-boot for secrets, softened to "degrade" per Task 4.1: the
	// non-ingest surface must still come up).
	var ingester mcp.Ingester
	var ingestCore *ingest.Core // concrete core, also fed by the event-plane consumer (Task 6.1)
	var linter *lint.Linter
	var asker mcp.Asker
	if cfg.apiKey == "" {
		logger.Warn("ANTHROPIC_API_KEY is not set — ingest and ask are DISABLED (wiki_whoami, wiki_search, and tools/list still work); set it via SSM app-config (box) or .envrc (dev) to enable the agent verbs")
	} else {
		newClient := func() (provider.Client, error) {
			return anthropic.New(cfg.apiKey, cfg.ingestModel)
		}
		core := ingest.New(st, idx, conn, newClient, ingest.Config{
			Model:     cfg.ingestModel,
			MaxTokens: cfg.maxTokens,
			JobTTL:    cfg.jobTTL,
		})
		// Boot-time crash recovery: flip any 'running' rows orphaned by a crash to
		// 'failed' before serving (the runner's per-spawn nature means this is the
		// one place the whole table is swept). Lint jobs live in the same wiki_jobs
		// table, so this one sweep recovers both ingest and lint orphans.
		if n, err := core.Recover(ctx); err != nil {
			return fmt.Errorf("ingest recover: %w", err)
		} else if n > 0 {
			logger.Warn("swept crash-orphaned ingest/lint jobs at boot", "count", n)
		}
		ingester = core
		ingestCore = core

		// Lint maintenance pass (Task 5.2): reuses the same agent/job machinery as
		// ingest, sharing the single-writer DB, the wiki_jobs table, and ingest's
		// per-(owner,collection) flight key (so a lint and an ingest never run
		// concurrently over the same wiki). The trigger (Linter.Lint) is MANUAL and
		// internal for now — cadence (manual/scheduled/post-ingest) is DEFERRED per
		// GOALS, so lint is deliberately NOT a public MCP verb (the surface stays the
		// five Task-5.1 verbs). It is constructed here so it is callable today and
		// schedulable later without re-plumbing.
		linter = lint.New(st, idx, conn, newClient, lint.Config{
			Model:     cfg.lintModel,
			MaxTokens: cfg.lintMaxTokens,
			JobTTL:    cfg.lintJobTTL,
		})

		// Ask synthesis pass (Task 6.2): the agentic, async read behind wiki_ask. It
		// reuses the same agent/job machinery as ingest/lint, shares the single-writer
		// DB, the wiki_jobs table, and ingest's per-(owner,collection) flight key (so an
		// ask never runs concurrently with an ingest/lint over the same wiki — it files
		// a synthesis page and re-indexes, so it is a write-pass). Its client factory
		// closes over the ask-specific model so its cost/latency can be tuned
		// independently of ingest. Like ingest it requires ANTHROPIC_API_KEY, so it is
		// only constructed inside this branch; otherwise wiki_ask returns a clear
		// "ask unavailable" tool-error while the rest of the surface stays up.
		askClient := func() (provider.Client, error) {
			return anthropic.New(cfg.apiKey, cfg.askModel)
		}
		asker = ask.New(st, idx, conn, askClient, ask.Config{
			Model:     cfg.askModel,
			MaxTokens: cfg.askMaxTokens,
			JobTTL:    cfg.askJobTTL,
		})
	}
	// linter is wired and ready (manual/internal trigger); reference it so the
	// composition root keeps it live even though no MCP verb exposes it yet
	// (cadence DEFERRED — Task 5.2). When a cadence is chosen (Phase 7), this is
	// where the scheduler / post-ingest hook attaches.
	_ = linter

	// wiki_search is a SYNCHRONOUS read over the BM25 index — no agent, no key —
	// so it is wired independently of ingest and stays available even when
	// ANTHROPIC_API_KEY is absent (ingest disabled). The *search.BM25Index
	// satisfies mcp.Searcher directly.
	mcpHandler := mcp.NewHandler(ingester, idx, asker)

	addr := net.JoinHostPort(cfg.ip, strconv.Itoa(cfg.port))
	srv, err := server.New(server.Options{
		Addr:       addr,
		Logger:     logger,
		ResourceID: cfg.resourceID,
		AuthServer: cfg.authServer,
		MCP:        mcpHandler,
	})
	if err != nil {
		return err
	}

	// Event-plane consumer (Task 6.1): wiki is a notify-shaped consumer of
	// dropbox's file-lifecycle feed for the hardcoded wiki/ingest folder. It runs
	// ONLY when ingest is enabled (it feeds the same ingest core — no core, nothing
	// to file), a feed URL resolved, and a box owner is configured. Otherwise the
	// service still boots and serves the MCP surface (graceful, mirroring the
	// ingest-disabled path). The consumer feeds Core.Ingest, which spawns the async
	// integration job, so it never blocks the MCP server.
	consumerEnabled := ingestCore != nil && cfg.feedURL != "" && cfg.consumerOwner != ""
	switch {
	case ingestCore == nil:
		logger.Warn("event-plane consumer DISABLED: ingest is off (no ANTHROPIC_API_KEY) — nothing to file dropbox events into")
	case cfg.feedURL == "":
		logger.Warn("event-plane consumer DISABLED: no DROPBOX_FEED_URL (the bin/build wrapper resolves it via `registry feed-url dropbox`)")
	case cfg.consumerOwner == "":
		logger.Warn("event-plane consumer DISABLED: no WIKI_OWNER (dropbox is single-owner; the box owner must be configured to file wiki/ingest events)")
	}

	logger.Info("starting wiki",
		"addr", addr, "resource_id", cfg.resourceID, "auth_server", cfg.authServer,
		"db_path", cfg.dbPath, "data_root", cfg.dataRoot,
		"ingest_model", cfg.ingestModel, "ingest_enabled", cfg.apiKey != "",
		"consumer_enabled", consumerEnabled, "feed_url", cfg.feedURL,
		"consumer_owner", cfg.consumerOwner, "version", version)

	if !consumerEnabled {
		// No consumer: just run the HTTP server, as in earlier phases.
		return server.Run(ctx, srv, logger)
	}

	// Run the server and the consumer concurrently. errCh collects both
	// terminations; the first fatal error cancels ctx so the other unwinds. A
	// structural consumer fault (decision 11) crashes the whole process rather than
	// lingering HTTP-up / consumer-dead; a transport fault (dropbox down) is retried
	// indefinitely inside the engine and never escapes Run.
	consumerCfg := consumer.Config{
		FeedURL:    cfg.feedURL,
		From:       cfg.consumerFrom,
		DB:         conn,
		Source:     upstreamSource,
		ConsumerID: consumerID,
		Logger:     logger,
	}
	handler := consume.Handler(consume.Config{
		Owner:    cfg.consumerOwner,
		Ingester: ingestCore,
		Logger:   logger,
	})

	errCh := make(chan error, 2)
	go func() {
		err := consumer.Run(ctx, consumerCfg, handler)
		if err != nil {
			err = fmt.Errorf("event-plane consumer: %w", err)
		}
		cancel() // bring the server down too — no half-alive state (decision 11)
		errCh <- err
	}()
	go func() {
		errCh <- server.Run(ctx, srv, logger)
	}()

	var firstErr error
	for i := 0; i < 2; i++ {
		if e := <-errCh; e != nil && firstErr == nil {
			firstErr = e
		}
	}
	return firstErr
}

func envOr(getenv func(string) string, key, def string) string {
	if v := getenv(key); v != "" {
		return v
	}
	return def
}

// envOrInt returns def when key is unset/empty, the parsed value when it holds
// a valid integer, and an error naming the variable otherwise — a malformed
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
