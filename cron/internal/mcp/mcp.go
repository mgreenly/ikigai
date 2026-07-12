// Package mcp exposes cron's crontab CRUD tools through the shared appkit MCP
// transport.
package mcp

import (
	"encoding/json"
	"errors"
	"fmt"
	"net/http"

	"appkit"
	appkitmcp "appkit/mcp"

	"cron/internal/crontab"
)

// Instructions describes cron's MCP surface to clients during initialize.
const Instructions = "Named UTC cron schedules that publish a cron:tick/<name> event on " +
	"a timer. Create a schedule, then wire consumers to its event."

// NewHandler builds the POST /mcp handler from the appkit Router seam. The
// shared transport owns JSON-RPC, health, and reflection; cron declares only its
// crontab domain tools.
func NewHandler(store *crontab.Store, rt *appkit.Router) (http.Handler, error) {
	if store == nil {
		panic("mcp: crontab store is required")
	}
	if rt == nil {
		return nil, fmt.Errorf("mcp: router is required")
	}
	return appkitmcp.New(appkitmcp.Options{
		Service:       rt.Service(),
		Version:       rt.Version(),
		Instructions:  Instructions,
		Tools:         Tools(store),
		Health:        rt.Health(),
		Events:        rt.Events(),
		Publishes:     rt.Publishes(),
		Subscriptions: rt.Subscriptions(),
	})
}

// errorEnvelope renders a crontab/parse error into the uniform, closed-vocabulary
// error envelope {error:{code,message,field?}} that crm/ledger use. The store
// returns typed sentinels; the parser's message names the bad field.
func errorEnvelope(err error) map[string]any {
	e := map[string]any{}
	var pe *parseError
	switch {
	case errors.As(err, &pe):
		e["code"] = "validation"
		e["message"] = pe.Error()
		e["field"] = "expr"
	case errors.Is(err, crontab.ErrExists):
		e["code"] = "duplicate"
		e["message"] = err.Error()
	case errors.Is(err, crontab.ErrNotFound):
		e["code"] = "not_found"
		e["message"] = err.Error()
	case errors.Is(err, crontab.ErrInvalid):
		e["code"] = "validation"
		e["message"] = err.Error()
		e["field"] = "name"
	default:
		e["code"] = "internal"
		e["message"] = "internal error"
	}
	return map[string]any{"error": e}
}

// toolErr renders a domain error as a tool-call error result carrying the JSON
// envelope text.
func toolErr(err error) map[string]any {
	b, _ := json.Marshal(errorEnvelope(err))
	return appkitmcp.ErrorResult(string(b))
}
