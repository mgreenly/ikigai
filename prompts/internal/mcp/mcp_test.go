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

	"prompts/internal/db"
	"prompts/internal/prompt"
	"prompts/internal/sandbox"
)

// fakeRunner records Spawn/Cancel and leaves the run running (does not
// auto-complete), mirroring the session/runner test pattern.
type fakeRunner struct {
	mu      sync.Mutex
	spawned []prompt.Run
}

func (f *fakeRunner) Spawn(run prompt.Run) {
	f.mu.Lock()
	defer f.mu.Unlock()
	f.spawned = append(f.spawned, run)
}

func (f *fakeRunner) Cancel(runID string) bool { return false }

const (
	ownerEmail = "owner@example.com"
	clientID   = "client-123"
)

func newTestHandler(t *testing.T) (*Handler, *sandbox.Manager) {
	t.Helper()
	t.Setenv("ANTHROPIC_API_KEY", "sk-test")
	ctx := t.Context()

	conn, err := db.Open(filepath.Join(t.TempDir(), "prompts.db"))
	if err != nil {
		t.Fatalf("db.Open: %v", err)
	}
	t.Cleanup(func() { conn.Close() })
	if err := db.Migrate(ctx, conn); err != nil {
		t.Fatalf("db.Migrate: %v", err)
	}
	// The sandbox Manager is rooted at the runs dir (A2): a run's sandbox is
	// runs/<run_id>/sandbox, sharing the run directory with output.jsonl.
	runsDir := t.TempDir()
	sb, err := sandbox.New(runsDir)
	if err != nil {
		t.Fatalf("sandbox.New: %v", err)
	}
	store := prompt.NewStore(conn)
	svc := prompt.NewService(store, sb, runsDir, &fakeRunner{})
	return NewHandler(svc, "1.2.3", "prompts", nil), sb
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

// resultText returns the first text content block of a tool result.
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

func TestToolsListReturns16(t *testing.T) {
	h, _ := newTestHandler(t)
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
	}
	sort.Strings(got)
	want := []string{
		"clear_trigger",
		"create",
		"delete",
		"describe",
		"get",
		"health",
		"list",
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
	if len(got) != len(want) {
		t.Fatalf("name count mismatch: %v", got)
	}
	for i := range want {
		if got[i] != want[i] {
			t.Fatalf("tool names mismatch:\n got %v\nwant %v", got, want)
		}
	}
}

// TestSetAndClearTrigger drives the two new MCP tools end-to-end: create a
// session, set a trigger (defaults applied), then clear it.
func TestSetAndClearTrigger(t *testing.T) {
	h, _ := newTestHandler(t)

	created := call(t, h, "create", map[string]any{
		"user_prompt": "hi",
		"config":      map[string]any{"model": "haiku"},
	})
	var cv struct {
		PromptID string `json:"prompt_id"`
	}
	if err := json.Unmarshal([]byte(resultText(t, created)), &cv); err != nil {
		t.Fatalf("decode create: %v", err)
	}

	set := call(t, h, "set_trigger", map[string]any{
		"prompt_id":    cv.PromptID,
		"source":       "dropbox",
		"event_filter": "file.created",
	})
	if isError(set) {
		t.Fatalf("set_trigger returned isError: %+v", set)
	}
	var tv struct {
		Source      string `json:"source"`
		EventFilter string `json:"event_filter"`
		CreatedAt   string `json:"created_at"`
	}
	if err := json.Unmarshal([]byte(resultText(t, set)), &tv); err != nil {
		t.Fatalf("decode set: %v", err)
	}
	if tv.Source != "dropbox" || tv.EventFilter != "file.created" || tv.CreatedAt == "" {
		t.Fatalf("unexpected trigger: %+v", tv)
	}

	// An unknown source is rejected (validation).
	bad := call(t, h, "set_trigger", map[string]any{
		"prompt_id":    cv.PromptID,
		"source":       "nope",
		"event_filter": "x",
	})
	if !isError(bad) {
		t.Fatalf("set_trigger with unknown source must return isError, got %+v", bad)
	}

	cleared := call(t, h, "clear_trigger", map[string]any{
		"prompt_id":    cv.PromptID,
		"source":       "dropbox",
		"event_filter": "file.created",
	})
	if isError(cleared) {
		t.Fatalf("clear_trigger returned isError: %+v", cleared)
	}
}

func TestDescribe(t *testing.T) {
	h, _ := newTestHandler(t)
	res := call(t, h, "describe", nil)
	if isError(res) {
		t.Fatalf("describe returned isError: %+v", res)
	}
	txt := resultText(t, res)
	if len(txt) == 0 {
		t.Fatal("describe returned empty text")
	}
	// Sanity: it should actually describe the lifecycle entry points, not just
	// be non-empty.
	for _, want := range []string{"prompt", "create", "run"} {
		if !strings.Contains(txt, want) {
			t.Fatalf("describe text missing %q:\n%s", want, txt)
		}
	}
}

func TestInitializeIncludesInstructions(t *testing.T) {
	h, _ := newTestHandler(t)
	body, _ := json.Marshal(map[string]any{"jsonrpc": "2.0", "id": 1, "method": "initialize"})
	rr := do(t, h, body)
	var resp struct {
		Result struct {
			Instructions string `json:"instructions"`
		} `json:"result"`
	}
	if err := json.Unmarshal(rr.Body.Bytes(), &resp); err != nil {
		t.Fatalf("decode: %v", err)
	}
	if resp.Result.Instructions == "" {
		t.Fatal("initialize result missing instructions")
	}
	if !strings.Contains(resp.Result.Instructions, "describe") {
		t.Fatalf("instructions should point at prompts_describe, got: %q", resp.Result.Instructions)
	}
}

func TestHealth(t *testing.T) {
	h, _ := newTestHandler(t)
	res := call(t, h, "health", nil)
	var out struct {
		Status     string         `json:"status"`
		Version    string         `json:"version"`
		Service    string         `json:"service"`
		OwnerEmail string         `json:"owner_email"`
		ClientID   string         `json:"client_id"`
		Details    map[string]any `json:"details"`
	}
	if err := json.Unmarshal([]byte(resultText(t, res)), &out); err != nil {
		t.Fatalf("decode health: %v", err)
	}
	if out.Status != "ok" || out.Version != "1.2.3" || out.Service != "prompts" {
		t.Fatalf("health envelope: got %+v", out)
	}
	if out.OwnerEmail != ownerEmail || out.ClientID != clientID {
		t.Fatalf("health identity: got %+v", out)
	}
	// prompts supplies no reporter → details is an empty object, always present.
	if out.Details == nil || len(out.Details) != 0 {
		t.Fatalf("health details: want empty {}, got %+v", out.Details)
	}
}

func createPrompt(t *testing.T, h *Handler) string {
	t.Helper()
	res := call(t, h, "create", map[string]any{
		"user_prompt": "do a thing",
		"config":      map[string]any{"model": "haiku"},
		"name":        "test",
	})
	var out struct {
		PromptID string `json:"prompt_id"`
	}
	if err := json.Unmarshal([]byte(resultText(t, res)), &out); err != nil {
		t.Fatalf("decode create: %v", err)
	}
	if out.PromptID == "" {
		t.Fatalf("create: empty prompt_id")
	}
	return out.PromptID
}

func TestDispatchRoundtrip(t *testing.T) {
	h, sb := newTestHandler(t)

	id := createPrompt(t, h)

	// get returns it, no last_run.
	getRes := call(t, h, "get", map[string]any{"prompt_id": id})
	var detail prompt.PromptDetail
	if err := json.Unmarshal([]byte(resultText(t, getRes)), &detail); err != nil {
		t.Fatalf("decode get: %v", err)
	}
	if detail.ID != id || detail.LastRun != nil {
		t.Fatalf("get before run: %+v", detail)
	}

	// list includes it.
	listRes := call(t, h, "list", nil)
	var listOut struct {
		Prompts []prompt.Prompt `json:"prompts"`
	}
	if err := json.Unmarshal([]byte(resultText(t, listRes)), &listOut); err != nil {
		t.Fatalf("decode list: %v", err)
	}
	if len(listOut.Prompts) != 1 || listOut.Prompts[0].ID != id {
		t.Fatalf("list: %+v", listOut.Prompts)
	}

	// run returns a run_id + running status.
	runRes := call(t, h, "run", map[string]any{"prompt_id": id})
	var runOut struct {
		RunID  string `json:"run_id"`
		Status string `json:"status"`
	}
	if err := json.Unmarshal([]byte(resultText(t, runRes)), &runOut); err != nil {
		t.Fatalf("decode run: %v", err)
	}
	if runOut.Status != "running" || runOut.RunID == "" {
		t.Fatalf("run: want running + run_id, got %+v", runOut)
	}
	runID := runOut.RunID

	// run_get reflects the run by run_id.
	rgRes := call(t, h, "run_get", map[string]any{"run_id": runID})
	var run prompt.Run
	if err := json.Unmarshal([]byte(resultText(t, rgRes)), &run); err != nil {
		t.Fatalf("decode run_get: %v", err)
	}
	if run.ID != runID || run.Status != prompt.RunRunning {
		t.Fatalf("run_get: %+v", run)
	}

	// run_list shows the run under the prompt.
	rlRes := call(t, h, "run_list", map[string]any{"prompt_id": id})
	var rlOut struct {
		Runs []prompt.Run `json:"runs"`
	}
	if err := json.Unmarshal([]byte(resultText(t, rlRes)), &rlOut); err != nil {
		t.Fatalf("decode run_list: %v", err)
	}
	if len(rlOut.Runs) != 1 || rlOut.Runs[0].ID != runID {
		t.Fatalf("run_list: %+v", rlOut.Runs)
	}

	// run_fs_list on the now-created (empty) run sandbox returns [].
	fsRes := call(t, h, "run_fs_list", map[string]any{"run_id": runID})
	var fsOut struct {
		Entries []sandbox.Entry `json:"entries"`
	}
	if err := json.Unmarshal([]byte(resultText(t, fsRes)), &fsOut); err != nil {
		t.Fatalf("decode run_fs_list: %v", err)
	}
	if len(fsOut.Entries) != 0 {
		t.Fatalf("run_fs_list empty: got %+v", fsOut.Entries)
	}

	// write a file into the run's sandbox dir, then run_fs_read returns it.
	if err := os.WriteFile(filepath.Join(sb.Root(runID), "hello.txt"), []byte("line one\nline two\n"), 0o644); err != nil {
		t.Fatalf("write sandbox file: %v", err)
	}
	readRes := call(t, h, "run_fs_read", map[string]any{"run_id": runID, "path": "hello.txt"})
	if got := resultText(t, readRes); got != "line one\nline two\n" {
		t.Fatalf("run_fs_read: got %q", got)
	}
}

func TestErrorMapping(t *testing.T) {
	h, _ := newTestHandler(t)

	// unknown id -> isError (not found).
	res := call(t, h, "get", map[string]any{"prompt_id": "nope"})
	if !isError(res) {
		t.Fatalf("get unknown: want isError, got %+v", res)
	}

	id := createPrompt(t, h)

	// run is always accepted now (full concurrency, no single-flight): two runs
	// of the same prompt both succeed.
	if r := call(t, h, "run", map[string]any{"prompt_id": id}); isError(r) {
		t.Fatalf("first run: unexpected isError %+v", r)
	}
	if r := call(t, h, "run", map[string]any{"prompt_id": id}); isError(r) {
		t.Fatalf("second run: want success, got isError %+v", r)
	}

	// update/delete are always allowed now (no ErrRunning), even with runs live.
	upd := call(t, h, "update", map[string]any{
		"prompt_id": id, "user_prompt": "x", "config": map[string]any{"model": "haiku"},
	})
	if isError(upd) {
		t.Fatalf("update while running: want success, got isError %+v", upd)
	}
	del := call(t, h, "delete", map[string]any{"prompt_id": id})
	if isError(del) {
		t.Fatalf("delete while running: want success, got isError %+v", del)
	}

	// Re-create + run for the path-escape check (the prior prompt was deleted).
	id = createPrompt(t, h)
	runRes := call(t, h, "run", map[string]any{"prompt_id": id})
	if isError(runRes) {
		t.Fatalf("run for escape check: unexpected isError %+v", runRes)
	}
	var runOut struct {
		RunID string `json:"run_id"`
	}
	if err := json.Unmarshal([]byte(resultText(t, runRes)), &runOut); err != nil {
		t.Fatalf("decode run for escape: %v", err)
	}
	// run_fs_read path escape -> isError.
	esc := call(t, h, "run_fs_read", map[string]any{"run_id": runOut.RunID, "path": "../escape"})
	if !isError(esc) {
		t.Fatalf("run_fs_read escape: want isError, got %+v", esc)
	}
}

func contains(s, sub string) bool { return bytes.Contains([]byte(s), []byte(sub)) }
