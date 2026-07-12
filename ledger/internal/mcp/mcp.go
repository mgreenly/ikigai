// Package mcp exposes ledger's domain tools through the shared appkit MCP
// transport.
package mcp

import (
	"encoding/json"
	"errors"
	"fmt"
	"net/http"

	"appkit"
	appkitmcp "appkit/mcp"

	"ledger/internal/ledger"
)

const Instructions = "Double-entry bookkeeping over an immutable journal. Call " +
	"describe first for the account model, then use record, balance, and register."

// NewHandler builds the POST /mcp handler from the appkit Router seam. The
// shared transport owns JSON-RPC, health, and reflection; ledger declares only
// its domain tools.
func NewHandler(svc *ledger.Service, rt *appkit.Router) (http.Handler, error) {
	if svc == nil {
		panic("mcp: ledger service is required")
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

// translateLedgerError maps a ledger domain/validation sentinel to the
// structured wire error the tool surface returns — the same sentinel→wire
// pattern crm uses. bad_root points the agent at describe so it
// can discover the five typed roots.
func translateLedgerError(err error) string {
	switch {
	case errors.Is(err, ledger.ErrUnbalanced):
		return `{"error":{"code":"unbalanced","message":"` + jsonEscape(err.Error()) + `"}}`
	case errors.Is(err, ledger.ErrBadRoot):
		return `{"error":{"code":"bad_root","message":"account root must be one of Assets, Liabilities, Equity, Income (alias Revenue), Expenses — call describe"}}`
	case errors.Is(err, ledger.ErrAlreadyReversed):
		return `{"error":{"code":"already_reversed","message":"transaction already has a reversal; reverse its mirror instead"}}`
	case errors.Is(err, ledger.ErrNotFound):
		return `{"error":{"code":"not_found","message":"` + jsonEscape(err.Error()) + `"}}`
	case errors.Is(err, ledger.ErrValidation):
		return `{"error":{"code":"validation","message":"` + jsonEscape(err.Error()) + `"}}`
	case errors.Is(err, ledger.ErrDuplicateRef):
		return `{"error":{"code":"duplicate_ref","message":"` + jsonEscape(err.Error()) + `"}}`
	default:
		return `{"error":{"code":"internal","message":"internal error"}}`
	}
}

// jsonEscape renders s as a JSON string body (without the surrounding quotes) so
// it can be embedded safely in the hand-built error envelopes above.
func jsonEscape(s string) string {
	b, _ := json.Marshal(s)
	return string(b[1 : len(b)-1])
}
