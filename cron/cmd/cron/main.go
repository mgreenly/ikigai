// Command cron is the loopback-only scheduled-event-emitter service behind
// nginx. Under /srv/cron/ it serves a bearer-gated MCP surface for agents
// alongside a dashboard-session-cookie-gated human web landing page. It trusts
// the X-Owner-Email / X-Client-Id headers nginx injects after a successful
// auth_request against the dashboard's authorization server, and performs no
// token logic of its own; nginx remains the sole trust boundary for both doors.
//
// The uniform chassis — the fixed subcommands (serve/version/manifest/migrate/
// backup/restore), config-from-env, the migration runner + downgrade guard, the
// loopback HTTP server + PRM + identity gate, and the /feed producer mount — is
// owned by appkit. main.go declares only cron's identity (the Spec) and wires its
// domain surface: the crontab CRUD MCP tools, the minute-aligned tick worker that
// emits cron.<name> events, and the LIVE Publishes provider that reports those
// types from the crontab. RESOURCE_ID / AUTH_SERVER are composed in-binary by
// appkit/config from IKIGENBA_DOMAIN + MOUNT.
package main

import (
	"context"
	"fmt"
	"log/slog"

	"appkit"

	"cron/internal/crontab"
	"cron/internal/db"
	"cron/internal/event"
	"cron/internal/mcp"
	"cron/internal/tick"
	"cron/internal/web"

	"eventplane/outbox"
)

func main() {
	// The crontab Store and the tick Worker are built across the chassis hooks and
	// shared by closure: Handlers builds the Store over rt.DB() and mounts the MCP
	// surface + the live Publishes provider over it; Producer (called AFTER
	// Handlers, once appkit has constructed the outbox) builds the tick Worker over
	// the same DB handle, Store, and the injected outbox; the Workers entry runs
	// that Worker for the serve lifetime.
	var store *crontab.Store
	var worker *tick.Worker

	appkit.Main(appkit.Spec{
		App:        "cron",
		Mount:      "/srv/cron/",
		Port:       3007,
		MCP:        true,
		Feed:       "/feed", // event-plane producer
		Migrations: db.FS,
		// Events is intentionally empty: cron's published types are dynamic
		// (cron.<name>, computed from the crontab), so there is NO Append-time
		// registry guard — validity rides on the DB CHECK (decisions §2). The live
		// types are reported via Publishes below.
		Publishes: func() outbox.Registry {
			if store == nil {
				return outbox.Registry{}
			}
			return event.Publishes(store)()
		},
		ManifestExtras: []appkit.ManifestKV{
			{Key: "OUTBOX_RETENTION_DAYS", Value: "7"},
			{Key: "OUTBOX_RETENTION_MAX_ROWS", Value: "1000000"},
		},
		// Handlers builds the crontab Store over appkit's shared single-writer DB
		// handle and mounts the ikigenba_cron_* MCP surface behind the nginx-injected
		// identity gate. The same Store is reused by the producer hook and the
		// Publishes provider.
		Handlers: func(rt *appkit.Router) error {
			conn := rt.DB()
			if conn == nil {
				return fmt.Errorf("cron: no DB handle on router")
			}
			store = crontab.NewStore(conn)
			rt.Handle("GET /{$}", web.LandingHandler(rt.Service(), rt.Version()))
			rt.Handle("GET /static/", web.StaticHandler())
			rt.Handle("POST /mcp", rt.RequireIdentity(
				mcp.NewHandler(store, rt.Version(), rt.Service(), rt.Health(),
					rt.Publishes(), rt.Subscriptions())))
			return nil
		},
		// Producer fires after Handlers: build the tick Worker over the DB handle,
		// the Store, and the injected outbox. The Worker Appends cron.<name> events
		// and Rings the feed.
		Producer: func(ob *outbox.Outbox) error {
			if store == nil {
				return fmt.Errorf("cron: Producer called before Handlers built the Store")
			}
			worker = tick.New(store.DB(), store, ob, slog.Default())
			return nil
		},
		// Workers runs the tick loop for the serve lifetime. It is a thin shim over
		// the worker built in Producer (which runs before Workers start), so the
		// closure capture is resolved by the time Run is invoked.
		Workers: []func(context.Context) error{
			func(ctx context.Context) error {
				if worker == nil {
					return fmt.Errorf("cron: tick worker not wired (Producer did not run)")
				}
				return worker.Run(ctx)
			},
		},
	})
}
