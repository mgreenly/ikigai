// Package mcp implements a minimal MCP transport for the /mcp endpoint and the
// fixed six-verb ikigenba_crm_* tool surface (PLAN.md §2) that wraps
// internal/crm.
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
	"context"
	"encoding/json"
	"errors"
	"net/http"

	"crm/internal/crm"
)

// Identity is the authenticated caller, as told to us authoritatively by nginx
// (via the server's requireIdentityHeaders gate) through request headers.
type Identity struct {
	OwnerEmail string
	ClientID   string
}

// Handler is the http.Handler for POST /mcp. It is constructed once at wiring
// time with a non-nil crm service and the health-envelope inputs (version,
// service, optional reporter) threaded from appkit's Router accessors, and
// dispatches JSON-RPC methods.
type Handler struct {
	svc     *crm.Service
	version string
	service string
	health  func(context.Context) (map[string]any, error)
}

// NewHandler builds a Handler. The crm service is required; a nil service is a
// wiring error and panics at this seam rather than deferring a nil dereference to
// first request. version/service/health populate the ikigenba_crm_health
// envelope; health is the optional per-service reporter (nil → details is {}).
func NewHandler(s *crm.Service, version, service string,
	health func(context.Context) (map[string]any, error)) *Handler {
	if s == nil {
		panic("mcp: crm service is required")
	}
	return &Handler{svc: s, version: version, service: service, health: health}
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
			"serverInfo":      map[string]any{"name": "CRM", "version": "1"},
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

// errorEnvelope renders a crm domain error into the uniform, closed-vocabulary
// error envelope (PLAN.md §4). Entities return typed sentinels; this is the
// single sentinel→wire translation. The message is the entity's corrective text;
// `field` and `existing_id` are surfaced when present.
func errorEnvelope(err error) map[string]any {
	e := map[string]any{}
	var dup *crm.DuplicateError
	var val *crm.ValidationError
	switch {
	case errors.As(err, &dup):
		e["code"] = "duplicate"
		e["existing_id"] = dup.ExistingID
		e["message"] = dup.Error()
	case errors.As(err, &val):
		e["code"] = "validation"
		e["message"] = val.Error()
		if val.Field != "" {
			e["field"] = val.Field
		}
	case errors.Is(err, crm.ErrValidation):
		e["code"] = "validation"
		e["message"] = err.Error()
	case errors.Is(err, crm.ErrNotFound):
		e["code"] = "not_found"
		e["message"] = err.Error()
	case errors.Is(err, crm.ErrConflict):
		e["code"] = "conflict"
		e["message"] = err.Error()
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
	return toolResultErr(string(b))
}
