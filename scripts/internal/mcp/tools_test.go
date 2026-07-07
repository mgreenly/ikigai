package mcp

import (
	"bytes"
	"context"
	"encoding/json"
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

	appkitdb "appkit/db"
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
}

// newTestHarness wires a real store over a temp DB + a fake recording runner,
// captures an appkit Router from server.New, and assembles the shared MCP
// handler through NewHandler. runsDir is the on-disk run tree root.
func newTestHarness(t *testing.T) testHarness {
	t.Helper()
	ctx := t.Context()

	conn, err := appkitdb.Open(filepath.Join(t.TempDir(), "scripts_test.db"))
	if err != nil {
		t.Fatalf("open test db: %v", err)
	}
	t.Cleanup(func() { conn.Close() })
	migs, err := appkitdb.LoadMigrations(scriptdb.FS, "migrations")
	if err != nil {
		t.Fatalf("load migrations: %v", err)
	}
	if err := appkitdb.Migrate(ctx, conn, migs); err != nil {
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
	return testHarness{mcpHandler: h, server: srv.Handler, runner: fr, runsDir: runsDir}
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
	conn, err := appkitdb.Open(filepath.Join(t.TempDir(), "scripts_test.db"))
	if err != nil {
		t.Fatalf("open test db: %v", err)
	}
	t.Cleanup(func() { conn.Close() })
	migs, err := appkitdb.LoadMigrations(scriptdb.FS, "migrations")
	if err != nil {
		t.Fatalf("load migrations: %v", err)
	}
	if err := appkitdb.Migrate(ctx, conn, migs); err != nil {
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
	h, _, _ := newTestHandler(t)
	id := createScript(t, h)

	ok := call(t, h, tool("set_trigger"), map[string]any{
		"script_id":    id,
		"source":       "crm",
		"event_filter": "contact.created",
	})
	if isError(ok) {
		t.Fatalf("set_trigger valid returned isError: %+v", ok)
	}
	var trig script.Trigger
	if err := json.Unmarshal([]byte(resultText(t, ok)), &trig); err != nil {
		t.Fatalf("decode set_trigger: %v", err)
	}
	if trig.Source != "crm" || trig.EventFilter != "contact.created" {
		t.Fatalf("set_trigger: %+v", trig)
	}

	bad := call(t, h, tool("set_trigger"), map[string]any{
		"script_id":    id,
		"source":       "nope",
		"event_filter": "x.*",
	})
	if !isError(bad) {
		t.Fatalf("set_trigger invalid source: want isError, got %+v", bad)
	}

	// Clearing the valid trigger succeeds.
	cleared := call(t, h, tool("clear_trigger"), map[string]any{
		"script_id":    id,
		"source":       "crm",
		"event_filter": "contact.created",
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
