// Package mcp exposes webhooks's domain tools through the shared appkit MCP
// transport.
package mcp

import (
	"fmt"
	"net/http"
	"strings"

	"appkit"
	appkitmcp "appkit/mcp"

	"webhooks/internal/webhooks"
)

// Instructions is shown to clients during MCP initialize.
const Instructions = "Owner-scoped inbound webhooks. create mints a named hook " +
	"and a show-once signing secret; list shows your hooks; rotate issues a " +
	"fresh secret; delete removes one. POST to a hook's trigger_url to fire it."

// NewHandler builds the POST /mcp handler from the appkit Router seam. The
// shared transport owns JSON-RPC, health, and reflection; webhooks declares only
// its domain tools.
func NewHandler(svc *webhooks.Service, rt *appkit.Router) (http.Handler, error) {
	if svc == nil {
		panic("mcp: webhooks service is required")
	}
	if rt == nil {
		return nil, fmt.Errorf("mcp: router is required")
	}
	baseURL := strings.TrimSuffix(rt.ResourceID(), "mcp")
	return appkitmcp.New(appkitmcp.Options{
		Service:      rt.Service(),
		Version:      rt.Version(),
		Instructions: Instructions,
		Tools:        Tools(svc, baseURL),
		Health:       rt.Health(),
		Events:       rt.Events(),
	})
}
