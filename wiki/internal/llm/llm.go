// Package llm is the thin, config-driven wrapper every wiki LLM call site calls
// with its injected (prompt, model, effort) triple. It is THE enablement seam of
// plan obligation 1: a site that goes through this wrapper is automatically
// harness-callable with a swapped triple, because the wrapper takes the triple
// as data (a config.CallSite) rather than reading a constant or env at the site.
//
// The wrapper resolves a site's model id to a provider through agentkit/model, so
// a call site's model may resolve to EITHER an Anthropic or an OpenAI model (the
// P0a backend) purely by config — the site never knows which provider it ran on.
//
// Two call shapes are exposed (design §10): Structured (a single structured
// generation — extract/match/merge/compile/the lint judges) and Agent (a
// tool-using agent run — ask). P2 establishes the seam and the request-building
// half (triple → provider.Request); the actual streaming/parse/validate bodies
// are filled by the call-site phases (P6a onward) once they pin their schemas.
// Until a client factory is wired, the call shapes return ErrNotWired rather than
// silently no-op — the seam is present and typed, not yet live.
package llm

import (
	"context"
	"encoding/json"
	"errors"
	"fmt"
	"log/slog"
	"strings"
	"time"

	"agentkit/model"
	"agentkit/provider"

	"wiki/internal/config"
)

// ErrNotWired is returned by a call shape when no provider client factory has
// been wired into the Wrapper yet (the P2 scaffold state). Call-site phases wire
// the factory and replace the stub bodies.
var ErrNotWired = errors.New("llm: provider client not wired (scaffold seam)")

// Client is the minimal provider streaming surface the wrapper drives. It is
// satisfied by every agentkit backend (anthropic, openai) and by test fakes.
type Client = provider.Client

// loggerSetter is the optional accounting hook a backend may expose (the
// anthropic/openai Client.SetLogger of P0c). The wrapper binds per-call
// attribution (call_site, run_id) onto the logger and attaches it so the backend
// emits exactly one usage/cost record per request, attributed to the site. A
// backend (or test fake) that does not implement it simply emits no accounting
// line — the wrapper never requires it.
type loggerSetter interface {
	SetLogger(*slog.Logger)
}

// ClientFactory resolves a model id to a streaming Client. The composition root
// supplies it; it dispatches claude-* → anthropic, gpt-* → openai (the P0a
// backend), keyed purely on the resolved provider. A nil factory leaves the
// wrapper in the not-wired scaffold state.
type ClientFactory func(r model.Resolved) (Client, error)

// Wrapper is the per-service llm seam. It is constructed once at the composition
// root with the (optional) client factory and the accounting logger, then every
// call site invokes Structured/Agent through it with its injected triple.
type Wrapper struct {
	factory ClientFactory
	logger  *slog.Logger
}

// New builds a Wrapper. factory may be nil in the scaffold (call shapes then
// return ErrNotWired). logger is the appkit JSON logger the accounting line
// (P0c) lands in; a nil logger is a no-op there.
func New(factory ClientFactory, logger *slog.Logger) *Wrapper {
	return &Wrapper{factory: factory, logger: logger}
}

// buildRequest turns a call site's injected triple plus its messages/schema into
// a provider.Request. This is the load-bearing config-injection step: model and
// effort come from the CallSite, never from a constant. Exported via Request for
// the harness and for per-site phases that assemble the request.
//
// MaxTokens is resolved from the model's registry-pinned maximum output tokens
// (falling back to the backend's own conservative cap when the model is unknown).
// This is required, not cosmetic: Anthropic enables extended thinking from
// req.Effort (P0c) with a token budget, and rejects any request whose max_tokens
// does not exceed that budget — so leaving MaxTokens at zero made every
// effort-bearing site (extract runs at "medium") get rejected by the backend's
// 4096 fallback, which is below the medium thinking budget.
func (w *Wrapper) buildRequest(site config.CallSite, schema json.RawMessage, msgs []provider.Message, tools []provider.Tool) provider.Request {
	var maxTokens int
	if r, err := model.Resolve(site.Model); err == nil {
		maxTokens = model.ModelContext(r).MaxOutputTokens
	}
	return provider.Request{
		Model:          site.Model,
		Effort:         site.Effort,
		SystemPrompt:   site.Prompt,
		Messages:       msgs,
		Tools:          tools,
		ResponseSchema: schema,
		MaxTokens:      maxTokens,
	}
}

// Request exposes buildRequest so a call-site phase (and the eval harness) can
// see exactly the request a triple produces — the harness swaps the triple and
// re-builds. It performs no I/O.
func (w *Wrapper) Request(site config.CallSite, schema json.RawMessage, msgs []provider.Message, tools []provider.Tool) provider.Request {
	return w.buildRequest(site, schema, msgs, tools)
}

// resolveClient resolves a site's model to a streaming client, or ErrNotWired
// when no factory is configured. Shared by the call shapes.
func (w *Wrapper) resolveClient(site config.CallSite) (Client, error) {
	if w.factory == nil {
		return nil, ErrNotWired
	}
	r, err := model.Resolve(site.Model)
	if err != nil {
		return nil, err
	}
	return w.factory(r)
}

// StructuredResult is the parsed output of a Structured call. The raw text is
// preserved alongside the parsed value so call-site phases can both validate and
// keep the model's verbatim output (a golden source, obligation 4).
type StructuredResult struct {
	Raw    string
	Parsed json.RawMessage
}

// Structured runs one structured generation for a single-shot, tool-less call
// site (extract, match, merge, compile, the lint judges). It resolves the site's
// model to a provider (config-injected, never a constant), drives the stream to
// completion, and returns the assistant's verbatim text alongside it as a
// json.RawMessage for the caller to parse + schema-validate. The model and effort
// come entirely from the injected CallSite — the harness sweeps the site by
// swapping the triple (obligation 1).
//
// Returns ErrNotWired until a factory is configured (the scaffold state). The
// per-call accounting line (P0c) is bound here: the wrapper attaches a logger
// pre-bound with the site name (and any caller attribution already on w.logger)
// so the backend emits one usage/cost record attributed to this site.
func (w *Wrapper) Structured(ctx context.Context, site config.CallSite, schema json.RawMessage, msgs []provider.Message) (*StructuredResult, error) {
	client, err := w.resolveClient(site)
	if err != nil {
		return nil, err
	}
	w.bindAccounting(client, site)

	req := w.buildRequest(site, schema, msgs, nil)
	events, err := client.Stream(ctx, req)
	if err != nil {
		return nil, err
	}

	var text strings.Builder
	for ev := range events {
		switch e := ev.(type) {
		case provider.EventTextDelta:
			text.WriteString(e.Text)
		case provider.EventDone:
			// terminal; usage/cost is logged by the backend.
		}
		// Thinking, tool-use, and usage events are ignored for a tool-less
		// structured call — the structured payload arrives as assistant text.
	}
	if err := ctx.Err(); err != nil {
		return nil, err
	}

	raw := strings.TrimSpace(text.String())
	return &StructuredResult{Raw: raw, Parsed: json.RawMessage(raw)}, nil
}

// bindAccounting attaches the per-call accounting logger to a backend that
// supports it (P0c). The site name is bound as call_site; a nil base logger
// leaves the backend's sink nil (a no-op). A backend that does not implement
// loggerSetter is left untouched.
func (w *Wrapper) bindAccounting(client Client, site config.CallSite) {
	ls, ok := client.(loggerSetter)
	if !ok {
		return
	}
	if w.logger == nil {
		ls.SetLogger(nil)
		return
	}
	ls.SetLogger(w.logger.With(slog.String("call_site", site.Name)))
}

// ToolDispatch executes a tool the model requested and returns the result text
// (the tool_result block content). A non-nil error is surfaced to the model as an
// is_error tool_result so the loop keeps going (a tool failure is recoverable
// information, not a run-ending crash) — the same stance agentkit's agent loop
// takes. The read side (P10) supplies this for ask's six read tools.
type ToolDispatch func(ctx context.Context, name string, input json.RawMessage) (string, error)

// AgentBudget bounds an Agent run server-side (design §9.1): a max turn count, a
// cumulative token ceiling, and a wall-clock deadline. All three are
// config-injected (eval obligation 2 — the harness tunes them), never constants
// at the site. A zero field means "unbounded on that axis" (the caller's config
// normally sets all three).
type AgentBudget struct {
	MaxTurns  int
	MaxTokens int
	MaxWall   time.Duration
}

// Agent runs a tool-using agent loop for the ask call site (design §9.1): it
// streams the model, dispatches every tool_use block through dispatch, appends
// both turns to history, and re-invokes — until the model returns a non-tool stop
// reason (the final cited answer) or the budget is exhausted. The model and
// effort come entirely from the injected CallSite (obligation 1); the loop body
// is config-injected and the harness sweeps the site by swapping the triple.
//
// It is deliberately NOT the agentkit wire-session loop: ask is read-only, owns
// its own tools, and wants a plain text answer (not a wire transcript). Driving
// provider.Client.Stream directly keeps the read path free of the run/session
// machinery the write spine needs. Returns ErrNotWired until a factory is wired.
func (w *Wrapper) Agent(ctx context.Context, site config.CallSite, msgs []provider.Message, tools []provider.Tool, budget AgentBudget, dispatch ToolDispatch) (*StructuredResult, error) {
	client, err := w.resolveClient(site)
	if err != nil {
		return nil, err
	}
	w.bindAccounting(client, site)

	if budget.MaxWall > 0 {
		var cancel context.CancelFunc
		ctx, cancel = context.WithTimeout(ctx, budget.MaxWall)
		defer cancel()
	}

	req := w.buildRequest(site, nil, msgs, tools)

	var totalTokens int
	for turn := 0; ; turn++ {
		if budget.MaxTurns > 0 && turn >= budget.MaxTurns {
			return nil, fmt.Errorf("llm: ask exceeded max turns (%d)", budget.MaxTurns)
		}
		events, err := client.Stream(ctx, req)
		if err != nil {
			return nil, err
		}

		var (
			text       strings.Builder
			toolUses   []provider.EventToolUse
			respBlocks []provider.Block
			stop       string
		)
		for ev := range events {
			switch e := ev.(type) {
			case provider.EventTextDelta:
				text.WriteString(e.Text)
			case provider.EventThinking:
				respBlocks = append(respBlocks, provider.ThinkingBlock{Text: e.Text, Signature: e.Signature})
			case provider.EventToolUse:
				toolUses = append(toolUses, e)
				respBlocks = append(respBlocks, provider.ToolUseBlock{ID: e.ID, Name: e.Name, Input: e.Input})
			case provider.EventUsage:
				totalTokens += e.InputTokens + e.OutputTokens
			case provider.EventDone:
				stop = e.StopReason
			}
		}
		if err := ctx.Err(); err != nil {
			return nil, err
		}
		if budget.MaxTokens > 0 && totalTokens > budget.MaxTokens {
			return nil, fmt.Errorf("llm: ask exceeded token budget (%d)", budget.MaxTokens)
		}

		if stop != "tool_use" || len(toolUses) == 0 {
			// Terminal turn: the assistant's final text is the cited answer.
			raw := strings.TrimSpace(text.String())
			return &StructuredResult{Raw: raw, Parsed: json.RawMessage(raw)}, nil
		}

		// Assemble the assistant turn (text first, then tool-use blocks) for history.
		asst := make([]provider.Block, 0, len(respBlocks)+1)
		if t := strings.TrimSpace(text.String()); t != "" {
			asst = append(asst, provider.TextBlock{Text: t})
		}
		asst = append(asst, respBlocks...)

		// Dispatch each tool; a dispatch error becomes an is_error tool_result so the
		// loop continues (a tool failure is information, not a crash).
		results := make([]provider.Block, 0, len(toolUses))
		for _, tu := range toolUses {
			out, derr := dispatch(ctx, tu.Name, tu.Input)
			if derr != nil {
				results = append(results, provider.ToolResultBlock{ToolUseID: tu.ID, Content: derr.Error(), IsError: true})
				continue
			}
			results = append(results, provider.ToolResultBlock{ToolUseID: tu.ID, Content: out})
		}

		newMsgs := make([]provider.Message, len(req.Messages), len(req.Messages)+2)
		copy(newMsgs, req.Messages)
		newMsgs = append(newMsgs,
			provider.Message{Role: provider.RoleAssistant, Blocks: asst},
			provider.Message{Role: provider.RoleUser, Blocks: results},
		)
		req.Messages = newMsgs
	}
}
