package mcp

import (
	"context"
	"encoding/json"
	"errors"
	"net/http"
	"strings"
	"time"

	"appkit"

	"cron/internal/cron"
	"cron/internal/crontab"

	"eventplane/consumer"
	"eventplane/outbox"
)

// toolPrefix brands every MCP tool name (DECISIONS §1): the suite name ikigenba
// + the service name. HTTP route paths are NOT branded.
const toolPrefix = ""

// tool returns the branded, fully-qualified MCP tool name. Used by BOTH
// toolDescriptors and dispatchTool so the two sites cannot drift.
func tool(verb string) string { return toolPrefix + verb }

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

// toolDescriptors returns the crontab CRUD surface plus the shared health +
// reflection tools (the same fixed shape crm/ledger expose). A crontab entry is
// {name, expr}: name is the identity and the cron.<name> event suffix; expr is
// 5-field cron (minute hour day-of-month month day-of-week), evaluated in UTC.
func toolDescriptors() []map[string]any {
	return []map[string]any{
		desc(tool("create"),
			"Create a named schedule. 'name' is the identity and the suffix of the emitted event type cron.<name> — lowercase letters, digits and hyphens only. 'expr' is a 5-field cron expression evaluated in UTC. The expression is validated here (fails loudly naming the bad field) and rejected if 'name' already exists.",
			obj(map[string]any{
				"name": descTyp("string", "schedule identity / cron.<name> suffix; [a-z0-9-]"),
				"expr": descTyp("string", "5-field cron (min hour dom mon dow), UTC. Operators: *, n, lists a,b, ranges a-b, steps */n and a-b/n; dow 0 or 7 = Sunday. e.g. \"0 3 * * *\" = daily 03:00 UTC"),
			}, "name", "expr")),
		desc(tool("list"),
			"List every schedule, ordered by name. Each entry returns {name, expr, created_at, updated_at, last_slot}. The authoritative view of which cron.<name> events exist. Takes no inputs.",
			obj(map[string]any{})),
		desc(tool("get"),
			"Fetch one schedule by name: {name, expr, created_at, updated_at, last_slot}. 'last_slot' is the last minute (RFC3339 UTC) the schedule fired, or null if it never has.",
			obj(map[string]any{"name": descTyp("string", "schedule name")}, "name")),
		desc(tool("update"),
			"Change an existing schedule's cron expression. 'expr' is validated here exactly as on create. 'name' is immutable (the event type cron.<name> depends on it) — to rename, delete and create. The double-emit guard (last_slot) is deliberately NOT reset on an expr change.",
			obj(map[string]any{
				"name": descTyp("string", "schedule name"),
				"expr": descTyp("string", "new 5-field cron expression, UTC"),
			}, "name", "expr")),
		desc(tool("delete"),
			"Delete a schedule by name. The cron.<name> event type stops being published. Consumers subscribed to it simply stop receiving (cron is subscriber-blind).",
			obj(map[string]any{"name": descTyp("string", "schedule name")}, "name")),
		desc(tool("health"),
			"Health + diagnostics for the cron service. Returns the fixed envelope (status, version, service, details) plus the authenticated caller's identity (owner_email, client_id). Takes no inputs.",
			obj(map[string]any{})),
		desc(tool("reflection"),
			"Self-describe cron's edges in the event graph. With no arguments, returns the index {publishes:[{type,description}], subscribes:[]} — cron is a producer (subscribes empty), and publishes is the live cron.<name> types, one per crontab row. Pass 'event_type' (a published cron.<name>) for its detail {type, description, schema, example}.",
			obj(map[string]any{
				"event_type": descTyp("string", "optional; a published cron.<name> type to fetch schema+example for"),
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
	case tool("create"):
		return h.toolCreate(ctx, argsRaw)
	case tool("list"):
		return h.toolList(ctx)
	case tool("get"):
		return h.toolGet(ctx, argsRaw)
	case tool("update"):
		return h.toolUpdate(ctx, argsRaw)
	case tool("delete"):
		return h.toolDelete(ctx, argsRaw)
	case tool("health"):
		return h.toolHealth(ctx, id)
	case tool("reflection"):
		return h.toolReflection(argsRaw)
	default:
		return nil, errors.New("unknown tool: " + name)
	}
}

// ── tool implementations ─────────────────────────────────────────────────

func (h *Handler) toolCreate(ctx context.Context, raw json.RawMessage) (map[string]any, error) {
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
	return toolResultJSON(renderEntry(*e))
}

func (h *Handler) toolList(ctx context.Context) (map[string]any, error) {
	entries, err := h.store.List(ctx)
	if err != nil {
		return toolErr(err), nil
	}
	items := make([]map[string]any, 0, len(entries))
	for _, e := range entries {
		items = append(items, renderEntry(e))
	}
	return toolResultJSON(map[string]any{"items": items})
}

func (h *Handler) toolGet(ctx context.Context, raw json.RawMessage) (map[string]any, error) {
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
	return toolResultJSON(renderEntry(*e))
}

func (h *Handler) toolUpdate(ctx context.Context, raw json.RawMessage) (map[string]any, error) {
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
	return toolResultJSON(renderEntry(*e))
}

func (h *Handler) toolDelete(ctx context.Context, raw json.RawMessage) (map[string]any, error) {
	var a struct {
		Name string `json:"name"`
	}
	if err := json.Unmarshal(raw, &a); err != nil {
		return nil, err
	}
	if err := h.store.Delete(ctx, a.Name); err != nil {
		return toolErr(err), nil
	}
	return toolResultJSON(map[string]any{"ok": true})
}

// toolHealth renders the shared health envelope plus the authenticated caller's
// identity — the gated MCP-side variant of the health surface (DECISIONS §6).
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
	env := appkit.Envelope(h.version, h.service, details)
	env["owner_email"] = id.OwnerEmail
	env["client_id"] = id.ClientID
	return toolResultJSON(env)
}

// toolReflection self-describes cron's edges in the event graph. The publishes
// half is the LIVE cron.<name> set (h.publishes(), read from the crontab); the
// subscribes half is empty (cron is a producer). An unknown event_type returns a
// corrective error listing the valid types (the crm/ledger pattern).
func (h *Handler) toolReflection(raw json.RawMessage) (map[string]any, error) {
	var a struct {
		EventType string `json:"event_type,omitempty"`
	}
	if len(raw) > 0 {
		if err := json.Unmarshal(raw, &a); err != nil {
			return nil, err
		}
	}
	reg := outbox.Registry{}
	if h.publishes != nil {
		reg = h.publishes()
	}
	if a.EventType != "" {
		detail, err := reg.Detail(a.EventType)
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
		"publishes":  reg.Index(),
		"subscribes": renderSubscriptions(h.subscriptions),
	})
}

// renderSubscriptions flattens the live subscription provider to reflection
// in-edges. A nil provider (cron) renders as an empty list.
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

func reflectionUnknownTypeError(e *outbox.UnknownEventTypeError) string {
	env := map[string]any{"error": map[string]any{
		"code":    "unknown_event_type",
		"message": "unknown event_type " + e.Type + "; valid types: " + strings.Join(e.Valid, ", "),
	}}
	b, _ := json.Marshal(env)
	return string(b)
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

func toolResultJSON(v any) (map[string]any, error) {
	b, err := json.Marshal(v)
	if err != nil {
		return nil, err
	}
	return toolResultText(string(b)), nil
}
