// Package mcp implements a minimal MCP transport for the /mcp endpoint and the
// wiki_* tool surface.
//
// This is the scaffold wiki service (Task 2.1): the only tool is wiki_whoami,
// the end-to-end auth proof. The real wiki domain tools (wiki_ingest_text,
// wiki_ingest_url, wiki_search, …) are added here in later phases, wired to a
// domain service the same way crm wires internal/contacts.
//
// The transport speaks JSON-RPC 2.0 over plain HTTP POST (no SSE/streaming),
// responding with Content-Type: application/json. It carries NO token logic:
// nginx introspects every request via auth_request against the dashboard's
// authorization server and injects X-Owner-Email / X-Client-Id authoritatively
// before forwarding here. The handler is mounted behind the server's
// requireIdentityHeaders gate, so by the time a request arrives the caller
// identity is already established. There is intentionally no bearer parsing, no
// token store, no rate limiter, and no WWW-Authenticate / 401 / 429 emission in
// this package — that all lives in the dashboard.
package mcp

import (
	"context"
	"encoding/json"
	"net/http"

	"wiki/internal/ask"
	"wiki/internal/ingest"
	"wiki/internal/search"
	"wiki/internal/store"
)

// Identity is the authenticated caller, as told to us authoritatively by nginx
// (via the server's requireIdentityHeaders gate) through request headers.
type Identity struct {
	OwnerEmail string
	ClientID   string
}

// Ingester is the MCP surface's dependency on the ingest core (Task 4.1). The
// handler holds the interface, not the concrete *ingest.Core, so the verb tests
// can drive a stub and main.go injects the real core. Collection is always the
// default ("") — no collection arg on the verbs yet (PLAN Decision 4).
type Ingester interface {
	// Ingest persists content to the immutable raw store and spawns the async
	// integration job for owner; it returns the job id + raw outcome.
	Ingest(ctx context.Context, owner, collection string, content []byte, meta store.RawMeta) (ingest.Result, error)
	// IngestURL fetches url server-side, extracts HTML→markdown, then runs the same
	// ingest pipeline as Ingest with source defaulted to the URL.
	IngestURL(ctx context.Context, owner, collection, url string, meta store.RawMeta) (ingest.Result, error)
	// JobStatus reads one job's owner-scoped status (ingest.ErrJobNotFound for a
	// missing/foreign id).
	JobStatus(ctx context.Context, owner, collection, jobID string) (ingest.Status, error)
}

// Searcher is the MCP surface's dependency on the BM25 search backend (Task 5.1).
// The handler holds the interface, not the concrete *search.BM25Index, so the
// verb tests can drive a stub and main.go injects the real index. Search is a
// SYNCHRONOUS read — no agent/LLM, no job — so it is a separate capability from
// Ingester. Collection is always the default ("") — no collection arg on the
// verbs yet (PLAN Decision 4); the search backend defaults an empty collection.
type Searcher interface {
	// Search runs a BM25 query over the (owner, collection) index and returns
	// ranked whole pages plus the collection's index page. limit caps the hits
	// (limit <= 0 selects an implementation default).
	Search(ctx context.Context, owner, collection, query string, limit int) (search.Results, error)
}

// Asker is the MCP surface's dependency on the agentic synthesis pass (Task 6.2 —
// wiki_ask). The handler holds the interface, not the concrete *ask.Asker, so the
// verb tests can drive a stub and main.go injects the real Asker. Ask is ASYNC
// (it rides the agentkit job lifecycle like ingest): Ask returns a job id pollable
// via wiki_job_status, and the synthesized cited answer is filed back as a
// synthesis page (findable via wiki_search once the job succeeds). Like the other
// agentic passes it needs ANTHROPIC_API_KEY, so it may be nil when the service
// boots without an agent backend. Collection is always the default ("") — no
// collection arg on the verbs yet (PLAN Decision 4).
type Asker interface {
	// Ask spawns the async synthesis job for owner and returns its job id. The
	// agent navigates the (owner, collection) page tree index-first, synthesizes a
	// cited answer, and files it back as a synthesis page.
	Ask(ctx context.Context, owner, collection, question string) (ask.Result, error)
}

// Handler is the http.Handler for POST /mcp. It dispatches JSON-RPC methods. It
// holds the ingest core (backs wiki_ingest_text/_url and wiki_job_status), the
// search index (backs wiki_search), and the asker (backs wiki_ask); the
// no-side-effect wiki_whoami needs no dependency. ingest/ask may be nil when the
// service boots without an agent backend (e.g. ANTHROPIC_API_KEY absent) — those
// verbs then return a clear "unavailable" tool-error while wiki_whoami,
// wiki_search, and tools/list keep working. search may be nil only in
// degenerate/test setups; the wiki_search verb returns a clear "search
// unavailable" tool-error in that case.
type Handler struct {
	ingest Ingester
	search Searcher
	ask    Asker
}

// NewHandler builds a Handler over the given ingest core (may be nil to run the
// non-ingest surface only), search index (may be nil to disable wiki_search), and
// asker (may be nil to disable wiki_ask).
func NewHandler(ing Ingester, srch Searcher, asker Asker) *Handler {
	return &Handler{ingest: ing, search: srch, ask: asker}
}

// ServeHTTP dispatches a single JSON-RPC 2.0 request. Identity is read from the
// nginx-injected headers (always present behind requireIdentityHeaders).
func (h *Handler) ServeHTTP(w http.ResponseWriter, r *http.Request) {
	id := Identity{
		OwnerEmail: r.Header.Get("X-Owner-Email"),
		ClientID:   r.Header.Get("X-Client-Id"),
	}
	var req jsonRPCRequest
	if err := json.NewDecoder(r.Body).Decode(&req); err != nil {
		writeJSONRPCError(w, nil, -32700, "parse error")
		return
	}
	switch req.Method {
	case "initialize":
		writeJSONRPCResult(w, req.ID, map[string]any{
			"protocolVersion": "2025-03-26",
			"capabilities":    map[string]any{"tools": map[string]any{}},
			"serverInfo":      map[string]any{"name": "Wiki", "version": "1"},
		})
	case "notifications/initialized":
		// fire-and-forget notification — no response per JSON-RPC.
		w.WriteHeader(http.StatusAccepted)
	case "tools/list":
		writeJSONRPCResult(w, req.ID, map[string]any{"tools": toolDescriptors()})
	case "tools/call":
		h.handleToolCall(r.Context(), w, req, id)
	default:
		writeJSONRPCError(w, req.ID, -32601, "method not found")
	}
}

type jsonRPCRequest struct {
	JSONRPC string          `json:"jsonrpc"`
	ID      json.RawMessage `json:"id,omitempty"`
	Method  string          `json:"method"`
	Params  json.RawMessage `json:"params,omitempty"`
}

func writeJSONRPCResult(w http.ResponseWriter, id json.RawMessage, result any) {
	w.Header().Set("Content-Type", "application/json; charset=utf-8")
	_ = json.NewEncoder(w).Encode(map[string]any{
		"jsonrpc": "2.0",
		"id":      idOrNull(id),
		"result":  result,
	})
}

func writeJSONRPCError(w http.ResponseWriter, id json.RawMessage, code int, msg string) {
	w.Header().Set("Content-Type", "application/json; charset=utf-8")
	_ = json.NewEncoder(w).Encode(map[string]any{
		"jsonrpc": "2.0",
		"id":      idOrNull(id),
		"error":   map[string]any{"code": code, "message": msg},
	})
}

func idOrNull(id json.RawMessage) any {
	if len(id) == 0 {
		return nil
	}
	return json.RawMessage(id)
}

// Result-shape helpers for tool calls. MCP `tools/call` returns
// {content: [{type: "text", text: "..."}], isError?: bool}.
func toolResultText(text string) map[string]any {
	return map[string]any{"content": []map[string]any{{"type": "text", "text": text}}}
}

func toolResultErr(msg string) map[string]any {
	return map[string]any{"isError": true, "content": []map[string]any{{"type": "text", "text": msg}}}
}
