// Package mcp exposes sites's domain tools through the shared appkit MCP
// transport.
package mcp

import (
	"fmt"
	"net/http"

	"appkit"
	appkitmcp "appkit/mcp"

	"sites/internal/sites"
)

// Instructions describes sites's MCP surface to clients during initialize.
const Instructions = "Host static websites behind the front door. Call describe " +
	"first for the create→edit→publish lifecycle."

// NewHandler builds the POST /mcp handler from the appkit Router seam. The
// shared transport owns JSON-RPC, health, and reflection; sites declares only
// its domain tools.
func NewHandler(store *sites.Store, layout sites.Layout, baseURL string, mirror sites.MirrorClient, rt *appkit.Router) (http.Handler, error) {
	if store == nil {
		panic("mcp: sites store is required")
	}
	if rt == nil {
		return nil, fmt.Errorf("mcp: router is required")
	}
	return appkitmcp.New(appkitmcp.Options{
		Service:       rt.Service(),
		Version:       rt.Version(),
		Instructions:  Instructions,
		Tools:         Tools(store, layout, baseURL, mirror),
		Health:        rt.Health(),
		Events:        rt.Events(),
		Publishes:     rt.Publishes(),
		Subscriptions: rt.Subscriptions(),
	})
}
