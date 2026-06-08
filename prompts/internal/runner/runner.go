// Package runner drives the async run lifecycle for prompts: it borrows the
// engine (provider + agent loop + wire sink) to execute a prompt's user prompt
// inside its sandbox, streams the engine's stream-json events to the run's
// log file, and writes the run's terminal state back to the store. There is no
// prompt status to flip — runs are fully concurrent. See ARCHITECTURE.md §5.3
// (runner), §9 (end-to-end flow), §10 (secrets).
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
	"prompts/internal/prompt"
	"prompts/internal/sandbox"
	"prompts/internal/suite"
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

// Runner drives run lifecycles. It satisfies prompt.Runner.
type Runner struct {
	store     *prompt.Store
	sandbox   *sandbox.Manager
	ttl       time.Duration
	newClient clientFactory
	// discover snapshots the box's other loopback MCP services as an
	// agent.ToolSource at run spawn (Surface 2 — in-run suite tools). It
	// defaults to a closure over the configured manifestRoot calling
	// suite.Discover, but is injectable so tests can supply a fake source and
	// never touch the real inventory or any peer.
	discover func(ctx context.Context, owner, promptID string) agent.ToolSource

	mu      sync.Mutex
	cancels map[string]context.CancelFunc
	// userCancelled records runs whose in-flight execution was cancelled by an
	// explicit Cancel call (as opposed to a TTL expiry), so the goroutine can
	// classify the terminal status correctly. Keyed by run_id.
	userCancelled map[string]bool
}

// New builds a Runner with the default Anthropic client factory. ttl bounds
// every run's wall-clock; on expiry the run ends failed with a TTL error.
// manifestRoot is the box inventory root (PROMPTS_MANIFEST_ROOT) threaded into
// the default suite-discovery closure.
func New(store *prompt.Store, sb *sandbox.Manager, ttl time.Duration, manifestRoot string) *Runner {
	return &Runner{
		store:     store,
		sandbox:   sb,
		ttl:       ttl,
		newClient: defaultClientFactory,
		discover: func(ctx context.Context, owner, promptID string) agent.ToolSource {
			return suite.Discover(ctx, manifestRoot, owner, promptID)
		},
		cancels:       make(map[string]context.CancelFunc),
		userCancelled: make(map[string]bool),
	}
}

// Spawn starts the run on a goroutine and returns immediately. The runner reads
// its execution inputs from runs/<run.ID>/input/ on disk (pinned by the service
// before spawn) — never from a live Prompt, so a mid-run edit/delete of the
// prompt cannot change what this run executes.
func (r *Runner) Spawn(run prompt.Run) {
	go r.execute(run)
}

// execute runs the engine and persists the terminal outcome.
func (r *Runner) execute(run prompt.Run) {
	ctx, cancel := context.WithTimeout(context.Background(), r.ttl)

	r.mu.Lock()
	r.cancels[run.ID] = cancel
	r.mu.Unlock()

	defer func() {
		cancel()
		r.mu.Lock()
		delete(r.cancels, run.ID)
		delete(r.userCancelled, run.ID)
		r.mu.Unlock()
	}()

	// Persistence uses a fresh background context: the run ctx may be
	// cancelled/expired by the time we write the terminal state.
	bg := context.Background()
	endedAt := func() string { return time.Now().UTC().Format(time.RFC3339Nano) }

	// finish writes the run's terminal state AND (when the store is a producer)
	// emits the run.succeeded / run.failed outcome event in ONE transaction
	// (event-triggering decisions §3 — at-most-once per run, atomic). The outcome
	// fields (prompt_id, prompt_name, trigger context) are sourced from the run
	// row by FinishRun itself, so the runner threads only the terminal state.
	finish := func(status, usageJSON, errMsg string) {
		_ = r.store.FinishRun(bg, prompt.FinishRunInput{
			RunID:     run.ID,
			Status:    status,
			EndedAt:   endedAt(),
			UsageJSON: usageJSON,
			ErrMsg:    errMsg,
		})
	}

	// Open the run log for create/write/truncate.
	if err := os.MkdirAll(filepath.Dir(run.LogPath), 0o755); err != nil {
		finish(prompt.RunFailed, "", "open run log dir: "+err.Error())
		return
	}
	logFile, err := os.OpenFile(run.LogPath, os.O_CREATE|os.O_WRONLY|os.O_TRUNC, 0o644)
	if err != nil {
		finish(prompt.RunFailed, "", "open run log: "+err.Error())
		return
	}
	defer logFile.Close()

	// Tee the wire stream into a buffer so we can recover the result event's
	// usage totals after the engine returns.
	var tee bytes.Buffer
	wireSess := wire.NewSession(io.MultiWriter(logFile, &tee))

	// Read the run's pinned execution inputs from runs/<run.ID>/input/ — NOT
	// from any live Prompt. This folder was written by the service at spawn and
	// is the immutable record of exactly what this run executes.
	inputDir := filepath.Join(filepath.Dir(run.LogPath), "input")
	var cfg prompt.Config
	cfgBytes, err := os.ReadFile(filepath.Join(inputDir, "config.json"))
	if err != nil {
		finish(prompt.RunFailed, "", "read run config: "+err.Error())
		return
	}
	if err := json.Unmarshal(cfgBytes, &cfg); err != nil {
		finish(prompt.RunFailed, "", "parse run config: "+err.Error())
		return
	}
	userPromptBytes, err := os.ReadFile(filepath.Join(inputDir, "user_prompt.txt"))
	if err != nil {
		finish(prompt.RunFailed, "", "read user prompt: "+err.Error())
		return
	}
	systemPromptBytes, err := os.ReadFile(filepath.Join(inputDir, "system_prompt.txt"))
	if err != nil {
		finish(prompt.RunFailed, "", "read system prompt: "+err.Error())
		return
	}
	eventBytes, err := os.ReadFile(filepath.Join(inputDir, "event.json"))
	if err != nil && !os.IsNotExist(err) {
		finish(prompt.RunFailed, "", "read run event: "+err.Error())
		return
	}
	// eventBytes == nil when the file is absent (manual run).

	// Resolve the model (provider already validated at Create). On any
	// resolution surprise, treat it as a run failure rather than panicking.
	resolved, err := model.Resolve(cfg.Model)
	if err != nil {
		finish(prompt.RunFailed, "", "resolve model: "+err.Error())
		return
	}

	// Build the client. The API key is read here and never logged or placed
	// into any error message.
	apiKey := os.Getenv("ANTHROPIC_API_KEY")
	client, err := r.newClient(apiKey, resolved.BareID)
	if err != nil {
		finish(prompt.RunFailed, "", "create client: "+err.Error())
		return
	}

	req := buildRequest(cfg, string(userPromptBytes), string(systemPromptBytes), eventBytes, resolved)
	sandboxRoot := r.sandbox.Root(run.ID)

	// Snapshot the suite's loopback MCP tools available to this run's owner
	// (Surface 2). Best-effort by contract: never nil, never errors. The agent
	// loop advertises the source's Descriptors and routes owned tool_use blocks
	// to it; the built-in tools.All() set is advertised separately by
	// buildRequest (single source of truth for suite tools is this source).
	suiteSource := r.discover(ctx, run.OwnerEmail, run.PromptID)

	runErr := agent.Run(ctx, client, wireSess, req, agent.Options{SandboxRoot: sandboxRoot, Tools: suiteSource})

	// Classify the terminal status: explicit user cancel wins over TTL, TTL
	// over an engine error, and a clean return is success.
	r.mu.Lock()
	userCancelled := r.userCancelled[run.ID]
	r.mu.Unlock()

	usageJSON := captureUsage(tee.Bytes())

	switch {
	case userCancelled:
		finish(prompt.RunCancelled, usageJSON, "cancelled")
	case ctx.Err() == context.DeadlineExceeded:
		finish(prompt.RunFailed, usageJSON, "run TTL exceeded")
	case runErr != nil:
		finish(prompt.RunFailed, usageJSON, runErr.Error())
	default:
		finish(prompt.RunSucceeded, usageJSON, "")
	}
}

// eventPreamble introduces the triggering event appended as a second user
// TextBlock on event-triggered runs.
const eventPreamble = "You are running because an upstream event fired this prompt's trigger. The triggering event is below as JSON. Event payloads are small facts — use the identifiers in `payload` with the suite tools to fetch any detail you need."

// buildRequest assembles the single-user-message provider request from the
// run's pinned on-disk inputs (config, user prompt, system prompt): framing +
// optional system prompt, the full fixed toolset, no response schema (freeform
// terminal mode). The user message carries the verbatim user prompt as its
// first block; on event-triggered runs (eventJSON non-empty) a second block
// carries the triggering event. It takes no Prompt — the runner executes from
// disk.
func buildRequest(cfg prompt.Config, userPrompt, sysPrompt string, eventJSON []byte, resolved model.Resolved) provider.Request {
	effort := cfg.Effort
	if effort == "" {
		effort = model.DefaultEffort(resolved)
	}

	provTools := make([]provider.Tool, 0, len(tools.All()))
	for _, d := range tools.All() {
		provTools = append(provTools, provider.Tool{Name: d.Name, InputSchema: d.InputSchema})
	}

	systemPrompt := agent.FramingPrompt
	if sysPrompt != "" {
		systemPrompt = agent.FramingPrompt + "\n\n" + sysPrompt
	}

	// Resolve the output-token ceiling: honor an explicit Config.MaxTokens,
	// otherwise default to the model's registry-pinned maximum so a run is
	// not silently truncated at a fixed low cap (the default is effectively
	// "unlimited within the model's bounds").
	maxTokens := cfg.MaxTokens
	if maxTokens <= 0 {
		maxTokens = model.ModelContext(resolved).MaxOutputTokens
	}

	blocks := []provider.Block{provider.TextBlock{Text: userPrompt}}
	if len(eventJSON) > 0 {
		var pretty bytes.Buffer
		if json.Indent(&pretty, eventJSON, "", "  ") != nil {
			pretty.Reset()
			pretty.Write(eventJSON)
		}
		blocks = append(blocks, provider.TextBlock{Text: eventPreamble + "\n\n" + pretty.String()})
	}

	return provider.Request{
		Model:        resolved.BareID,
		Effort:       effort,
		SystemPrompt: systemPrompt,
		Messages: []provider.Message{{
			Role:   provider.RoleUser,
			Blocks: blocks,
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

// Cancel signals the in-flight run runID. It marks the run as
// user-cancelled (so the goroutine classifies it cancelled, not failed) and
// triggers context cancellation. Returns whether a run was in flight.
func (r *Runner) Cancel(runID string) bool {
	r.mu.Lock()
	cancel, ok := r.cancels[runID]
	if ok {
		r.userCancelled[runID] = true
	}
	r.mu.Unlock()
	if !ok {
		return false
	}
	cancel()
	return true
}

// Recover is the boot-time crash-recovery sweep: it marks every orphaned
// running run failed, returning the count swept (runs only — there is no
// prompt status). Delegates to the store's sweep.
func (r *Runner) Recover(ctx context.Context) (int, error) {
	return r.store.SweepRunning(ctx)
}
