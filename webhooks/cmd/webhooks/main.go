// Command webhooks is the loopback-only inbound-webhook service behind nginx. The
// owner-facing MCP surface is the appkit/mcp tool table reached through the
// front-door auth chain: nginx introspects each /srv/webhooks/ request via
// auth_request against the dashboard's authorization server and injects
// X-Owner-Email / X-Client-Id authoritatively, so the MCP handler runs behind the
// chassis identity gate and performs no token logic of its own.
//
// The one exception is the public ingress endpoint POST /in/<name>: it is reached
// directly by third parties with NO front-door auth chain, so it self-guards on a
// per-webhook secret and is mounted bare (not behind RequireIdentity).
//
// The uniform chassis — the fixed subcommands (serve/version/manifest/migrate/
// schema), config-from-env, the migration runner + downgrade guard, the
// loopback HTTP server + PRM + identity gate, and the /feed producer mount — is
// owned by appkit. main.go declares only webhooks's identity (the Spec) and wires
// its domain surface (the appkit/mcp tools, the share/www web surface, the public
// ingress, and the received-event producer) through the Spec hooks.
package main

import (
	"fmt"
	"net/http"

	"appkit"
	"appkit/web"
	"registry"

	"webhooks/internal/db"
	"webhooks/internal/mcp"
	"webhooks/internal/webhooks"

	"eventplane/outbox"
)

func main() { appkit.Main(webhooksSpec()) }

func webhooksSpec() appkit.Spec {
	// The domain Service is built once and shared by the route hook (which mounts
	// the gated MCP surface and the bare public ingress over it) and the producer-
	// injection hook (which attaches the outbox so received-event appends append
	// atomically with the domain write). Both close over svc; appkit calls Producer
	// after Handlers when webhooks is a producer (Spec.Feed != "").
	var svc *webhooks.Service

	return appkit.Spec{
		App:        "webhooks",
		Mount:      "/srv/webhooks/",
		Port:       registry.MustPort("webhooks"),
		MCP:        true,
		Feed:       "/feed", // event-plane producer
		Migrations: db.FS,
		Events:     webhooks.Events, // published event types: reflection + Append validation
		WWW:        true,
		ManifestExtras: []appkit.ManifestKV{
			{Key: "OUTBOX_RETENTION_DAYS", Value: "7"},
			{Key: "OUTBOX_RETENTION_MAX_ROWS", Value: "1000000"},
		},
		// Handlers builds the domain Service over the chassis's shared
		// single-writer DB handle and mounts both surfaces: the MCP tools gated
		// behind nginx-injected identity, and the public ingress reached bare.
		Handlers: func(rt *appkit.Router) error {
			conn := rt.DB()
			if conn == nil {
				return fmt.Errorf("webhooks: no DB handle on router")
			}
			svc = webhooks.NewService(conn, webhooks.RealClock{})
			handler, err := mcp.NewHandler(svc, rt)
			if err != nil {
				return err
			}
			rt.Handle("POST /mcp", rt.RequireIdentity(handler))
			rt.Handle("/in/", webhooks.NewIngressHandler(svc, rt.Logger()))
			rt.Handle("GET /{$}", landingHandler(rt.WWW(), rt.Service(), rt.Version()))
			return nil
		},
		// Producer fires after Handlers: inject the outbox so every committed
		// inbound webhook emits its received event on the same tx.
		Producer: func(ob *outbox.Outbox) error {
			if svc == nil {
				return fmt.Errorf("webhooks: Producer called before Handlers built the Service")
			}
			svc.Outbox = ob
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
