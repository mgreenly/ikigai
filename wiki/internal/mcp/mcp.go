// Package mcp implements wiki's minimal MCP transport for the /mcp endpoint and
// its tool surface: the JSON-RPC 2.0 transport, the tool registration
// (ingest_text, ingest_url, status, lint_run, search, ask, timeline), and the two
// live cross-cutting tools (health, reflection). The write doors dispatch to the
// ingest service (P3); the read tools (search / ask / timeline) dispatch to the
// read side (P10) via the Reader seam. A door whose service is nil returns a
// not-wired error result rather than a silent no-op.
//
// The transport speaks JSON-RPC 2.0 over plain HTTP POST (no SSE/streaming),
// responding with Content-Type: application/json. It carries NO token logic:
// nginx introspects every request via auth_request against the dashboard and
// injects X-Owner-Email / X-Client-Id authoritatively before forwarding here.
// The handler is mounted behind the server's requireIdentityHeaders gate.
package mcp

import (
	"context"
	"encoding/json"
	"errors"
	"net/http"
	"strings"

	"appkit"

	"eventplane/consumer"
	"eventplane/outbox"

	"wiki/internal/inbox"
)

// Identity is the authenticated caller, as told to us authoritatively by nginx
// (via the server's requireIdentityHeaders gate) through request headers.
type Identity struct {
	OwnerEmail string
	ClientID   string
}

// Handler is the http.Handler for POST /mcp. Constructed once at wiring time
// with the health-envelope inputs (version, service, optional reporter) and the
// event registry / subscription provider threaded from appkit's Router accessors.
// P2 carries no domain service yet — the domain tools are stubs.
type Handler struct {
	version       string
	service       string
	health        func(context.Context) (map[string]any, error)
	events        outbox.Registry
	subscriptions func() []consumer.Subscription
	ingest        Ingester
	lint          LintRunner
	reader        Reader
}

// Ingester is the interactive ingest front-door surface the MCP write doors
// dispatch to (design §2.1, internal/ingest.Service). A nil Ingester leaves the
// write/status tools as not-implemented stubs — the scaffold shape (P2) before
// the ingest service is wired (P3).
type Ingester interface {
	IngestText(ctx context.Context, owner, title, source, tags string, text []byte) (inbox.Receipt, error)
	IngestURL(ctx context.Context, owner, url, tags string) (inbox.Receipt, error)
	StatusAny(ctx context.Context, id string) (any, error)
}

// Reader is the read side the search / ask / timeline tools dispatch to (design
// §9, internal/read). A nil Reader leaves the read tools as not-implemented stubs
// (the scaffold shape before the read service is wired). The methods return plain
// serializable values so the mcp package stays decoupled from the read package's
// concrete types.
type Reader interface {
	// Search runs the public search verb: registry-first whole-page hits.
	Search(ctx context.Context, query string, limit int) (any, error)
	// Ask runs the hosted-ask agent and returns the page-cited answer.
	Ask(ctx context.Context, owner, question string) (any, error)
	// Timeline lists event subjects in [from, to] (ISO-8601 prefixes).
	Timeline(ctx context.Context, from, to string, limit int) (any, error)
}

// LintRunner is the on-demand lint trigger surface the lint_run verb dispatches to
// (design §6: "lint_run(job) is just another front door that Accepts a trigger
// row"). LintRun Accepts a trigger row for the named job and returns its inbox id;
// the worker selects and runs the job asynchronously. A nil LintRunner leaves
// lint_run a not-implemented stub (the scaffold shape before the lint trigger is
// wired).
type LintRunner interface {
	LintRun(ctx context.Context, owner, job string) (inbox.Receipt, error)
}

// NewHandler builds a Handler. version/service/health populate the health
// envelope; health is the optional per-service reporter (nil → details is {}).
// events is the published-event registry and subscriptions the live subscription
// provider, both rendered by reflection. ingest is the interactive write/status
// surface (nil → the write doors stay not-implemented stubs); lint is the
// on-demand lint trigger surface (nil → lint_run stays a stub).
func NewHandler(version, service string,
	health func(context.Context) (map[string]any, error),
	events outbox.Registry, subscriptions func() []consumer.Subscription,
	ingest Ingester, lint LintRunner, reader Reader) *Handler {
	return &Handler{
		version:       version,
		service:       service,
		health:        health,
		events:        events,
		subscriptions: subscriptions,
		ingest:        ingest,
		lint:          lint,
		reader:        reader,
	}
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
			"serverInfo":      map[string]any{"name": "Wiki", "version": h.version},
			"instructions": "A personal wiki: ingest text or URLs, then search or ask. " +
				"search is a fast keyword read; ask runs an agent for a cited answer. " +
				"Poll async jobs with the status verb.",
		})
	case "notifications/initialized":
		w.WriteHeader(http.StatusAccepted)
	case "tools/list":
		writeJSONRPCResult(w, req.ID, map[string]any{"tools": toolDescriptors()})
	case "tools/call":
		h.handleToolCall(r.Context(), w, req, id)
	default:
		writeJSONRPCError(w, req.ID, -32601, "method not found")
	}
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
	case "ingest_text":
		return h.toolIngestText(ctx, argsRaw, id)
	case "ingest_url":
		return h.toolIngestURL(ctx, argsRaw, id)
	case "status":
		return h.toolStatus(ctx, argsRaw)
	case "lint_run":
		return h.toolLintRun(ctx, argsRaw, id)
	case "search":
		return h.toolSearch(ctx, argsRaw)
	case "ask":
		return h.toolAsk(ctx, argsRaw, id)
	case "timeline":
		return h.toolTimeline(ctx, argsRaw)
	case "health":
		return h.toolHealth(ctx, id)
	case "reflection":
		return h.toolReflection(argsRaw)
	default:
		return nil, errors.New("unknown tool: " + name)
	}
}

// toolHealth renders the shared health envelope plus the authenticated caller's
// identity — the gated, MCP-side variant of the health surface.
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

// toolIngestText is the ingest_text write door (§2.1). It Accepts the text as a
// document under the authenticated caller's identity and returns the receipt
// (inbox id + sha256 + dup flag — NOT a job id). An oversized payload returns the
// refusal as a tool error (and the door has already emitted wiki.ingest_refused).
func (h *Handler) toolIngestText(ctx context.Context, raw json.RawMessage, id Identity) (map[string]any, error) {
	if h.ingest == nil {
		return toolResultErr("ingest not wired"), nil
	}
	var a struct {
		Text   string   `json:"text"`
		Title  string   `json:"title"`
		Source string   `json:"source"`
		Tags   []string `json:"tags"`
	}
	if err := unmarshalArgs(raw, &a); err != nil {
		return nil, err
	}
	if strings.TrimSpace(a.Text) == "" {
		return toolResultErr("ingest_text: 'text' is required"), nil
	}
	rec, err := h.ingest.IngestText(ctx, id.OwnerEmail, a.Title, a.Source, tagsJSON(a.Tags), []byte(a.Text))
	if err != nil {
		return toolResultErr(err.Error()), nil
	}
	return receiptResult(rec.ID, rec.SHA256, rec.Dup)
}

// toolIngestURL is the ingest_url write door (§2.1): fetch + extract server-side
// then Accept, returning the same receipt as ingest_text.
func (h *Handler) toolIngestURL(ctx context.Context, raw json.RawMessage, id Identity) (map[string]any, error) {
	if h.ingest == nil {
		return toolResultErr("ingest not wired"), nil
	}
	var a struct {
		URL  string   `json:"url"`
		Tags []string `json:"tags"`
	}
	if err := unmarshalArgs(raw, &a); err != nil {
		return nil, err
	}
	if strings.TrimSpace(a.URL) == "" {
		return toolResultErr("ingest_url: 'url' is required"), nil
	}
	rec, err := h.ingest.IngestURL(ctx, id.OwnerEmail, a.URL, tagsJSON(a.Tags))
	if err != nil {
		return toolResultErr(err.Error()), nil
	}
	return receiptResult(rec.ID, rec.SHA256, rec.Dup)
}

// toolStatus polls the integration state of an inbox id (§2.1).
func (h *Handler) toolStatus(ctx context.Context, raw json.RawMessage) (map[string]any, error) {
	if h.ingest == nil {
		return toolResultErr("ingest not wired"), nil
	}
	var a struct {
		ID string `json:"id"`
	}
	if err := unmarshalArgs(raw, &a); err != nil {
		return nil, err
	}
	if strings.TrimSpace(a.ID) == "" {
		return toolResultErr("status: 'id' is required"), nil
	}
	st, err := h.ingest.StatusAny(ctx, a.ID)
	if err != nil {
		return toolResultErr(err.Error()), nil
	}
	return toolResultJSON(st)
}

// toolLintRun is the lint_run verb (design §6): Accept a trigger row for the named
// lint job so a worker runs it now. It returns the trigger row's inbox id (a
// receipt, not a result — the job runs asynchronously). An unknown job name is
// surfaced as the LintRunner's error.
func (h *Handler) toolLintRun(ctx context.Context, raw json.RawMessage, id Identity) (map[string]any, error) {
	if h.lint == nil {
		return toolResultErr("lint not wired"), nil
	}
	var a struct {
		Job string `json:"job"`
	}
	if err := unmarshalArgs(raw, &a); err != nil {
		return nil, err
	}
	if strings.TrimSpace(a.Job) == "" {
		return toolResultErr("lint_run: 'job' is required"), nil
	}
	rec, err := h.lint.LintRun(ctx, id.OwnerEmail, a.Job)
	if err != nil {
		return toolResultErr(err.Error()), nil
	}
	return toolResultJSON(map[string]any{"id": rec.ID, "job": a.Job})
}

// toolSearch is the public search verb (design §9.3): registry-first whole-page
// hits, rank order only. 'query' required; 'limit' optional (the read service
// clamps it to the default/cap contract).
func (h *Handler) toolSearch(ctx context.Context, raw json.RawMessage) (map[string]any, error) {
	if h.reader == nil {
		return toolResultErr("read not wired"), nil
	}
	var a struct {
		Query string `json:"query"`
		Limit int    `json:"limit"`
	}
	if err := unmarshalArgs(raw, &a); err != nil {
		return nil, err
	}
	if strings.TrimSpace(a.Query) == "" {
		return toolResultErr("search: 'query' is required"), nil
	}
	res, err := h.reader.Search(ctx, a.Query, a.Limit)
	if err != nil {
		return toolResultErr(err.Error()), nil
	}
	return toolResultJSON(map[string]any{"hits": res})
}

// toolAsk is the hosted-ask verb (design §9.1/§9.2): synchronous, read-only,
// returns the page-cited answer. 'question' required; the caller's identity is the
// asks-row owner (attribution + eval golden).
func (h *Handler) toolAsk(ctx context.Context, raw json.RawMessage, id Identity) (map[string]any, error) {
	if h.reader == nil {
		return toolResultErr("read not wired"), nil
	}
	var a struct {
		Question string `json:"question"`
	}
	if err := unmarshalArgs(raw, &a); err != nil {
		return nil, err
	}
	if strings.TrimSpace(a.Question) == "" {
		return toolResultErr("ask: 'question' is required"), nil
	}
	res, err := h.reader.Ask(ctx, id.OwnerEmail, a.Question)
	if err != nil {
		return toolResultErr(err.Error()), nil
	}
	return toolResultJSON(res)
}

// toolTimeline is the public timeline verb (design §9.2): list event subjects in
// a date window. Both bounds optional (ISO-8601 prefixes); 'limit' optional.
func (h *Handler) toolTimeline(ctx context.Context, raw json.RawMessage) (map[string]any, error) {
	if h.reader == nil {
		return toolResultErr("read not wired"), nil
	}
	var a struct {
		From  string `json:"from"`
		To    string `json:"to"`
		Limit int    `json:"limit"`
	}
	if err := unmarshalArgs(raw, &a); err != nil {
		return nil, err
	}
	res, err := h.reader.Timeline(ctx, a.From, a.To, a.Limit)
	if err != nil {
		return toolResultErr(err.Error()), nil
	}
	return toolResultJSON(map[string]any{"events": res})
}

// receiptResult renders the MCP receipt contract: inbox id + sha256 + dup flag.
func receiptResult(id, sha256 string, dup bool) (map[string]any, error) {
	return toolResultJSON(map[string]any{"id": id, "sha256": sha256, "dup": dup})
}

// tagsJSON renders a string slice as a JSON-array string for inbox.Accept (the
// tags column is a JSON array). An empty/nil slice becomes "[]".
func tagsJSON(tags []string) string {
	if len(tags) == 0 {
		return "[]"
	}
	b, err := json.Marshal(tags)
	if err != nil {
		return "[]"
	}
	return string(b)
}

// unmarshalArgs decodes a tools/call arguments object, treating an absent
// arguments field as an empty object.
func unmarshalArgs(raw json.RawMessage, v any) error {
	if len(raw) == 0 {
		return nil
	}
	return json.Unmarshal(raw, v)
}

// toolReflection self-describes wiki's edges in the event graph. No event_type →
// the index {publishes, subscribes}; with event_type → that published type's
// {type, description, schema, example}. An unknown event_type returns a
// corrective error listing the valid types.
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

// ── transport ──────────────────────────────────────────────────────────────

type jsonRPCRequest struct {
	JSONRPC string          `json:"jsonrpc"`
	ID      json.RawMessage `json:"id,omitempty"`
	Method  string          `json:"method"`
	Params  json.RawMessage `json:"params,omitempty"`
}

type toolCallParams struct {
	Name      string          `json:"name"`
	Arguments json.RawMessage `json:"arguments"`
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

func toolResultText(text string) map[string]any {
	return map[string]any{"content": []map[string]any{{"type": "text", "text": text}}}
}

func toolResultErr(msg string) map[string]any {
	return map[string]any{"isError": true, "content": []map[string]any{{"type": "text", "text": msg}}}
}

func toolResultJSON(v any) (map[string]any, error) {
	b, err := json.Marshal(v)
	if err != nil {
		return nil, err
	}
	return toolResultText(string(b)), nil
}
