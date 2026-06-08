// Package mcp implements a minimal MCP transport for the /mcp endpoint and the
// * tool surface.
//
// gmail is a connector + event-plane producer (gmail-connector-decisions §1).
// This is the P4 surface: the full normal-mailbox tool set over the P2 Gmail
// client — list/read/thread/send/draft/labels/label/unlabel/trash/delete — plus
// the two chassis tools, health (the end-to-end auth proof +
// health envelope) and reflection (self-describes the three
// mail.* events the producer emits). The producer that actually emits those
// events lives in P3 (internal/gmail). Read-only tools (list/read/thread/labels)
// only fetch; mutating tools (send/draft/label/unlabel/trash/delete) change the
// mailbox — trash and delete are the full-scope destructive verbs.
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

	gm "gmail/internal/gmail"

	"eventplane/consumer"
	"eventplane/outbox"
)

// Identity is the authenticated caller, as told to us authoritatively by nginx
// (via the server's RequireIdentity gate) through request headers.
type Identity struct {
	OwnerEmail string
	ClientID   string
}

// Client is the slice of the P2 Gmail REST client the MCP tool surface drives.
// It is an interface (not the concrete *gmail.Client) so the tool handlers can
// be unit-tested against a fake without any network. The concrete *gmail.Client
// satisfies it directly.
type Client interface {
	MessagesList(ctx context.Context, q, pageToken string) (gm.MessagesListResult, error)
	MessageGet(ctx context.Context, id, format string) (gm.Message, error)
	ThreadGet(ctx context.Context, id string) (gm.Thread, error)
	MessagesSend(ctx context.Context, raw string) (gm.Message, error)
	DraftCreate(ctx context.Context, raw string) (gm.Draft, error)
	LabelsList(ctx context.Context) (gm.LabelsListResult, error)
	MessageModify(ctx context.Context, id string, add, remove []string) (gm.Message, error)
	MessageTrash(ctx context.Context, id string) (gm.Message, error)
	MessageDelete(ctx context.Context, id string) error
}

// Handler is the http.Handler for POST /mcp. It is constructed once at wiring
// time with the P2 Gmail client (the mailbox surface), the health-envelope
// inputs (version, service, optional reporter), and the event-graph inputs
// (events registry, subscriptions provider) threaded from appkit's Router
// accessors, and dispatches JSON-RPC methods.
type Handler struct {
	client        Client
	version       string
	service       string
	health        func(context.Context) (map[string]any, error)
	events        outbox.Registry
	subscriptions func() []consumer.Subscription
}

// NewHandler builds a Handler. client is the P2 Gmail client backing the full
// mailbox tool set; a nil client is a wiring error and panics at this seam
// rather than deferring a nil dereference to first request. version/service/
// health populate the health envelope; health is gmail's optional
// per-service reporter. events is the published-event registry and subscriptions
// the live subscription provider, both rendered by reflection. A
// nil events registry falls back to gmail's static mail.* registry so reflection
// always describes the producer even before appkit threads Spec.Events.
func NewHandler(client Client, version, service string,
	health func(context.Context) (map[string]any, error),
	events outbox.Registry, subscriptions func() []consumer.Subscription) *Handler {
	if client == nil {
		panic("mcp: gmail client is required")
	}
	if events == nil {
		events = Events
	}
	return &Handler{
		client:        client,
		version:       version,
		service:       service,
		health:        health,
		events:        events,
		subscriptions: subscriptions,
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
			"serverInfo":      map[string]any{"name": "Gmail", "version": "1"},
			"instructions": "Read and send Gmail and manage labels. Start with list to " +
				"find messages, then read or thread.",
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
