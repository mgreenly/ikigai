// Command ledger is the loopback-only double-entry bookkeeping service behind
// nginx. It trusts the X-Owner-Email / X-Client-Id headers nginx injects after a
// successful auth_request against the dashboard's authorization server, and
// performs no token logic of its own.
//
// The uniform chassis — the fixed subcommands (serve/version/manifest/migrate/
// schema), config-from-env, the migration runner + downgrade guard, the
// loopback HTTP server + PRM + identity gate, and the /feed producer mount — is
// owned by appkit. main.go declares only ledger's identity (the Spec) and wires
// its domain surface (the ledger_* MCP tools and the transaction.recorded
// producer) through the Spec hooks. RESOURCE_ID / AUTH_SERVER are composed
// in-binary by appkit/config from IKIGENBA_DOMAIN + MOUNT (was the deleted
// bin/build run-wrapper's job).
package main

import (
	"fmt"
	"net/http"

	"appkit"
	"appkit/web"

	"ledger/internal/db"
	"ledger/internal/ledger"
	"ledger/internal/mcp"

	"eventplane/outbox"
	"registry"
)

func main() {
	appkit.Main(ledgerSpec())
}

func ledgerSpec() appkit.Spec {
	// The domain Service is built once and shared by the producer-injection hook
	// (which attaches the outbox so transaction.recorded events append atomically
	// with the journal write) and the route hook (which mounts the ledger_* MCP
	// surface over it). Both close over svc; appkit calls Producer before Handlers
	// when ledger is a producer (Spec.Feed != "").
	var svc *ledger.Service

	return appkit.Spec{
		App:        "ledger",
		Mount:      "/srv/ledger/",
		Port:       registry.MustPort("ledger"),
		MCP:        true,
		WWW:        true,
		Feed:       "/feed", // event-plane producer
		Migrations: db.FS,
		Events:     ledger.Events, // published event types: reflection + Append validation
		ManifestExtras: []appkit.ManifestKV{
			{Key: "OUTBOX_RETENTION_DAYS", Value: "7"},
			{Key: "OUTBOX_RETENTION_MAX_ROWS", Value: "1000000"},
		},
		// Producer fires first: build the Service over appkit's shared DB handle
		// and inject the outbox so every committed transaction emits exactly one
		// transaction.recorded on the same tx (the payload builders stay app-side
		// in internal/ledger/events.go).
		Producer: func(ob *outbox.Outbox) error {
			if svc == nil {
				return fmt.Errorf("ledger: Producer called before DB available")
			}
			svc.Outbox = ledger.NewOutboxProducer(ob)
			return nil
		},
		// Handlers mounts the ledger_* MCP surface, gated behind nginx-injected
		// identity, over the same Service the producer hook injected the outbox into.
		Handlers: func(rt *appkit.Router) error {
			conn := rt.DB()
			if conn == nil {
				return fmt.Errorf("ledger: no DB handle on router")
			}
			svc = ledger.NewService(conn)

			rt.Handle("GET /{$}", landingHandler(rt.WWW(), rt.Service(), rt.Version()))

			rt.Handle("POST /mcp", rt.RequireIdentity(
				mcp.NewHandler(svc, rt.Version(), rt.Service(), rt.Health(),
					rt.Events(), rt.Subscriptions())))
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
