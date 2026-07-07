// Command cron is the loopback-only scheduled-event-emitter service behind
// nginx. It serves a bearer-gated MCP surface for agents and a
// dashboard-session-cookie-gated human web landing page under /srv/cron/. It
// trusts the X-Owner-Email / X-Client-Id headers nginx injects after a
// successful auth_request against the dashboard's authorization server, and
// performs no token logic of its own; nginx remains the sole trust boundary for
// both doors.
//
// The uniform chassis - the fixed subcommands, config-from-env, the migration
// runner + downgrade guard, the loopback HTTP server + PRM + identity gate, and
// the /feed producer mount - is owned by appkit. main.go declares cron's
// appkit.Spec inline, including registry.MustPort("cron"), Migrations, WWW, the
// dynamic Publishes provider, and the tick Producer/Workers. The human landing
// page is served from the on-disk share/www tree through the chassis
// (Spec.WWW/rt.WWW()), and the MCP surface is the internal/mcp tool table
// (Instructions + Tools(store) + NewHandler) over the shared appkit/mcp
// transport. RESOURCE_ID / AUTH_SERVER are composed in-binary by appkit/config
// from IKIGENBA_DOMAIN + MOUNT.
package main

import (
	"context"
	"fmt"
	"log/slog"
	"net/http"

	"appkit"
	"appkit/web"

	"cron/internal/crontab"
	"cron/internal/db"
	"cron/internal/event"
	"cron/internal/mcp"
	"cron/internal/tick"

	"eventplane/outbox"
	"registry"
)

func main() {
	appkit.Main(cronSpec())
}

func cronSpec() appkit.Spec {
	var store *crontab.Store
	var worker *tick.Worker

	return appkit.Spec{
		App:        "cron",
		Mount:      "/srv/cron/",
		Port:       registry.MustPort("cron"),
		MCP:        true,
		WWW:        true,
		Feed:       "/feed",
		Migrations: db.FS,
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
		Handlers: func(rt *appkit.Router) error {
			conn := rt.DB()
			if conn == nil {
				return fmt.Errorf("cron: no DB handle on router")
			}
			store = crontab.NewStore(conn)
			rt.Handle("GET /{$}", landingHandler(rt.WWW(), rt.Service(), rt.Version()))
			mcpHandler, err := mcp.NewHandler(store, rt)
			if err != nil {
				return err
			}
			rt.Handle("POST /mcp", rt.RequireIdentity(mcpHandler))
			return nil
		},
		Producer: func(ob *outbox.Outbox) error {
			if store == nil {
				return fmt.Errorf("cron: Producer called before Handlers built the Store")
			}
			worker = tick.New(store.DB(), store, ob, slog.Default())
			return nil
		},
		Workers: []func(context.Context) error{
			func(ctx context.Context) error {
				if worker == nil {
					return fmt.Errorf("cron: tick worker not wired (Producer did not run)")
				}
				return worker.Run(ctx)
			},
		},
	}
}

func landingHandler(site *web.Site, service, version string) http.Handler {
	return http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		if r.URL.Path != "/" {
			http.NotFound(w, r)
			return
		}
		_ = site.Render(w, "landing.html",
			struct{ Service, Version string }{service, version})
	})
}
