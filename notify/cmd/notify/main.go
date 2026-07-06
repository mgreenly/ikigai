// Command notify is the loopback-only domain service behind nginx. It trusts the
// X-Owner-Email / X-Client-Id headers nginx injects after a successful
// auth_request against the dashboard's authorization server, and performs no
// token logic of its own. See appkit/server for the auth contract.
//
// notify is the suite's first event-plane CONSUMER. The uniform chassis — the
// fixed subcommands (serve/version/manifest/migrate/schema), config-
// from-env, the migration runner + downgrade guard, the loopback HTTP server +
// PRM + identity gate, and the serve lifecycle — is owned by appkit. main.go
// declares only notify's identity (the Spec) and wires its domain surface: the
// health MCP tool and the event-plane consumer entries that subscribe to crm and
// prompts feeds and fire best-effort ntfy.sh pushes (internal/push).
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
	"errors"
	"net/http"
	"os"

	"appkit"
	"appkit/web"

	"eventplane/consumer"

	"notify/internal/db"
	"notify/internal/mcp"
	"notify/internal/push"

	"registry"
)

func main() {
	appkit.Main(notifySpec())
}

func notifySpec() appkit.Spec {
	return appkit.Spec{
		App:   "notify",
		Mount: "/srv/notify/",
		Port:  registry.MustPort("notify"),
		MCP:   true,
		WWW:   true,
		Consumers: []appkit.Consumer{
			{
				Source:        "crm",
				Subscriptions: []consumer.Subscription{push.Subscription()},
				Handler: func(rt *appkit.Router) consumer.Handler {
					cfg, err := mustNtfyCfg(os.Getenv)
					if err != nil {
						panic(err)
					}
					client := push.NewClient(cfg.ntfyBase, cfg.ntfyTopic, cfg.ntfyToken, rt.Logger())
					return push.Handler(client, rt.Logger())
				},
			},
			{
				Source:        "prompts",
				Subscriptions: push.PromptsSubscriptions(),
				Handler: func(rt *appkit.Router) consumer.Handler {
					cfg, err := mustNtfyCfg(os.Getenv)
					if err != nil {
						panic(err)
					}
					client := push.NewClient(cfg.ntfyBase, cfg.ntfyTopic, cfg.ntfyToken, rt.Logger())
					return push.PromptsHandler(client, rt.Logger())
				},
			},
		},
		Migrations: db.FS,
		// Handlers mounts the health MCP surface (gated behind
		// nginx-injected identity) plus notify's landing route.
		Handlers: func(r *appkit.Router) error {
			// The MCP send verb publishes through a push client built here at the
			// composition root, reusing the same ntfy config (base/topic/token) the
			// consumer entries resolve. mustNtfyCfg fails loudly if a secret is absent,
			// so a misconfigured deploy never silently disables send.
			cfg, err := mustNtfyCfg(os.Getenv)
			if err != nil {
				return err
			}
			pushClient := push.NewClient(cfg.ntfyBase, cfg.ntfyTopic, cfg.ntfyToken, r.Logger())
			r.Handle("GET /{$}", landingHandler(r.WWW(), r.Service(), r.Version()))
			handler, err := mcp.NewHandler(pushClient, r)
			if err != nil {
				return err
			}
			r.Handle("POST /mcp", r.RequireIdentity(handler))
			return nil
		},
	}
}

func landingHandler(site *web.Site, service, version string) http.Handler {
	return http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		_ = site.Render(w, "landing.html",
			struct{ Service, Version string }{service, version})
	})
}

// ntfyCfg is notify's push configuration, read once at the composition root. The
// ntfy base carries a dev fallback; topic and token are deployment SECRETS
// injected via the environment (.envrc locally; app-config in prod).
type ntfyCfg struct {
	ntfyBase  string // NOTIFY_NTFY_BASE_URL — plain config (tests/dev point at a mock)
	ntfyTopic string // NTFY_TOPIC — SECRET
	ntfyToken string // NTFY_API_KEY — SECRET
}

// mustNtfyCfg reads notify's push config from the environment and fails loudly if
// a required secret is absent — the push hop cannot work without its topic and
// key, and silently degrading would hide a misconfigured deploy.
func mustNtfyCfg(getenv func(string) string) (ntfyCfg, error) {
	cfg := ntfyCfg{
		ntfyBase:  envOr(getenv, "NOTIFY_NTFY_BASE_URL", "https://ntfy.sh"),
		ntfyTopic: getenv("NTFY_TOPIC"),
		ntfyToken: getenv("NTFY_API_KEY"),
	}
	if cfg.ntfyTopic == "" {
		return ntfyCfg{}, errors.New("NTFY_TOPIC is required (inject via .envrc from ~/.secrets/NTFY_TOPIC)")
	}
	if cfg.ntfyToken == "" {
		return ntfyCfg{}, errors.New("NTFY_API_KEY is required (inject via .envrc from ~/.secrets/NTFY_API_KEY)")
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
