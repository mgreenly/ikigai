// Package runner drives the async run lifecycle for prompts: it borrows the
// engine (provider + agent loop + wire sink) to execute a session's prompt
// inside its sandbox, streams the engine's stream-json events to the run's
// log file, and writes the run's terminal state back to the store, flipping
// the session to idle. See ARCHITECTURE.md §5.3 (runner), §9 (end-to-end
// flow), §10 (secrets).
//
// Spawn returns immediately; the work happens on a goroutine. Cancel signals
// an in-flight run (distinguished from a TTL expiry so the run is classified
// cancelled rather than failed). Recover is the boot-time crash sweep.
package runner

import (
	"bytes"
	"context"
	"encoding/json"
	"io"
	"os"
	"path/filepath"
	"sync"
	"time"

	"agentkit/agent"
	"agentkit/model"
	"agentkit/provider"
	"agentkit/provider/anthropic"
	"agentkit/tools"
	"agentkit/wire"
	"prompts/internal/sandbox"
	"prompts/internal/session"
)

// clientFactory builds a provider.Client from an API key and the resolved
// bare model ID. It defaults to anthropic.New (wrapped to the interface
// return type) but is injectable so tests can supply a fake client and never
// make a real API call.
type clientFactory func(apiKey, model string) (provider.Client, error)

// defaultClientFactory adapts anthropic.New to the clientFactory shape: it
// returns the concrete *anthropic.Client as a provider.Client, normalizing a
// typed-nil on the error path.
func defaultClientFactory(apiKey, modelID string) (provider.Client, error) {
	c, err := anthropic.New(apiKey, modelID)
	if err != nil {
		return nil, err
	}
	return c, nil
}

// Runner drives run lifecycles. It satisfies session.Runner.
type Runner struct {
	store     *session.Store
	sandbox   *sandbox.Manager
	ttl       time.Duration
	newClient clientFactory

	mu      sync.Mutex
	cancels map[string]context.CancelFunc
	// userCancelled records sessions whose in-flight run was cancelled by an
	// explicit Cancel call (as opposed to a TTL expiry), so the goroutine can
	// classify the terminal status correctly.
	userCancelled map[string]bool
}

// New builds a Runner with the default Anthropic client factory. ttl bounds
// every run's wall-clock; on expiry the run ends failed with a TTL error.
func New(store *session.Store, sb *sandbox.Manager, ttl time.Duration) *Runner {
	return &Runner{
		store:         store,
		sandbox:       sb,
		ttl:           ttl,
		newClient:     defaultClientFactory,
		cancels:       make(map[string]context.CancelFunc),
		userCancelled: make(map[string]bool),
	}
}

// Spawn starts the run on a goroutine and returns immediately.
func (r *Runner) Spawn(sess session.Session, run session.Run) {
	go r.execute(sess, run)
}

// execute runs the engine and persists the terminal outcome.
func (r *Runner) execute(sess session.Session, run session.Run) {
	ctx, cancel := context.WithTimeout(context.Background(), r.ttl)

	r.mu.Lock()
	r.cancels[sess.ID] = cancel
	r.mu.Unlock()

	defer func() {
		cancel()
		r.mu.Lock()
		delete(r.cancels, sess.ID)
		delete(r.userCancelled, sess.ID)
		r.mu.Unlock()
	}()

	// Persistence uses a fresh background context: the run ctx may be
	// cancelled/expired by the time we write the terminal state.
	bg := context.Background()
	endedAt := func() string { return time.Now().UTC().Format(time.RFC3339Nano) }

	// finish writes the run's terminal state AND (when the store is a producer)
	// emits the run.succeeded / run.failed outcome event in ONE transaction
	// (event-triggering decisions §3 — at-most-once per run, atomic). The trigger
	// context (the cron event + matched slot that started this run) rides on the
	// Run struct from the fire path; it is empty for a manual run. session_name is
	// the session's human-readable task name.
	finish := func(status, usageJSON, errMsg string) {
		_ = r.store.FinishRun(bg, session.FinishRunInput{
			RunID:        run.ID,
			SessionID:    sess.ID,
			SessionName:  sess.Name,
			Status:       status,
			EndedAt:      endedAt(),
			UsageJSON:    usageJSON,
			ErrMsg:       errMsg,
			TriggerEvent: run.TriggerEvent,
			ScheduledFor: run.ScheduledFor,
		})
	}

	// Open the run log for create/write/truncate.
	if err := os.MkdirAll(filepath.Dir(run.LogPath), 0o755); err != nil {
		finish(session.RunFailed, "", "open run log dir: "+err.Error())
		return
	}
	logFile, err := os.OpenFile(run.LogPath, os.O_CREATE|os.O_WRONLY|os.O_TRUNC, 0o644)
	if err != nil {
		finish(session.RunFailed, "", "open run log: "+err.Error())
		return
	}
	defer logFile.Close()

	// Tee the wire stream into a buffer so we can recover the result event's
	// usage totals after the engine returns.
	var tee bytes.Buffer
	wireSess := wire.NewSession(io.MultiWriter(logFile, &tee))

	// Resolve the model (provider already validated at Create). On any
	// resolution surprise, treat it as a run failure rather than panicking.
	resolved, err := model.Resolve(sess.Config.Model)
	if err != nil {
		finish(session.RunFailed, "", "resolve model: "+err.Error())
		return
	}

	// Build the client. The API key is read here and never logged or placed
	// into any error message.
	apiKey := os.Getenv("ANTHROPIC_API_KEY")
	client, err := r.newClient(apiKey, resolved.BareID)
	if err != nil {
		finish(session.RunFailed, "", "create client: "+err.Error())
		return
	}

	req := r.buildRequest(sess, resolved)
	sandboxRoot := r.sandbox.Root(sess.ID)

	runErr := agent.Run(ctx, client, wireSess, req, nil, sandboxRoot, nil)

	// Classify the terminal status: explicit user cancel wins over TTL, TTL
	// over an engine error, and a clean return is success.
	r.mu.Lock()
	userCancelled := r.userCancelled[sess.ID]
	r.mu.Unlock()

	usageJSON := captureUsage(tee.Bytes())

	switch {
	case userCancelled:
		finish(session.RunCancelled, usageJSON, "cancelled")
	case ctx.Err() == context.DeadlineExceeded:
		finish(session.RunFailed, usageJSON, "run TTL exceeded")
	case runErr != nil:
		finish(session.RunFailed, usageJSON, runErr.Error())
	default:
		finish(session.RunSucceeded, usageJSON, "")
	}
}

// buildRequest assembles the single-user-message provider request: framing +
// optional session system prompt, the full fixed toolset, no response schema
// (freeform terminal mode).
func (r *Runner) buildRequest(sess session.Session, resolved model.Resolved) provider.Request {
	effort := sess.Config.Effort
	if effort == "" {
		effort = model.DefaultEffort(resolved)
	}

	provTools := make([]provider.Tool, 0, len(tools.All()))
	for _, d := range tools.All() {
		provTools = append(provTools, provider.Tool{Name: d.Name, InputSchema: d.InputSchema})
	}

	systemPrompt := agent.FramingPrompt
	if sess.SystemPrompt != "" {
		systemPrompt = agent.FramingPrompt + "\n\n" + sess.SystemPrompt
	}

	// Resolve the output-token ceiling: honor an explicit Config.MaxTokens,
	// otherwise default to the model's registry-pinned maximum so a run is
	// not silently truncated at a fixed low cap (the default is effectively
	// "unlimited within the model's bounds").
	maxTokens := sess.Config.MaxTokens
	if maxTokens <= 0 {
		maxTokens = model.ModelContext(resolved).MaxOutputTokens
	}

	return provider.Request{
		Model:        resolved.BareID,
		Effort:       effort,
		SystemPrompt: systemPrompt,
		Messages: []provider.Message{{
			Role:   provider.RoleUser,
			Blocks: []provider.Block{provider.TextBlock{Text: sess.Prompt}},
		}},
		Tools:     provTools,
		MaxTokens: maxTokens,
		// ResponseSchema left nil → freeform terminal mode.
		// Config.Temperature has no field on provider.Request yet, so it
		// is intentionally not applied here.
	}
}

// captureUsage scans the streamed wire output for the last result event and
// returns a small JSON object carrying its usage / modelUsage sub-objects.
// Returns "" when no result event with usage is present. Best-effort: a
// decode failure simply yields no usage rather than failing the run.
func captureUsage(streamed []byte) string {
	var (
		usage      json.RawMessage
		modelUsage json.RawMessage
		costUSD    *float64
		found      bool
	)
	for _, line := range bytes.Split(streamed, []byte("\n")) {
		line = bytes.TrimSpace(line)
		if len(line) == 0 {
			continue
		}
		var ev struct {
			Type         string          `json:"type"`
			Usage        json.RawMessage `json:"usage"`
			ModelUsage   json.RawMessage `json:"modelUsage"`
			TotalCostUSD *float64        `json:"total_cost_usd"`
		}
		if err := json.Unmarshal(line, &ev); err != nil {
			continue
		}
		if ev.Type != "result" {
			continue
		}
		// Keep the last result line's values.
		usage = ev.Usage
		modelUsage = ev.ModelUsage
		costUSD = ev.TotalCostUSD
		found = true
	}
	if !found {
		return ""
	}
	out := map[string]json.RawMessage{}
	if len(usage) > 0 && !bytes.Equal(bytes.TrimSpace(usage), []byte("null")) {
		out["usage"] = usage
	}
	if len(modelUsage) > 0 && !bytes.Equal(bytes.TrimSpace(modelUsage), []byte("null")) {
		out["modelUsage"] = modelUsage
	}
	if costUSD != nil {
		b, _ := json.Marshal(*costUSD)
		out["total_cost_usd"] = b
	}
	if len(out) == 0 {
		return ""
	}
	b, err := json.Marshal(out)
	if err != nil {
		return ""
	}
	return string(b)
}

// Cancel signals the in-flight run for sessionID. It marks the run as
// user-cancelled (so the goroutine classifies it cancelled, not failed) and
// triggers context cancellation. Returns whether a run was in flight.
func (r *Runner) Cancel(sessionID string) bool {
	r.mu.Lock()
	cancel, ok := r.cancels[sessionID]
	if ok {
		r.userCancelled[sessionID] = true
	}
	r.mu.Unlock()
	if !ok {
		return false
	}
	cancel()
	return true
}

// Recover is the boot-time crash-recovery sweep: it marks every orphaned
// running run failed and flips its session back to idle, returning the count
// swept. Delegates to the store's transactional sweep.
func (r *Runner) Recover(ctx context.Context) (int, error) {
	return r.store.SweepRunning(ctx)
}
