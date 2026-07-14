// Command gmail is the loopback-only Gmail connector + event-plane producer
// behind nginx. It trusts the X-Owner-Email / X-Client-Id headers nginx injects
// after a successful auth_request against the dashboard's authorization server,
// and performs no token logic of its own. See appkit/server for the auth
// contract.
//
// The uniform chassis — the fixed subcommands (serve/version/manifest/migrate/
// schema), config-from-env, the migration runner + downgrade guard, the
// loopback HTTP server + PRM + identity gate, and the /feed producer mount — is
// owned by appkit. main.go declares only gmail's identity (the Spec) and wires
// its domain surface through the Spec hooks. RESOURCE_ID / AUTH_SERVER are
// composed in-binary by appkit/config from IKIGENBA_DOMAIN + MOUNT.
//
// gmail is structurally dropbox's twin (decisions §1): an external-OAuth
// connector with an MCP surface, an internal poll daemon, and an event-plane
// producer half. The History-API producer + poll daemon is wired through
// Producer/Workers (P3); the full normal-mailbox MCP tool set over the P2 client
// is wired through Handlers (P4) in gmailSpec(). The three GMAIL_* secrets +
// GMAIL_POLL_INTERVAL are read there at gmail's own composition root via getenv
// and never logged; appkit never touches them.
package main

import (
	"context"
	"fmt"
	"net/http"
	"os"
	"strconv"
	"time"

	"appkit"
	"appkit/config"
	"appkit/web"

	"gmail/internal/db"
	gm "gmail/internal/gmail"
	"gmail/internal/mcp"

	"eventplane/outbox"
	"registry"
)

func main() {
	appkit.Main(gmailSpec())
}

// gmailSpec returns the production-shaped appkit service declaration.
func gmailSpec() appkit.Spec {
	// The Gmail client + producer Engine are built once in Handlers (over appkit's
	// shared single-writer DB handle), then shared by the producer-injection hook
	// (which attaches the outbox so mail.* events append atomically with the cursor
	// advance) and the Workers hook (which runs the poll loop on the serve
	// context). appkit calls Handlers -> Producer -> Workers in that order.
	var engine *gm.Engine

	return appkit.Spec{
		App:        "gmail",
		Mount:      "/srv/gmail/",
		Port:       registry.MustPort("gmail"),
		MCP:        true,
		WWW:        true,
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
			port, err := config.EnvOrInt(os.Getenv, "GMAIL_PORT", registry.MustPort("gmail"))
			if err != nil {
				return err
			}
			ip := config.EnvOr(os.Getenv, "GMAIL_IP", "127.0.0.1")
			contentBase := "http://" + ip + ":" + strconv.Itoa(port)

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

			rt.HandleLoopback("GET /attachment", gm.AttachmentHandler(client))
			rt.Handle("GET /{$}", landingHandler(rt.WWW(), rt.Service(), rt.Version()))
			handler, err := mcp.NewHandler(client, contentBase, rt)
			if err != nil {
				return err
			}
			rt.Handle("POST /mcp", rt.RequireIdentity(handler))
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

func landingHandler(site *web.Site, service, version string) http.Handler {
	return http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		_ = site.Render(w, "landing.html",
			struct{ Service, Version string }{service, version})
	})
}
