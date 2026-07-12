package mcp

import (
	"context"
	"encoding/json"
	"time"

	appkitmcp "appkit/mcp"
	"appkit/server"

	"cron/internal/cron"
	"cron/internal/crontab"
)

// toolPrefix brands every MCP tool name (DECISIONS §1): the suite name ikigenba
// + the service name. HTTP route paths are NOT branded.
const toolPrefix = ""

// tool returns the branded, fully-qualified MCP tool name. Used by BOTH
// toolDescriptors and dispatchTool so the two sites cannot drift.
func tool(verb string) string { return toolPrefix + verb }

type toolHandlers struct {
	store *crontab.Store
}

// parseError wraps a cron.Parse failure so the error envelope can tag field:expr
// and the dispatcher can keep parse-validation a one-liner at the boundary.
type parseError struct{ err error }

func (e *parseError) Error() string { return e.err.Error() }
func (e *parseError) Unwrap() error { return e.err }

// validateExpr parses an expr at the MCP boundary (the authority — decisions §2):
// it fails loud, naming the bad field, in addition to the DB CHECK. A nil return
// means the expr is structurally valid 5-field cron.
func validateExpr(expr string) error {
	if _, err := cron.Parse(expr); err != nil {
		return &parseError{err: err}
	}
	return nil
}

// Tools returns cron's service-owned MCP tool declarations. The shared appkit
// MCP transport appends the chassis health and reflection tools. A crontab entry
// is {name, expr}: name is the identity and the event subject suffix; expr
// is 5-field cron (minute hour day-of-month month day-of-week), evaluated in UTC.
func Tools(store *crontab.Store) []appkitmcp.Tool {
	if store == nil {
		panic("mcp: crontab store is required")
	}
	h := &toolHandlers{store: store}
	return []appkitmcp.Tool{
		desc(tool("create"),
			"Create a named schedule. 'name' is the identity and the suffix of the emitted event cron:tick/<name> — lowercase letters, digits and hyphens only. 'expr' is a 5-field cron expression evaluated in UTC. The expression is validated here (fails loudly naming the bad field) and rejected if 'name' already exists.",
			obj(map[string]any{
				"name": descTyp("string", "schedule identity / cron:tick/<name> suffix; [a-z0-9-]"),
				"expr": descTyp("string", "5-field cron (min hour dom mon dow), UTC. Operators: *, n, lists a,b, ranges a-b, steps */n and a-b/n; dow 0 or 7 = Sunday. e.g. \"0 3 * * *\" = daily 03:00 UTC"),
			}, "name", "expr"),
			func(ctx context.Context, args json.RawMessage, _ server.Identity) (map[string]any, error) {
				return h.toolCreate(ctx, args)
			}),
		desc(tool("list"),
			"List every schedule, ordered by name. Each entry returns {name, expr, created_at, updated_at, last_slot}. The authoritative view of which cron:tick/<name> events exist. Takes no inputs.",
			obj(map[string]any{}),
			func(ctx context.Context, _ json.RawMessage, _ server.Identity) (map[string]any, error) {
				return h.toolList(ctx)
			}),
		desc(tool("get"),
			"Fetch one schedule by name: {name, expr, created_at, updated_at, last_slot}. 'last_slot' is the last minute (RFC3339 UTC) the schedule fired, or null if it never has.",
			obj(map[string]any{"name": descTyp("string", "schedule name")}, "name"),
			func(ctx context.Context, args json.RawMessage, _ server.Identity) (map[string]any, error) {
				return h.toolGet(ctx, args)
			}),
		desc(tool("update"),
			"Change an existing schedule's cron expression. 'expr' is validated here exactly as on create. 'name' is immutable (the event cron:tick/<name> depends on it) — to rename, delete and create. The double-emit guard (last_slot) is deliberately NOT reset on an expr change.",
			obj(map[string]any{
				"name": descTyp("string", "schedule name"),
				"expr": descTyp("string", "new 5-field cron expression, UTC"),
			}, "name", "expr"),
			func(ctx context.Context, args json.RawMessage, _ server.Identity) (map[string]any, error) {
				return h.toolUpdate(ctx, args)
			}),
		desc(tool("delete"),
			"Delete a schedule by name. The cron:tick/<name> event stops being published. Consumers subscribed to it simply stop receiving (cron is subscriber-blind).",
			obj(map[string]any{"name": descTyp("string", "schedule name")}, "name"),
			func(ctx context.Context, args json.RawMessage, _ server.Identity) (map[string]any, error) {
				return h.toolDelete(ctx, args)
			}),
	}
}

func desc(name, description string, schema map[string]any, handler func(context.Context, json.RawMessage, server.Identity) (map[string]any, error)) appkitmcp.Tool {
	return appkitmcp.Tool{Name: name, Description: description, InputSchema: schema, Handler: handler}
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

// ── tool implementations ─────────────────────────────────────────────────

func (h *toolHandlers) toolCreate(ctx context.Context, raw json.RawMessage) (map[string]any, error) {
	var a struct {
		Name string `json:"name"`
		Expr string `json:"expr"`
	}
	if err := json.Unmarshal(raw, &a); err != nil {
		return nil, err
	}
	if err := validateExpr(a.Expr); err != nil {
		return toolErr(err), nil
	}
	e, err := h.store.Create(ctx, a.Name, a.Expr, time.Now())
	if err != nil {
		return toolErr(err), nil
	}
	return appkitmcp.JSONResult(renderEntry(*e))
}

func (h *toolHandlers) toolList(ctx context.Context) (map[string]any, error) {
	entries, err := h.store.List(ctx)
	if err != nil {
		return toolErr(err), nil
	}
	items := make([]map[string]any, 0, len(entries))
	for _, e := range entries {
		items = append(items, renderEntry(e))
	}
	return appkitmcp.JSONResult(map[string]any{"items": items})
}

func (h *toolHandlers) toolGet(ctx context.Context, raw json.RawMessage) (map[string]any, error) {
	var a struct {
		Name string `json:"name"`
	}
	if err := json.Unmarshal(raw, &a); err != nil {
		return nil, err
	}
	e, err := h.store.Get(ctx, a.Name)
	if err != nil {
		return toolErr(err), nil
	}
	return appkitmcp.JSONResult(renderEntry(*e))
}

func (h *toolHandlers) toolUpdate(ctx context.Context, raw json.RawMessage) (map[string]any, error) {
	var a struct {
		Name string `json:"name"`
		Expr string `json:"expr"`
	}
	if err := json.Unmarshal(raw, &a); err != nil {
		return nil, err
	}
	if err := validateExpr(a.Expr); err != nil {
		return toolErr(err), nil
	}
	e, err := h.store.Update(ctx, a.Name, a.Expr, time.Now())
	if err != nil {
		return toolErr(err), nil
	}
	return appkitmcp.JSONResult(renderEntry(*e))
}

func (h *toolHandlers) toolDelete(ctx context.Context, raw json.RawMessage) (map[string]any, error) {
	var a struct {
		Name string `json:"name"`
	}
	if err := json.Unmarshal(raw, &a); err != nil {
		return nil, err
	}
	if err := h.store.Delete(ctx, a.Name); err != nil {
		return toolErr(err), nil
	}
	return appkitmcp.JSONResult(map[string]any{"ok": true})
}

// ── shared helpers ──────────────────────────────────────────────────────

// renderEntry shapes a crontab.Entry for the wire: name/expr plus the timestamps
// in canonical UTC RFC3339, with last_slot null until the schedule first fires.
func renderEntry(e crontab.Entry) map[string]any {
	m := map[string]any{
		"name":       e.Name,
		"expr":       e.Expr,
		"created_at": e.CreatedAt.UTC().Format(time.RFC3339),
		"updated_at": e.UpdatedAt.UTC().Format(time.RFC3339),
		"last_slot":  nil,
	}
	if e.LastSlot != nil {
		m["last_slot"] = e.LastSlot.UTC().Format(time.RFC3339)
	}
	return m
}
