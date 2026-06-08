package server

// This file builds the "Connect an MCP client" view shown on the logged-in
// index: for each MCP-exposing service on the box (from the inventory manifests)
// a set of per-client install/remove snippets. The index page picks which
// service the snippets target via a dropdown; the snippet markup itself is the
// crm reference's install_card, unchanged.

import (
	"net/http"

	"appkit/inventory"
)

// installCard is one client's install/removal command pair (e.g. Claude Code),
// rendered by the install_card partial. The command strings are NOT pre-escaped;
// the template renders them through html/template's contextual auto-escaping.
type installCard struct {
	Name           string
	InstallCommand string
	RemoveCommand  string
}

// mcpInstall is one MCP service the dropdown can target: a stable ID (the
// service name, used as the <option> value and the card-set's data-mcp key), a
// display Name, and the per-client cards whose commands point at this service's
// resource URL.
type mcpInstall struct {
	ID    string
	Name  string
	Cards []installCard
}

// requestScheme resolves the external scheme for a request behind nginx: the
// front door terminates TLS and forwards X-Forwarded-Proto, so trust it and
// default to https when absent. Shared by the index install snippets and the
// /services inventory endpoint so the two can't drift.
func requestScheme(r *http.Request) string {
	if scheme := r.Header.Get("X-Forwarded-Proto"); scheme != "" {
		return scheme
	}
	return "https"
}

// mcpResourceURL is the full MCP endpoint URL for a service, self-templated from
// the request: <scheme>://<host><mount>mcp. Mount carries its own trailing slash
// (e.g. "/srv/crm/"), so "mcp" appends directly.
func mcpResourceURL(scheme, host, mount string) string {
	return scheme + "://" + host + mount + "mcp"
}

// mcpLocalName is the local MCP registration handle for a service: the bare
// service name namespaced with an "ikigenba_" prefix (e.g. "ikigenba_crm"), used
// as the `claude mcp add`/`codex mcp add` <name> argument so the suite's MCP
// servers don't collide with generically-named servers in a user's config. Only
// the registration handle is prefixed — the resource URL (mcpResourceURL) and the
// on-box service name are unchanged. Shared by the index install snippets and the
// /install one-paste script so the two can't drift.
func mcpLocalName(svc string) string {
	return "ikigenba_" + svc
}

// installCardsFor returns the per-client connect snippets for one service. name
// is the local MCP registration handle (mcpLocalName: "ikigenba_<svc>"); resource
// is its MCP endpoint URL. The leading backslash on each command bypasses any
// shell alias named claude/codex so the real binary runs — kept from the crm
// reference verbatim.
func installCardsFor(name, resource string) []installCard {
	return []installCard{
		{
			Name:           "Claude Code",
			InstallCommand: `\claude mcp add --transport http ` + name + ` ` + resource,
			RemoveCommand:  `\claude mcp remove ` + name,
		},
		{
			Name:           "Codex",
			InstallCommand: `\codex mcp add --transport http ` + name + ` ` + resource,
			RemoveCommand:  `\codex mcp remove ` + name,
		},
	}
}

// mcpInstalls turns the box's MCP-exposing services into the index's connect
// view: one mcpInstall per service, ordered as inventory returns them (by name).
func mcpInstalls(scheme, host string, svcs []inventory.Service) []mcpInstall {
	out := make([]mcpInstall, 0, len(svcs))
	for _, s := range svcs {
		resource := mcpResourceURL(scheme, host, s.Mount)
		out = append(out, mcpInstall{
			ID:    s.Name,
			Name:  s.Name,
			Cards: installCardsFor(mcpLocalName(s.Name), resource),
		})
	}
	return out
}
