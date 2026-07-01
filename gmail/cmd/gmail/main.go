// Command gmail is the loopback-only Gmail connector + event-plane producer
// behind nginx. It trusts the X-Owner-Email / X-Client-Id headers nginx injects
// after a successful auth_request against the dashboard's authorization server,
// and performs no token logic of its own. See appkit/server for the auth
// contract.
//
// The uniform chassis — the fixed subcommands (serve/version/manifest/migrate/
// backup/restore), config-from-env, the migration runner + downgrade guard, the
// loopback HTTP server + PRM + identity gate, and the /feed producer mount — is
// owned by appkit. main.go declares only gmail's identity (the Spec) and wires
// its domain surface through the Spec hooks. RESOURCE_ID / AUTH_SERVER are
// composed in-binary by appkit/config from IKIGENBA_DOMAIN + MOUNT.
//
// gmail is structurally dropbox's twin (decisions §1): an external-OAuth
// connector with an MCP surface, an internal poll daemon, and an event-plane
// producer half. The History-API producer + poll daemon is wired through
// Producer/Workers (P3); the full normal-mailbox MCP tool set over the P2 client
// is wired through Handlers (P4) in gmailapp.Spec. The three GMAIL_* secrets +
// GMAIL_POLL_INTERVAL are read there at gmail's own composition root via getenv
// and never logged; appkit never touches them.
package main

import (
	"appkit"

	"gmail/internal/gmailapp"
)

func main() {
	appkit.Main(gmailapp.Spec())
}
