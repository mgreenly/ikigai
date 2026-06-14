package read

import (
	"context"
	"encoding/json"
	"fmt"
	"strings"

	"agentkit/provider"

	"wiki/internal/config"
	"wiki/internal/inbox"
	"wiki/internal/llm"
	"wiki/internal/page"
)

// readSourceCap bounds how many bytes of an original arrival read_source returns
// (design §9.2: read_source is "size-capped"). The original may be large; the
// agent needs a representative slice, not the whole blob in its context.
const readSourceCap = 8000

// caller is the agent-loop surface ask drives — satisfied by *llm.Wrapper. Kept
// as an interface so the unit gate can mock the inner agent (the P10 verify:
// "ask happy path (mocked inner agent)").
type caller interface {
	Agent(ctx context.Context, site config.CallSite, msgs []provider.Message, tools []provider.Tool, budget llm.AgentBudget, dispatch llm.ToolDispatch) (*llm.StructuredResult, error)
}

// sourceReader is the read_source primitive: resolve an inbox id to its original
// arrival bytes (design §9.2). Satisfied by *inbox.Store.
type sourceReader interface {
	GetRow(ctx context.Context, id string) (row inbox.Row, title string, ok bool, err error)
	ReadPayload(row inbox.Row) ([]byte, error)
}

// Asker is the hosted-ask agent (design §9.1): synchronous, strictly read-only.
// It owns the asks lifecycle, the six read tools (five built — search, lookup,
// read_page, read_source, timeline — `related` goldens-gated and NOT built), and
// the config-injected (prompt, model, effort) triple + server-side budget.
type Asker struct {
	svc    *Service
	caller caller
	src    sourceReader
	asks   *AskStore
	site   config.CallSite
	budget llm.AgentBudget
}

// NewAsker builds the hosted-ask agent. The caller is the llm wrapper (or a mock
// in the unit gate); site is the config-injected ask triple; budget is the
// config-injected server-side budget (eval obligation 2).
func NewAsker(svc *Service, caller caller, src sourceReader, asks *AskStore, site config.CallSite, budget llm.AgentBudget) *Asker {
	return &Asker{svc: svc, caller: caller, src: src, asks: asks, site: site, budget: budget}
}

// Ask runs one hosted-ask: insert a `running` asks row, drive the inner agent
// over the six read tools under the budget, parse the page-cited answer, and
// finalize the row (design §9.1/§9.2). It is strictly read-only — no flight lock,
// no transaction. The answer is returned to the caller AND captured into the asks
// row (the eval golden, obligation 4).
func (a *Asker) Ask(ctx context.Context, owner, question string) (Answer, error) {
	question = strings.TrimSpace(question)
	if question == "" {
		return Answer{}, fmt.Errorf("read: ask requires a non-empty question")
	}

	askID, err := a.asks.Begin(ctx, owner, question)
	if err != nil {
		return Answer{}, err
	}

	ans, callErr := a.run(ctx, question)
	if ferr := a.asks.Finish(ctx, askID, ptrOrNil(ans, callErr), callErr); ferr != nil {
		// A failed finalize must not mask a good answer, but it is worth surfacing.
		if callErr == nil {
			return ans, ferr
		}
	}
	return ans, callErr
}

// ptrOrNil returns &ans on success, nil on error (Finish stores usage only on
// success).
func ptrOrNil(ans Answer, callErr error) *Answer {
	if callErr != nil {
		return nil
	}
	return &ans
}

// run drives the inner agent loop and parses the answer.
func (a *Asker) run(ctx context.Context, question string) (Answer, error) {
	msgs := []provider.Message{{
		Role:   provider.RoleUser,
		Blocks: []provider.Block{provider.TextBlock{Text: question}},
	}}
	res, err := a.caller.Agent(ctx, a.site, msgs, askTools(), a.budget, a.dispatch)
	if err != nil {
		return Answer{}, fmt.Errorf("read: ask agent: %w", err)
	}
	return ParseAnswer(res.Raw)
}

// askTools advertises the five built read tools (design §9.2). `related` is
// goldens-gated and NOT built. The descriptions are part of the design surface
// (they steer the agent's procedure §9.2).
func askTools() []provider.Tool {
	return []provider.Tool{
		tool("search", "Keyword search over the wiki's pages (FTS5). Returns ranked whole-page hits. Input: {\"query\": string, \"limit\"?: integer}.",
			objSchema(map[string]any{"query": str(), "limit": intg()}, "query")),
		tool("lookup", "Resolve an exact subject name (or alias) to its page(s) — the precise identity lookup. Prefer this when the question names a specific subject. Input: {\"name\": string}.",
			objSchema(map[string]any{"name": str()}, "name")),
		tool("read_page", "Read one subject's full page body by its subject id. Input: {\"subject\": string}.",
			objSchema(map[string]any{"subject": str()}, "subject")),
		tool("read_source", "Follow a page's [inbox-id] citation to the original arrival's text (size-capped). Use only to check exact wording or resolve a contradiction. Input: {\"inbox_id\": string}.",
			objSchema(map[string]any{"inbox_id": str()}, "inbox_id")),
		tool("timeline", "List event subjects whose date falls in a window (ISO-8601 prefixes). Input: {\"from\"?: string, \"to\"?: string, \"limit\"?: integer}.",
			objSchema(map[string]any{"from": str(), "to": str(), "limit": intg()})),
	}
}

// dispatch executes one tool the inner agent requested and returns its result as
// text (JSON). A dispatch error is surfaced to the model as an is_error
// tool_result (the loop continues) — never a run-ending crash.
func (a *Asker) dispatch(ctx context.Context, name string, input json.RawMessage) (string, error) {
	switch name {
	case "search":
		var p struct {
			Query string `json:"query"`
			Limit int    `json:"limit"`
		}
		if err := json.Unmarshal(input, &p); err != nil {
			return "", fmt.Errorf("search: bad input: %w", err)
		}
		hits, err := a.svc.Search(ctx, p.Query, p.Limit)
		if err != nil {
			return "", err
		}
		return jsonResult(map[string]any{"hits": renderPages(hits)})

	case "lookup":
		var p struct {
			Name string `json:"name"`
		}
		if err := json.Unmarshal(input, &p); err != nil {
			return "", fmt.Errorf("lookup: bad input: %w", err)
		}
		pages, err := a.svc.resolveName(ctx, p.Name)
		if err != nil {
			return "", err
		}
		return jsonResult(map[string]any{"matches": renderPages(pages)})

	case "read_page":
		var p struct {
			Subject string `json:"subject"`
		}
		if err := json.Unmarshal(input, &p); err != nil {
			return "", fmt.Errorf("read_page: bad input: %w", err)
		}
		wp, ok, err := a.svc.readPage(ctx, p.Subject)
		if err != nil {
			return "", err
		}
		if !ok {
			return jsonResult(map[string]any{"found": false})
		}
		return jsonResult(renderPage(wp, true))

	case "read_source":
		var p struct {
			InboxID string `json:"inbox_id"`
		}
		if err := json.Unmarshal(input, &p); err != nil {
			return "", fmt.Errorf("read_source: bad input: %w", err)
		}
		row, title, ok, err := a.src.GetRow(ctx, p.InboxID)
		if err != nil {
			return "", err
		}
		if !ok {
			return jsonResult(map[string]any{"found": false})
		}
		body, err := a.src.ReadPayload(row)
		if err != nil {
			return "", err
		}
		text := string(body)
		truncated := false
		if len(text) > readSourceCap {
			text = truncateRunes(text, readSourceCap)
			truncated = true
		}
		return jsonResult(map[string]any{"found": true, "inbox_id": p.InboxID, "title": title, "text": text, "truncated": truncated})

	case "timeline":
		var p struct {
			From  string `json:"from"`
			To    string `json:"to"`
			Limit int    `json:"limit"`
		}
		if err := json.Unmarshal(input, &p); err != nil {
			return "", fmt.Errorf("timeline: bad input: %w", err)
		}
		evs, err := a.svc.Timeline(ctx, p.From, p.To, p.Limit)
		if err != nil {
			return "", err
		}
		return jsonResult(map[string]any{"events": evs})

	default:
		return "", fmt.Errorf("unknown tool %q", name)
	}
}

// renderPages renders search/lookup hits as page-citation summaries (subject id +
// title + type + body) — everything the agent needs to cite and quote.
func renderPages(pages []page.WholePage) []map[string]any {
	out := make([]map[string]any, 0, len(pages))
	for _, p := range pages {
		out = append(out, renderPage(p, true))
	}
	return out
}

func renderPage(p page.WholePage, withBody bool) map[string]any {
	m := map[string]any{
		"subject": p.Subject,
		"title":   p.Title,
		"type":    p.Type,
		"kind":    p.Kind,
	}
	if withBody {
		m["body"] = p.Body
	}
	return m
}

func jsonResult(v any) (string, error) {
	b, err := json.Marshal(v)
	if err != nil {
		return "", err
	}
	return string(b), nil
}

// truncateRunes returns the first n bytes of s cut on a rune boundary.
func truncateRunes(s string, n int) string {
	if n <= 0 || len(s) <= n {
		if n <= 0 {
			return ""
		}
		return s
	}
	end := n
	for end > 0 && (s[end]&0xC0) == 0x80 {
		end--
	}
	return s[:end]
}

// ── provider.Tool helpers ────────────────────────────────────────────────────

// tool builds a provider.Tool. provider.Tool carries no separate description
// field (agentkit sends only name + input_schema), so the steering description —
// part of the design surface (§9.2) — is folded into the schema's top-level
// "description", which Anthropic honors for tools.
func tool(name, desc string, schema json.RawMessage) provider.Tool {
	var m map[string]any
	if err := json.Unmarshal(schema, &m); err == nil {
		m["description"] = desc
		if b, err := json.Marshal(m); err == nil {
			schema = b
		}
	}
	return provider.Tool{Name: name, InputSchema: schema}
}

func objSchema(props map[string]any, required ...string) json.RawMessage {
	o := map[string]any{"type": "object", "properties": props}
	if len(required) > 0 {
		o["required"] = required
	}
	b, _ := json.Marshal(o)
	return b
}

func str() map[string]any  { return map[string]any{"type": "string"} }
func intg() map[string]any { return map[string]any{"type": "integer"} }
