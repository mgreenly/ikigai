// Command crm is the loopback-only sales-CRM service behind nginx. It trusts the
// X-Owner-Email / X-Client-Id headers nginx injects after a successful
// auth_request against the dashboard's authorization server, and performs no
// token logic of its own.
//
// The uniform chassis — the fixed subcommands (serve/version/manifest/migrate/
// schema), config-from-env, the migration runner + downgrade guard, the
// loopback HTTP server + PRM + identity gate, and the /feed producer mount — is
// owned by appkit. main.go declares only crm's identity (the Spec) and wires its
// domain surface (the ikigenba_crm_* MCP tools and the contact.* producer) through the
// Spec hooks. RESOURCE_ID / AUTH_SERVER are composed in-binary by appkit/config
// from IKIGENBA_DOMAIN + MOUNT (was the deleted bin/build run-wrapper's job).
package main

import (
	"fmt"
	"net/http"

	"appkit"

	"crm/internal/crm"
	"crm/internal/db"
	"crm/internal/mcp"

	"eventplane/outbox"
)

func main() {
	// The domain Service is built once and shared by the producer-injection hook
	// (which attaches the outbox so contact.* events append atomically with the
	// domain write) and the route hook (which mounts the ikigenba_crm_* MCP surface over
	// it). Both close over svc; appkit calls Producer after Handlers when crm is a
	// producer (Spec.Feed != "").
	var svc *crm.Service

	appkit.Main(appkit.Spec{
		App:        "crm",
		Mount:      "/srv/crm/",
		Port:       3100,
		MCP:        true,
		WWW:        true,
		Feed:       "/feed", // event-plane producer
		Migrations: db.FS,
		Events:     crm.Events, // published event types: reflection + Append validation
		ManifestExtras: []appkit.ManifestKV{
			{Key: "OUTBOX_RETENTION_DAYS", Value: "7"},
			{Key: "OUTBOX_RETENTION_MAX_ROWS", Value: "1000000"},
		},
		// Handlers mounts the ikigenba_crm_* MCP surface, gated behind nginx-injected
		// identity, over the Service built on appkit's shared single-writer DB
		// handle. The same Service is reused by the producer hook below.
		Handlers: func(rt *appkit.Router) error {
			conn := rt.DB()
			if conn == nil {
				return fmt.Errorf("crm: no DB handle on router")
			}
			svc = crm.NewService(conn)
			rt.Handle("GET /{$}", http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
				if r.URL.Path != "/" {
					http.NotFound(w, r)
					return
				}
				if err := rt.WWW().Render(w, "landing.html", struct {
					Service string
					Version string
				}{
					Service: rt.Service(),
					Version: rt.Version(),
				}); err != nil {
					http.Error(w, "template error", http.StatusInternalServerError)
				}
			}))
			handler, err := mcp.NewHandler(svc, rt)
			if err != nil {
				return err
			}
			rt.Handle("POST /mcp", rt.RequireIdentity(handler))
			return nil
		},
		// Producer fires after Handlers: inject the outbox so every committed
		// domain write emits its first-wave contact.* events on the same tx (the
		// payload builders stay app-side in internal/crm/events.go).
		Producer: func(ob *outbox.Outbox) error {
			if svc == nil {
				return fmt.Errorf("crm: Producer called before Handlers built the Service")
			}
			svc.Outbox = ob
			return nil
		},
	})
}
