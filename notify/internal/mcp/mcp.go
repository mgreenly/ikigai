// Package mcp exposes notify's domain tools through the shared appkit MCP
// transport.
package mcp

import (
	"fmt"
	"net/http"

	"appkit"
	appkitmcp "appkit/mcp"

	"notify/internal/push"
)

// Instructions describes notify's MCP surface to clients during initialize.
const Instructions = "Push notifications to the owner's device. Call send to push a " +
	"notification proactively. notify is also an event-plane consumer that pushes " +
	"automatically for each subscribed event. Check health for status and reflection " +
	"for what it subscribes to."

// NewHandler builds the POST /mcp handler from the appkit Router seam. The
// shared transport owns JSON-RPC, health, and reflection; notify declares only
// its send tool.
func NewHandler(client *push.Client, rt *appkit.Router) (http.Handler, error) {
	if client == nil {
		panic("mcp: push client is required")
	}
	if rt == nil {
		return nil, fmt.Errorf("mcp: router is required")
	}
	return appkitmcp.New(appkitmcp.Options{
		Service:       rt.Service(),
		Version:       rt.Version(),
		Instructions:  Instructions,
		Tools:         Tools(client),
		Health:        rt.Health(),
		Events:        rt.Events(),
		Publishes:     rt.Publishes(),
		Subscriptions: rt.Subscriptions(),
	})
}
