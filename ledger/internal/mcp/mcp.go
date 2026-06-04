// Package mcp implements a minimal MCP transport for the /mcp endpoint and the
// ledger_* tool surface.
//
// This is the skeleton ledger service: the only tool is ledger_whoami, the
// end-to-end auth proof. Real ledger domain tools are added here later, wired to
// a domain service the same way crm wires internal/contacts.
//
// The transport speaks JSON-RPC 2.0 over plain HTTP POST (no SSE/streaming),
// responding with Content-Type: application/json. It carries NO token logic:
// nginx introspects every request via auth_request against the dashboard's
// authorization server and injects X-Owner-Email / X-Client-Id authoritatively
// before forwarding here. The handler is mounted behind the server's
// requireIdentityHeaders gate, so by the time a request arrives the caller
// identity is already established. There is intentionally no bearer parsing, no
// token store, no rate limiter, and no WWW-Authenticate / 401 / 429 emission in
// this package — that all lives in the dashboard.
package mcp

import (
	"encoding/json"
	"errors"
	"net/http"

	"ledger/internal/ledger"
)

// Identity is the authenticated caller, as told to us authoritatively by nginx
// (via the server's requireIdentityHeaders gate) through request headers.
type Identity struct {
	OwnerEmail string
	ClientID   string
}

// Handler is the http.Handler for POST /mcp. It is constructed once at wiring
// time with a non-nil ledger service and dispatches JSON-RPC methods.
type Handler struct {
	ledger *ledger.Service
}

// NewHandler builds a Handler. The ledger service is required; a nil service is
// a wiring error and panics at this seam rather than deferring a nil dereference
// to first request.
func NewHandler(svc *ledger.Service) *Handler {
	if svc == nil {
		panic("mcp: ledger service is required")
	}
	return &Handler{ledger: svc}
}

// ServeHTTP dispatches a single JSON-RPC 2.0 request. Identity is read from the
// nginx-injected headers (always present behind requireIdentityHeaders).
func (h *Handler) ServeHTTP(w http.ResponseWriter, r *http.Request) {
	id := Identity{
		OwnerEmail: r.Header.Get("X-Owner-Email"),
		ClientID:   r.Header.Get("X-Client-Id"),
	}
	var req jsonRPCRequest
	if err := json.NewDecoder(r.Body).Decode(&req); err != nil {
		writeJSONRPCError(w, nil, -32700, "parse error")
		return
	}
	switch req.Method {
	case "initialize":
		writeJSONRPCResult(w, req.ID, map[string]any{
			"protocolVersion": "2025-03-26",
			"capabilities":    map[string]any{"tools": map[string]any{}},
			"serverInfo":      map[string]any{"name": "Ledger", "version": "1"},
		})
	case "notifications/initialized":
		// fire-and-forget notification — no response per JSON-RPC.
		w.WriteHeader(http.StatusAccepted)
	case "tools/list":
		writeJSONRPCResult(w, req.ID, map[string]any{"tools": toolDescriptors()})
	case "tools/call":
		h.handleToolCall(r.Context(), w, req, id)
	default:
		writeJSONRPCError(w, req.ID, -32601, "method not found")
	}
}

type jsonRPCRequest struct {
	JSONRPC string          `json:"jsonrpc"`
	ID      json.RawMessage `json:"id,omitempty"`
	Method  string          `json:"method"`
	Params  json.RawMessage `json:"params,omitempty"`
}

func writeJSONRPCResult(w http.ResponseWriter, id json.RawMessage, result any) {
	w.Header().Set("Content-Type", "application/json; charset=utf-8")
	_ = json.NewEncoder(w).Encode(map[string]any{
		"jsonrpc": "2.0",
		"id":      idOrNull(id),
		"result":  result,
	})
}

func writeJSONRPCError(w http.ResponseWriter, id json.RawMessage, code int, msg string) {
	w.Header().Set("Content-Type", "application/json; charset=utf-8")
	_ = json.NewEncoder(w).Encode(map[string]any{
		"jsonrpc": "2.0",
		"id":      idOrNull(id),
		"error":   map[string]any{"code": code, "message": msg},
	})
}

func idOrNull(id json.RawMessage) any {
	if len(id) == 0 {
		return nil
	}
	return json.RawMessage(id)
}

// Result-shape helpers for tool calls. MCP `tools/call` returns
// {content: [{type: "text", text: "..."}], isError?: bool}.
func toolResultText(text string) map[string]any {
	return map[string]any{"content": []map[string]any{{"type": "text", "text": text}}}
}

func toolResultErr(msg string) map[string]any {
	return map[string]any{"isError": true, "content": []map[string]any{{"type": "text", "text": msg}}}
}

// translateLedgerError maps a ledger domain/validation sentinel to the
// structured wire error the tool surface returns — the same sentinel→wire
// pattern crm uses. bad_root points the agent at ledger_describe so it can
// discover the five typed roots.
func translateLedgerError(err error) string {
	switch {
	case errors.Is(err, ledger.ErrUnbalanced):
		return `{"error":{"code":"unbalanced","message":"` + jsonEscape(err.Error()) + `"}}`
	case errors.Is(err, ledger.ErrBadRoot):
		return `{"error":{"code":"bad_root","message":"account root must be one of Assets, Liabilities, Equity, Income (alias Revenue), Expenses — call ledger_describe"}}`
	case errors.Is(err, ledger.ErrAlreadyReversed):
		return `{"error":{"code":"already_reversed","message":"transaction already has a reversal; reverse its mirror instead"}}`
	case errors.Is(err, ledger.ErrNotFound):
		return `{"error":{"code":"not_found","message":"` + jsonEscape(err.Error()) + `"}}`
	case errors.Is(err, ledger.ErrValidation):
		return `{"error":{"code":"validation","message":"` + jsonEscape(err.Error()) + `"}}`
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
