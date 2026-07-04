// Command webhooks is the loopback-only inbound-webhook service behind nginx. The
// owner-facing MCP surface (create / list / delete / rotate) is reached through
// the front-door auth chain — nginx introspects each /srv/webhooks/ request via
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
// its domain surface (the ikigenba_webhooks_* MCP tools, the public ingress, and
// the webhook.received producer) through the Spec hooks.
package main

import (
	"fmt"

	"appkit"

	"webhooks/internal/db"
	"webhooks/internal/mcp"
	"webhooks/internal/web"
	"webhooks/internal/webhooks"

	"eventplane/outbox"
)

func main() {
	// The domain Service is built once and shared by the route hook (which mounts
	// the gated MCP surface and the bare public ingress over it) and the producer-
	// injection hook (which attaches the outbox so webhook.received appends append
	// atomically with the domain write). Both close over svc; appkit calls Producer
	// after Handlers when webhooks is a producer (Spec.Feed != "").
	var svc *webhooks.Service

	spec := webhooksSpec()
	// Handlers builds the domain Service over the chassis's shared single-writer
	// DB handle and mounts both surfaces: the ikigenba_webhooks_* MCP tools gated
	// behind nginx-injected identity, and the public ingress reached bare.
	spec.Handlers = func(rt *appkit.Router) error {
		conn := rt.DB()
		if conn == nil {
			return fmt.Errorf("webhooks: no DB handle on router")
		}
		svc = webhooks.NewService(conn, webhooks.RealClock{})
		rt.Handle("POST /mcp", rt.RequireIdentity(
			mcp.NewHandler(svc, rt.Version(), rt.Service(), rt.ResourceID(),
				rt.Health(), rt.Events())))
		rt.Handle("/in/", webhooks.NewIngressHandler(svc, rt.Logger()))
		rt.Handle("GET /{$}", web.LandingHandler(rt.Service(), rt.Version()))
		rt.Handle("GET /static/", web.StaticHandler())
		return nil
	}
	// Producer fires after Handlers: inject the outbox so every committed inbound
	// webhook emits its webhook.received event on the same tx.
	spec.Producer = func(ob *outbox.Outbox) error {
		if svc == nil {
			return fmt.Errorf("webhooks: Producer called before Handlers built the Service")
		}
		svc.Outbox = ob
		return nil
	}

	appkit.Main(spec)
}

func webhooksSpec() appkit.Spec {
	return appkit.Spec{
		App:        "webhooks",
		Mount:      "/srv/webhooks/",
		Port:       3006,
		MCP:        true,
		Feed:       "/feed", // event-plane producer
		Migrations: db.FS,
		Events:     webhooks.Events, // published event types: reflection + Append validation
		ManifestExtras: []appkit.ManifestKV{
			{Key: "OUTBOX_RETENTION_DAYS", Value: "7"},
			{Key: "OUTBOX_RETENTION_MAX_ROWS", Value: "1000000"},
		},
	}
}
