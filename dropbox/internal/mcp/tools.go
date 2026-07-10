package mcp

import (
	"context"
	"encoding/base64"
	"encoding/json"
	"errors"
	"io"

	appkitmcp "appkit/mcp"
	"appkit/server"

	"dropbox/internal/dropbox"
)

// maxGetBytes caps the file size the `get` tool will return (DECISIONS §5):
// base64-in-JSON is buffered whole (unlike the streamed /content route), so a
// file larger than this is rejected with the too_large code rather than read
// into memory. 25 MiB is a deliberately generous cap.
const maxGetBytes = 25 << 20

// toolPrefix brands every MCP tool name (DECISIONS §1). It is the suite name
// ikigenba + the service name; HTTP route paths are NOT branded.
const toolPrefix = ""

// tool returns the branded, fully-qualified MCP tool name.
func tool(verb string) string { return toolPrefix + verb }

type toolHandlers struct {
	svc *dropbox.Service
}

// Tools returns dropbox's service-owned MCP tool declarations. The shared
// appkit MCP transport appends the chassis health and reflection tools.
func Tools(svc *dropbox.Service) []appkitmcp.Tool {
	if svc == nil {
		panic("mcp: dropbox service is required")
	}
	h := &toolHandlers{svc: svc}
	return []appkitmcp.Tool{
		{
			Name:        tool("list"),
			Description: "List files in the local Dropbox mirror, recursively, ordered by path. Use this to browse what the mirror holds without a local mount. With no arguments it lists every file. Pass 'path' to scope to a folder prefix (e.g. \"/notes\" matches /notes and /notes/a.md but not /notesxyz; matching is case-insensitive). 'limit' bounds the page (default 1000, clamped to 1..1000). Pagination is cursor-based: when a full page is returned the result includes 'next_cursor'; pass it back as 'cursor' to fetch the next page, and stop when no next_cursor is returned. Each entry is {path, size, hash, rev, updated_at}, where 'hash' is the first 8 chars of the content hash (abbreviated) and 'updated_at' is the last time the index row changed. Only files are listed (the mirror index does not track directories). Read-only.",
			InputSchema: obj(map[string]any{
				"path":   descTyp("string", "optional; a folder prefix to scope the listing (e.g. \"/notes\"). Empty, absent, or \"/\" lists everything. Case-insensitive."),
				"cursor": descTyp("string", "optional; the opaque next_cursor from a previous page. Omit for the first page."),
				"limit":  descTyp("integer", "optional; page size, default 1000, clamped to 1..1000."),
			}),
			Handler: func(ctx context.Context, args json.RawMessage, _ server.Identity) (map[string]any, error) {
				return h.toolList(ctx, args)
			},
		},
		{
			Name:        tool("get"),
			Description: "Fetch one file's current bytes and metadata from the local Dropbox mirror. 'path' (required) is a file path resolved case-insensitively through the index. Returns {path, size, content_hash, rev, updated_at, content_base64}, where content_base64 is the standard base64 encoding of the mirror bytes and content_hash is the full content hash. Pass 'rev' to pin a specific revision: if it no longer matches the indexed rev the call returns a conflict (the bytes have moved on). Files larger than 25 MiB are rejected with too_large (base64-in-JSON is not streamed; fetch those via /content instead). Unknown paths return not_found. Read-only.",
			InputSchema: obj(map[string]any{
				"path": descTyp("string", "required; the file path to fetch (case-insensitive)."),
				"rev":  descTyp("string", "optional; pin a revision. A mismatch with the indexed rev returns a conflict."),
			}, "path"),
			Handler: func(ctx context.Context, args json.RawMessage, _ server.Identity) (map[string]any, error) {
				return h.toolGet(ctx, args)
			},
		},
	}
}

// ── schema helpers ──────────────────────────────────────────────────────────

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

// ── tool implementations ─────────────────────────────────────────────────

// toolList renders a path_lower-ordered page of the mirror index for the `list`
// tool (DECISIONS §5 read surface). It clamps the page size to [1,1000]
// (default 1000), runs the read-only query via Service.List, and derives the
// opaque cursor: next_cursor is the last row's path_lower, included ONLY when a
// full page was returned (len==limit) so a caller knows to fetch again. Each
// entry carries the abbreviated 8-char hash under `hash`; the full content_hash
// lives on `get`.
func (h *toolHandlers) toolList(ctx context.Context, raw json.RawMessage) (map[string]any, error) {
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
	return appkitmcp.JSONResult(out)
}

// toolGet fetches one file's bytes + metadata for the `get` tool (DECISIONS §5).
// It validates the required path, enforces the 25 MiB cap on the INDEXED size
// (row.Size) before base64-encoding, and reuses Service.Content for case-folded
// resolution + the optional rev-pin (ErrRevMismatch → conflict). The bytes are
// returned standard-base64 under content_base64; the full content_hash and the
// index's updated_at accompany them.
func (h *toolHandlers) toolGet(ctx context.Context, raw json.RawMessage) (map[string]any, error) {
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
	row, err := h.svc.Content(a.Path, a.Rev)
	if err != nil {
		return toolErr(err), nil
	}
	if row.Size > maxGetBytes {
		return appkitmcp.ErrorResult(toolErrorJSON("too_large",
			"file is larger than the 25 MiB get limit; fetch it via /content instead")), nil
	}
	f, _, err := h.svc.Mirror.Open(row.Path)
	if err != nil {
		return toolErr(err), nil
	}
	defer f.Close()
	data, err := io.ReadAll(io.LimitReader(f, maxGetBytes+1))
	if err != nil {
		return toolErr(err), nil
	}
	if int64(len(data)) > maxGetBytes {
		return appkitmcp.ErrorResult(toolErrorJSON("too_large",
			"file is larger than the 25 MiB get limit; fetch it via /content instead")), nil
	}
	return appkitmcp.JSONResult(map[string]any{
		"path":           row.Path,
		"size":           row.Size,
		"content_hash":   row.ContentHash,
		"rev":            row.Rev,
		"updated_at":     row.UpdatedAt,
		"content_base64": base64.StdEncoding.EncodeToString(data),
	})
}

// toolErr maps a domain sentinel to the shared tool-error envelope
// {error:{code,message}} with isError:true. Unrecognized errors fall through to
// the internal code so they never leak as a bare transport error.
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
	return appkitmcp.ErrorResult(toolErrorJSON(code, err.Error()))
}

// toolErrorJSON marshals the {error:{code,message}} body shared by the
// tool-error results.
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
