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
	appkitmcp "appkit/mcp"
	"appkit/server"

	"eventplane/consumer"

	"prompts/internal/consume"
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

var testSources = []string{"cron", "crm", "ledger", "dropbox", "scripts", "prompts"}

func newTestHandler(t *testing.T) (http.Handler, *sandbox.Manager, *prompt.Service) {
	t.Helper()
	t.Setenv("ANTHROPIC_API_KEY", "sk-test")
	ctx := t.Context()

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
	// The sandbox Manager is rooted at the runs dir (A2): a run's sandbox is
	// runs/<run_id>/sandbox, sharing the run directory with output.jsonl.
	runsDir := t.TempDir()
	sb, err := sandbox.New(runsDir)
	if err != nil {
		t.Fatalf("sandbox.New: %v", err)
	}
	store := prompt.NewStore(conn)
	svc := prompt.NewService(store, sb, runsDir, &fakeRunner{})
	var handler http.Handler
	_, err = server.New(server.Options{
		Addr:    "127.0.0.1:0",
		Apex:    true,
		Logger:  slog.New(slog.NewTextHandler(io.Discard, nil)),
		Version: "1.2.3",
		Service: "prompts",
		Events:  prompt.Events,
		Subscriptions: func() []consumer.Subscription {
			return consume.Subscriptions(testSources)
		},
		Register: func(rt *server.Router) error {
			h, err := NewHandler(svc, rt)
			if err != nil {
				return err
			}
			handler = rt.RequireIdentity(h)
			return nil
		},
	})
	if err != nil {
		t.Fatalf("server.New: %v", err)
	}
	if handler == nil {
		t.Fatal("server.New did not build MCP handler")
	}
	return handler, sb, svc
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

func stringField(t *testing.T, m map[string]any, key string) string {
	t.Helper()
	v, ok := m[key].(string)
	if !ok {
		t.Fatalf("%s has type %T: %#v", key, m[key], m[key])
	}
	return v
}

func assertNoHandlerKey(t *testing.T, m map[string]any) {
	t.Helper()
	if _, ok := m["handler"]; ok {
		t.Fatalf("reflection leaked handler key: %#v", m)
	}
	if _, ok := m["Handler"]; ok {
		t.Fatalf("reflection leaked Handler key: %#v", m)
	}
}

func equalStrings(a, b []string) bool {
	if len(a) != len(b) {
		return false
	}
	for i := range a {
		if a[i] != b[i] {
			return false
		}
	}
	return true
}

func TestToolsListThroughAssembledHandlerReturnsDomainPlusChassisTools(t *testing.T) {
	// R-DKQP-QZ3Q
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
	if len(got) != len(want) {
		t.Fatalf("name count mismatch: %v", got)
	}
	for i := range want {
		if got[i] != want[i] {
			t.Fatalf("tool names mismatch:\n got %v\nwant %v", got, want)
		}
	}

	withReserved := append(Tools(nil), appkitmcp.Tool{Name: "health"})
	if _, err := appkitmcp.New(appkitmcp.Options{Tools: withReserved}); err == nil {
		t.Fatal("appkit MCP New accepted a domain-declared reserved health tool")
	}
}

func TestReflectionThroughAssembledHandlerReportsPromptsEventGraph(t *testing.T) {
	// R-DLYM-4QUF
	h, _, _ := newTestHandler(t)
	res := call(t, h, "reflection", nil)
	if isError(res) {
		t.Fatalf("reflection returned isError: %+v", res)
	}
	var out struct {
		Publishes  []map[string]any `json:"publishes"`
		Subscribes []map[string]any `json:"subscribes"`
	}
	if err := json.Unmarshal([]byte(resultText(t, res)), &out); err != nil {
		t.Fatalf("decode reflection: %v", err)
	}
	gotPublishes := make([]string, 0, len(out.Publishes))
	for _, pub := range out.Publishes {
		assertNoHandlerKey(t, pub)
		gotPublishes = append(gotPublishes, stringField(t, pub, "kind"))
	}
	wantPublishes := []string{"run.succeeded", "run.failed"}
	if !equalStrings(gotPublishes, wantPublishes) {
		t.Fatalf("publishes = %v, want %v", gotPublishes, wantPublishes)
	}
	if len(out.Subscribes) != len(testSources) {
		t.Fatalf("subscribes count = %d, want %d: %+v", len(out.Subscribes), len(testSources), out.Subscribes)
	}
	for i, sub := range out.Subscribes {
		assertNoHandlerKey(t, sub)
		if got := stringField(t, sub, "source"); got != testSources[i] {
			t.Fatalf("subscribes[%d].source = %q, want %q", i, got, testSources[i])
		}
		if got := stringField(t, sub, "filter"); got != "*" {
			t.Fatalf("subscribes[%d].filter = %q, want *", i, got)
		}
	}
}

// TestSetAndClearTrigger drives the two new MCP tools end-to-end: create a
// session, set a trigger (defaults applied), then clear it.
func TestSetAndClearTrigger(t *testing.T) {
	h, _, _ := newTestHandler(t)

	created := call(t, h, "create", map[string]any{
		"user_prompt": "hi",
		"config":      map[string]any{"model": "claude-haiku-4-5"},
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
	h, _, _ := newTestHandler(t)
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
	h, _, _ := newTestHandler(t)
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
	h, _, _ := newTestHandler(t)
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

func createPrompt(t *testing.T, h http.Handler) string {
	t.Helper()
	res := call(t, h, "create", map[string]any{
		"user_prompt": "do a thing",
		"config":      map[string]any{"model": "claude-haiku-4-5"},
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
	h, sb, _ := newTestHandler(t)

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

// stubFetcher is a prompt.ContentFetcher returning canned bytes, so the import
// dispatch is exercised without a live dropbox.
type stubFetcher struct{ data []byte }

func (f stubFetcher) Fetch(ctx context.Context, path string) ([]byte, error) { return f.data, nil }

// TestDispatchImport proves the import verb routes to svc.Import and returns
// {prompt_id, name}, with the file body adopted as the prompt's user_prompt.
func TestDispatchImport(t *testing.T) {
	h, _, svc := newTestHandler(t)
	svc.Fetcher = stubFetcher{data: []byte("draft the weekly update\n")}

	res := call(t, h, "import", map[string]any{"source_path": "/prompts/weekly.md"})
	if isError(res) {
		t.Fatalf("import returned isError: %+v", res)
	}
	var out struct {
		PromptID string `json:"prompt_id"`
		Name     string `json:"name"`
	}
	if err := json.Unmarshal([]byte(resultText(t, res)), &out); err != nil {
		t.Fatalf("decode import: %v", err)
	}
	if out.PromptID == "" || out.Name != "weekly.md" {
		t.Fatalf("import result: %+v", out)
	}

	// get the imported prompt and confirm the body landed as user_prompt.
	getRes := call(t, h, "get", map[string]any{"prompt_id": out.PromptID})
	var detail prompt.PromptDetail
	if err := json.Unmarshal([]byte(resultText(t, getRes)), &detail); err != nil {
		t.Fatalf("decode get: %v", err)
	}
	if detail.UserPrompt != "draft the weekly update\n" {
		t.Fatalf("imported user_prompt: %q", detail.UserPrompt)
	}
}

func TestErrorMapping(t *testing.T) {
	h, _, _ := newTestHandler(t)

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
		"prompt_id": id, "user_prompt": "x", "config": map[string]any{"model": "claude-haiku-4-5"},
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
