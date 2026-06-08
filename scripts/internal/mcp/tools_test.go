package mcp

import (
	"bytes"
	"encoding/json"
	"net/http"
	"net/http/httptest"
	"os"
	"path/filepath"
	"sort"
	"strings"
	"sync"
	"testing"

	"scripts/internal/db"
	"scripts/internal/script"
)

const (
	ownerEmail = "owner@example.com"
	clientID   = "client-123"
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

// newTestHandler wires a real store over a temp DB + a fake recording runner,
// and the MCP Handler over that Service. runsDir is the on-disk run tree root.
func newTestHandler(t *testing.T) (*Handler, *fakeRunner, string) {
	t.Helper()
	ctx := t.Context()

	conn, err := db.Open(filepath.Join(t.TempDir(), "scripts.db"))
	if err != nil {
		t.Fatalf("db.Open: %v", err)
	}
	t.Cleanup(func() { conn.Close() })
	if err := db.Migrate(ctx, conn); err != nil {
		t.Fatalf("db.Migrate: %v", err)
	}

	runsDir := t.TempDir()
	store := script.NewStore(conn)
	fr := &fakeRunner{}
	svc := script.NewService(store, runsDir, fr)
	// No reporter wired (mirrors main.go): health.details is the static contract.
	return NewHandler(svc, "1.2.3", "scripts", nil), fr, runsDir
}

// call drives one tools/call over ServeHTTP and returns the decoded result.
func call(t *testing.T, h *Handler, tool string, args map[string]any) map[string]any {
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

func do(t *testing.T, h *Handler, body []byte) *httptest.ResponseRecorder {
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

// TestToolsListReturns16 asserts the exact 16-tool surface, every name carrying
// the ikigenba_scripts_ prefix, with the run-scoped run_fs_* readers present and
// NO bare fs_list / fs_read.
func TestToolsListReturns16(t *testing.T) {
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
	if len(resp.Result.Tools) != 16 {
		t.Fatalf("want 16 tools, got %d", len(resp.Result.Tools))
	}
	got := make([]string, 0, 16)
	for _, tl := range resp.Result.Tools {
		got = append(got, tl.Name)
		if !strings.HasPrefix(tl.Name, "ikigenba_scripts_") {
			t.Fatalf("tool %q is missing the ikigenba_scripts_ prefix", tl.Name)
		}
		if tl.Name == "ikigenba_scripts_fs_list" || tl.Name == "ikigenba_scripts_fs_read" {
			t.Fatalf("found bare fs reader %q: must be run-scoped run_fs_*", tl.Name)
		}
	}
	sort.Strings(got)
	want := []string{
		"ikigenba_scripts_clear_trigger",
		"ikigenba_scripts_create",
		"ikigenba_scripts_delete",
		"ikigenba_scripts_describe",
		"ikigenba_scripts_get",
		"ikigenba_scripts_health",
		"ikigenba_scripts_list",
		"ikigenba_scripts_run",
		"ikigenba_scripts_run_cancel",
		"ikigenba_scripts_run_fs_list",
		"ikigenba_scripts_run_fs_read",
		"ikigenba_scripts_run_get",
		"ikigenba_scripts_run_list",
		"ikigenba_scripts_run_output",
		"ikigenba_scripts_set_trigger",
		"ikigenba_scripts_update",
	}
	for i := range want {
		if got[i] != want[i] {
			t.Fatalf("tool names mismatch:\n got %v\nwant %v", got, want)
		}
	}
}

// createScript runs create and returns the new script_id.
func createScript(t *testing.T, h *Handler) string {
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

// TestHealthDetailsContract asserts health returns the exact static runtime
// contract under details, plus the injected identity.
func TestHealthDetailsContract(t *testing.T) {
	h, _, _ := newTestHandler(t)
	res := call(t, h, tool("health"), nil)
	var out struct {
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
	if err := json.Unmarshal([]byte(resultText(t, res)), &out); err != nil {
		t.Fatalf("decode health: %v", err)
	}
	if out.Version != "1.2.3" || out.Service != "scripts" {
		t.Fatalf("health envelope: %+v", out)
	}
	if out.OwnerEmail != ownerEmail || out.ClientID != clientID {
		t.Fatalf("health identity: %+v", out)
	}
	if out.Details.PythonVersion != ">=3.11" || out.Details.BashVersion != ">=5.0" ||
		out.Details.Network != true || out.Details.Packages != "stdlib" {
		t.Fatalf("health details contract mismatch: %+v", out.Details)
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
	for _, want := range []string{"script", "ikigenba_scripts_create", "ikigenba_scripts_run"} {
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
