package mcp

import (
	"context"
	"encoding/json"
	"errors"
	"net/http"

	"appkit"

	"crm/internal/crm"
)

// toolPrefix brands every MCP tool name (DECISIONS §1). It is the suite name
// ikigenba + the service name; HTTP route paths are NOT branded.
const toolPrefix = "ikigenba_crm_"

// tool returns the branded, fully-qualified MCP tool name. Used by BOTH
// toolDescriptors and dispatchTool so the two sites cannot drift.
func tool(verb string) string { return toolPrefix + verb }

// toolDescriptors returns the fixed six-verb ikigenba_crm_* surface (PLAN.md
// §2). Tool count is a function of verbs, not entities: adding an entity type
// later adds fields, never tools.
//
// crm_save's inputSchema is intentionally loose (PLAN.md §4) — the per-type field
// shapes live in the description, and the server validates per type with
// corrective error messages the agent self-corrects from.
func toolDescriptors() []map[string]any {
	return []map[string]any{
		desc(tool("search"),
			"Filtered, recency-ordered (updated_at DESC) summaries across all entities, or scoped to one 'type'. The first move on almost any request, and the list/paginate verb. 'query' substring-matches the entity's key text (LIKE). 'type' is one of organization|contact|deal|task|interaction. 'filters' is an object of entity-specific predicates, e.g. {\"subject_id\":\"<id>\"} for interactions, {\"tag\":\"newsletter\"} or {\"lifecycle\":\"customer\"} for contacts, {\"stage\":\"proposal\"} or {\"status\":\"open\"} for deals, {\"status\":\"open\"} for tasks. Use 'after_id' (the returned next_cursor) to paginate.",
			obj(map[string]any{
				"query":    typ("string"),
				"type":     typ("string"),
				"filters":  typ("object"),
				"limit":    typ("integer"),
				"after_id": typ("string"),
			})),
		desc(tool("get"),
			"Fetch one entity as a rich card by ULID: a contact comes back with its organization, open deals, recent interactions, and open tasks already attached — one call, full context. The type is resolved from the id automatically.",
			obj(map[string]any{"id": typ("string")}, "id")),
		desc(tool("save"),
			saveDescription,
			obj(map[string]any{
				"type":   enumTyp("string", "organization", "contact", "deal", "task"),
				"id":     descTyp("string", "omit to create; provide to update"),
				"fields": descTyp("object", "entity-specific; see this tool's description"),
				"force":  descTyp("boolean", "override a duplicate match on create"),
			}, "type")),
		desc(tool("delete"),
			"Shallow soft-delete of any entity by type and id (type is one of organization|contact|deal|task|interaction). Owned children (a contact's emails/phones/tags) are soft-deleted too; references from other entities are left intact and simply hidden from reads.",
			obj(map[string]any{
				"type": enumTyp("string", "organization", "contact", "deal", "task", "interaction"),
				"id":   typ("string"),
			}, "type", "id")),
		desc(tool("log"),
			"Append an interaction to a subject's timeline (the most frequent CRM write). 'subject_id' is the id of the contact, organization, or deal the interaction is about (resolved automatically). 'kind' is one of note|call|email|meeting. 'occurred_at' is optional (RFC3339; defaults to now). Append-only — to correct an entry, delete it (ikigenba_crm_delete type:interaction) and log a new one.",
			obj(map[string]any{
				"subject_id":  typ("string"),
				"kind":        enumTyp("string", "note", "call", "email", "meeting"),
				"body":        typ("string"),
				"occurred_at": typ("string"),
			}, "subject_id", "kind")),
		desc(tool("health"),
			"Health + diagnostics for the crm service. Returns the fixed envelope (status, version, service, details) plus the authenticated caller's identity (owner_email, client_id) as established by the platform's auth gate — the end-to-end auth-chain proof. Takes no inputs.",
			obj(map[string]any{})),
	}
}

// saveDescription documents the polymorphic save per-type field shapes (PLAN.md
// §4). The agent reads this to know what to put in `fields`.
const saveDescription = `Create (omit id) or update (provide id) any mutable entity. Upsert. On create, a dedup probe runs (contact by primary email, organization by domain or exact name); a match returns a "duplicate" error with existing_id unless force:true. On update, only the fields you provide change; omit a field to leave it untouched. Set-valued fields (emails, phones, tags, deal contacts) are declarative: the array you send is the complete desired set — omit it to leave it untouched, send [] to clear it.

Fields by type:
- organization: name (required on create), domain (website/email domain; drives dedup).
- contact: given_name, family_name, display_name (derived if absent), org_id, title, lifecycle (subscriber|lead|opportunity|customer; default lead), emails [{email,label}] (first is primary), phones [{phone,label}] E.164 (first is primary), tags ["..."] (e.g. "newsletter"; the monthly newsletter audience is a tag).
- deal: name (required on create), org_id, stage (lead|qualified|proposal|negotiation|won|lost; default lead), amount_cents (integer), currency (default USD), close_date (RFC3339 date), contacts [{id,role}] participants. Note: a deal's status (open|won|lost) is derived from stage and is read-only — do not set it.
- task: title (required on create), status (open|done; default open), due_at, done_at, contact_id, org_id, deal_id (optional subject). Complete a task with fields:{status:"done"}.

Interactions are not saved here — use ikigenba_crm_log.`

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

func enumTyp(t string, vals ...string) map[string]any {
	return map[string]any{"type": t, "enum": vals}
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
	case tool("search"):
		return h.toolSearch(ctx, argsRaw)
	case tool("get"):
		return h.toolGet(ctx, argsRaw)
	case tool("save"):
		return h.toolSave(ctx, argsRaw)
	case tool("delete"):
		return h.toolDelete(ctx, argsRaw)
	case tool("log"):
		return h.toolLog(ctx, argsRaw)
	case tool("health"):
		return h.toolHealth(ctx, id)
	default:
		return nil, errors.New("unknown tool: " + name)
	}
}

// ── tool implementations ─────────────────────────────────────────────────

// toolHealth renders the shared health envelope (status/version/service/details)
// via appkit.Envelope and then adds the authenticated caller's identity — the
// gated, MCP-side variant of the health surface (DECISIONS §6). crm supplies no
// reporter, so details renders as {}.
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

func (h *Handler) toolSearch(ctx context.Context, raw json.RawMessage) (map[string]any, error) {
	var a struct {
		Query   string         `json:"query,omitempty"`
		Type    string         `json:"type,omitempty"`
		Filters map[string]any `json:"filters,omitempty"`
		Limit   int            `json:"limit,omitempty"`
		AfterID string         `json:"after_id,omitempty"`
	}
	if len(raw) > 0 {
		if err := json.Unmarshal(raw, &a); err != nil {
			return nil, err
		}
	}
	items, err := h.svc.Search(ctx, crm.SearchParams{
		Query: a.Query, Type: a.Type, Filters: a.Filters, Limit: a.Limit, AfterID: a.AfterID,
	})
	if err != nil {
		return toolErr(err), nil
	}
	var next string
	if a.Limit > 0 && len(items) == a.Limit {
		next = items[len(items)-1].ID
	}
	return toolResultJSON(map[string]any{"items": items, "next_cursor": next})
}

func (h *Handler) toolGet(ctx context.Context, raw json.RawMessage) (map[string]any, error) {
	var a struct {
		ID string `json:"id"`
	}
	if err := json.Unmarshal(raw, &a); err != nil {
		return nil, err
	}
	card, err := h.svc.Get(ctx, a.ID)
	if err != nil {
		return toolErr(err), nil
	}
	return toolResultJSON(card)
}

func (h *Handler) toolSave(ctx context.Context, raw json.RawMessage) (map[string]any, error) {
	var a struct {
		Type   string          `json:"type"`
		ID     string          `json:"id,omitempty"`
		Fields json.RawMessage `json:"fields,omitempty"`
		Force  bool            `json:"force,omitempty"`
	}
	if err := json.Unmarshal(raw, &a); err != nil {
		return nil, err
	}
	sum, err := h.svc.Save(ctx, a.Type, a.ID, a.Fields, a.Force)
	if err != nil {
		return toolErr(err), nil
	}
	return toolResultJSON(sum)
}

func (h *Handler) toolDelete(ctx context.Context, raw json.RawMessage) (map[string]any, error) {
	var a struct {
		Type string `json:"type"`
		ID   string `json:"id"`
	}
	if err := json.Unmarshal(raw, &a); err != nil {
		return nil, err
	}
	if err := h.svc.Delete(ctx, a.Type, a.ID); err != nil {
		return toolErr(err), nil
	}
	return toolResultJSON(map[string]any{"ok": true})
}

func (h *Handler) toolLog(ctx context.Context, raw json.RawMessage) (map[string]any, error) {
	var a struct {
		SubjectID  string  `json:"subject_id"`
		Kind       string  `json:"kind"`
		Body       string  `json:"body,omitempty"`
		OccurredAt *string `json:"occurred_at,omitempty"`
	}
	if err := json.Unmarshal(raw, &a); err != nil {
		return nil, err
	}
	sum, err := h.svc.Log(ctx, crm.LogInput{
		SubjectID: a.SubjectID, Kind: a.Kind, Body: a.Body, OccurredAt: a.OccurredAt,
	})
	if err != nil {
		return toolErr(err), nil
	}
	return toolResultJSON(sum)
}

// ── shared helpers ──────────────────────────────────────────────────────

func toolResultJSON(v any) (map[string]any, error) {
	b, err := json.Marshal(v)
	if err != nil {
		return nil, err
	}
	return toolResultText(string(b)), nil
}
