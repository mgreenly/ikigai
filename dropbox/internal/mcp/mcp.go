// Package mcp exposes dropbox's domain tools through the shared appkit MCP
// transport.
package mcp

import (
	"fmt"
	"net/http"

	"appkit"
	appkitmcp "appkit/mcp"

	"dropbox/internal/dropbox"
)

// Instructions describes dropbox's MCP surface to clients during initialize.
const Instructions = "Keeps a local mirror of one Dropbox app folder and publishes " +
	"file.created/modified/deleted events. Check health for sync status and " +
	"reflection for its events. Use list to browse the mirror's files and get " +
	"to fetch one file's bytes."

// NewHandler builds the POST /mcp handler from the appkit Router seam. The
// shared transport owns JSON-RPC, health, and reflection; dropbox declares only
// its read-only mirror tools.
func NewHandler(svc *dropbox.Service, rt *appkit.Router) (http.Handler, error) {
	if svc == nil {
		panic("mcp: dropbox service is required")
	}
	if rt == nil {
		return nil, fmt.Errorf("mcp: router is required")
	}
	return appkitmcp.New(appkitmcp.Options{
		Service:       rt.Service(),
		Version:       rt.Version(),
		Instructions:  Instructions,
		Tools:         Tools(svc),
		Health:        rt.Health(),
		Events:        rt.Events(),
		Subscriptions: rt.Subscriptions(),
	})
}
