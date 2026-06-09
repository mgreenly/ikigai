package mcp

import (
	"context"
	"encoding/base64"
	"encoding/json"
	"errors"
	"net/http"
	"strings"

	"appkit"

	"dropbox/internal/dropbox"

	"eventplane/consumer"
	"eventplane/outbox"
)

// maxGetBytes caps the file size the `get` tool will return (DECISIONS §5):
// base64-in-JSON is buffered whole (unlike the streamed /content route), so a
// file larger than this is rejected with the too_large code rather than read
// into memory. 25 MiB is a deliberately generous cap.
const maxGetBytes = 25 << 20

// toolPrefix brands every MCP tool name (DECISIONS §1). It is the suite name
// ikigenba + the service name; HTTP route paths are NOT branded.
const toolPrefix = ""

// tool returns the branded, fully-qualified MCP tool name. Used by BOTH
// toolDescriptors and dispatchTool so the two sites cannot drift.
func tool(verb string) string { return toolPrefix + verb }

// toolDescriptors returns the four-tool dropbox surface (DECISIONS §7): health,
// reflection, list, and get. dropbox is a download-only mirror daemon — list and
// get are READ verbs (browse/fetch the mirror's files), so the service side
// still has no write verbs: nothing here uploads to Dropbox or mutates the
// mirror. The former dropbox_whoami identity probe and the richer dropbox_health
// status tool have been folded into the one branded health tool — its
// sync/mirror telemetry lives under the envelope's details, supplied by
// dropbox's Spec.Health reporter.
func toolDescriptors() []map[string]any {
	return []map[string]any{
		desc(tool("health"),
			"Health + diagnostics for the dropbox service. Returns the fixed envelope (status, version, service, details) plus the authenticated caller's identity (owner_email, client_id). details carries the mirror daemon's telemetry: mirror_bytes (indexed logical size), disk_free_bytes / disk_total_bytes (mirror filesystem), and failed_files (count of files the sync engine could not download). Takes no inputs.",
			obj(map[string]any{})),
		desc(tool("reflection"),
			"Self-describe dropbox's edges in the event graph. With no arguments, returns the index {publishes:[{type,description}], subscribes:[{source,filter,description}]} — dropbox is a producer, so subscribes is empty. Pass 'event_type' (a published type) for its detail {type, description, schema, example}.",
			obj(map[string]any{
				"event_type": descTyp("string", "optional; a published event type to fetch the schema+example detail for"),
			})),
		desc(tool("list"),
			"List files in the local Dropbox mirror, recursively, ordered by path. Use this to browse what the mirror holds without a local mount. With no arguments it lists every file. Pass 'path' to scope to a folder prefix (e.g. \"/notes\" matches /notes and /notes/a.md but not /notesxyz; matching is case-insensitive). 'limit' bounds the page (default 1000, clamped to 1..1000). Pagination is cursor-based: when a full page is returned the result includes 'next_cursor'; pass it back as 'cursor' to fetch the next page, and stop when no next_cursor is returned. Each entry is {path, size, hash, rev, updated_at}, where 'hash' is the first 8 chars of the content hash (abbreviated) and 'updated_at' is the last time the index row changed. Only files are listed (the mirror index does not track directories). Read-only.",
			obj(map[string]any{
				"path":   descTyp("string", "optional; a folder prefix to scope the listing (e.g. \"/notes\"). Empty, absent, or \"/\" lists everything. Case-insensitive."),
				"cursor": descTyp("string", "optional; the opaque next_cursor from a previous page. Omit for the first page."),
				"limit":  descTyp("integer", "optional; page size, default 1000, clamped to 1..1000."),
			})),
		desc(tool("get"),
			"Fetch one file's current bytes and metadata from the local Dropbox mirror. 'path' (required) is a file path resolved case-insensitively through the index. Returns {path, size, content_hash, rev, updated_at, content_base64}, where content_base64 is the standard base64 encoding of the mirror bytes and content_hash is the full content hash. Pass 'rev' to pin a specific revision: if it no longer matches the indexed rev the call returns a conflict (the bytes have moved on). Files larger than 25 MiB are rejected with too_large (base64-in-JSON is not streamed; fetch those via /content instead). Unknown paths return not_found. Read-only.",
			obj(map[string]any{
				"path": descTyp("string", "required; the file path to fetch (case-insensitive)."),
				"rev":  descTyp("string", "optional; pin a revision. A mismatch with the indexed rev returns a conflict."),
			}, "path")),
	}
}

// ── schema helpers ──────────────────────────────────────────────────────────

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
	case tool("list"):
		return h.toolList(ctx, argsRaw)
	case tool("get"):
		return h.toolGet(ctx, argsRaw)
	default:
		return nil, errors.New("unknown tool: " + name)
	}
}

// ── tool implementations ─────────────────────────────────────────────────

// toolHealth renders the shared health envelope (status/version/service/details)
// via appkit.Envelope and then adds the authenticated caller's identity — the
// end-to-end auth-chain proof (DECISIONS §6). dropbox supplies a reporter
// (Spec.Health), so details carries the mirror/disk telemetry (mirror_bytes,
// disk_free_bytes, disk_total_bytes, failed_files) — namespaced under details,
// never splatted at the top level (DECISIONS §3).
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

// toolReflection self-describes dropbox's edges in the event graph (the
// ikigenba_<svc>_reflection tool). No event_type → the index {publishes,
// subscribes}; with event_type → that published type's {type, description,
// schema, example}. An unknown event_type returns a corrective error listing the
// valid types (the ledger bad_root pattern), not an empty result.
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

// toolList renders a path_lower-ordered page of the mirror index for the `list`
// tool (DECISIONS §5 read surface). It clamps the page size to [1,1000]
// (default 1000), runs the read-only query via Service.List, and derives the
// opaque cursor: next_cursor is the last row's path_lower, included ONLY when a
// full page was returned (len==limit) so a caller knows to fetch again. Each
// entry carries the abbreviated 8-char hash under `hash`; the full content_hash
// lives on `get`.
func (h *Handler) toolList(ctx context.Context, raw json.RawMessage) (map[string]any, error) {
	var a struct {
		Path   string `json:"path"`
		Cursor string `json:"cursor"`
		Limit  int    `json:"limit"`
	}
	if len(raw) > 0 {
		if err := json.Unmarshal(raw, &a); err != nil {
			return nil, err
		}
	}
	limit := a.Limit
	if limit <= 0 {
		limit = 1000
	}
	if limit > 1000 {
		limit = 1000
	}
	rows, err := h.svc.List(a.Path, a.Cursor, limit)
	if err != nil {
		return toolErr(err), nil
	}
	files := []map[string]any{}
	for _, r := range rows {
		files = append(files, map[string]any{
			"path":       r.Path,
			"size":       r.Size,
			"hash":       firstN(r.ContentHash, 8),
			"rev":        r.Rev,
			"updated_at": r.UpdatedAt,
		})
	}
	out := map[string]any{"files": files}
	if len(rows) == limit {
		out["next_cursor"] = rows[len(rows)-1].PathLower
	}
	return toolResultJSON(out)
}

// toolGet fetches one file's bytes + metadata for the `get` tool (DECISIONS §5).
// It validates the required path, enforces the 25 MiB cap on the INDEXED size
// (row.Size) before base64-encoding, and reuses Service.Content for case-folded
// resolution + the optional rev-pin (ErrRevMismatch → conflict). The bytes are
// returned standard-base64 under content_base64; the full content_hash and the
// index's updated_at accompany them.
func (h *Handler) toolGet(ctx context.Context, raw json.RawMessage) (map[string]any, error) {
	var a struct {
		Path string  `json:"path"`
		Rev  *string `json:"rev"`
	}
	if len(raw) > 0 {
		if err := json.Unmarshal(raw, &a); err != nil {
			return nil, err
		}
	}
	if a.Path == "" {
		return toolErr(dropbox.ErrValidation), nil
	}
	data, row, err := h.svc.Content(a.Path, a.Rev)
	if err != nil {
		return toolErr(err), nil
	}
	if row.Size > maxGetBytes {
		return toolResultErr(toolErrorJSON("too_large",
			"file is larger than the 25 MiB get limit; fetch it via /content instead")), nil
	}
	return toolResultJSON(map[string]any{
		"path":           row.Path,
		"size":           row.Size,
		"content_hash":   row.ContentHash,
		"rev":            row.Rev,
		"updated_at":     row.UpdatedAt,
		"content_base64": base64.StdEncoding.EncodeToString(data),
	})
}

// toolErr maps a domain sentinel to the shared tool-error envelope
// {error:{code,message}} with isError:true (the reflectionUnknownTypeError
// shape). Unrecognized errors fall through to the internal code so they never
// leak as a bare transport error.
func toolErr(err error) map[string]any {
	code := "internal"
	switch {
	case errors.Is(err, dropbox.ErrNotFound):
		code = "not_found"
	case errors.Is(err, dropbox.ErrRevMismatch):
		code = "conflict"
	case errors.Is(err, dropbox.ErrValidation):
		code = "validation"
	case errors.Is(err, dropbox.ErrPathEscape):
		code = "validation"
	}
	return toolResultErr(toolErrorJSON(code, err.Error()))
}

// toolErrorJSON marshals the {error:{code,message}} body shared by the tool-error
// results (same shape as reflectionUnknownTypeError).
func toolErrorJSON(code, message string) string {
	env := map[string]any{"error": map[string]any{
		"code":    code,
		"message": message,
	}}
	b, _ := json.Marshal(env)
	return string(b)
}

// firstN returns the first n chars of s, or all of s when it is shorter — used
// to abbreviate the content hash to 8 chars in the `list` entries.
func firstN(s string, n int) string {
	if len(s) < n {
		return s
	}
	return s[:n]
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
