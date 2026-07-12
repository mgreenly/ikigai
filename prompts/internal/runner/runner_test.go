package runner

import (
	"context"
	"encoding/json"
	"errors"
	"os"
	"path/filepath"
	"strings"
	"sync"
	"testing"
	"time"

	appkitdb "appkit/db"

	"prompts/internal/db"
	"prompts/internal/ids"
	"prompts/internal/prompt"
	"prompts/internal/sandbox"

	"github.com/ikigenba/agentkit"
)

// fakeProvider implements agentkit.Provider with a pre-canned one-turn
// response. If block is true, RoundTrip waits for the context to finish before
// returning, modelling a hung provider call without any network dependency.
type fakeProvider struct {
	block      bool
	roundTrips []*agentkit.RoundTrip

	mu       sync.Mutex
	requests []*agentkit.Request
	next     int
}

func (f *fakeProvider) Name() string { return "fake" }

func (f *fakeProvider) Pricing(model string) (agentkit.Pricing, bool) {
	return agentkit.Pricing{Tiers: []agentkit.RateTier{{InputUncached: 1, Output: 1}}}, true
}

func (f *fakeProvider) RoundTrip(ctx context.Context, req *agentkit.Request) *agentkit.RoundTrip {
	f.mu.Lock()
	f.requests = append(f.requests, req)
	if f.block {
		f.mu.Unlock()
		<-ctx.Done()
		return agentkit.NewRoundTrip(agentkit.Message{}, agentkit.FinishOther, agentkit.Usage{}, nil, ctx.Err())
	}
	if len(f.roundTrips) > 0 {
		next := f.next
		f.next++
		f.mu.Unlock()
		if next >= len(f.roundTrips) {
			return agentkit.NewRoundTrip(agentkit.Message{}, agentkit.FinishOther, agentkit.Usage{}, nil, errors.New("fake provider script exhausted"))
		}
		return f.roundTrips[next]
	}
	f.mu.Unlock()

	return agentkit.NewRoundTrip(
		agentkit.Message{Role: agentkit.RoleAssistant, Blocks: []agentkit.Block{agentkit.TextBlock{Text: "all done"}}},
		agentkit.FinishStop,
		agentkit.Usage{InputUncached: 12, Output: 7, Total: 19},
		nil,
		nil,
	)
}

func (f *fakeProvider) requestCount() int {
	f.mu.Lock()
	defer f.mu.Unlock()
	return len(f.requests)
}

func (f *fakeProvider) request(i int) *agentkit.Request {
	f.mu.Lock()
	defer f.mu.Unlock()
	if i < 0 || i >= len(f.requests) {
		return nil
	}
	return f.requests[i]
}

func (f *fakeProvider) lastRequest() *agentkit.Request {
	f.mu.Lock()
	defer f.mu.Unlock()
	if len(f.requests) == 0 {
		return nil
	}
	return f.requests[len(f.requests)-1]
}

func scriptedRoundTrip(blocks ...agentkit.Block) *agentkit.RoundTrip {
	return agentkit.NewRoundTrip(
		agentkit.Message{Role: agentkit.RoleAssistant, Blocks: blocks},
		agentkit.FinishToolUse,
		agentkit.Usage{InputUncached: 1, Output: 1, Total: 2},
		nil,
		nil,
	)
}

func scriptedTextRoundTrip(text string) *agentkit.RoundTrip {
	return agentkit.NewRoundTrip(
		agentkit.Message{Role: agentkit.RoleAssistant, Blocks: []agentkit.Block{agentkit.TextBlock{Text: text}}},
		agentkit.FinishStop,
		agentkit.Usage{InputUncached: 1, Output: 1, Total: 2},
		nil,
		nil,
	)
}

func TestBuildProviderUsesInjectedEnvironment(t *testing.T) {
	// R-K5I9-YGS9
	cfg := prompt.Config{Provider: "anthropic", Model: "claude-haiku-4-5"}
	var lookedUp []string
	prov, err := buildProvider(cfg, func(key string) string {
		lookedUp = append(lookedUp, key)
		if key == "ANTHROPIC_API_KEY" {
			return "test-key"
		}
		return ""
	})
	if err != nil {
		t.Fatalf("buildProvider: %v", err)
	}
	if prov == nil {
		t.Fatalf("buildProvider returned nil provider")
	}
	if got, want := strings.Join(lookedUp, ","), "ANTHROPIC_API_KEY"; got != want {
		t.Fatalf("getenv keys = %q, want %q", got, want)
	}

	_, err = buildProvider(cfg, func(string) string { return "" })
	if err == nil || !strings.Contains(err.Error(), "ANTHROPIC_API_KEY") {
		t.Fatalf("missing key error = %v, want ANTHROPIC_API_KEY failure", err)
	}
}

// newTestRunner builds a Runner backed by a real temp store + sandbox, with
// the provider factory replaced by one that always returns fp.
func newTestRunner(t *testing.T, ttl time.Duration, fp agentkit.Provider) (*Runner, *prompt.Store) {
	t.Helper()
	ctx := context.Background()
	conn, err := appkitdb.Open(filepath.Join(t.TempDir(), "prompts.db"))
	if err != nil {
		t.Fatalf("appkitdb.Open: %v", err)
	}
	t.Cleanup(func() { conn.Close() })
	migs, err := appkitdb.LoadMigrations(db.FS, "migrations")
	if err != nil {
		t.Fatalf("appkitdb.LoadMigrations: %v", err)
	}
	if err := appkitdb.Migrate(ctx, conn, migs); err != nil {
		t.Fatalf("appkitdb.Migrate: %v", err)
	}
	store := prompt.NewStore(conn)

	sb, err := sandbox.New(filepath.Join(t.TempDir(), "sandboxes"))
	if err != nil {
		t.Fatalf("sandbox.New: %v", err)
	}

	r := New(store, sb, ttl, t.TempDir(), func(int) bool { return false })
	r.buildProvider = func(prompt.Config, func(string) string) (agentkit.Provider, error) { return fp, nil }
	return r, store
}

// seedRunning inserts a prompt, makes a run-scoped sandbox, materializes the
// run's input/ on disk (what the runner now reads from), then opens a running
// run on it — mirroring Service.Run, so the runner can take it terminal.
func seedRunning(t *testing.T, store *prompt.Store, sb *sandbox.Manager, runsDir string) (prompt.Prompt, prompt.Run) {
	t.Helper()
	ctx := context.Background()
	now := time.Now().UTC().Format(time.RFC3339Nano)
	sess := prompt.Prompt{
		ID:         ids.NewULID(),
		OwnerEmail: "owner@example.com",
		Name:       "n",
		UserPrompt: "do the thing",
		Config:     prompt.Config{Provider: "anthropic", Model: "claude-haiku-4-5"},
		CreatedAt:  now,
		UpdatedAt:  now,
	}
	if err := store.InsertPrompt(ctx, sess); err != nil {
		t.Fatalf("InsertPrompt: %v", err)
	}
	runID := ids.NewULID()
	// Per-run sandbox is keyed by run_id (runs/<run_id>/sandbox).
	if err := sb.Create(runID); err != nil {
		t.Fatalf("sandbox.Create: %v", err)
	}
	run := prompt.Run{
		ID:         runID,
		PromptID:   sess.ID,
		OwnerEmail: sess.OwnerEmail,
		PromptName: sess.Name,
		Status:     prompt.RunRunning,
		StartedAt:  now,
		LogPath:    filepath.Join(runsDir, runID, "output.jsonl"),
	}
	writeRunInput(t, runsDir, runID, sess.UserPrompt, sess.SystemPrompt, sess.Config)
	if err := store.InsertRun(ctx, run); err != nil {
		t.Fatalf("InsertRun: %v", err)
	}
	return sess, run
}

// writeRunInput pins a run's execution inputs to runs/<run_id>/input/, the
// disk source the runner reads (mirrors Service.materializeInput).
func writeRunInput(t *testing.T, runsDir, runID, userPrompt, sysPrompt string, cfg prompt.Config) {
	t.Helper()
	inputDir := filepath.Join(runsDir, runID, "input")
	if err := os.MkdirAll(inputDir, 0o755); err != nil {
		t.Fatalf("mkdir input: %v", err)
	}
	if err := os.WriteFile(filepath.Join(inputDir, "user_prompt.txt"), []byte(userPrompt), 0o644); err != nil {
		t.Fatalf("write user_prompt: %v", err)
	}
	if err := os.WriteFile(filepath.Join(inputDir, "system_prompt.txt"), []byte(sysPrompt), 0o644); err != nil {
		t.Fatalf("write system_prompt: %v", err)
	}
	cfgJSON, err := json.Marshal(cfg)
	if err != nil {
		t.Fatalf("marshal config: %v", err)
	}
	if err := os.WriteFile(filepath.Join(inputDir, "config.json"), cfgJSON, 0o644); err != nil {
		t.Fatalf("write config: %v", err)
	}
}

// waitRun polls the store until the run reaches a terminal status or the
// deadline passes. Returns the final run row.
func waitRun(t *testing.T, store *prompt.Store, sessionID string) prompt.Run {
	t.Helper()
	deadline := time.Now().Add(5 * time.Second)
	for time.Now().Before(deadline) {
		run, err := store.GetLatestRun(context.Background(), sessionID)
		if err != nil {
			t.Fatalf("GetLatestRun: %v", err)
		}
		if run != nil && run.Status != prompt.RunRunning {
			return *run
		}
		time.Sleep(5 * time.Millisecond)
	}
	t.Fatalf("run for session %s did not reach a terminal state", sessionID)
	return prompt.Run{}
}

func TestSpawn_TerminalSuccess(t *testing.T) {
	// R-K7Y2-Q09N
	fp := &fakeProvider{}
	runsDir := t.TempDir()
	r, store := newTestRunner(t, time.Minute, fp)
	sess, run := seedRunning(t, store, r.sandbox, runsDir)

	r.Spawn(run)
	got := waitRun(t, store, sess.ID)

	if got.Status != prompt.RunSucceeded {
		t.Fatalf("run status = %q, want succeeded (error=%q)", got.Status, got.Error)
	}
	if got.Error != "" {
		t.Fatalf("run error = %q, want empty", got.Error)
	}
	if got.EndedAt == "" {
		t.Fatalf("run ended_at empty")
	}

	data, err := os.ReadFile(run.LogPath)
	if err != nil {
		t.Fatalf("read log: %v", err)
	}
	logStr := string(data)
	if !strings.Contains(logStr, "all done") {
		t.Fatalf("log missing emitted assistant text: %s", logStr)
	}
	if !strings.Contains(logStr, `"type":"message"`) {
		t.Fatalf("log missing message event: %s", logStr)
	}
	if got.UsageJSON == "" {
		t.Fatalf("usage_json empty; want captured usage")
	}
	if !strings.Contains(got.UsageJSON, "usage") {
		t.Fatalf("usage_json = %q, want usage totals", got.UsageJSON)
	}
	if fp.requestCount() != 1 {
		t.Fatalf("provider RoundTrip calls = %d, want 1", fp.requestCount())
	}
}

func TestSpawn_UsesInjectedProviderFactoryWithoutLiveEnvironment(t *testing.T) {
	// R-K6Q6-C8IY
	fp := &fakeProvider{}
	runsDir := t.TempDir()
	r, store := newTestRunner(t, time.Minute, fp)
	sess, run := seedRunning(t, store, r.sandbox, runsDir)

	var called bool
	var gotCfg prompt.Config
	r.buildProvider = func(cfg prompt.Config, getenv func(string) string) (agentkit.Provider, error) {
		called = true
		gotCfg = cfg
		return fp, nil
	}

	r.Spawn(run)
	got := waitRun(t, store, sess.ID)

	if !called {
		t.Fatalf("injected buildProvider was not called")
	}
	if gotCfg.Provider != "anthropic" || gotCfg.Model != "claude-haiku-4-5" {
		t.Fatalf("buildProvider cfg = %+v, want pinned run config", gotCfg)
	}
	if got.Status != prompt.RunSucceeded {
		t.Fatalf("run status = %q, want succeeded (error=%q)", got.Status, got.Error)
	}
	if fp.requestCount() != 1 {
		t.Fatalf("provider RoundTrip calls = %d, want 1", fp.requestCount())
	}
}

func TestSpawn_RequestAdvertisesSandboxAndLoaderOnlyForDeferredTools(t *testing.T) {
	// R-9NBD-XAU5
	withDeferred := &fakeProvider{}
	runsDir := t.TempDir()
	r, store := newTestRunner(t, time.Minute, withDeferred)
	_, run := seedRunning(t, store, r.sandbox, runsDir)
	r.discover = func(context.Context, string, string) []agentkit.DeferredToolGroup {
		return []agentkit.DeferredToolGroup{{
			Name:  "crm",
			Blurb: "CRM tools",
			Tools: []agentkit.Tool{
				agentkit.RawTool("ikigenba_crm_lookup", "Lookup CRM records.", json.RawMessage(`{"type":"object"}`), func(context.Context, json.RawMessage) (string, error) {
					return "ok", nil
				}),
			},
		}}
	}

	r.execute(run)

	req := withDeferred.request(0)
	if req == nil {
		t.Fatalf("fake provider saw no first request")
	}
	names := requestToolNames(req)
	for _, want := range []string{"Bash", "Read", "Write", "Edit", "Glob", "Grep"} {
		if !names[want] {
			t.Fatalf("first request tools = %v, missing sandbox tool %q", sortedToolNames(req.Tools), want)
		}
	}
	if !names["load_tools"] {
		t.Fatalf("first request tools = %v, want load_tools for non-empty deferred groups", sortedToolNames(req.Tools))
	}
	for name := range names {
		if strings.HasPrefix(name, "ikigenba_") {
			t.Fatalf("first request included eager suite tool %q; want only sandbox tools plus load_tools", name)
		}
	}

	withoutDeferred := &fakeProvider{}
	runsDir = t.TempDir()
	r, store = newTestRunner(t, time.Minute, withoutDeferred)
	_, run = seedRunning(t, store, r.sandbox, runsDir)
	r.discover = func(context.Context, string, string) []agentkit.DeferredToolGroup {
		return []agentkit.DeferredToolGroup{}
	}

	r.execute(run)

	req = withoutDeferred.request(0)
	if req == nil {
		t.Fatalf("empty-deferred fake provider saw no first request")
	}
	names = requestToolNames(req)
	if names["load_tools"] {
		t.Fatalf("first request tools = %v, want no load_tools when deferred groups are empty", sortedToolNames(req.Tools))
	}
	for _, want := range []string{"Bash", "Read", "Write", "Edit", "Glob", "Grep"} {
		if !names[want] {
			t.Fatalf("empty-deferred first request tools = %v, missing sandbox tool %q", sortedToolNames(req.Tools), want)
		}
	}
}

func TestSpawn_SystemFramesDeferredToolsWithoutServiceEnumeration(t *testing.T) {
	// R-9OJA-B2KU
	// R-6AUG-NHQY
	// R-6I5U-Y474
	fp := &fakeProvider{}
	runsDir := t.TempDir()
	r, store := newTestRunner(t, time.Minute, fp)
	_, run := seedRunning(t, store, r.sandbox, runsDir)
	r.discover = func(context.Context, string, string) []agentkit.DeferredToolGroup {
		return []agentkit.DeferredToolGroup{{
			Name: "crm",
			Tools: []agentkit.Tool{
				agentkit.RawTool("ikigenba_crm_lookup", "Lookup CRM records.", json.RawMessage(`{"type":"object"}`), func(context.Context, json.RawMessage) (string, error) {
					return "ok", nil
				}),
			},
		}}
	}

	r.execute(run)

	req := fp.request(0)
	if req == nil {
		t.Fatalf("fake provider saw no first request")
	}
	if !strings.Contains(req.System, "deferred tools") || !strings.Contains(req.System, "load_tools") {
		t.Fatalf("system prompt = %q, want deferred-tools guidance naming load_tools", req.System)
	}
	if strings.Contains(req.System, "ikigenba_") {
		t.Fatalf("system prompt enumerates suite tool names: %q", req.System)
	}
	for _, phrase := range []string{"bash, read, write, edit, glob, grep, and fetch", "Fetch takes a suite content URL", "pdftotext", "pdftoppm", "pdfinfo"} {
		if !strings.Contains(req.System, phrase) {
			t.Fatalf("system prompt = %q, missing %q", req.System, phrase)
		}
	}
}

func TestSpawn_SystemFramesDeferredToolGroupNameLoading(t *testing.T) {
	// R-A69O-ATWI
	fp := &fakeProvider{}
	runsDir := t.TempDir()
	r, store := newTestRunner(t, time.Minute, fp)
	_, run := seedRunning(t, store, r.sandbox, runsDir)
	r.discover = func(context.Context, string, string) []agentkit.DeferredToolGroup {
		return []agentkit.DeferredToolGroup{{
			Name: "crm",
			Tools: []agentkit.Tool{
				agentkit.RawTool("ikigenba_crm_lookup", "Lookup CRM records.", json.RawMessage(`{"type":"object"}`), func(context.Context, json.RawMessage) (string, error) {
					return "ok", nil
				}),
			},
		}}
	}

	r.execute(run)

	req := fp.request(0)
	if req == nil {
		t.Fatalf("fake provider saw no first request")
	}
	want := "call `load_tools` with tool names — or a service's name to load all of its tools — to make them callable"
	if !strings.Contains(req.System, want) {
		t.Fatalf("system prompt = %q, want group-name loading guidance %q", req.System, want)
	}
}

func TestExecute_LoadsAndCallsDeferredSuiteTool(t *testing.T) {
	// R-9PR6-OUBJ
	const toolName = "ikigenba_crm_lookup"
	fp := &fakeProvider{roundTrips: []*agentkit.RoundTrip{
		scriptedRoundTrip(agentkit.ToolUseBlock{ID: "toolu_load", Name: "load_tools", Input: json.RawMessage(`{"tools":["` + toolName + `"]}`)}),
		scriptedRoundTrip(agentkit.ToolUseBlock{ID: "toolu_crm", Name: toolName, Input: json.RawMessage(`{"id":"contact_123"}`)}),
		scriptedTextRoundTrip("done"),
	}}
	runsDir := t.TempDir()
	r, store := newTestRunner(t, time.Minute, fp)
	sess, run := seedRunning(t, store, r.sandbox, runsDir)

	var calledWith json.RawMessage
	r.discover = func(context.Context, string, string) []agentkit.DeferredToolGroup {
		return []agentkit.DeferredToolGroup{{
			Name:  "crm",
			Blurb: "CRM tools",
			Tools: []agentkit.Tool{
				agentkit.RawTool(toolName, "Lookup CRM records.", json.RawMessage(`{"type":"object","properties":{"id":{"type":"string"}}}`), func(_ context.Context, input json.RawMessage) (string, error) {
					calledWith = append(json.RawMessage(nil), input...)
					return `{"name":"Ada Lovelace"}`, nil
				}),
			},
		}}
	}

	r.execute(run)

	got, err := store.GetLatestRun(context.Background(), sess.ID)
	if err != nil {
		t.Fatalf("GetLatestRun: %v", err)
	}
	if got == nil || got.Status != prompt.RunSucceeded {
		t.Fatalf("run = %#v, want succeeded", got)
	}
	if string(calledWith) != `{"id":"contact_123"}` {
		t.Fatalf("deferred tool input = %s, want contact id payload", calledWith)
	}
	if fp.requestCount() != 3 {
		t.Fatalf("provider RoundTrip calls = %d, want load, native call, final", fp.requestCount())
	}
	second := fp.request(1)
	if second == nil {
		t.Fatalf("fake provider saw no second request")
	}
	if !requestToolNames(second)[toolName] {
		t.Fatalf("second request tools = %v, want loaded deferred tool", sortedToolNames(second.Tools))
	}

	records := readRunnerLogRecords(t, run.LogPath)
	if !hasToolUse(records, "load_tools") || !hasToolResult(records, "load_tools") {
		t.Fatalf("log records missing load_tools tool_use/tool_result: %v", logToolEvents(records))
	}
	if !hasToolUse(records, toolName) || !hasToolResult(records, toolName) {
		t.Fatalf("log records missing deferred tool_use/tool_result: %v", logToolEvents(records))
	}
}

// TestSpawn_DiscoversSuiteTools asserts the runner builds in-run suite deferred
// tool groups at spawn via the injectable discover seam, calling it with the
// run's OwnerEmail/PromptID, and that the resulting catalog is threaded into the
// engine (the run completes successfully with the fake source wired). It reuses
// the fake-client seam so no real Anthropic call is made.
func TestSpawn_DiscoversSuiteTools(t *testing.T) {
	// R-K95Z-3S0C
	fp := &fakeProvider{}
	runsDir := t.TempDir()
	r, store := newTestRunner(t, time.Minute, fp)
	sess, run := seedRunning(t, store, r.sandbox, runsDir)

	var (
		mu          sync.Mutex
		calls       int
		gotOwner    string
		gotPromptID string
	)
	r.discover = func(ctx context.Context, owner, promptID string) []agentkit.DeferredToolGroup {
		mu.Lock()
		calls++
		gotOwner = owner
		gotPromptID = promptID
		mu.Unlock()
		return []agentkit.DeferredToolGroup{{
			Name:  "suite",
			Blurb: "suite tools",
			Tools: []agentkit.Tool{
				agentkit.RawTool("suite_lookup", "suite lookup", json.RawMessage(`{"type":"object"}`), func(context.Context, json.RawMessage) (string, error) {
					return "ok", nil
				}),
			},
		}}
	}

	r.Spawn(run)
	got := waitRun(t, store, sess.ID)

	if got.Status != prompt.RunSucceeded {
		t.Fatalf("run status = %q, want succeeded (error=%q)", got.Status, got.Error)
	}

	mu.Lock()
	defer mu.Unlock()
	if calls != 1 {
		t.Fatalf("discover seam called %d times, want exactly 1", calls)
	}
	if gotOwner != run.OwnerEmail {
		t.Fatalf("discover owner = %q, want %q", gotOwner, run.OwnerEmail)
	}
	if gotPromptID != run.PromptID {
		t.Fatalf("discover promptID = %q, want %q", gotPromptID, run.PromptID)
	}
	req := fp.lastRequest()
	if req == nil {
		t.Fatalf("fake provider saw no request")
	}
	var foundLoader bool
	var foundSuiteLookup bool
	for _, tool := range req.Tools {
		switch tool.Name() {
		case "load_tools":
			foundLoader = strings.Contains(tool.Description(), "suite") &&
				strings.Contains(tool.Description(), "suite tools") &&
				strings.Contains(tool.Description(), "suite_lookup")
		case "suite_lookup":
			foundSuiteLookup = true
		}
	}
	if !foundLoader {
		t.Fatalf("provider request tools did not include load_tools catalog for discovered suite group")
	}
	if foundSuiteLookup {
		t.Fatalf("provider request included deferred suite_lookup eagerly")
	}
}

func requestToolNames(req *agentkit.Request) map[string]bool {
	names := make(map[string]bool, len(req.Tools))
	for _, tool := range req.Tools {
		names[tool.Name()] = true
	}
	return names
}

func sortedToolNames(tools []agentkit.Tool) []string {
	names := make([]string, 0, len(tools))
	for _, tool := range tools {
		names = append(names, tool.Name())
	}
	return names
}

type runnerLogRecord struct {
	Type    string               `json:"type"`
	ToolUse *agentkit.ToolUse    `json:"tool_use"`
	Result  *agentkit.ToolResult `json:"tool_result"`
}

func readRunnerLogRecords(t *testing.T, path string) []runnerLogRecord {
	t.Helper()
	data, err := os.ReadFile(path)
	if err != nil {
		t.Fatalf("read log: %v", err)
	}
	var records []runnerLogRecord
	for _, line := range strings.Split(strings.TrimSpace(string(data)), "\n") {
		if strings.TrimSpace(line) == "" {
			continue
		}
		var record runnerLogRecord
		if err := json.Unmarshal([]byte(line), &record); err != nil {
			t.Fatalf("parse log line %q: %v", line, err)
		}
		records = append(records, record)
	}
	return records
}

func hasToolUse(records []runnerLogRecord, name string) bool {
	for _, record := range records {
		if record.Type == "tool_use" && record.ToolUse != nil && record.ToolUse.Name == name {
			return true
		}
	}
	return false
}

func hasToolResult(records []runnerLogRecord, name string) bool {
	for _, record := range records {
		if record.Type == "tool_result" && record.Result != nil && record.Result.Name == name {
			return true
		}
	}
	return false
}

func logToolEvents(records []runnerLogRecord) []string {
	var events []string
	for _, record := range records {
		switch {
		case record.Type == "tool_use" && record.ToolUse != nil:
			events = append(events, "use:"+record.ToolUse.Name)
		case record.Type == "tool_result" && record.Result != nil:
			events = append(events, "result:"+record.Result.Name)
		}
	}
	return events
}

// TestNew_DefaultDiscoverWired confirms the default construction (no seam
// override) installs a working discover closure over the configured
// manifestRoot — a smoke assertion that the default path is wired and returns a
// non-nil group slice (suite.Discover's best-effort contract) without standing up
// real peers.
func TestNew_DefaultDiscoverWired(t *testing.T) {
	ctx := context.Background()
	conn, err := appkitdb.Open(filepath.Join(t.TempDir(), "prompts.db"))
	if err != nil {
		t.Fatalf("appkitdb.Open: %v", err)
	}
	t.Cleanup(func() { conn.Close() })
	migs, err := appkitdb.LoadMigrations(db.FS, "migrations")
	if err != nil {
		t.Fatalf("appkitdb.LoadMigrations: %v", err)
	}
	if err := appkitdb.Migrate(ctx, conn, migs); err != nil {
		t.Fatalf("appkitdb.Migrate: %v", err)
	}
	sb, err := sandbox.New(filepath.Join(t.TempDir(), "sandboxes"))
	if err != nil {
		t.Fatalf("sandbox.New: %v", err)
	}

	r := New(prompt.NewStore(conn), sb, time.Minute, t.TempDir(), func(int) bool { return false })
	if r.discover == nil {
		t.Fatalf("New left discover seam nil")
	}
	if groups := r.discover(ctx, "owner@example.com", "p_123"); groups == nil {
		t.Fatalf("default discover returned nil group slice; want non-nil (best-effort contract)")
	}
}

func TestCancel(t *testing.T) {
	fp := &fakeProvider{block: true}
	runsDir := t.TempDir()
	r, store := newTestRunner(t, time.Minute, fp)
	sess, run := seedRunning(t, store, r.sandbox, runsDir)

	r.Spawn(run)

	// Wait until the run goroutine has registered its cancel func, then cancel
	// by run_id.
	var cancelled bool
	deadline := time.Now().Add(2 * time.Second)
	for time.Now().Before(deadline) {
		if r.Cancel(run.ID) {
			cancelled = true
			break
		}
		time.Sleep(2 * time.Millisecond)
	}
	if !cancelled {
		t.Fatalf("Cancel never returned true")
	}

	got := waitRun(t, store, sess.ID)
	if got.Status != prompt.RunCancelled {
		t.Fatalf("run status = %q, want cancelled", got.Status)
	}
	if got.Error != "cancelled" {
		t.Fatalf("run error = %q, want \"cancelled\"", got.Error)
	}

	// Cancelling an absent run returns false.
	if r.Cancel("no-such-run") {
		t.Fatalf("Cancel of absent run returned true")
	}
}

func TestTTLFires(t *testing.T) {
	fp := &fakeProvider{block: true}
	runsDir := t.TempDir()
	r, store := newTestRunner(t, 50*time.Millisecond, fp)
	sess, run := seedRunning(t, store, r.sandbox, runsDir)

	r.Spawn(run)
	got := waitRun(t, store, sess.ID)

	if got.Status != prompt.RunFailed {
		t.Fatalf("run status = %q, want failed", got.Status)
	}
	if got.Error != "run TTL exceeded" {
		t.Fatalf("run error = %q, want \"run TTL exceeded\"", got.Error)
	}
}

func TestRecover(t *testing.T) {
	fp := &fakeProvider{} // unused; Recover does not run the engine
	runsDir := t.TempDir()
	r, store := newTestRunner(t, time.Minute, fp)
	// Seed a running session+run but never spawn it — it is an orphan.
	sess, _ := seedRunning(t, store, r.sandbox, runsDir)

	n, err := r.Recover(context.Background())
	if err != nil {
		t.Fatalf("Recover: %v", err)
	}
	if n < 1 {
		t.Fatalf("Recover swept %d runs, want >= 1", n)
	}

	run, err := store.GetLatestRun(context.Background(), sess.ID)
	if err != nil {
		t.Fatalf("GetLatestRun: %v", err)
	}
	if run == nil || run.Status != prompt.RunFailed {
		t.Fatalf("swept run status = %v, want failed", run)
	}
}
