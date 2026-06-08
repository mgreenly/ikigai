// Package mcp implements a minimal MCP transport for the /mcp endpoint and the
// ikigenba_sites_* tool surface.
//
// sites hosts static websites: an owner creates a slug, edits its working tree
// through the file tools, and publishes it to a tier (public/private) which the
// nginx front door serves. This phase wires the JSON-RPC transport plus the
// lifecycle (non-file) tools — health, describe, create, list, delete, mkdir,
// publish, unpublish. The five file tools (write/read/edit/glob/grep) arrive in
// a later phase and append to toolDescriptors + dispatchTool without rewriting
// the existing lines.
//
// The transport speaks JSON-RPC 2.0 over plain HTTP POST (no SSE/streaming),
// responding with Content-Type: application/json. It carries NO token logic:
// nginx introspects every request via auth_request against the dashboard's
// authorization server and injects X-Owner-Email / X-Client-Id authoritatively
// before forwarding here. The handler is mounted behind the server's
// RequireIdentity gate, so by the time a request arrives the caller identity is
// already established. There is intentionally no bearer parsing, no token store,
// no rate limiter, and no WWW-Authenticate / 401 / 429 emission in this package —
// that all lives in the dashboard.
package mcp

import (
	"context"
	"encoding/json"
	"net/http"

	"sites/internal/sites"
)

// Identity is the authenticated caller, as told to us authoritatively by nginx
// (via the server's RequireIdentity gate) through request headers.
type Identity struct {
	OwnerEmail string
	ClientID   string
}

// Handler is the http.Handler for POST /mcp. It is constructed once at wiring
// time with the sites domain store + layout (the registry/publish surface) and
// the health-envelope inputs (version, service, optional reporter) threaded from
// appkit's Router accessors, and dispatches JSON-RPC methods.
type Handler struct {
	store   *sites.Store
	layout  sites.Layout
	version string
	service string
	baseURL string
	health  func(context.Context) (map[string]any, error)
}

// NewHandler builds a Handler. store is the sites registry/publish service; a nil
// store is a wiring error and panics at this seam rather than deferring a nil
// dereference to first request. layout pins the SITES_ROOT working/served tree
// the create/delete/mkdir tools mutate. baseURL is the front-door base the nginx
// serves published sites under — the canonical "https://<domain>/srv/sites/"
// (trailing slash), composed at the wiring root from the server's ResourceID; the
// tools append "<tier>/<name>/" to it so every rendered site carries its serving
// URL and an agent never has to guess the host. version/service/health populate
// the ikigenba_sites_health envelope; health is the optional per-service reporter
// (nil → details is {}).
func NewHandler(store *sites.Store, layout sites.Layout, version, service, baseURL string,
	health func(context.Context) (map[string]any, error)) *Handler {
	if store == nil {
		panic("mcp: sites store is required")
	}
	return &Handler{
		store:   store,
		layout:  layout,
		version: version,
		service: service,
		baseURL: baseURL,
		health:  health,
	}
}

// ServeHTTP dispatches a single JSON-RPC 2.0 request. Identity is read from the
// nginx-injected headers (always present behind RequireIdentity).
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
			"serverInfo":      map[string]any{"name": "Sites", "version": "1"},
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
