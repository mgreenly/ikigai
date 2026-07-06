// Package mcp exposes crm's domain tools through the shared appkit MCP
// transport.
package mcp

import (
	"fmt"
	"net/http"

	"appkit"
	appkitmcp "appkit/mcp"

	"crm/internal/crm"
)

const instructions = "Sales CRM over organizations (companies), contacts (people), deals " +
	"(pipeline/opportunities), tasks (to-dos), and interactions (notes, calls, " +
	"emails, meetings). Typical flow: search to find things, get for a full " +
	"record, save to create or update, log to record an interaction. Call guide " +
	"once for field catalogs and worked examples before your first save."

// NewHandler builds the POST /mcp handler from the appkit Router seam. The
// shared transport owns JSON-RPC, health, and reflection; crm declares only its
// domain tools.
func NewHandler(svc *crm.Service, rt *appkit.Router) (http.Handler, error) {
	if svc == nil {
		panic("mcp: crm service is required")
	}
	if rt == nil {
		return nil, fmt.Errorf("mcp: router is required")
	}
	return appkitmcp.New(appkitmcp.Options{
		Service:       rt.Service(),
		Version:       rt.Version(),
		Instructions:  instructions,
		Tools:         Tools(svc),
		Health:        rt.Health(),
		Events:        rt.Events(),
		Publishes:     rt.Publishes(),
		Subscriptions: rt.Subscriptions(),
	})
}
