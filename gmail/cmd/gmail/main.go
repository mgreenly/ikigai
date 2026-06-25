// Command gmail is the loopback-only Gmail connector + event-plane producer
// behind nginx. It trusts the X-Owner-Email / X-Client-Id headers nginx injects
// after a successful auth_request against the dashboard's authorization server,
// and performs no token logic of its own. See appkit/server for the auth
// contract.
//
// The uniform chassis — the fixed subcommands (serve/version/manifest/migrate/
// backup/restore), config-from-env, the migration runner + downgrade guard, the
// loopback HTTP server + PRM + identity gate, and the /feed producer mount — is
// owned by appkit. main.go declares only gmail's identity (the Spec) and wires
// its domain surface through the Spec hooks. RESOURCE_ID / AUTH_SERVER are
// composed in-binary by appkit/config from IKIGENBA_DOMAIN + MOUNT.
//
// gmail is structurally dropbox's twin (decisions §1): an external-OAuth
// connector with an MCP surface, an internal poll daemon, and an event-plane
// producer half. The History-API producer + poll daemon is wired through
// Producer/Workers (P3); the full normal-mailbox MCP tool set over the P2 client
// is wired through Handlers (P4). The three GMAIL_* secrets + GMAIL_POLL_INTERVAL
// are read here at gmail's own composition root via getenv and never logged;
// appkit never touches them.
package main

import (
	"context"
	"fmt"
	"net/http"
	"os"
	"time"

	"appkit"
	"appkit/config"

	"gmail/internal/db"
	gm "gmail/internal/gmail"
	"gmail/internal/mcp"
	"gmail/internal/web"

	"eventplane/outbox"
)

func main() {
	// The Gmail client + producer Engine are built once in Handlers (over appkit's
	// shared single-writer DB handle), then shared by the producer-injection hook
	// (which attaches the outbox so mail.* events append atomically with the cursor
	// advance) and the Workers hook (which runs the poll loop on the serve
	// context). appkit calls Handlers → Producer → Workers in that order.
	var engine *gm.Engine

	appkit.Main(appkit.Spec{
		App:        "gmail",
		Mount:      "/srv/gmail/",
		Port:       3008,
		MCP:        true,
		Feed:       "/feed", // event-plane producer
		Migrations: db.FS,
		// Events is the static published-event registry — the three mail.* types
		// the producer emits (decisions §1). It backs both the reflection tool and
		// the outbox's Append-time validation.
		Events: mcp.Events,
		ManifestExtras: []appkit.ManifestKV{
			{Key: "OUTBOX_RETENTION_DAYS", Value: "7"},
			{Key: "OUTBOX_RETENTION_MAX_ROWS", Value: "1000000"},
		},
		// Handlers builds the Gmail client + producer Engine over appkit's shared DB
		// handle, then mounts the full normal-mailbox MCP surface (the P2 client wired
		// into the P4 tool set) behind the nginx-injected identity gate. The three
		// GMAIL_* secrets + GMAIL_POLL_INTERVAL are read here at the boundary and
		// passed into the client/engine — NEVER logged.
		Handlers: func(rt *appkit.Router) error {
			conn := rt.DB()
			if conn == nil {
				return fmt.Errorf("gmail: no DB handle on router")
			}

			cfg := gm.Config{
				ClientID:     os.Getenv("GMAIL_CLIENT_ID"),
				ClientSecret: os.Getenv("GMAIL_CLIENT_SECRET"),
				RefreshToken: os.Getenv("GMAIL_REFRESH_TOKEN"),
			}
			client := gm.NewClient(cfg, nil)

			interval, err := config.EnvOrDuration(os.Getenv, "GMAIL_POLL_INTERVAL", 60*time.Second)
			if err != nil {
				return err
			}

			engine = gm.NewEngine(gm.EngineOptions{
				DB:       conn,
				Store:    gm.NewStore(),
				Client:   client,
				Logger:   rt.Logger(),
				Interval: interval,
			})

			rt.Handle("GET /{$}", web.LandingHandler(rt.Service(), rt.Version()))
			rt.Handle("GET /static/", http.StripPrefix("/static/", web.StaticHandler()))
			rt.Handle("POST /mcp", rt.RequireIdentity(
				mcp.NewHandler(client, rt.Version(), rt.Service(), rt.Health(),
					rt.Events(), rt.Subscriptions())))
			return nil
		},
		// Producer fires after Handlers: attach the outbox as the engine's EventSink
		// so every derived mail.* event appends on the same tx as the cursor advance
		// (the "emitted == recorded as emitted" guarantee, decisions §1).
		Producer: func(ob *outbox.Outbox) error {
			if engine == nil {
				return fmt.Errorf("gmail: Producer called before Handlers built the engine")
			}
			engine.SetSink(gm.NewOutboxProducer(ob))
			return nil
		},
		// Workers carries gmail's background poll daemon. appkit launches it on the
		// serve context alongside the HTTP server: a SIGTERM cancels both; the engine
		// returns nil on clean ctx cancel (so it never falsely takes the server down)
		// and on a dead refresh token (logged loudly, decisions §2). Transient faults
		// are retried on the next tick and never escape Run.
		Workers: []func(context.Context) error{
			func(ctx context.Context) error {
				if engine == nil {
					return fmt.Errorf("gmail: Workers ran before Handlers built the engine")
				}
				return engine.Run(ctx)
			},
		},
	})
}
