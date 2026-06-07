// Command sites is the loopback-only static-website host behind nginx. It trusts
// the X-Owner-Email / X-Client-Id headers nginx injects after a successful
// auth_request against the dashboard's authorization server, and performs no
// token logic of its own. See appkit/server for the auth contract.
//
// The uniform chassis — the fixed subcommands (serve/version/manifest/migrate/
// backup/restore), config-from-env, the migration runner + downgrade guard, and
// the loopback HTTP server + PRM + identity gate — is owned by appkit. main.go
// declares only sites's identity (the Spec) and wires its domain surface through
// the Spec hooks. RESOURCE_ID / AUTH_SERVER are composed in-binary by
// appkit/config from IKIGENBA_DOMAIN + MOUNT.
//
// sites is a pure MCP service: it publishes nothing to the event plane, so the
// Feed / Producer / Workers / Events hooks are deliberately omitted. Handlers
// mounts the ikigenba_sites_* MCP surface (POST /mcp) behind appkit's
// nginx-injected identity gate; the domain store + layout are built from the
// shared DB handle and SITES_ROOT at this composition root.
package main

import (
	"os"

	"appkit"

	"sites/internal/db"
	"sites/internal/mcp"
	"sites/internal/sites"
)

func main() {
	appkit.Main(appkit.Spec{
		App:        "sites",
		Mount:      "/srv/sites/",
		Port:       3010,
		MCP:        true,
		Migrations: db.FS,
		// Handlers builds the sites domain store + layout (the single SITES_ROOT env
		// read happens here, per layout.go's contract) and mounts the MCP handler
		// behind RequireIdentity. The chassis has already opened + migrated the
		// shared single-writer DB by the time this runs, so rt.DB() is ready.
		Handlers: func(rt *appkit.Router) error {
			layout := sites.NewLayout(os.Getenv("SITES_ROOT"))
			store := sites.NewStoreWithLayout(rt.DB(), layout)
			rt.Handle("POST /mcp", rt.RequireIdentity(
				mcp.NewHandler(store, layout, rt.Version(), rt.Service(), rt.Health())))
			return nil
		},
	})
}
