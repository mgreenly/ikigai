package dropbox

import (
	"encoding/json"
	"net/http"
	"strconv"
)

// ListHandler returns the loopback GET /list handler (ADR-import-sync §3): the
// enumeration twin of GET /content. A peer service that wants to walk a mirror
// subtree over plain loopback HTTP (sites' `sync`) needs to list it without
// going through the identity-gated, off-box MCP `list` tool. This route gives
// it that — a thin wrapper over Service.List, cursor-paginated.
//
// Trust model (mirrors GET /content / GET /feed exactly): the route is
// UNAUTHENTICATED and loopback-only — one box is one owner, so the perimeter is
// "it is on 127.0.0.1". The HANDLER is the primary guard: any nginx-injected
// identity header (X-Owner-Email OR X-Forwarded-Proto — nginx always sets the
// latter) means the request was proxied through the public, authenticated front
// door, which must never reach /list; we 404 it. This is the same defence copied
// verbatim from content.go (and eventplane/outbox/feed.go ~line 50).
//
// Query params: `path` (a display-path prefix; empty or "/" lists everything),
// `cursor` (an opaque value, = a prior response's next_cursor), and `limit`
// (page size; default 1000, clamped to [1,1000] — the same bounds as the MCP
// `list` tool, mcp/tools.go toolList).
//
// Response shape mirrors the MCP `list` tool — {files:[{path, size, hash, rev,
// updated_at}], next_cursor} — with one intentional divergence (ADR decision 4):
// `hash` is the FULL content_hash, not the 8-char abbreviation the MCP tool
// emits. The consumer is code, not an LLM, so there is no brevity concern and the
// full hash keeps the route useful for a future per-file sync manifest.
// next_cursor is the last row's path_lower, included ONLY when a full page was
// returned (len(rows)==limit) so the caller knows to fetch again (same "full page
// ⇒ more" rule as toolList).
//
// On a Service.List error we return 500 plain: this is a machine route (not the
// path-confinement-sensitive /content), so an internal error is reported as one
// rather than masked as a 404.
func (s *Service) ListHandler() http.Handler {
	return http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		// Primary guard, copied from content.go — keep the public side out by the
		// handler, not by nginx.
		if r.Header.Get("X-Owner-Email") != "" || r.Header.Get("X-Forwarded-Proto") != "" {
			http.Error(w, "not found", http.StatusNotFound)
			return
		}

		q := r.URL.Query()
		path := q.Get("path")
		cursor := q.Get("cursor")

		// Parse + clamp limit to [1,1000], default 1000 — the same bounds as
		// mcp/tools.go toolList. A non-integer or absent value falls back to the
		// default rather than erroring (lenient like the MCP tool).
		limit := 1000
		if v := q.Get("limit"); v != "" {
			if n, err := strconv.Atoi(v); err == nil {
				limit = n
			}
		}
		if limit <= 0 {
			limit = 1000
		}
		if limit > 1000 {
			limit = 1000
		}

		rows, err := s.List(path, cursor, limit)
		if err != nil {
			http.Error(w, "internal error", http.StatusInternalServerError)
			return
		}

		files := []map[string]any{}
		for _, row := range rows {
			files = append(files, map[string]any{
				"path":       row.Path,
				"size":       row.Size,
				"hash":       row.ContentHash, // FULL hash (ADR decision 4), not abbreviated
				"rev":        row.Rev,
				"updated_at": row.UpdatedAt,
			})
		}
		out := map[string]any{"files": files}
		if len(rows) == limit {
			out["next_cursor"] = rows[len(rows)-1].PathLower
		}

		w.Header().Set("Content-Type", "application/json")
		_ = json.NewEncoder(w).Encode(out)
	})
}
