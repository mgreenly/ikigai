// Package mcp implements a minimal MCP transport for the /mcp endpoint and the
// crontab CRUD tool surface (bare verbs) over internal/crontab. It mirrors
// crm/ledger's transport exactly (JSON-RPC 2.0 over plain HTTP POST, no
// SSE/streaming) and carries NO token logic: nginx introspects every request and
// injects X-Owner-Email / X-Client-Id authoritatively before forwarding, and the
// handler is mounted behind the server's requireIdentityHeaders gate.
package mcp

import (
	"context"
	"encoding/json"
	"errors"
	"net/http"

	"cron/internal/crontab"

	"eventplane/consumer"
	"eventplane/outbox"
)

// Identity is the authenticated caller as told to us authoritatively by nginx.
type Identity struct {
	OwnerEmail string
	ClientID   string
}

// Handler is the http.Handler for POST /mcp. It is constructed once at wiring
// time with a non-nil crontab store and the health-envelope inputs threaded from
// appkit's Router accessors.
type Handler struct {
	store         *crontab.Store
	version       string
	service       string
	health        func(context.Context) (map[string]any, error)
	publishes     func() outbox.Registry
	subscriptions func() []consumer.Subscription
}

// NewHandler builds a Handler. The store is required; a nil store is a wiring
// error and panics at this seam. version/service/health populate the
// health envelope. publishes is the LIVE published-type provider
// (the dynamic cron.<name> set, read from the crontab) rendered by
// reflection; subscriptions is nil (cron is a producer only).
func NewHandler(s *crontab.Store, version, service string,
	health func(context.Context) (map[string]any, error),
	publishes func() outbox.Registry, subscriptions func() []consumer.Subscription) *Handler {
	if s == nil {
		panic("mcp: crontab store is required")
	}
	return &Handler{
		store:         s,
		version:       version,
		service:       service,
		health:        health,
		publishes:     publishes,
		subscriptions: subscriptions,
	}
}

// ServeHTTP dispatches a single JSON-RPC 2.0 request.
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
			"serverInfo":      map[string]any{"name": "Cron", "version": "1"},
			"instructions": "Named UTC cron schedules that publish a cron.<name> event on " +
				"a timer. Create a schedule, then wire consumers to its event.",
		})
	case "notifications/initialized":
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

// Result-shape helpers. MCP tools/call returns
// {content: [{type:"text", text:"..."}], isError?: bool}.
func toolResultText(text string) map[string]any {
	return map[string]any{"content": []map[string]any{{"type": "text", "text": text}}}
}

func toolResultErr(msg string) map[string]any {
	return map[string]any{"isError": true, "content": []map[string]any{{"type": "text", "text": msg}}}
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
	return toolResultErr(string(b))
}
