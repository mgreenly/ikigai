// Package gmailapp wires gmail's service skeleton into the shared appkit chassis.
package gmailapp

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

// Spec returns the production-shaped appkit service declaration.
func Spec() appkit.Spec {
	// The Gmail client + producer Engine are built once in Handlers (over appkit's
	// shared single-writer DB handle), then shared by the producer-injection hook
	// (which attaches the outbox so mail.* events append atomically with the cursor
	// advance) and the Workers hook (which runs the poll loop on the serve
	// context). appkit calls Handlers -> Producer -> Workers in that order.
	var engine *gm.Engine

	return appkit.Spec{
		App:        "gmail",
		Mount:      "/srv/gmail/",
		Port:       3008,
		MCP:        true,
		Feed:       "/feed", // event-plane producer
		Migrations: db.FS,
		// Events is the static published-event registry -- the three mail.* types
		// the producer emits. It backs both the reflection tool and the outbox's
		// Append-time validation.
		Events: mcp.Events,
		ManifestExtras: []appkit.ManifestKV{
			{Key: "OUTBOX_RETENTION_DAYS", Value: "7"},
			{Key: "OUTBOX_RETENTION_MAX_ROWS", Value: "1000000"},
		},
		// Handlers builds the Gmail client + producer Engine over appkit's shared DB
		// handle, then mounts the normal-mailbox MCP surface behind the
		// nginx-injected identity gate. The three GMAIL_* secrets +
		// GMAIL_POLL_INTERVAL are read here at the boundary and passed into the
		// client/engine -- never logged.
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
		// so every derived mail.* event appends on the same tx as the cursor advance.
		Producer: func(ob *outbox.Outbox) error {
			if engine == nil {
				return fmt.Errorf("gmail: Producer called before Handlers built the engine")
			}
			engine.SetSink(gm.NewOutboxProducer(ob))
			return nil
		},
		// Workers carries gmail's background poll daemon. appkit launches it on the
		// serve context alongside the HTTP server: a SIGTERM cancels both; the engine
		// returns nil on clean ctx cancel and on a dead refresh token. Transient
		// faults are retried on the next tick and never escape Run.
		Workers: []func(context.Context) error{
			func(ctx context.Context) error {
				if engine == nil {
					return fmt.Errorf("gmail: Workers ran before Handlers built the engine")
				}
				return engine.Run(ctx)
			},
		},
	}
}
