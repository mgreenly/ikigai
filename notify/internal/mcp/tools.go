package mcp

import (
	"context"
	"encoding/json"
	"errors"
	"net/http"
	"strings"

	"appkit"

	"eventplane/consumer"
	"eventplane/outbox"
)

// toolPrefix brands every MCP tool name (DECISIONS §1). It is the suite name
// ikigenba + the service name; HTTP route paths are NOT branded.
const toolPrefix = "ikigenba_notify_"

// tool returns the branded, fully-qualified MCP tool name. Used by BOTH
// toolDescriptors and dispatchTool so the two sites cannot drift.
func tool(verb string) string { return toolPrefix + verb }

// toolDescriptors returns the ikigenba_notify_* tool set. For the skeleton this
// is just ikigenba_notify_health; real notify tools are added to this list and to
// dispatchTool as the domain is built out. Schemas are hand-coded; a full JSON
// Schema isn't required by MCP clients but improves the LLM hinting.
func toolDescriptors() []map[string]any {
	return []map[string]any{
		desc(tool("health"), "Health + diagnostics for the notify service. Returns the fixed envelope (status, version, service, details) plus the authenticated caller's identity (owner_email, client_id) as established by the platform's auth gate — the end-to-end auth-chain proof. Takes no inputs.", obj(map[string]any{})),
		desc(tool("reflection"),
			"Self-describe notify's place in the event graph: 'publishes' (the event types this service emits — empty for notify, a consumer) and 'subscribes' (the event types it listens to). With no arguments, returns the index: {publishes:[{type,description}], subscribes:[{source,filter,description}]}. Pass 'event_type' (one of the published types) to get its publish detail — {type, description, schema (JSON Schema of the payload), example (a worked instance)}. Resolve a subscribed edge's shape by calling the source service's reflection tool.",
			obj(map[string]any{
				"event_type": descTyp("string", "optional; a published event type to fetch the schema+example detail for"),
			})),
	}
}

func desc(name, description string, schema map[string]any) map[string]any {
	return map[string]any{"name": name, "description": description, "inputSchema": schema}
}

func obj(props map[string]any, required ...string) map[string]any {
	o := map[string]any{"type": "object", "properties": props}
	if len(required) > 0 {
		o["required"] = required
	}
	return o
}

func typ(t string) map[string]any { return map[string]any{"type": t} }

func descTyp(t, description string) map[string]any {
	return map[string]any{"type": t, "description": description}
}

// ── dispatch ──────────────────────────────────────────────────────────────

type toolCallParams struct {
	Name      string          `json:"name"`
	Arguments json.RawMessage `json:"arguments"`
}

func (h *Handler) handleToolCall(ctx context.Context, w http.ResponseWriter, req jsonRPCRequest, id Identity) {
	var p toolCallParams
	if err := json.Unmarshal(req.Params, &p); err != nil {
		writeJSONRPCError(w, req.ID, -32602, "invalid params")
		return
	}
	res, err := h.dispatchTool(ctx, p.Name, p.Arguments, id)
	if err != nil {
		writeJSONRPCResult(w, req.ID, toolResultErr(err.Error()))
		return
	}
	writeJSONRPCResult(w, req.ID, res)
}

func (h *Handler) dispatchTool(ctx context.Context, name string, argsRaw json.RawMessage, id Identity) (map[string]any, error) {
	switch name {
	case tool("health"):
		return h.toolHealth(ctx, id)
	case tool("reflection"):
		return h.toolReflection(argsRaw)
	default:
		return nil, errors.New("unknown tool: " + name)
	}
}

// ── tool implementations ─────────────────────────────────────────────────

// toolHealth renders the shared health envelope (status/version/service/details)
// via appkit.Envelope and then adds the authenticated caller's identity — the
// end-to-end auth-chain proof (DECISIONS §6). notify supplies no reporter, so
// details renders as {}.
func (h *Handler) toolHealth(ctx context.Context, id Identity) (map[string]any, error) {
	details := map[string]any{}
	if h.health != nil {
		d, err := h.health(ctx)
		if err != nil {
			details = map[string]any{"error": err.Error()}
		} else if d != nil {
			details = d
		}
	}
	env := appkit.Envelope(h.version, h.service, details) // status/version/service/details
	env["owner_email"] = id.OwnerEmail
	env["client_id"] = id.ClientID
	return toolResultJSON(env)
}

// toolReflection self-describes notify's edges in the event graph (the
// ikigenba_<svc>_reflection tool). No event_type → the index {publishes,
// subscribes}; notify is a consumer, so publishes is empty and subscribes lists
// its one crm/contact.created in-edge. With event_type → that published type's
// detail; against notify's empty registry every type is unknown, so it returns
// the corrective error (the ledger bad_root pattern) with an empty valid list.
func (h *Handler) toolReflection(raw json.RawMessage) (map[string]any, error) {
	var a struct {
		EventType string `json:"event_type,omitempty"`
	}
	if len(raw) > 0 {
		if err := json.Unmarshal(raw, &a); err != nil {
			return nil, err
		}
	}
	if a.EventType != "" {
		detail, err := h.events.Detail(a.EventType)
		if err != nil {
			var unknown *outbox.UnknownEventTypeError
			if errors.As(err, &unknown) {
				return toolResultErr(reflectionUnknownTypeError(unknown)), nil
			}
			return nil, err
		}
		return toolResultJSON(detail)
	}
	return toolResultJSON(map[string]any{
		"publishes":  h.events.Index(),
		"subscribes": renderSubscriptions(h.subscriptions),
	})
}

// renderSubscriptions flattens the live subscription provider to the reflection
// in-edges: one {source, filter, description} per Subscription. The Handler is
// dropped — only the declared graph edge is reported. A nil provider (or nil
// result) renders as an empty list.
func renderSubscriptions(provider func() []consumer.Subscription) []map[string]any {
	out := []map[string]any{}
	if provider == nil {
		return out
	}
	for _, s := range provider() {
		out = append(out, map[string]any{
			"source":      s.Source,
			"filter":      s.Filter,
			"description": s.Description,
		})
	}
	return out
}

// reflectionUnknownTypeError renders the corrective error envelope for an unknown
// event_type, listing the valid types so the agent can self-correct (mirrors
// ledger's bad_root corrective message).
func reflectionUnknownTypeError(e *outbox.UnknownEventTypeError) string {
	env := map[string]any{"error": map[string]any{
		"code":    "unknown_event_type",
		"message": "unknown event_type " + e.Type + "; valid types: " + strings.Join(e.Valid, ", "),
	}}
	b, _ := json.Marshal(env)
	return string(b)
}

// ── shared helpers ──────────────────────────────────────────────────────

func toolResultJSON(v any) (map[string]any, error) {
	b, err := json.Marshal(v)
	if err != nil {
		return nil, err
	}
	return toolResultText(string(b)), nil
}
