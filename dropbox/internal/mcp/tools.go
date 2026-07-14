package mcp

import (
	"bytes"
	"context"
	"encoding/base64"
	"encoding/json"
	"errors"
	"io"
	"net"
	"net/http"
	"net/url"
	"strconv"
	"strings"
	"time"

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
	svc               *dropbox.Service
	sourcePortAllowed func(port int) bool
	sourceClient      *http.Client
}

// Tools returns dropbox's service-owned MCP tool declarations. The shared
// appkit MCP transport appends the chassis health and reflection tools.
func Tools(svc *dropbox.Service, sourcePortAllowed func(port int) bool) []appkitmcp.Tool {
	if svc == nil {
		panic("mcp: dropbox service is required")
	}
	if sourcePortAllowed == nil {
		sourcePortAllowed = func(int) bool { return false }
	}
	h := &toolHandlers{
		svc:               svc,
		sourcePortAllowed: sourcePortAllowed,
		sourceClient: &http.Client{
			Transport: &http.Transport{
				DialContext:           (&net.Dialer{Timeout: 5 * time.Second}).DialContext,
				ResponseHeaderTimeout: 5 * time.Second,
			},
			CheckRedirect: func(*http.Request, []*http.Request) error {
				return http.ErrUseLastResponse
			},
		},
	}
	return []appkitmcp.Tool{
		{
			Name:        tool("list"),
			Description: "List entries in the local Dropbox mirror, recursively, ordered by path. Use this to browse what the mirror holds without a local mount. With no arguments it lists every entry. Pass 'path' to scope to a folder prefix (e.g. \"/notes\" matches /notes and /notes/a.md but not /notesxyz; matching is case-insensitive). 'limit' bounds the page (default 1000, clamped to 1..1000). Pagination is cursor-based: when a full page is returned the result includes 'next_cursor'; pass it back as 'cursor' to fetch the next page, and stop when no next_cursor is returned. Each entry has a `kind` of `file` or `dir`; file entries additionally carry {size, hash, rev, updated_at}. Read-only.",
			InputSchema: obj(map[string]any{
				"path":   descTyp("string", "optional; a folder prefix to scope the listing (e.g. \"/notes\"). Empty, absent, or \"/\" lists everything. Case-insensitive."),
				"cursor": descTyp("string", "optional; the opaque next_cursor from a previous page. Omit for the first page."),
				"limit":  descTyp("integer", "optional; page size, default 1000, clamped to 1..1000."),
			}),
			OutputSchema: obj(map[string]any{
				"files": arrayOf(obj(map[string]any{
					"path":       typ("string"),
					"kind":       typ("string"),
					"size":       typ("integer"),
					"hash":       typ("string"),
					"rev":        typ("string"),
					"updated_at": typ("string"),
				}, "path", "kind", "size", "hash", "rev", "updated_at")),
				"next_cursor": typ("string"),
			}, "files"),
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
			OutputSchema: obj(map[string]any{
				"path":           typ("string"),
				"size":           typ("integer"),
				"content_hash":   typ("string"),
				"rev":            typ("string"),
				"updated_at":     typ("string"),
				"content_base64": typ("string"),
			}, "path", "size", "content_hash", "rev", "updated_at", "content_base64"),
			Handler: func(ctx context.Context, args json.RawMessage, _ server.Identity) (map[string]any, error) {
				return h.toolGet(ctx, args)
			},
		},
		{
			Name:        tool("put"),
			Description: "Create or replace one file in the local Dropbox mirror. Supply exactly one of 'source_url' (an allowed loopback http reference, fetched server-side and streamed without a size cap) or standard-base64 'content_base64' (capped at 25 MiB).",
			InputSchema: obj(map[string]any{
				"path":           descTyp("string", "required; destination file path."),
				"source_url":     descTyp("string", "optional; allowed 127.0.0.1 or ::1 http URL with an explicitly allowed port; fetched server-side."),
				"content_base64": descTyp("string", "optional; standard base64 file bytes, limited to 25 MiB decoded."),
			}, "path"),
			OutputSchema: obj(map[string]any{
				"path":         typ("string"),
				"size":         typ("integer"),
				"content_hash": typ("string"),
				"rev":          typ("string"),
			}, "path", "size", "content_hash", "rev"),
			Handler: func(ctx context.Context, args json.RawMessage, id server.Identity) (map[string]any, error) {
				return h.toolPut(ctx, args, id.ClientID)
			},
		},
		{
			Name:        tool("mkdir"),
			Description: "Create a directory in the local Dropbox mirror.",
			InputSchema: obj(map[string]any{
				"path": descTyp("string", "required; directory path to create."),
			}, "path"),
			OutputSchema: obj(map[string]any{"path": typ("string")}, "path"),
			Handler: func(ctx context.Context, args json.RawMessage, id server.Identity) (map[string]any, error) {
				return h.toolMkdir(ctx, args, id.ClientID)
			},
		},
		{
			Name:        tool("delete"),
			Description: "Delete a file or directory tree from the local Dropbox mirror. Directory deletes are recursive and absent paths are idempotent.",
			InputSchema: obj(map[string]any{
				"path": descTyp("string", "required; file or directory path to delete."),
			}, "path"),
			OutputSchema: obj(map[string]any{"removed": typ("integer")}, "removed"),
			Handler: func(ctx context.Context, args json.RawMessage, id server.Identity) (map[string]any, error) {
				return h.toolDelete(ctx, args, id.ClientID)
			},
		},
		{
			Name:        tool("move"),
			Description: "Move a file or directory in the local Dropbox mirror in one operation.",
			InputSchema: obj(map[string]any{
				"from": descTyp("string", "required; existing file or directory path."),
				"to":   descTyp("string", "required; destination path."),
			}, "from", "to"),
			OutputSchema: obj(map[string]any{
				"from": typ("string"),
				"to":   typ("string"),
			}, "from", "to"),
			Handler: func(ctx context.Context, args json.RawMessage, id server.Identity) (map[string]any, error) {
				return h.toolMove(ctx, args, id.ClientID)
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

func typ(t string) map[string]any { return map[string]any{"type": t} }

func arrayOf(items map[string]any) map[string]any {
	return map[string]any{"type": "array", "items": items}
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
			"kind":       r.Kind,
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
	return appkitmcp.StructuredResult(out)
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
		return appkitmcp.ErrorResult(appkitmcp.ErrTooLarge,
			"file is larger than the 25 MiB get limit; fetch it via /content instead"), nil
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
		return appkitmcp.ErrorResult(appkitmcp.ErrTooLarge,
			"file is larger than the 25 MiB get limit; fetch it via /content instead"), nil
	}
	return appkitmcp.StructuredResult(map[string]any{
		"path":           row.Path,
		"size":           row.Size,
		"content_hash":   row.ContentHash,
		"rev":            row.Rev,
		"updated_at":     row.UpdatedAt,
		"content_base64": base64.StdEncoding.EncodeToString(data),
	})
}

// toolPut fetches an allowed source_url server-side or decodes a bounded
// small-file body, then delegates the mutation to the same Service.Write seam
// used by the loopback content route.
func (h *toolHandlers) toolPut(ctx context.Context, raw json.RawMessage, clientID string) (map[string]any, error) {
	var a struct {
		Path          string `json:"path"`
		SourceURL     string `json:"source_url"`
		ContentBase64 string `json:"content_base64"`
	}
	if err := json.Unmarshal(raw, &a); err != nil {
		return nil, err
	}
	if a.Path == "" || (a.SourceURL == "" && a.ContentBase64 == "") || (a.SourceURL != "" && a.ContentBase64 != "") {
		return toolErr(dropbox.ErrValidation), nil
	}
	if a.SourceURL != "" {
		return h.putSourceURL(ctx, a.Path, a.SourceURL, clientID)
	}
	decoded, err := io.ReadAll(io.LimitReader(base64.NewDecoder(base64.StdEncoding, strings.NewReader(a.ContentBase64)), maxGetBytes+1))
	if err != nil {
		return toolErr(dropbox.ErrValidation), nil
	}
	if int64(len(decoded)) > maxGetBytes {
		return appkitmcp.ErrorResult(appkitmcp.ErrTooLarge,
			"file is larger than the 25 MiB put limit; upload it via /content instead"), nil
	}
	row, err := h.svc.Write(ctx, a.Path, bytes.NewReader(decoded), clientID)
	if err != nil {
		return toolErr(err), nil
	}
	return appkitmcp.StructuredResult(map[string]any{
		"path":         row.Path,
		"size":         row.Size,
		"content_hash": row.ContentHash,
		"rev":          row.Rev,
	})
}

func (h *toolHandlers) putSourceURL(ctx context.Context, path, sourceURL, clientID string) (map[string]any, error) {
	u, err := url.Parse(sourceURL)
	if err != nil || u.Scheme != "http" || u.User != nil || u.Hostname() == "" {
		return toolErr(dropbox.ErrValidation), nil
	}
	if host := u.Hostname(); host != "127.0.0.1" && host != "::1" {
		return toolErr(dropbox.ErrValidation), nil
	}
	portText := u.Port()
	port, err := strconv.Atoi(portText)
	if portText == "" || err != nil || port < 1 || port > 65535 || !h.sourcePortAllowed(port) {
		return toolErr(dropbox.ErrValidation), nil
	}

	req, err := http.NewRequestWithContext(ctx, http.MethodGet, u.String(), nil)
	if err != nil {
		return toolErr(dropbox.ErrValidation), nil
	}
	resp, err := h.sourceClient.Do(req)
	if err != nil {
		return sourceUnavailable(sourceURL), nil
	}
	defer resp.Body.Close()
	if resp.StatusCode == http.StatusNotFound {
		return toolErr(dropbox.ErrNotFound), nil
	}
	if resp.StatusCode == http.StatusConflict {
		return toolErr(dropbox.ErrRevMismatch), nil
	}
	if resp.StatusCode < http.StatusOK || resp.StatusCode >= http.StatusMultipleChoices {
		return sourceUnavailable(sourceURL), nil
	}
	row, err := h.svc.Write(ctx, path, resp.Body, clientID)
	if err != nil {
		if errors.Is(err, dropbox.ErrValidation) || errors.Is(err, dropbox.ErrPathEscape) {
			return toolErr(err), nil
		}
		return sourceUnavailable(sourceURL), nil
	}
	return appkitmcp.StructuredResult(map[string]any{
		"path":         row.Path,
		"size":         row.Size,
		"content_hash": row.ContentHash,
		"rev":          row.Rev,
	})
}

func sourceUnavailable(sourceURL string) map[string]any {
	return appkitmcp.ErrorResult(appkitmcp.ErrSourceUnavailable, "source URL is unavailable: "+sourceURL)
}

func (h *toolHandlers) toolMkdir(ctx context.Context, raw json.RawMessage, clientID string) (map[string]any, error) {
	var a struct {
		Path string `json:"path"`
	}
	if err := json.Unmarshal(raw, &a); err != nil {
		return nil, err
	}
	if a.Path == "" {
		return toolErr(dropbox.ErrValidation), nil
	}
	if err := h.svc.Mkdir(ctx, a.Path, clientID); err != nil {
		return toolErr(err), nil
	}
	return appkitmcp.StructuredResult(map[string]any{"path": a.Path})
}

func (h *toolHandlers) toolDelete(ctx context.Context, raw json.RawMessage, clientID string) (map[string]any, error) {
	var a struct {
		Path string `json:"path"`
	}
	if err := json.Unmarshal(raw, &a); err != nil {
		return nil, err
	}
	if a.Path == "" {
		return toolErr(dropbox.ErrValidation), nil
	}
	removed, err := h.svc.Delete(ctx, a.Path, clientID)
	if err != nil {
		return toolErr(err), nil
	}
	return appkitmcp.StructuredResult(map[string]any{"removed": removed})
}

func (h *toolHandlers) toolMove(ctx context.Context, raw json.RawMessage, clientID string) (map[string]any, error) {
	var a struct {
		From string `json:"from"`
		To   string `json:"to"`
	}
	if err := json.Unmarshal(raw, &a); err != nil {
		return nil, err
	}
	if a.From == "" || a.To == "" {
		return toolErr(dropbox.ErrValidation), nil
	}
	if err := h.svc.Move(ctx, a.From, a.To, clientID); err != nil {
		return toolErr(err), nil
	}
	return appkitmcp.StructuredResult(map[string]any{"from": a.From, "to": a.To})
}

// toolErr maps a domain sentinel to the shared typed tool-error envelope.
// Unrecognized errors fall through to internal so they never leak as a bare
// transport error.
func toolErr(err error) map[string]any {
	code := appkitmcp.ErrInternal
	switch {
	case errors.Is(err, dropbox.ErrNotFound):
		code = appkitmcp.ErrNotFound
	case errors.Is(err, dropbox.ErrRevMismatch):
		code = appkitmcp.ErrConflict
	case errors.Is(err, dropbox.ErrValidation):
		code = appkitmcp.ErrValidation
	case errors.Is(err, dropbox.ErrPathEscape):
		code = appkitmcp.ErrValidation
	}
	return appkitmcp.ErrorResult(code, err.Error())
}

// firstN returns the first n chars of s, or all of s when it is shorter — used
// to abbreviate the content hash to 8 chars in the `list` entries.
func firstN(s string, n int) string {
	if len(s) < n {
		return s
	}
	return s[:n]
}
