// Package mcp implements the shared minimal MCP JSON-RPC transport for suite
// services.
package mcp

import (
	"context"
	"encoding/json"
	"errors"
	"fmt"
	"net/http"
	"strings"

	"appkit"
	"appkit/server"

	"eventplane/consumer"
	"eventplane/outbox"
)

const protocolVersion = "2025-06-18"

var reservedToolNames = map[string]struct{}{
	"health":     {},
	"reflection": {},
}

// Tool is one declared MCP tool: its wire descriptor plus its handler.
type Tool struct {
	Name         string
	Description  string
	InputSchema  map[string]any
	OutputSchema map[string]any
	Handler      func(ctx context.Context, args json.RawMessage, id server.Identity) (map[string]any, error)
}

// ErrorCode is the closed vocabulary for errors returned inside tool results.
type ErrorCode string

const (
	ErrValidation        ErrorCode = "validation"
	ErrNotFound          ErrorCode = "not_found"
	ErrConflict          ErrorCode = "conflict"
	ErrTooLarge          ErrorCode = "too_large"
	ErrSourceUnavailable ErrorCode = "source_unavailable"
	ErrInternal          ErrorCode = "internal"
)

// Options assembles a service's MCP surface.
type Options struct {
	Service       string
	Version       string
	Instructions  string
	Tools         []Tool
	Health        func(ctx context.Context) (map[string]any, error)
	Events        outbox.Registry
	Publishes     func() outbox.Registry
	Subscriptions func() []consumer.Subscription
}

// Handler is the http.Handler for POST /mcp.
type Handler struct {
	service       string
	version       string
	instructions  string
	tools         []Tool
	toolByName    map[string]Tool
	health        func(context.Context) (map[string]any, error)
	events        outbox.Registry
	publishes     func() outbox.Registry
	subscriptions func() []consumer.Subscription
}

// New validates the tool table and returns the shared MCP transport handler.
func New(opts Options) (*Handler, error) {
	toolByName := make(map[string]Tool, len(opts.Tools))
	for _, tool := range opts.Tools {
		if _, reserved := reservedToolNames[tool.Name]; reserved {
			return nil, fmt.Errorf("mcp: tool name %q is reserved", tool.Name)
		}
		if _, exists := toolByName[tool.Name]; exists {
			return nil, fmt.Errorf("mcp: duplicate tool name %q", tool.Name)
		}
		toolByName[tool.Name] = tool
	}
	tools := append([]Tool(nil), opts.Tools...)
	return &Handler{
		service:       opts.Service,
		version:       opts.Version,
		instructions:  opts.Instructions,
		tools:         tools,
		toolByName:    toolByName,
		health:        opts.Health,
		events:        opts.Events,
		publishes:     opts.Publishes,
		subscriptions: opts.Subscriptions,
	}, nil
}

// ServeHTTP dispatches a single JSON-RPC 2.0 request over plain HTTP.
func (h *Handler) ServeHTTP(w http.ResponseWriter, r *http.Request) {
	var req jsonRPCRequest
	if err := json.NewDecoder(r.Body).Decode(&req); err != nil {
		writeJSONRPCError(w, nil, -32700, "parse error")
		return
	}

	switch req.Method {
	case "initialize":
		writeJSONRPCResult(w, req.ID, map[string]any{
			"protocolVersion": protocolVersion,
			"capabilities":    map[string]any{"tools": map[string]any{}},
			"serverInfo": map[string]any{
				"name":    h.service,
				"version": h.version,
			},
			"instructions": h.instructions,
		})
	case "notifications/initialized":
		w.WriteHeader(http.StatusAccepted)
	case "tools/list":
		writeJSONRPCResult(w, req.ID, map[string]any{"tools": h.toolDescriptors()})
	case "tools/call":
		h.handleToolCall(r.Context(), w, req, identityFromRequest(r))
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

type toolCallParams struct {
	Name      string          `json:"name"`
	Arguments json.RawMessage `json:"arguments"`
}

func (h *Handler) handleToolCall(ctx context.Context, w http.ResponseWriter, req jsonRPCRequest, id server.Identity) {
	var p toolCallParams
	if err := json.Unmarshal(req.Params, &p); err != nil {
		writeJSONRPCError(w, req.ID, -32602, "invalid params")
		return
	}
	result, err := h.dispatchTool(ctx, p.Name, p.Arguments, id)
	if err != nil {
		code := -32603
		if errors.Is(err, errUnknownTool) {
			code = -32602
		}
		writeJSONRPCError(w, req.ID, code, err.Error())
		return
	}
	writeJSONRPCResult(w, req.ID, result)
}

var errUnknownTool = errors.New("unknown tool")

func (h *Handler) dispatchTool(ctx context.Context, name string, args json.RawMessage, id server.Identity) (map[string]any, error) {
	switch name {
	case "health":
		return h.toolHealth(ctx, id)
	case "reflection":
		return h.toolReflection(args)
	}
	tool, ok := h.toolByName[name]
	if !ok {
		return nil, fmt.Errorf("%w: %s", errUnknownTool, name)
	}
	if tool.Handler == nil {
		return nil, fmt.Errorf("tool %s has no handler", name)
	}
	return tool.Handler(ctx, args, id)
}

func (h *Handler) toolDescriptors() []map[string]any {
	descriptors := []map[string]any{
		{
			"name":        "health",
			"description": "Health + diagnostics for this service. Returns the fixed envelope (status, version, service, details) plus the authenticated caller's identity (owner_email, client_id). Takes no inputs.",
			"inputSchema": objectSchema(map[string]any{}),
		},
		{
			"name":        "reflection",
			"description": "Self-describe this service's event graph. With no arguments, returns {publishes, subscribes}; pass kind for one published event family's detail.",
			"inputSchema": objectSchema(map[string]any{
				"kind": map[string]any{
					"type":        "string",
					"description": "optional; a published event family kind to fetch the schema+example detail for",
				},
			}),
		},
	}
	for _, tool := range h.tools {
		descriptor := map[string]any{
			"name":        tool.Name,
			"description": tool.Description,
			"inputSchema": tool.InputSchema,
		}
		if tool.OutputSchema != nil {
			descriptor["outputSchema"] = tool.OutputSchema
		}
		descriptors = append(descriptors, descriptor)
	}
	return descriptors
}

func (h *Handler) toolHealth(ctx context.Context, id server.Identity) (map[string]any, error) {
	details := map[string]any{}
	if h.health != nil {
		reported, err := h.health(ctx)
		if err != nil {
			details = map[string]any{"error": err.Error()}
		} else if reported != nil {
			details = reported
		}
	}
	envelope := appkit.Envelope(h.version, h.service, details)
	envelope["owner_email"] = id.OwnerEmail
	envelope["client_id"] = id.ClientID
	return StructuredResult(envelope)
}

func (h *Handler) toolReflection(args json.RawMessage) (map[string]any, error) {
	var p struct {
		Kind string `json:"kind,omitempty"`
	}
	if len(args) > 0 {
		if err := json.Unmarshal(args, &p); err != nil {
			return nil, err
		}
	}
	events := h.events
	if h.publishes != nil {
		events = h.publishes()
	}
	if p.Kind != "" {
		detail, err := events.Detail(p.Kind)
		if err != nil {
			var unknown *outbox.UnknownKindError
			if errors.As(err, &unknown) {
				return ErrorResult(ErrNotFound, fmt.Sprintf("unknown event kind %q; known kinds: %s", unknown.Kind, strings.Join(unknown.Valid, ", "))), nil
			}
			return nil, err
		}
		return StructuredResult(detail)
	}
	return StructuredResult(map[string]any{
		"publishes":  events.Index(),
		"subscribes": renderSubscriptions(h.subscriptions),
	})
}

func renderSubscriptions(provider func() []consumer.Subscription) []map[string]any {
	if provider == nil {
		return []map[string]any{}
	}
	subscriptions := provider()
	out := make([]map[string]any, 0, len(subscriptions))
	for _, sub := range subscriptions {
		out = append(out, map[string]any{
			"source":      sub.Source,
			"filter":      sub.Filter,
			"description": sub.Description,
		})
	}
	return out
}

func objectSchema(properties map[string]any, required ...string) map[string]any {
	schema := map[string]any{"type": "object", "properties": properties}
	if len(required) > 0 {
		schema["required"] = required
	}
	return schema
}

func identityFromRequest(r *http.Request) server.Identity {
	if id, ok := server.IdentityFrom(r.Context()); ok {
		return id
	}
	return server.Identity{
		OwnerEmail: r.Header.Get("X-Owner-Email"),
		ClientID:   r.Header.Get("X-Client-Id"),
	}
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

// TextResult returns one MCP text content item.
func TextResult(text string) map[string]any {
	return map[string]any{"content": []map[string]any{{"type": "text", "text": text}}}
}

// StructuredResult returns matching machine-readable and text JSON renderings.
func StructuredResult(v any) (map[string]any, error) {
	b, err := json.Marshal(v)
	if err != nil {
		return nil, err
	}
	result := TextResult(string(b))
	result["structuredContent"] = v
	return result, nil
}

func structuredResultFrom(v map[string]any, err error) (map[string]any, error) {
	if err != nil {
		return nil, err
	}
	return StructuredResult(v)
}

// ErrorResult returns an MCP tool error envelope.
func ErrorResult(code ErrorCode, msg string) map[string]any {
	result := TextResult(msg)
	result["isError"] = true
	result["structuredContent"] = map[string]any{
		"code":    code,
		"message": msg,
	}
	return result
}
