// Command sites is the loopback-only static-website host behind nginx. It trusts
// the X-Owner-Email / X-Client-Id headers nginx injects after a successful
// auth_request against the dashboard's authorization server, and performs no
// token logic of its own. See appkit/server for the auth contract.
//
// The uniform chassis — the fixed subcommands (serve/version/manifest/migrate/
// schema), config-from-env, the migration runner + downgrade guard, and
// the loopback HTTP server + PRM + identity gate — is owned by appkit. main.go
// declares only sites's identity (the Spec) and wires its domain surface through
// the Spec hooks. RESOURCE_ID / AUTH_SERVER are composed in-binary by
// appkit/config from IKIGENBA_DOMAIN + MOUNT.
//
// sites is not an event-plane producer: it publishes nothing to the event plane,
// so the Feed / Producer / Workers / Events hooks are deliberately omitted.
// Handlers serves a human web landing page at the bare mount root /srv/sites/,
// gated by the dashboard session, alongside the ikigenba_sites_* MCP surface
// (POST /mcp) behind appkit's nginx-injected identity gate; the domain store +
// layout are built from the shared DB handle and SITES_ROOT at this composition
// root.
package main

import (
	"net/http"
	"os"
	"strings"

	"appkit"
	"appkit/config"
	"registry"

	"sites/internal/db"
	"sites/internal/mcp"
	"sites/internal/sites"
)

func main() {
	appkit.Main(sitesSpec())
}

func sitesSpec() appkit.Spec {
	return appkit.Spec{
		App:        "sites",
		Mount:      "/srv/sites/",
		Port:       registry.MustPort("sites"),
		MCP:        true,
		WWW:        true,
		Migrations: db.FS,
		Handlers: func(rt *appkit.Router) error {
			layout := sites.NewLayout(os.Getenv("SITES_ROOT"))
			store := sites.NewStoreWithLayout(rt.DB(), layout)
			// The front-door base under which nginx serves published sites is the
			// service's ResourceID minus its trailing "mcp" — RESOURCE_ID is
			// "https://<domain>/srv/sites/mcp", so this yields
			// "https://<domain>/srv/sites/" and the tools append "<tier>/<name>/".
			baseURL := strings.TrimSuffix(rt.ResourceID(), "mcp")
			// Wire the dropbox loopback mirror client the `sync` verb enumerates and
			// fetches through. DROPBOX_BASE_URL is env-only (not a manifest extra),
			// defaulting to the standard loopback layout, exactly the shape notify uses
			// for its peer *_FEED_URL (ADR dropbox-import-sync; plan cross-cutting
			// decision 2). The client derives <base>/list and <base>/content.
			base := config.EnvOr(os.Getenv, "DROPBOX_BASE_URL", registry.BaseURL("dropbox"))
			mirror := sites.NewMirrorClient(base)
			handler, err := mcp.NewHandler(store, layout, baseURL, mirror, rt)
			if err != nil {
				return err
			}
			rt.Handle("GET /{$}", http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
				if r.URL.Path != "/" {
					http.NotFound(w, r)
					return
				}
				_ = rt.WWW().Render(w, "landing.html",
					struct{ Service, Version string }{rt.Service(), rt.Version()})
			}))
			rt.Handle("POST /mcp", rt.RequireIdentity(handler))
			return nil
		},
	}
}
