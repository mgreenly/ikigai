package mcp

import (
	"context"
	"encoding/json"
	"errors"
	"time"

	appkitmcp "appkit/mcp"
	"appkit/server"

	"webhooks/internal/db"
	"webhooks/internal/webhooks"
)

// toolPrefix brands every MCP tool name. The webhooks surface uses bare verbs
// (empty prefix) — route paths are not part of the wire tool names.
const toolPrefix = ""

func tool(verb string) string { return toolPrefix + verb }

type toolHandlers struct {
	svc     *webhooks.Service
	baseURL string
}

// Tools returns webhooks's service-owned MCP tool declarations. The shared
// appkit MCP transport appends the chassis health and reflection tools.
func Tools(svc *webhooks.Service, baseURL string) []appkitmcp.Tool {
	if svc == nil {
		panic("mcp: webhooks service is required")
	}
	h := &toolHandlers{svc: svc, baseURL: baseURL}
	return []appkitmcp.Tool{
		{
			Name:        tool("create"),
			Description: "Provision a new inbound webhook owned by you. Omit 'name' for a freshly-generated opaque name, or pass a name matching ^[A-Za-z0-9_-]{1,64}$. Returns the webhook's trigger_url and a show-once signing secret (prefix ms_wh_) — the secret is shown ONLY here, never again, so capture it now.",
			InputSchema: obj(map[string]any{
				"name": descTyp("string", "optional; ^[A-Za-z0-9_-]{1,64}$. Omit for a generated name."),
			}),
			Handler: func(ctx context.Context, args json.RawMessage, id server.Identity) (map[string]any, error) {
				return h.toolCreate(ctx, args, id)
			},
		},
		{
			Name:        tool("list"),
			Description: "List your webhooks (owner-scoped — only your own). Each entry has name, trigger_url, created_at, and last_triggered_at (null until first fired). Secrets are never returned by list.",
			InputSchema: obj(map[string]any{}),
			Handler: func(ctx context.Context, _ json.RawMessage, id server.Identity) (map[string]any, error) {
				return h.toolList(ctx, id)
			},
		},
		{
			Name:        tool("delete"),
			Description: "Delete one of your webhooks by name. Owner-scoped: a name you do not own returns not_found and changes nothing. Returns {deleted:true} on success.",
			InputSchema: obj(map[string]any{"name": typ("string")}, "name"),
			Handler: func(ctx context.Context, args json.RawMessage, id server.Identity) (map[string]any, error) {
				return h.toolDelete(ctx, args, id)
			},
		},
		{
			Name:        tool("rotate"),
			Description: "Issue a fresh show-once signing secret for one of your webhooks. The name and trigger_url are unchanged; the previous secret stops verifying immediately. Owner-scoped: a name you do not own returns not_found.",
			InputSchema: obj(map[string]any{"name": typ("string")}, "name"),
			Handler: func(ctx context.Context, args json.RawMessage, id server.Identity) (map[string]any, error) {
				return h.toolRotate(ctx, args, id)
			},
		},
	}
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

// triggerURL renders a webhook's public POST endpoint: baseURL (trailing slash)
// + "in/" + name. Both create/rotate and list go through this one site so the
// rendered URL cannot drift between mint and read.
func (h *toolHandlers) triggerURL(name string) string { return h.baseURL + "in/" + name }

// webhookView is the secret-free projection of a stored webhook returned by list:
// name, trigger_url, created_at, last_triggered_at (null until first fired). It
// deliberately omits any secret material.
func (h *toolHandlers) webhookView(wh db.Webhook) map[string]any {
	v := map[string]any{
		"name":              wh.Name,
		"trigger_url":       h.triggerURL(wh.Name),
		"created_at":        wh.CreatedAt.UTC().Format(time.RFC3339Nano),
		"last_triggered_at": nil,
	}
	if wh.LastTriggeredAt != nil {
		v["last_triggered_at"] = wh.LastTriggeredAt.UTC().Format(time.RFC3339Nano)
	}
	return v
}

func (h *toolHandlers) toolCreate(ctx context.Context, raw json.RawMessage, id server.Identity) (map[string]any, error) {
	var a struct {
		Name string `json:"name,omitempty"`
	}
	if len(raw) > 0 {
		if err := json.Unmarshal(raw, &a); err != nil {
			return nil, err
		}
	}
	wh, secret, err := h.svc.Create(ctx, id.OwnerEmail, a.Name)
	if err != nil {
		return toolErr(err), nil
	}
	out := h.webhookView(wh)
	out["secret"] = secret
	return appkitmcp.JSONResult(out)
}

func (h *toolHandlers) toolList(ctx context.Context, id server.Identity) (map[string]any, error) {
	whs, err := h.svc.List(ctx, id.OwnerEmail)
	if err != nil {
		return toolErr(err), nil
	}
	items := make([]map[string]any, 0, len(whs))
	for _, wh := range whs {
		items = append(items, h.webhookView(wh))
	}
	return appkitmcp.JSONResult(map[string]any{"items": items})
}

func (h *toolHandlers) toolDelete(ctx context.Context, raw json.RawMessage, id server.Identity) (map[string]any, error) {
	var a struct {
		Name string `json:"name"`
	}
	if err := json.Unmarshal(raw, &a); err != nil {
		return nil, err
	}
	deleted, err := h.svc.Delete(ctx, id.OwnerEmail, a.Name)
	if err != nil {
		return toolErr(err), nil
	}
	if !deleted {
		return toolErr(webhooks.ErrNotFound), nil
	}
	return appkitmcp.JSONResult(map[string]any{"deleted": true})
}

func (h *toolHandlers) toolRotate(ctx context.Context, raw json.RawMessage, id server.Identity) (map[string]any, error) {
	var a struct {
		Name string `json:"name"`
	}
	if err := json.Unmarshal(raw, &a); err != nil {
		return nil, err
	}
	secret, err := h.svc.Rotate(ctx, id.OwnerEmail, a.Name)
	if err != nil {
		return toolErr(err), nil
	}
	return appkitmcp.JSONResult(map[string]any{
		"name":        a.Name,
		"trigger_url": h.triggerURL(a.Name),
		"secret":      secret,
	})
}

// errorEnvelope renders a webhooks domain error into the uniform, closed-vocabulary
// error envelope. ErrInvalidName additionally pins field:"name" so the agent can
// self-correct the offending argument.
func errorEnvelope(err error) map[string]any {
	e := map[string]any{}
	switch {
	case errors.Is(err, webhooks.ErrNameTaken):
		e["code"] = "duplicate"
		e["message"] = err.Error()
	case errors.Is(err, webhooks.ErrInvalidName):
		e["code"] = "validation"
		e["message"] = err.Error()
		e["field"] = "name"
	case errors.Is(err, webhooks.ErrNotFound):
		e["code"] = "not_found"
		e["message"] = err.Error()
	default:
		e["code"] = "internal"
		e["message"] = "internal error"
	}
	return map[string]any{"error": e}
}

func toolErr(err error) map[string]any {
	b, _ := json.Marshal(errorEnvelope(err))
	return appkitmcp.ErrorResult(string(b))
}
