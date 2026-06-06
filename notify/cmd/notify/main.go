// Command notify is the loopback-only domain service behind nginx. It trusts the
// X-Owner-Email / X-Client-Id headers nginx injects after a successful
// auth_request against the dashboard's authorization server, and performs no
// token logic of its own. See appkit/server for the auth contract.
//
// notify is the suite's first event-plane CONSUMER. The uniform chassis — the
// fixed subcommands (serve/version/manifest/migrate/backup/restore), config-
// from-env, the migration runner + downgrade guard, the loopback HTTP server +
// PRM + identity gate, and the serve lifecycle — is owned by appkit. main.go
// declares only notify's identity (the Spec) and wires its domain surface: the
// ikigenba_notify_health MCP tool and, through the appkit Workers seam, the background
// event-plane consumer loop (eventplane/consumer) that subscribes to crm's
// east/west /feed and fires a best-effort ntfy.sh push (internal/push) on every
// contact.created.
//
// The HTTP server and the consumer worker share appkit's serve context: a
// SIGTERM cancels both; a STRUCTURAL consumer fault (consumer.Run escaping on a
// missing feed_offset table — a deploy bug, event-protocol.md decision 11)
// returns from the worker, which cancels the serve context and brings the server
// down too — no half-alive HTTP-up / consumer-dead state. A transport fault (crm
// down) is retried indefinitely inside the engine and never returns, so it never
// takes the service down.
package main

import (
	"context"
	"errors"
	"fmt"
	"os"

	"appkit"

	"eventplane/consumer"

	"notify/internal/db"
	"notify/internal/mcp"
	"notify/internal/push"
)

// The event-plane upstream notify consumes and the stable id it presents on every
// connect (event-protocol.md §7.1; decision 10). Both are fixed constants — notify
// consumes exactly crm's feed, and its X-Consumer-Id is the literal "notify".
// CONSUMES=crm in etc/manifest.env mirrors upstreamSource for the registry.
const (
	upstreamSource = "crm"
	consumerID     = "notify"
)

func main() {
	// rt is captured by the Handlers hook (which appkit runs after it opens +
	// migrates the shared single-writer DB) so the consumer worker can read the DB
	// handle and the server's logger. The worker runs strictly after the server is
	// built, so the capture is always set by the time it executes.
	var rt *appkit.Router

	appkit.Main(appkit.Spec{
		App:        "notify",
		Mount:      "/srv/notify/",
		Port:       3003,
		MCP:        true,
		Consumes:   []string{upstreamSource}, // event-plane consumer → CONSUMES=crm
		// Subscriptions is the LIVE provider the reflection tool reports (mirrors
		// Spec.Health). notify is a static consumer, so it returns the fixed list of
		// its one declared in-edge — the SAME push.Subscription() the consumer
		// Handler matches against, so the runtime filter and reflection cannot drift
		// (decision 10). Spec.Consumes stays the separate build-time envelope.
		Subscriptions: func() []consumer.Subscription {
			return []consumer.Subscription{push.Subscription()}
		},
		Migrations: db.FS,
		// Handlers mounts the ikigenba_notify_health MCP surface (gated behind
		// nginx-injected identity) and records the Router so the consumer worker below
		// can reach the shared DB handle and logger.
		Handlers: func(r *appkit.Router) error {
			rt = r
			rt.Handle("POST /mcp", rt.RequireIdentity(
				mcp.NewHandler(rt.Version(), rt.Service(), rt.Health(),
					rt.Events(), rt.Subscriptions())))
			return nil
		},
		// Workers carries notify's event-plane consumer loop. appkit launches it on
		// the serve context alongside the HTTP server (the Workers seam, E2). The
		// consumer Config + push Handler stay app-side — appkit owns the lifecycle,
		// not the event semantics. The ntfy secrets are read at notify's own
		// composition root (resolveConsumerCfg → os.Getenv); appkit never reads or
		// logs a secret (§2.8).
		Workers: []func(context.Context) error{
			func(ctx context.Context) error {
				return runConsumer(ctx, rt)
			},
		},
	})
}

// runConsumer is the event-plane consumer worker. It resolves the consumer +
// ntfy config from the environment (failing loudly if a required secret is
// absent), then drives eventplane/consumer.Run over crm's /feed until ctx is
// cancelled (clean shutdown → nil) or a structural fault escapes (→ error, which
// appkit propagates to cancel the server too — decision 11).
func runConsumer(ctx context.Context, rt *appkit.Router) error {
	cfg, err := resolveConsumerCfg(os.Getenv)
	if err != nil {
		return err
	}
	logger := rt.Logger()
	pushClient := push.NewClient(cfg.ntfyBase, cfg.ntfyTopic, cfg.ntfyToken, logger)
	consumerCfg := consumer.Config{
		FeedURL:    cfg.feedURL,
		From:       cfg.from,
		DB:         rt.DB(),
		Source:     upstreamSource,
		ConsumerID: consumerID,
		Logger:     logger,
	}
	// NOTE: ntfy topic/token are deliberately omitted from any log line — they are
	// secrets (§2.8). feed_url/from are safe non-secret config.
	logger.Info("starting notify consumer",
		"feed_url", cfg.feedURL, "from", cfg.from, "ntfy_base", cfg.ntfyBase)
	if err := consumer.Run(ctx, consumerCfg, push.Handler(pushClient, logger)); err != nil {
		return fmt.Errorf("event-plane consumer: %w", err)
	}
	return nil
}

// consumerCfg is notify's event-plane + ntfy push configuration, read once at the
// composition root. The plain config (feed URL, first-subscription choice, ntfy
// base) carries dev fallbacks; the ntfy topic and token are deployment SECRETS
// injected via the environment (.envrc locally; app-config in prod) and never
// read from source.
type consumerCfg struct {
	feedURL string // crm's loopback feed (CRM_FEED_URL)
	from    string // first-subscription choice: tail|earliest (NOTIFY_FROM)

	ntfyBase  string // NOTIFY_NTFY_BASE_URL — plain config (tests/dev point at a mock)
	ntfyTopic string // NTFY_TOPIC — SECRET
	ntfyToken string // NTFY_API_KEY — SECRET
}

// resolveConsumerCfg reads notify's consumer/push config from the environment and
// fails loudly if a required secret is absent — the push hop cannot work without
// its topic and key, and silently degrading would hide a misconfigured deploy.
func resolveConsumerCfg(getenv func(string) string) (consumerCfg, error) {
	cfg := consumerCfg{
		// CRM_FEED_URL is crm's loopback feed. The event plane bypasses nginx, so
		// this is a direct 127.0.0.1 address; the dev fallback is only for
		// `go run`/tests without env.
		feedURL: envOr(getenv, "CRM_FEED_URL", "http://127.0.0.1:3001/feed"),
		// NOTIFY_FROM is the first-subscription choice; tail by default so a fresh
		// notify only pushes for contacts created from now on.
		from:      envOr(getenv, "NOTIFY_FROM", "tail"),
		ntfyBase:  envOr(getenv, "NOTIFY_NTFY_BASE_URL", "https://ntfy.sh"),
		ntfyTopic: getenv("NTFY_TOPIC"),
		ntfyToken: getenv("NTFY_API_KEY"),
	}
	if cfg.ntfyTopic == "" {
		return consumerCfg{}, errors.New("NTFY_TOPIC is required (inject via .envrc from ~/.secrets/NTFY_TOPIC)")
	}
	if cfg.ntfyToken == "" {
		return consumerCfg{}, errors.New("NTFY_API_KEY is required (inject via .envrc from ~/.secrets/NTFY_API_KEY)")
	}
	return cfg, nil
}

// envOr returns getenv(key) when non-empty, else def.
func envOr(getenv func(string) string, key, def string) string {
	if v := getenv(key); v != "" {
		return v
	}
	return def
}
