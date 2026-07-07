// Package mcp exposes scripts' domain tools through the shared appkit MCP
// transport.
package mcp

import (
	"fmt"
	"net/http"

	"appkit"
	appkitmcp "appkit/mcp"

	"scripts/internal/script"
)

// Instructions are kept deliberately short because they ride on every client
// initialization. The deeper guide lives in the describe tool.
const Instructions = "Scripts runs Python scripts on your behalf, manually or on an event trigger. " +
	"If you haven't used scripts before, call describe first — it explains " +
	"what a script is, the create→run→poll→read lifecycle, triggers, and the " +
	"runtime contract — then use the other tools."

// NewHandler builds the POST /mcp handler from the appkit Router seam. The
// shared transport owns JSON-RPC, health, and reflection; scripts declares only
// its domain tools.
func NewHandler(svc *script.Service, rt *appkit.Router) (http.Handler, error) {
	if svc == nil {
		panic("mcp: script service is required")
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
		Publishes:     rt.Publishes(),
		Subscriptions: rt.Subscriptions(),
	})
}
