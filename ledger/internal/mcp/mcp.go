// Package mcp exposes ledger's domain tools through the shared appkit MCP
// transport.
package mcp

import (
	"errors"
	"fmt"
	"net/http"
	"strings"

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
func translateLedgerError(err error) map[string]any {
	switch {
	case errors.Is(err, ledger.ErrUnbalanced):
		return appkitmcp.ErrorResult(appkitmcp.ErrValidation, strings.TrimPrefix(err.Error(), ledger.ErrUnbalanced.Error()+": "))
	case errors.Is(err, ledger.ErrBadRoot):
		return appkitmcp.ErrorResult(appkitmcp.ErrValidation, "account root must be one of Assets, Liabilities, Equity, Income (alias Revenue), Expenses — call describe")
	case errors.Is(err, ledger.ErrAlreadyReversed):
		return appkitmcp.ErrorResult(appkitmcp.ErrConflict, "transaction already has a reversal; reverse its mirror instead")
	case errors.Is(err, ledger.ErrNotFound):
		return appkitmcp.ErrorResult(appkitmcp.ErrNotFound, err.Error())
	case errors.Is(err, ledger.ErrValidation):
		return appkitmcp.ErrorResult(appkitmcp.ErrValidation, err.Error())
	case errors.Is(err, ledger.ErrDuplicateRef):
		return appkitmcp.ErrorResult(appkitmcp.ErrConflict, err.Error())
	default:
		return appkitmcp.ErrorResult(appkitmcp.ErrInternal, "internal error")
	}
}
