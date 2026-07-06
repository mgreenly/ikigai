package mcp

import (
	"context"
	_ "embed"
	"encoding/json"
	"errors"

	appkitmcp "appkit/mcp"
	"appkit/server"

	"crm/internal/crm"
)

// toolPrefix brands every MCP tool name. It is currently empty: HTTP route
// paths are not branded, and the wire tool names remain the committed crm verbs.
const toolPrefix = ""

func tool(verb string) string { return toolPrefix + verb }

type toolHandlers struct {
	svc *crm.Service
}

// Tools returns crm's service-owned MCP tool declarations. The shared appkit
// MCP transport appends the chassis health and reflection tools.
func Tools(svc *crm.Service) []appkitmcp.Tool {
	if svc == nil {
		panic("mcp: crm service is required")
	}
	h := &toolHandlers{svc: svc}
	return []appkitmcp.Tool{
		{
			Name:        tool("search"),
			Description: "Filtered, recency-ordered (updated_at DESC) summaries across all entities, or scoped to one 'type'; the list/paginate verb. 'query' substring-matches the entity's key text (LIKE). 'type' is one of organization|contact|deal|task|interaction. 'filters' is an object of entity-specific predicates, e.g. {\"subject_id\":\"<id>\"} for interactions, {\"tag\":\"newsletter\"} or {\"lifecycle\":\"customer\"} for contacts, {\"stage\":\"proposal\"} or {\"status\":\"open\"} for deals, {\"status\":\"open\"} for tasks. Use 'after_id' (the returned next_cursor) to paginate.",
			InputSchema: obj(map[string]any{
				"query":    typ("string"),
				"type":     typ("string"),
				"filters":  typ("object"),
				"limit":    typ("integer"),
				"after_id": typ("string"),
			}),
			Handler: func(ctx context.Context, args json.RawMessage, _ server.Identity) (map[string]any, error) {
				return h.toolSearch(ctx, args)
			},
		},
		{
			Name:        tool("get"),
			Description: "Fetch one entity as a rich card by ULID: a contact comes back with its organization, open deals, recent interactions, and open tasks already attached. The type is resolved from the id automatically.",
			InputSchema: obj(map[string]any{"id": typ("string")}, "id"),
			Handler: func(ctx context.Context, args json.RawMessage, _ server.Identity) (map[string]any, error) {
				return h.toolGet(ctx, args)
			},
		},
		{
			Name:        tool("save"),
			Description: saveDescription,
			InputSchema: obj(map[string]any{
				"type":   enumTyp("string", "organization", "contact", "deal", "task"),
				"id":     descTyp("string", "omit to create; provide to update"),
				"fields": descTyp("object", "entity-specific; see this tool's description"),
				"force":  descTyp("boolean", "override a duplicate match on create"),
			}, "type"),
			Handler: func(ctx context.Context, args json.RawMessage, _ server.Identity) (map[string]any, error) {
				return h.toolSave(ctx, args)
			},
		},
		{
			Name:        tool("delete"),
			Description: "Shallow soft-delete of any entity by type and id (type is one of organization|contact|deal|task|interaction). Owned children (a contact's emails/phones/tags) are soft-deleted too; references from other entities are left intact and simply hidden from reads.",
			InputSchema: obj(map[string]any{
				"type": enumTyp("string", "organization", "contact", "deal", "task", "interaction"),
				"id":   typ("string"),
			}, "type", "id"),
			Handler: func(ctx context.Context, args json.RawMessage, _ server.Identity) (map[string]any, error) {
				return h.toolDelete(ctx, args)
			},
		},
		{
			Name:        tool("log"),
			Description: "Append an interaction to a subject's timeline. 'subject_id' is the id of the contact, organization, or deal the interaction is about (resolved automatically). 'kind' is one of note|call|email|meeting. 'occurred_at' is optional (RFC3339; defaults to now). Append-only — to correct an entry, delete it (delete type:interaction) and log a new one.",
			InputSchema: obj(map[string]any{
				"subject_id":  typ("string"),
				"kind":        enumTyp("string", "note", "call", "email", "meeting"),
				"body":        typ("string"),
				"occurred_at": typ("string"),
			}, "subject_id", "kind"),
			Handler: func(ctx context.Context, args json.RawMessage, _ server.Identity) (map[string]any, error) {
				return h.toolLog(ctx, args)
			},
		},
		{
			Name:        tool("guide"),
			Description: "Return the CRM usage guide — the entity model, per-type field catalogs, and worked basic and advanced examples. Read once before your first save.",
			InputSchema: obj(map[string]any{}),
			Handler: func(context.Context, json.RawMessage, server.Identity) (map[string]any, error) {
				return h.toolGuide()
			},
		},
	}
}

// saveDescription documents the polymorphic save per-type field shapes. The
// agent reads this to know what to put in `fields`.
const saveDescription = "Create (omit id) or update (provide id) an organization, contact, deal, or " +
	"task. Upsert. Only the fields you send change; set-valued fields (emails, " +
	"phones, tags, deal contacts) replace the whole set — omit to leave " +
	"untouched, send [] to clear. On create a dedup probe runs; a match returns a " +
	"duplicate error with existing_id unless force:true. Interactions are not " +
	"saved here — use log."

//go:embed guide.md
var guideDoc string

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

func (h *toolHandlers) toolGuide() (map[string]any, error) {
	return appkitmcp.TextResult(guideDoc), nil
}

func (h *toolHandlers) toolSearch(ctx context.Context, raw json.RawMessage) (map[string]any, error) {
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
	return appkitmcp.JSONResult(map[string]any{"items": items, "next_cursor": next})
}

func (h *toolHandlers) toolGet(ctx context.Context, raw json.RawMessage) (map[string]any, error) {
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
	return appkitmcp.JSONResult(card)
}

func (h *toolHandlers) toolSave(ctx context.Context, raw json.RawMessage) (map[string]any, error) {
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
	return appkitmcp.JSONResult(sum)
}

func (h *toolHandlers) toolDelete(ctx context.Context, raw json.RawMessage) (map[string]any, error) {
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
	return appkitmcp.JSONResult(map[string]any{"ok": true})
}

func (h *toolHandlers) toolLog(ctx context.Context, raw json.RawMessage) (map[string]any, error) {
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
	return appkitmcp.JSONResult(sum)
}

// errorEnvelope renders a crm domain error into the uniform, closed-vocabulary
// error envelope. The message is the entity's corrective text; `field` and
// `existing_id` are surfaced when present.
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

func toolErr(err error) map[string]any {
	b, _ := json.Marshal(errorEnvelope(err))
	return appkitmcp.ErrorResult(string(b))
}
