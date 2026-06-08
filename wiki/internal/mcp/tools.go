package mcp

import (
	"context"
	"encoding/json"
	"errors"
	"net/http"

	"appkit"

	"wiki/internal/ingest"
	"wiki/internal/store"
)

// toolPrefix brands every MCP tool name (DECISIONS §1). It is the suite name
// ikigenba + the service name; HTTP route paths are NOT branded.
const toolPrefix = ""

// tool returns the branded, fully-qualified MCP tool name. Used by BOTH
// toolDescriptors and dispatchTool so the two sites cannot drift.
func tool(verb string) string { return toolPrefix + verb }

// toolDescriptors returns the wiki MCP surface. After Task 6.2 that is six verbs:
// health (the auth proof + diagnostics), ingest_text
// (the inline-bytes ingest trigger), ingest_url (the
// service-fetches-a-URL ingest trigger), search (the synchronous
// BM25 read over curated whole pages), ask (the agentic, async
// synthesis read that returns a cited answer and files it back as a synthesis
// page), and job_status (the async-job status read). Schemas are
// hand-coded with per-field docs to improve LLM hinting.
func toolDescriptors() []map[string]any {
	return []map[string]any{
		desc(tool("health"), "Health + diagnostics for the wiki service. Returns the fixed envelope (status, version, service, details) plus the authenticated caller's identity (owner_email, client_id). Takes no inputs.", obj(map[string]any{})),
		desc(tool("ingest_text"),
			"Ingest inline text into your personal wiki. The bytes are stored immutably (sha256-keyed) and an asynchronous integration agent files them into curated, cross-linked pages. Returns a job_id you can poll with job_status. Provide provenance (title/source/tags) so the wiki can trace every page back to its origin.",
			obj(map[string]any{
				"content": strField("The raw text to ingest (the document body)."),
				"title":   strField("Optional human title for this document (provenance)."),
				"source":  strField("Optional origin of the text — a URL, a chat label, a filename (provenance)."),
				"tags": map[string]any{
					"type":        "array",
					"items":       map[string]any{"type": "string"},
					"description": "Optional tags to stamp onto the document (provenance).",
				},
			}, "content")),
		desc(tool("ingest_url"),
			"Ingest a web page into your personal wiki by URL. The service fetches the URL itself (http/https only) and extracts the page to markdown, then files it exactly like ingest_text: stored immutably (sha256-keyed) and integrated by an asynchronous agent into curated, cross-linked pages. The page's URL is recorded as the source. Returns a job_id you can poll with job_status.",
			obj(map[string]any{
				"url":   strField("The http/https URL to fetch and ingest."),
				"title": strField("Optional human title; defaults to the page <title> or a URL-derived title (provenance)."),
				"source": strField("Optional origin override; defaults to the URL itself (provenance)."),
				"tags": map[string]any{
					"type":        "array",
					"items":       map[string]any{"type": "string"},
					"description": "Optional tags to stamp onto the document (provenance).",
				},
			}, "url")),
		desc(tool("search"),
			"Search your personal wiki and get back whole curated pages — your own pre-curated 'internet'. This is a fast, synchronous BM25 keyword search over the integrated pages (no agent, no LLM); call it freely while exploring. The collection's index page (the navigation entry point) is returned first when present, followed by the matching pages ranked best-first, each as a complete page (path, title, full markdown body, relevance score where higher = more relevant). A query with no matches still returns the index page.",
			obj(map[string]any{
				"query": strField("The search query (free text). Plain keywords work best; FTS5 operator punctuation is sanitized away."),
				"limit": map[string]any{
					"type":        "integer",
					"description": "Optional maximum number of ranked pages to return (default 10, capped at 50). The index page is always returned in addition and does not count against this limit.",
				},
			}, "query")),
		desc(tool("ask"),
			"Ask your personal wiki a question and get a synthesized, cited answer. Runs an asynchronous agent that navigates your wiki index-first, reads the relevant curated pages, and composes a direct answer citing the pages it used — then files that answer back as a synthesis page so future questions compound. Returns a job_id to poll with job_status; when the job succeeds, the cited synthesis page is searchable. Answers are drawn ONLY from what your wiki already contains; prefer search for quick lookups.",
			obj(map[string]any{
				"question": strField("The question to answer from your wiki (free text)."),
			}, "question")),
		desc(tool("job_status"),
			"Read the status of an asynchronous wiki job (e.g. an ingest integration pass or an ask synthesis) by its job_id. Returns the lifecycle state (running|succeeded|failed), start/end timestamps, any error, and token usage. Owner-scoped: you can only read your own jobs.",
			obj(map[string]any{
				"job_id": strField("The job id returned by ingest_text, ask, or another async verb."),
			}, "job_id")),
	}
}

func strField(desc string) map[string]any {
	return map[string]any{"type": "string", "description": desc}
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

func (h *Handler) dispatchTool(ctx context.Context, name string, args json.RawMessage, id Identity) (map[string]any, error) {
	switch name {
	case tool("health"):
		return h.toolHealth(ctx, id)
	case tool("ingest_text"):
		return h.toolIngestText(ctx, args, id)
	case tool("ingest_url"):
		return h.toolIngestURL(ctx, args, id)
	case tool("search"):
		return h.toolSearch(ctx, args, id)
	case tool("ask"):
		return h.toolAsk(ctx, args, id)
	case tool("job_status"):
		return h.toolJobStatus(ctx, args, id)
	default:
		return nil, errors.New("unknown tool: " + name)
	}
}

// ── ingest verbs ──────────────────────────────────────────────────────────

type ingestTextArgs struct {
	Content string   `json:"content"`
	Title   string   `json:"title"`
	Source  string   `json:"source"`
	Tags    []string `json:"tags"`
}

// toolIngestText drives the ingest core for the caller's owner. Collection is
// always the default (no collection arg per PLAN Decision 4). It returns the job
// id plus the raw-store outcome (sha256 + whether the bytes were already had).
func (h *Handler) toolIngestText(ctx context.Context, raw json.RawMessage, id Identity) (map[string]any, error) {
	if h.ingest == nil {
		return nil, errors.New("ingest unavailable: this wiki has no ingest backend configured")
	}
	var a ingestTextArgs
	if err := json.Unmarshal(raw, &a); err != nil {
		return nil, errors.New("invalid arguments: " + err.Error())
	}
	if a.Content == "" {
		return nil, errors.New("content is required and must be non-empty")
	}
	res, err := h.ingest.Ingest(ctx, id.OwnerEmail, "" /* default collection */, []byte(a.Content), store.RawMeta{
		Title:  a.Title,
		Source: a.Source,
		Tags:   a.Tags,
	})
	if err != nil {
		return nil, errors.New("ingest failed: " + err.Error())
	}
	return toolResultJSON(map[string]any{
		"job_id":      res.JobID,
		"sha256":      res.Sha256,
		"raw_path":    res.RawRelPath,
		"already_had": res.AlreadyHad,
	})
}

type ingestURLArgs struct {
	URL    string   `json:"url"`
	Title  string   `json:"title"`
	Source string   `json:"source"`
	Tags   []string `json:"tags"`
}

// toolIngestURL drives the URL ingest core for the caller's owner. The service
// fetches the URL server-side and extracts HTML→markdown; the result feeds the
// same async ingest pipeline as ingest_text, with source defaulted to the
// URL. Collection is always the default (no collection arg per PLAN Decision 4).
func (h *Handler) toolIngestURL(ctx context.Context, raw json.RawMessage, id Identity) (map[string]any, error) {
	if h.ingest == nil {
		return nil, errors.New("ingest unavailable: this wiki has no ingest backend configured")
	}
	var a ingestURLArgs
	if err := json.Unmarshal(raw, &a); err != nil {
		return nil, errors.New("invalid arguments: " + err.Error())
	}
	if a.URL == "" {
		return nil, errors.New("url is required and must be non-empty")
	}
	res, err := h.ingest.IngestURL(ctx, id.OwnerEmail, "" /* default collection */, a.URL, store.RawMeta{
		Title:  a.Title,
		Source: a.Source, // empty → core defaults it to the URL
		Tags:   a.Tags,
	})
	if err != nil {
		return nil, errors.New("ingest failed: " + err.Error())
	}
	return toolResultJSON(map[string]any{
		"job_id":      res.JobID,
		"sha256":      res.Sha256,
		"raw_path":    res.RawRelPath,
		"already_had": res.AlreadyHad,
	})
}

// ── search verb ─────────────────────────────────────────────────────────────

// defaultSearchLimit / maxSearchLimit bound the number of ranked pages a single
// wiki_search returns. The default keeps the response readable for the outer
// agent; the cap prevents a caller asking for an unbounded page dump.
const (
	defaultSearchLimit = 10
	maxSearchLimit     = 50
)

type searchArgs struct {
	Query string `json:"query"`
	Limit int    `json:"limit"`
}

// searchResultPage is the JSON shape of one whole curated page in a wiki_search
// result. Score is presented as "higher = more relevant" (we negate the raw
// SQLite bm25 score at this edge, where more-negative is better); storage keeps
// the raw value unchanged. The index page is surfaced with no score.
type searchResultPage struct {
	Path  string   `json:"path"`
	Title string   `json:"title"`
	Body  string   `json:"body"`
	Score *float64 `json:"score,omitempty"`
}

// toolSearch is the SYNCHRONOUS wiki_search read: a direct BM25/FTS5 query over
// the caller's curated pages — no agent, no LLM, no job. It resolves the owner
// from the injected identity, runs the search against the default collection (no
// collection arg per PLAN Decision 4), and returns whole curated pages: the
// collection's index page first (the navigation entry point, present even on zero
// hits if it exists), then the ranked matching pages. Scores are negated at this
// edge so "higher = more relevant" for the reading client.
func (h *Handler) toolSearch(ctx context.Context, raw json.RawMessage, id Identity) (map[string]any, error) {
	if h.search == nil {
		return nil, errors.New("search unavailable: this wiki has no search backend configured")
	}
	var a searchArgs
	if err := json.Unmarshal(raw, &a); err != nil {
		return nil, errors.New("invalid arguments: " + err.Error())
	}
	if a.Query == "" {
		return nil, errors.New("query is required and must be non-empty")
	}
	limit := a.Limit
	if limit <= 0 {
		limit = defaultSearchLimit
	}
	if limit > maxSearchLimit {
		limit = maxSearchLimit
	}

	// The collection key is defaulted here (no collection arg per PLAN Decision 4).
	// Unlike the filesystem store, internal/search does not self-normalize an empty
	// collection, and the ingest integration pass indexes under store.DefaultCollection
	// — so we must pass the same concrete key for the query filter to match.
	res, err := h.search.Search(ctx, id.OwnerEmail, store.DefaultCollection, a.Query, limit)
	if err != nil {
		return nil, errors.New("search failed: " + err.Error())
	}

	out := map[string]any{
		"query": a.Query,
		"count": len(res.Hits),
	}
	// Index page first: the navigation entry point, surfaced even on zero hits.
	if res.Index != nil {
		out["index"] = searchResultPage{Path: res.Index.Path, Title: res.Index.Title, Body: res.Index.Body}
	}
	pages := make([]searchResultPage, 0, len(res.Hits))
	for _, hit := range res.Hits {
		relevance := -hit.Score // raw bm25: lower (more negative) is better → negate so higher = better.
		pages = append(pages, searchResultPage{
			Path:  hit.Path,
			Title: hit.Title,
			Body:  hit.Body,
			Score: &relevance,
		})
	}
	out["results"] = pages
	return toolResultJSON(out)
}

// ── ask verb ────────────────────────────────────────────────────────────────

type askArgs struct {
	Question string `json:"question"`
}

// toolAsk drives the agentic synthesis pass for the caller's owner. Unlike
// wiki_search (synchronous, no agent), wiki_ask is ASYNC: it spawns an agentkit
// job that navigates the wiki index-first, synthesizes a cited answer, and files
// it back as a synthesis page — then returns the job id (poll it with
// wiki_job_status; the answer is searchable once the job succeeds). Collection is
// always the default (no collection arg per PLAN Decision 4).
func (h *Handler) toolAsk(ctx context.Context, raw json.RawMessage, id Identity) (map[string]any, error) {
	if h.ask == nil {
		return nil, errors.New("ask unavailable: this wiki has no agent backend configured")
	}
	var a askArgs
	if err := json.Unmarshal(raw, &a); err != nil {
		return nil, errors.New("invalid arguments: " + err.Error())
	}
	if a.Question == "" {
		return nil, errors.New("question is required and must be non-empty")
	}
	res, err := h.ask.Ask(ctx, id.OwnerEmail, "" /* default collection */, a.Question)
	if err != nil {
		return nil, errors.New("ask failed: " + err.Error())
	}
	return toolResultJSON(map[string]any{
		"job_id": res.JobID,
	})
}

type jobStatusArgs struct {
	JobID string `json:"job_id"`
}

// toolJobStatus reads one job's owner-scoped status.
func (h *Handler) toolJobStatus(ctx context.Context, raw json.RawMessage, id Identity) (map[string]any, error) {
	if h.ingest == nil {
		return nil, errors.New("ingest unavailable: this wiki has no ingest backend configured")
	}
	var a jobStatusArgs
	if err := json.Unmarshal(raw, &a); err != nil {
		return nil, errors.New("invalid arguments: " + err.Error())
	}
	if a.JobID == "" {
		return nil, errors.New("job_id is required")
	}
	st, err := h.ingest.JobStatus(ctx, id.OwnerEmail, "" /* default collection */, a.JobID)
	if errors.Is(err, ingest.ErrJobNotFound) {
		return nil, errors.New("no such job: " + a.JobID)
	}
	if err != nil {
		return nil, errors.New("job status failed: " + err.Error())
	}
	return toolResultJSON(st)
}

// ── tool implementations ─────────────────────────────────────────────────

// toolHealth renders the shared health envelope (status/version/service/details)
// and adds the injected caller identity (owner_email/client_id) — the gated MCP
// diagnostics surface and end-to-end auth-chain proof. details comes from the
// optional per-service reporter (nil → {}); wiki supplies none, so details is {}.
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

// ── shared helpers ──────────────────────────────────────────────────────

func toolResultJSON(v any) (map[string]any, error) {
	b, err := json.Marshal(v)
	if err != nil {
		return nil, err
	}
	return toolResultText(string(b)), nil
}
