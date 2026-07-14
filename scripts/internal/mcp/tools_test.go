package mcp

import (
	"bytes"
	"context"
	"encoding/json"
	"errors"
	"io"
	"log/slog"
	"net/http"
	"net/http/httptest"
	"os"
	"path/filepath"
	"sort"
	"strings"
	"sync"
	"testing"

	appkitdatabase "appkit/db"
	appkitmcp "appkit/mcp"
	"appkit/server"

	scriptdb "scripts/internal/db"
	"scripts/internal/script"
)

const (
	ownerEmail  = "owner@example.com"
	clientID    = "client-123"
	testVersion = "1.2.3"
	testService = "scripts"
)

// fakeRunner records Spawn/Cancel and leaves runs running (does not
// auto-complete), so no python is ever execed. It satisfies script.Runner.
type fakeRunner struct {
	mu       sync.Mutex
	spawns   []script.Run
	cancels  []string
	cancelOK bool
}

func (f *fakeRunner) Spawn(run script.Run, input []byte) {
	f.mu.Lock()
	defer f.mu.Unlock()
	f.spawns = append(f.spawns, run)
}

func (f *fakeRunner) Cancel(runID string) bool {
	f.mu.Lock()
	defer f.mu.Unlock()
	f.cancels = append(f.cancels, runID)
	return f.cancelOK
}

func (f *fakeRunner) spawnCount() int {
	f.mu.Lock()
	defer f.mu.Unlock()
	return len(f.spawns)
}

type testHarness struct {
	mcpHandler http.Handler
	server     http.Handler
	runner     *fakeRunner
	runsDir    string
	svc        *script.Service
}

// newTestHarness wires a real store over a temp DB + a fake recording runner,
// captures an appkit Router from server.New, and assembles the shared MCP
// handler through NewHandler. runsDir is the on-disk run tree root.
func newTestHarness(t *testing.T) testHarness {
	t.Helper()
	ctx := t.Context()

	conn, err := appkitdatabase.Open(filepath.Join(t.TempDir(), "scripts_test.db"))
	if err != nil {
		t.Fatalf("open test db: %v", err)
	}
	t.Cleanup(func() { conn.Close() })
	migs, err := appkitdatabase.LoadMigrations(scriptdb.FS, "migrations")
	if err != nil {
		t.Fatalf("load migrations: %v", err)
	}
	if err := appkitdatabase.Migrate(ctx, conn, migs); err != nil {
		t.Fatalf("migrate test db: %v", err)
	}

	runsDir := t.TempDir()
	store := script.NewStore(conn)
	fr := &fakeRunner{}
	svc := script.NewService(store, runsDir, fr)
	var captured *server.Router
	srv, err := server.New(server.Options{
		Addr:       "127.0.0.1:0",
		Logger:     slog.New(slog.NewTextHandler(io.Discard, nil)),
		ResourceID: "https://example.test/srv/scripts",
		AuthServer: "https://auth.example.test",
		Version:    testVersion,
		Service:    testService,
		Events:     script.Events,
		Health: func(ctx context.Context) (map[string]any, error) {
			return map[string]any{
				"python_version": ">=3.11",
				"bash_version":   ">=5.0",
				"network":        true,
				"packages":       "stdlib",
			}, nil
		},
		DB: conn,
		Register: func(rt *server.Router) error {
			captured = rt
			return nil
		},
	})
	if err != nil {
		t.Fatalf("build test router: %v", err)
	}
	if captured == nil {
		t.Fatalf("server.New did not invoke Register")
	}
	h, err := NewHandler(svc, captured)
	if err != nil {
		t.Fatalf("build mcp handler: %v", err)
	}
	return testHarness{mcpHandler: h, server: srv.Handler, runner: fr, runsDir: runsDir, svc: svc}
}

func newTestHandler(t *testing.T) (http.Handler, *fakeRunner, string) {
	t.Helper()
	h := newTestHarness(t)
	return h.mcpHandler, h.runner, h.runsDir
}

// fakeFetcher is a script.ContentFetcher returning canned bytes, so the import
// dispatch is exercised with no live dropbox / network.
type fakeFetcher struct {
	data []byte
	err  error
}

func (f fakeFetcher) Fetch(ctx context.Context, path string) ([]byte, error) {
	return f.data, f.err
}

// newTestHandlerWithFetcher wires a handler whose Service has its Fetcher set to
// the given fake, for the import verb dispatch test.
func newTestHandlerWithFetcher(t *testing.T, f script.ContentFetcher) http.Handler {
	t.Helper()
	ctx := t.Context()
	conn, err := appkitdatabase.Open(filepath.Join(t.TempDir(), "scripts_test.db"))
	if err != nil {
		t.Fatalf("open test db: %v", err)
	}
	t.Cleanup(func() { conn.Close() })
	migs, err := appkitdatabase.LoadMigrations(scriptdb.FS, "migrations")
	if err != nil {
		t.Fatalf("load migrations: %v", err)
	}
	if err := appkitdatabase.Migrate(ctx, conn, migs); err != nil {
		t.Fatalf("migrate test db: %v", err)
	}
	svc := script.NewService(script.NewStore(conn), t.TempDir(), &fakeRunner{})
	svc.Fetcher = f
	var captured *server.Router
	_, err = server.New(server.Options{
		Addr:       "127.0.0.1:0",
		Logger:     slog.New(slog.NewTextHandler(io.Discard, nil)),
		ResourceID: "https://example.test/srv/scripts",
		AuthServer: "https://auth.example.test",
		Version:    testVersion,
		Service:    testService,
		Events:     script.Events,
		Health: func(ctx context.Context) (map[string]any, error) {
			return map[string]any{
				"python_version": ">=3.11",
				"bash_version":   ">=5.0",
				"network":        true,
				"packages":       "stdlib",
			}, nil
		},
		DB: conn,
		Register: func(rt *server.Router) error {
			captured = rt
			return nil
		},
	})
	if err != nil {
		t.Fatalf("build test router: %v", err)
	}
	if captured == nil {
		t.Fatalf("server.New did not invoke Register")
	}
	h, err := NewHandler(svc, captured)
	if err != nil {
		t.Fatalf("build mcp handler: %v", err)
	}
	return h
}

// TestImportDispatch asserts the import verb routes to the service and returns
// {script_id, name}, deriving the name from the source path basename.
func TestImportDispatch(t *testing.T) {
	h := newTestHandlerWithFetcher(t, fakeFetcher{data: []byte("print('hi')\n")})
	res := call(t, h, tool("import"), map[string]any{"source_path": "/scripts/nightly.py"})
	if isError(res) {
		t.Fatalf("import returned isError: %+v", res)
	}
	var out struct {
		ScriptID string `json:"script_id"`
		Name     string `json:"name"`
	}
	if err := json.Unmarshal([]byte(resultText(t, res)), &out); err != nil {
		t.Fatalf("decode import: %v", err)
	}
	if out.ScriptID == "" {
		t.Fatalf("import: empty script_id")
	}
	if out.Name != "nightly.py" {
		t.Fatalf("import: name not derived from basename: %q", out.Name)
	}
}

// call drives one tools/call over ServeHTTP and returns the decoded result.
func call(t *testing.T, h http.Handler, tool string, args map[string]any) map[string]any {
	t.Helper()
	argsJSON, _ := json.Marshal(args)
	body, _ := json.Marshal(map[string]any{
		"jsonrpc": "2.0",
		"id":      1,
		"method":  "tools/call",
		"params":  map[string]any{"name": tool, "arguments": json.RawMessage(argsJSON)},
	})
	rr := do(t, h, body)
	var resp struct {
		Result map[string]any `json:"result"`
		Error  *struct {
			Code    int    `json:"code"`
			Message string `json:"message"`
		} `json:"error"`
	}
	if err := json.Unmarshal(rr.Body.Bytes(), &resp); err != nil {
		t.Fatalf("decode response: %v (body=%s)", err, rr.Body.String())
	}
	if resp.Error != nil {
		t.Fatalf("tool %q: unexpected JSON-RPC error: %+v", tool, resp.Error)
	}
	if resp.Result == nil {
		t.Fatalf("tool %q: nil result (body=%s)", tool, rr.Body.String())
	}
	return resp.Result
}

func do(t *testing.T, h http.Handler, body []byte) *httptest.ResponseRecorder {
	t.Helper()
	req := httptest.NewRequest(http.MethodPost, "/mcp", bytes.NewReader(body))
	req.Header.Set("X-Owner-Email", ownerEmail)
	req.Header.Set("X-Client-Id", clientID)
	rr := httptest.NewRecorder()
	h.ServeHTTP(rr, req)
	return rr
}

func resultText(t *testing.T, res map[string]any) string {
	t.Helper()
	content, ok := res["content"].([]any)
	if !ok || len(content) == 0 {
		t.Fatalf("result has no content: %+v", res)
	}
	block := content[0].(map[string]any)
	return block["text"].(string)
}

func isError(res map[string]any) bool {
	v, _ := res["isError"].(bool)
	return v
}

// TestToolsListPartitionsDomainAndChassis asserts the exact 18-tool surface:
// the shared chassis health/reflection tools plus scripts' 16 domain verbs.
func TestToolsListPartitionsDomainAndChassis(t *testing.T) {
	// R-91IM-JYPA
	h, _, _ := newTestHandler(t)
	body, _ := json.Marshal(map[string]any{"jsonrpc": "2.0", "id": 1, "method": "tools/list"})
	rr := do(t, h, body)

	var resp struct {
		Result struct {
			Tools []struct {
				Name string `json:"name"`
			} `json:"tools"`
		} `json:"result"`
	}
	if err := json.Unmarshal(rr.Body.Bytes(), &resp); err != nil {
		t.Fatalf("decode: %v", err)
	}
	if len(resp.Result.Tools) != 18 {
		t.Fatalf("want 18 tools, got %d", len(resp.Result.Tools))
	}
	got := make([]string, 0, 18)
	for _, tl := range resp.Result.Tools {
		got = append(got, tl.Name)
		if tl.Name == "fs_list" || tl.Name == "fs_read" {
			t.Fatalf("found bare fs reader %q: must be run-scoped run_fs_*", tl.Name)
		}
	}
	sort.Strings(got)
	want := []string{
		"clear_trigger",
		"create",
		"delete",
		"describe",
		"get",
		"health",
		"import",
		"list",
		"reflection",
		"run",
		"run_cancel",
		"run_fs_list",
		"run_fs_read",
		"run_get",
		"run_list",
		"run_output",
		"set_trigger",
		"update",
	}
	for i := range want {
		if got[i] != want[i] {
			t.Fatalf("tool names mismatch:\n got %v\nwant %v", got, want)
		}
	}
}

func TestReflectionReportsScriptCompletionFamilies(t *testing.T) {
	// R-83IC-SYVO
	h, _, _ := newTestHandler(t)
	index := call(t, h, "reflection", map[string]any{})
	var indexBody struct {
		Publishes []struct {
			Kind    string `json:"kind"`
			Subject string `json:"subject"`
		} `json:"publishes"`
	}
	if err := json.Unmarshal([]byte(resultText(t, index)), &indexBody); err != nil {
		t.Fatalf("decode reflection index: %v", err)
	}
	if len(indexBody.Publishes) != 2 {
		t.Fatalf("publishes = %+v, want exactly two families", indexBody.Publishes)
	}
	for i, want := range []string{"succeeded", "failed"} {
		if got := indexBody.Publishes[i]; got.Kind != want || got.Subject != "/<script name>" {
			t.Fatalf("publish[%d] = %+v, want (%q, /<script name>)", i, got, want)
		}
	}

	detail := call(t, h, "reflection", map[string]any{"kind": "succeeded"})
	var detailBody map[string]any
	if err := json.Unmarshal([]byte(resultText(t, detail)), &detailBody); err != nil {
		t.Fatalf("decode reflection detail: %v", err)
	}
	schema, ok := detailBody["schema"].(map[string]any)
	if !ok {
		t.Fatalf("reflection detail schema = %#v", detailBody["schema"])
	}
	example, ok := detailBody["example"].(map[string]any)
	if !ok {
		t.Fatalf("reflection detail example = %#v", detailBody["example"])
	}
	properties, ok := schema["properties"].(map[string]any)
	if !ok {
		t.Fatalf("reflection schema properties = %#v", schema)
	}
	for field := range example {
		if _, ok := properties[field]; !ok {
			t.Errorf("example field %q absent from schema", field)
		}
	}
	trigger, ok := example["trigger"].(map[string]any)
	_, hasType := trigger["type"]
	if !ok || trigger["kind"] == nil || trigger["subject"] == nil || hasType {
		t.Fatalf("trigger example = %#v, want kind+subject and no type", example["trigger"])
	}

	unknown := call(t, h, "reflection", map[string]any{"kind": "missing"})
	if !isError(unknown) {
		t.Fatalf("unknown kind result = %+v, want tool error", unknown)
	}
	if text := resultText(t, unknown); !strings.Contains(text, "missing") || !strings.Contains(text, "succeeded") || !strings.Contains(text, "failed") {
		t.Fatalf("unknown kind error = %q, want missing kind and declared kinds", text)
	}
}

// createScript runs create and returns the new script_id.
func createScript(t *testing.T, h http.Handler) string {
	t.Helper()
	res := call(t, h, tool("create"), map[string]any{
		"name": "nightly",
		"body": "print('hi')",
	})
	if isError(res) {
		t.Fatalf("create returned isError: %+v", res)
	}
	var out struct {
		ScriptID string `json:"script_id"`
	}
	if err := json.Unmarshal([]byte(resultText(t, res)), &out); err != nil {
		t.Fatalf("decode create: %v", err)
	}
	if out.ScriptID == "" {
		t.Fatalf("create: empty script_id")
	}
	return out.ScriptID
}

// TestCreateGetList drives create -> get -> list end to end through the handlers.
func TestCreateGetList(t *testing.T) {
	h, _, _ := newTestHandler(t)
	id := createScript(t, h)

	getRes := call(t, h, tool("get"), map[string]any{"script_id": id})
	var detail script.ScriptDetail
	if err := json.Unmarshal([]byte(resultText(t, getRes)), &detail); err != nil {
		t.Fatalf("decode get: %v", err)
	}
	if detail.ID != id || detail.Name != "nightly" || detail.LastRun != nil {
		t.Fatalf("get: %+v", detail)
	}
	if detail.Config.Interpreter != "python3" {
		t.Fatalf("get: config interpreter not defaulted: %+v", detail.Config)
	}

	listRes := call(t, h, tool("list"), nil)
	var listOut struct {
		Scripts []script.ScriptDetail `json:"scripts"`
	}
	if err := json.Unmarshal([]byte(resultText(t, listRes)), &listOut); err != nil {
		t.Fatalf("decode list: %v", err)
	}
	if len(listOut.Scripts) != 1 || listOut.Scripts[0].ID != id {
		t.Fatalf("list: %+v", listOut.Scripts)
	}
}

// TestRunSpawns asserts a manual run inserts a run row and calls the fake
// runner's Spawn (no python exec).
func TestRunSpawns(t *testing.T) {
	h, fr, _ := newTestHandler(t)
	id := createScript(t, h)

	runRes := call(t, h, tool("run"), map[string]any{"script_id": id})
	if isError(runRes) {
		t.Fatalf("run returned isError: %+v", runRes)
	}
	var runOut struct {
		RunID  string `json:"run_id"`
		Status string `json:"status"`
	}
	if err := json.Unmarshal([]byte(resultText(t, runRes)), &runOut); err != nil {
		t.Fatalf("decode run: %v", err)
	}
	if runOut.RunID == "" || runOut.Status != script.RunRunning {
		t.Fatalf("run: %+v", runOut)
	}
	if fr.spawnCount() != 1 {
		t.Fatalf("expected exactly one Spawn, got %d", fr.spawnCount())
	}

	// The run row is visible via run_get and run_list.
	getRes := call(t, h, tool("run_get"), map[string]any{"run_id": runOut.RunID})
	var run script.Run
	if err := json.Unmarshal([]byte(resultText(t, getRes)), &run); err != nil {
		t.Fatalf("decode run_get: %v", err)
	}
	if run.ID != runOut.RunID || run.Status != script.RunRunning {
		t.Fatalf("run_get: %+v", run)
	}

	listRes := call(t, h, tool("run_list"), map[string]any{"script_id": id})
	var rl struct {
		Runs []script.Run `json:"runs"`
	}
	if err := json.Unmarshal([]byte(resultText(t, listRes)), &rl); err != nil {
		t.Fatalf("decode run_list: %v", err)
	}
	if len(rl.Runs) != 1 || rl.Runs[0].ID != runOut.RunID {
		t.Fatalf("run_list: %+v", rl.Runs)
	}
}

// TestSetTriggerValidAndInvalid: a valid binding succeeds; an unknown source
// surfaces ErrValidation as an MCP tool error (isError).
func TestSetTriggerValidAndInvalid(t *testing.T) {
	// R-812K-1FEA
	harness := newTestHarness(t)
	h := harness.mcpHandler
	var set, clear appkitmcp.Tool
	for _, candidate := range Tools(harness.svc) {
		switch candidate.Name {
		case tool("set_trigger"):
			set = candidate
		case tool("clear_trigger"):
			clear = candidate
		}
	}
	for _, candidate := range []appkitmcp.Tool{set, clear} {
		props := candidate.InputSchema["properties"].(map[string]any)
		if _, ok := props["filter"]; !ok {
			t.Fatalf("%s lacks filter: %v", candidate.Name, props)
		}
		if _, ok := props["source"]; ok {
			t.Fatalf("%s retains source", candidate.Name)
		}
		if _, ok := props["event"+"_filter"]; ok {
			t.Fatalf("%s retains retired filter", candidate.Name)
		}
	}
	if !strings.Contains(describeText, "source:kind") || strings.Contains(describeText, "event"+"_filter") {
		t.Fatalf("describe trigger contract is stale")
	}
	id := createScript(t, h)

	ok := call(t, h, tool("set_trigger"), map[string]any{
		"script_id": id,
		"filter":    "crm:contact.created",
	})
	if isError(ok) {
		t.Fatalf("set_trigger valid returned isError: %+v", ok)
	}
	var trig script.Trigger
	if err := json.Unmarshal([]byte(resultText(t, ok)), &trig); err != nil {
		t.Fatalf("decode set_trigger: %v", err)
	}
	if trig.Source != "crm" || trig.Filter != "crm:contact.created" {
		t.Fatalf("set_trigger: %+v", trig)
	}

	bad := call(t, h, tool("set_trigger"), map[string]any{
		"script_id": id,
		"filter":    "nope:x.*",
	})
	if !isError(bad) {
		t.Fatalf("set_trigger invalid source: want isError, got %+v", bad)
	}

	// Clearing the valid trigger succeeds.
	cleared := call(t, h, tool("clear_trigger"), map[string]any{
		"script_id": id,
		"filter":    "crm:contact.created",
	})
	if isError(cleared) {
		t.Fatalf("clear_trigger returned isError: %+v", cleared)
	}
}

// TestRunOutputAndFs drives run_output, run_fs_list, and run_fs_read against a
// hand-made run dir under runsDir/<run_id>/.
func TestRunOutputAndFs(t *testing.T) {
	h, _, runsDir := newTestHandler(t)
	id := createScript(t, h)

	runRes := call(t, h, tool("run"), map[string]any{"script_id": id})
	var runOut struct {
		RunID string `json:"run_id"`
	}
	if err := json.Unmarshal([]byte(resultText(t, runRes)), &runOut); err != nil {
		t.Fatalf("decode run: %v", err)
	}

	// Hand-build the persisted run dir the runner would have created.
	dir := filepath.Join(runsDir, runOut.RunID)
	if err := os.MkdirAll(dir, 0o755); err != nil {
		t.Fatalf("mkdir run dir: %v", err)
	}
	if err := os.WriteFile(filepath.Join(dir, "stdout.log"), []byte("out one\nout two\n"), 0o644); err != nil {
		t.Fatalf("write stdout.log: %v", err)
	}
	if err := os.WriteFile(filepath.Join(dir, "stderr.log"), []byte("err one\n"), 0o644); err != nil {
		t.Fatalf("write stderr.log: %v", err)
	}
	if err := os.WriteFile(filepath.Join(dir, "result.txt"), []byte("alpha\nbeta\ngamma\n"), 0o644); err != nil {
		t.Fatalf("write result.txt: %v", err)
	}

	// run_output stdout.
	outRes := call(t, h, tool("run_output"), map[string]any{"run_id": runOut.RunID, "stream": "stdout"})
	if got := resultText(t, outRes); got != "out one\nout two\n" {
		t.Fatalf("run_output stdout: got %q", got)
	}

	// run_fs_list shows the three files.
	fsRes := call(t, h, tool("run_fs_list"), map[string]any{"run_id": runOut.RunID})
	var fsOut struct {
		Entries []script.FileEntry `json:"entries"`
	}
	if err := json.Unmarshal([]byte(resultText(t, fsRes)), &fsOut); err != nil {
		t.Fatalf("decode run_fs_list: %v", err)
	}
	if len(fsOut.Entries) != 3 {
		t.Fatalf("run_fs_list: want 3 entries, got %+v", fsOut.Entries)
	}

	// run_fs_read with a line offset/limit slice.
	readRes := call(t, h, tool("run_fs_read"), map[string]any{
		"run_id": runOut.RunID, "path": "result.txt", "offset": 2, "limit": 1,
	})
	if got := resultText(t, readRes); got != "beta\n" {
		t.Fatalf("run_fs_read slice: got %q", got)
	}

	// Path traversal is rejected as a tool error.
	esc := call(t, h, tool("run_fs_read"), map[string]any{"run_id": runOut.RunID, "path": "../escape"})
	if !isError(esc) {
		t.Fatalf("run_fs_read escape: want isError, got %+v", esc)
	}
}

// TestHealthDetailsContract asserts the appkit health reporter serves the same
// runtime contract through MCP health and HTTP GET /health.
func TestHealthDetailsContract(t *testing.T) {
	// R-92QI-XQFZ
	h := newTestHarness(t)
	res := call(t, h.mcpHandler, "health", nil)
	var mcpOut struct {
		Status     string `json:"status"`
		Version    string `json:"version"`
		Service    string `json:"service"`
		OwnerEmail string `json:"owner_email"`
		ClientID   string `json:"client_id"`
		Details    struct {
			PythonVersion string `json:"python_version"`
			BashVersion   string `json:"bash_version"`
			Network       bool   `json:"network"`
			Packages      string `json:"packages"`
		} `json:"details"`
	}
	if err := json.Unmarshal([]byte(resultText(t, res)), &mcpOut); err != nil {
		t.Fatalf("decode health: %v", err)
	}
	if mcpOut.Version != testVersion || mcpOut.Service != testService {
		t.Fatalf("mcp health envelope: %+v", mcpOut)
	}
	if mcpOut.OwnerEmail != ownerEmail || mcpOut.ClientID != clientID {
		t.Fatalf("mcp health identity: %+v", mcpOut)
	}
	assertRuntimeContract(t, mcpOut.Details)

	req := httptest.NewRequest(http.MethodGet, "/health", nil)
	rr := httptest.NewRecorder()
	h.server.ServeHTTP(rr, req)
	if rr.Code != http.StatusOK {
		t.Fatalf("GET /health status = %d, body=%s", rr.Code, rr.Body.String())
	}
	var httpOut struct {
		Status  string `json:"status"`
		Version string `json:"version"`
		Service string `json:"service"`
		Details struct {
			PythonVersion string `json:"python_version"`
			BashVersion   string `json:"bash_version"`
			Network       bool   `json:"network"`
			Packages      string `json:"packages"`
		} `json:"details"`
	}
	if err := json.Unmarshal(rr.Body.Bytes(), &httpOut); err != nil {
		t.Fatalf("decode GET /health: %v", err)
	}
	if httpOut.Version != testVersion || httpOut.Service != testService {
		t.Fatalf("http health envelope: %+v", httpOut)
	}
	if httpOut.Status != "ok" {
		t.Fatalf("http health status = %q", httpOut.Status)
	}
	assertRuntimeContract(t, httpOut.Details)
}

func assertRuntimeContract(t *testing.T, details struct {
	PythonVersion string `json:"python_version"`
	BashVersion   string `json:"bash_version"`
	Network       bool   `json:"network"`
	Packages      string `json:"packages"`
}) {
	t.Helper()
	if details.PythonVersion != ">=3.11" || details.BashVersion != ">=5.0" ||
		details.Network != true || details.Packages != "stdlib" {
		t.Fatalf("health details contract mismatch: %+v", details)
	}
}

// TestDescribeNonEmpty asserts describe returns non-empty lifecycle markdown.
func TestDescribeNonEmpty(t *testing.T) {
	h, _, _ := newTestHandler(t)
	res := call(t, h, tool("describe"), nil)
	if isError(res) {
		t.Fatalf("describe returned isError: %+v", res)
	}
	txt := resultText(t, res)
	if len(txt) == 0 {
		t.Fatal("describe returned empty text")
	}
	for _, want := range []string{"script", "create", "run"} {
		if !strings.Contains(txt, want) {
			t.Fatalf("describe missing %q:\n%s", want, txt)
		}
	}
}

// TestErrorMapping: unknown ids surface as MCP tool errors (isError).
func TestErrorMapping(t *testing.T) {
	h, _, _ := newTestHandler(t)
	res := call(t, h, tool("get"), map[string]any{"script_id": "nope"})
	if !isError(res) {
		t.Fatalf("get unknown: want isError, got %+v", res)
	}
}

func listedTools(t *testing.T, h http.Handler) map[string]map[string]any {
	t.Helper()
	body, _ := json.Marshal(map[string]any{"jsonrpc": "2.0", "id": 1, "method": "tools/list"})
	rr := do(t, h, body)
	var resp struct {
		Result struct {
			Tools []map[string]any `json:"tools"`
		} `json:"result"`
	}
	if err := json.Unmarshal(rr.Body.Bytes(), &resp); err != nil {
		t.Fatalf("decode tools/list: %v", err)
	}
	tools := make(map[string]map[string]any, len(resp.Result.Tools))
	for _, descriptor := range resp.Result.Tools {
		name, _ := descriptor["name"].(string)
		tools[name] = descriptor
	}
	return tools
}

func assertStructuredMirrorsText(t *testing.T, name string, result map[string]any) {
	t.Helper()
	structured, ok := result["structuredContent"].(map[string]any)
	if !ok {
		t.Fatalf("%s structuredContent = %#v, want object", name, result["structuredContent"])
	}
	var mirrored map[string]any
	if err := json.Unmarshal([]byte(resultText(t, result)), &mirrored); err != nil {
		t.Fatalf("%s mirrored text is not a JSON object: %v", name, err)
	}
	if !mapsEqual(structured, mirrored) {
		t.Fatalf("%s structuredContent differs from mirrored text:\nstructured=%#v\ntext=%#v", name, structured, mirrored)
	}
}

func mapsEqual(a, b map[string]any) bool {
	aJSON, errA := json.Marshal(a)
	bJSON, errB := json.Marshal(b)
	return errA == nil && errB == nil && bytes.Equal(aJSON, bJSON)
}

func TestStructuredToolsMirrorMachineAndTextResults(t *testing.T) {
	// R-C0G0-V0QL
	h := newTestHarness(t)
	h.svc.Fetcher = fakeFetcher{data: []byte("print('imported')\n")}
	h.runner.cancelOK = true
	scriptID := createScript(t, h.mcpHandler)
	deleteID := createScript(t, h.mcpHandler)

	runResult := call(t, h.mcpHandler, tool("run"), map[string]any{"script_id": scriptID})
	runID := runResult["structuredContent"].(map[string]any)["run_id"].(string)
	runDir := filepath.Join(h.runsDir, runID)
	if err := os.MkdirAll(runDir, 0o755); err != nil {
		t.Fatalf("mkdir run dir: %v", err)
	}
	if err := os.WriteFile(filepath.Join(runDir, "result.txt"), []byte("result\n"), 0o644); err != nil {
		t.Fatalf("write run file: %v", err)
	}

	cases := []struct {
		name string
		args map[string]any
	}{
		{"create", map[string]any{"name": "created", "body": "print(1)"}},
		{"import", map[string]any{"source_path": "/scripts/imported.py"}},
		{"list", nil},
		{"get", map[string]any{"script_id": scriptID}},
		{"update", map[string]any{"script_id": scriptID, "name": "updated"}},
		{"delete", map[string]any{"script_id": deleteID}},
		{"set_trigger", map[string]any{"script_id": scriptID, "filter": "crm:contact.created"}},
		{"clear_trigger", map[string]any{"script_id": scriptID, "filter": "crm:contact.created"}},
		{"run", map[string]any{"script_id": scriptID}},
		{"run_list", map[string]any{"script_id": scriptID}},
		{"run_get", map[string]any{"run_id": runID}},
		{"run_fs_list", map[string]any{"run_id": runID}},
		{"run_cancel", map[string]any{"run_id": runID}},
	}
	assertStructuredMirrorsText(t, "run", runResult)
	for _, tc := range cases {
		t.Run(tc.name, func(t *testing.T) {
			result := call(t, h.mcpHandler, tool(tc.name), tc.args)
			if isError(result) {
				t.Fatalf("%s returned error: %#v", tc.name, result)
			}
			assertStructuredMirrorsText(t, tc.name, result)
		})
	}
}

func TestProseToolsRemainTextOnly(t *testing.T) {
	// R-C1NX-8SHA
	h := newTestHarness(t)
	scriptID := createScript(t, h.mcpHandler)
	run := call(t, h.mcpHandler, tool("run"), map[string]any{"script_id": scriptID})
	runID := run["structuredContent"].(map[string]any)["run_id"].(string)
	runDir := filepath.Join(h.runsDir, runID)
	if err := os.MkdirAll(runDir, 0o755); err != nil {
		t.Fatalf("mkdir run dir: %v", err)
	}
	for name, content := range map[string]string{"stdout.log": "stdout\n", "stderr.log": "", "note.txt": "note\n"} {
		if err := os.WriteFile(filepath.Join(runDir, name), []byte(content), 0o644); err != nil {
			t.Fatalf("write %s: %v", name, err)
		}
	}
	for _, tc := range []struct {
		name string
		args map[string]any
	}{
		{"describe", nil},
		{"run_output", map[string]any{"run_id": runID, "stream": "stdout"}},
		{"run_fs_read", map[string]any{"run_id": runID, "path": "note.txt"}},
	} {
		result := call(t, h.mcpHandler, tool(tc.name), tc.args)
		if _, present := result["structuredContent"]; present {
			t.Errorf("%s unexpectedly returned structuredContent: %#v", tc.name, result)
		}
		if resultText(t, result) == "" {
			t.Errorf("%s returned empty text", tc.name)
		}
	}
}

func TestToolOutputSchemaPartition(t *testing.T) {
	// R-C2VT-MK7Z
	h, _, _ := newTestHandler(t)
	descriptors := listedTools(t, h)
	structured := []string{"create", "import", "list", "get", "update", "delete", "set_trigger", "clear_trigger", "run", "run_list", "run_get", "run_cancel", "run_fs_list"}
	for _, name := range structured {
		schema, ok := descriptors[tool(name)]["outputSchema"].(map[string]any)
		if !ok || len(schema) == 0 {
			t.Errorf("%s outputSchema = %#v, want non-empty object", name, descriptors[tool(name)]["outputSchema"])
		}
	}
	for _, name := range []string{"describe", "run_output", "run_fs_read"} {
		if _, present := descriptors[tool(name)]["outputSchema"]; present {
			t.Errorf("%s unexpectedly declares outputSchema", name)
		}
	}
}

func TestOutputSchemasMatchScriptAndRunWireCasing(t *testing.T) {
	// R-C43Q-0BYO
	h := newTestHarness(t)
	scriptID := createScript(t, h.mcpHandler)
	update := call(t, h.mcpHandler, tool("update"), map[string]any{"script_id": scriptID, "name": "updated"})
	run := call(t, h.mcpHandler, tool("run"), map[string]any{"script_id": scriptID})
	runID := run["structuredContent"].(map[string]any)["run_id"].(string)
	runGet := call(t, h.mcpHandler, tool("run_get"), map[string]any{"run_id": runID})
	descriptors := listedTools(t, h.mcpHandler)

	checks := []struct {
		name     string
		result   map[string]any
		required []string
	}{
		{"update", update, []string{"ID", "OwnerEmail", "Name", "Body", "Config", "SourcePath", "CreatedAt", "UpdatedAt"}},
		{"run_get", runGet, []string{"id", "script_id", "exit_code", "started_at", "elapsed_secs"}},
	}
	for _, check := range checks {
		properties := descriptors[tool(check.name)]["outputSchema"].(map[string]any)["properties"].(map[string]any)
		for key := range check.result["structuredContent"].(map[string]any) {
			if _, declared := properties[key]; !declared {
				t.Errorf("%s wire key %q absent from outputSchema", check.name, key)
			}
		}
		for _, key := range check.required {
			if _, declared := properties[key]; !declared {
				t.Errorf("%s outputSchema missing casing-sensitive key %q", check.name, key)
			}
		}
	}
}

func TestStructuredErrorCodesFollowDomainSentinels(t *testing.T) {
	// R-C5BM-E3PD
	// R-C6JI-RVG2
	h, _, _ := newTestHandler(t)
	for _, tc := range []struct {
		name string
		args map[string]any
		code string
	}{
		{"create", map[string]any{"name": "", "body": "print(1)"}, "validation"},
		{"get", map[string]any{"script_id": "unknown"}, "not_found"},
	} {
		result := call(t, h, tool(tc.name), tc.args)
		if !isError(result) {
			t.Fatalf("%s result = %#v, want isError", tc.name, result)
		}
		structured, ok := result["structuredContent"].(map[string]any)
		if !ok || structured["code"] != tc.code {
			t.Errorf("%s error structuredContent = %#v, want code %q", tc.name, structured, tc.code)
		}
	}
}

func TestUnknownDomainErrorDefaultsToInternalCode(t *testing.T) {
	// R-C7RF-5N6R
	h := newTestHandlerWithFetcher(t, fakeFetcher{err: errors.New("mirror unavailable")})
	result := call(t, h, tool("import"), map[string]any{"source_path": "/scripts/fail.py"})
	if !isError(result) {
		t.Fatalf("import result = %#v, want isError", result)
	}
	structured, ok := result["structuredContent"].(map[string]any)
	if !ok || structured["code"] != "internal" {
		t.Fatalf("import error structuredContent = %#v, want internal", structured)
	}
}

func TestNonTestSourceUsesStructuredResultOnly(t *testing.T) {
	// R-CA77-X6O5
	legacy := "JSON" + "Result"
	for _, root := range []string{"..", "../../cmd"} {
		err := filepath.Walk(root, func(path string, info os.FileInfo, err error) error {
			if err != nil {
				return err
			}
			if info.IsDir() || !strings.HasSuffix(path, ".go") || strings.HasSuffix(path, "_test.go") {
				return nil
			}
			source, err := os.ReadFile(path)
			if err != nil {
				return err
			}
			if bytes.Contains(source, []byte(legacy)) {
				t.Errorf("%s retains legacy result helper %q", path, legacy)
			}
			return nil
		})
		if err != nil {
			t.Fatalf("walk %s: %v", root, err)
		}
	}
}
