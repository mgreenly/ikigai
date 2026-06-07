// Command cron is the loopback-only scheduled-event-emitter service behind
// nginx. It trusts the X-Owner-Email / X-Client-Id headers nginx injects after a
// successful auth_request against the dashboard's authorization server, and
// performs no token logic of its own.
//
// The uniform chassis — the fixed subcommands (serve/version/manifest/migrate/
// backup/restore), config-from-env, the migration runner + downgrade guard, and
// the loopback HTTP server + PRM + identity gate — is owned by appkit. main.go
// declares only cron's identity (the Spec) and its embedded schema. RESOURCE_ID
// / AUTH_SERVER are composed in-binary by appkit/config from IKIGENBA_DOMAIN +
// MOUNT.
//
// This phase (P4) is scaffold + DB + matcher only: the crontab MCP surface, the
// minute-aligned tick worker, and the cron.<name> /feed producer arrive in P5,
// so the Spec is intentionally minimal/serve-able for now.
package main

import (
	"appkit"

	"cron/internal/db"
)

func main() {
	appkit.Main(appkit.Spec{
		App:        "cron",
		Mount:      "/srv/cron/",
		Port:       3007,
		MCP:        true,
		Migrations: db.FS,
	})
}
