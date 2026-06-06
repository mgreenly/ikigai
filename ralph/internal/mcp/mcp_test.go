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

	"ralph/internal/db"
	"ralph/internal/sandbox"
	"ralph/internal/session"
)

// fakeRunner records Spawn/Cancel and leaves the run running (does not
// auto-complete), mirroring the session/runner test pattern.
type fakeRunner struct {
	mu      sync.Mutex
	spawned []session.Run
}

func (f *fakeRunner) Spawn(sess session.Session, run session.Run) {
	f.mu.Lock()
	defer f.mu.Unlock()
	f.spawned = append(f.spawned, run)
}

func (f *fakeRunner) Cancel(sessionID string) bool { return false }

const (
	ownerEmail = "owner@example.com"
	clientID   = "client-123"
)

func newTestHandler(t *testing.T) (*Handler, *sandbox.Manager) {
	t.Helper()
	t.Setenv("ANTHROPIC_API_KEY", "sk-test")
	ctx := t.Context()

	conn, err := db.Open(filepath.Join(t.TempDir(), "ralph.db"))
	if err != nil {
		t.Fatalf("db.Open: %v", err)
	}
	t.Cleanup(func() { conn.Close() })
	if err := db.Migrate(ctx, conn); err != nil {
		t.Fatalf("db.Migrate: %v", err)
	}
	sb, err := sandbox.New(filepath.Join(t.TempDir(), "sandboxes"))
	if err != nil {
		t.Fatalf("sandbox.New: %v", err)
	}
	store := session.NewStore(conn)
	svc := session.NewService(store, sb, t.TempDir(), &fakeRunner{})
	return NewHandler(svc, "1.2.3", "ralph", nil), sb
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

func TestToolsListReturns12(t *testing.T) {
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
	if len(resp.Result.Tools) != 12 {
		t.Fatalf("want 12 tools, got %d", len(resp.Result.Tools))
	}
	got := make([]string, 0, 12)
	for _, tl := range resp.Result.Tools {
		got = append(got, tl.Name)
	}
	sort.Strings(got)
	want := []string{
		"ikigenba_ralph_describe",
		"ikigenba_ralph_health",
		"ikigenba_ralph_session_cancel",
		"ikigenba_ralph_session_create",
		"ikigenba_ralph_session_delete",
		"ikigenba_ralph_session_fs_list",
		"ikigenba_ralph_session_fs_read",
		"ikigenba_ralph_session_get",
		"ikigenba_ralph_session_list",
		"ikigenba_ralph_session_output",
		"ikigenba_ralph_session_run",
		"ikigenba_ralph_session_update",
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

func TestDescribe(t *testing.T) {
	h, _ := newTestHandler(t)
	res := call(t, h, "ikigenba_ralph_describe", nil)
	if isError(res) {
		t.Fatalf("ikigenba_ralph_describe returned isError: %+v", res)
	}
	txt := resultText(t, res)
	if len(txt) == 0 {
		t.Fatal("ikigenba_ralph_describe returned empty text")
	}
	// Sanity: it should actually describe the lifecycle entry points, not just
	// be non-empty.
	for _, want := range []string{"session", "ikigenba_ralph_session_create", "ikigenba_ralph_session_run"} {
		if !strings.Contains(txt, want) {
			t.Fatalf("ikigenba_ralph_describe text missing %q:\n%s", want, txt)
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
	if !strings.Contains(resp.Result.Instructions, "ikigenba_ralph_describe") {
		t.Fatalf("instructions should point at ralph_describe, got: %q", resp.Result.Instructions)
	}
}

func TestHealth(t *testing.T) {
	h, _ := newTestHandler(t)
	res := call(t, h, "ikigenba_ralph_health", nil)
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
	if out.Status != "ok" || out.Version != "1.2.3" || out.Service != "ralph" {
		t.Fatalf("health envelope: got %+v", out)
	}
	if out.OwnerEmail != ownerEmail || out.ClientID != clientID {
		t.Fatalf("health identity: got %+v", out)
	}
	// ralph supplies no reporter → details is an empty object, always present.
	if out.Details == nil || len(out.Details) != 0 {
		t.Fatalf("health details: want empty {}, got %+v", out.Details)
	}
}

func createSession(t *testing.T, h *Handler) string {
	t.Helper()
	res := call(t, h, "ikigenba_ralph_session_create", map[string]any{
		"prompt": "do a thing",
		"config": map[string]any{"model": "haiku"},
		"name":   "test",
	})
	var out struct {
		SessionID string `json:"session_id"`
		Status    string `json:"status"`
	}
	if err := json.Unmarshal([]byte(resultText(t, res)), &out); err != nil {
		t.Fatalf("decode create: %v", err)
	}
	if out.SessionID == "" {
		t.Fatalf("create: empty session_id")
	}
	if out.Status != session.StatusIdle {
		t.Fatalf("create: want idle, got %q", out.Status)
	}
	return out.SessionID
}

func TestDispatchRoundtrip(t *testing.T) {
	h, sb := newTestHandler(t)

	id := createSession(t, h)

	// get returns it, idle, no last_run.
	getRes := call(t, h, "ikigenba_ralph_session_get", map[string]any{"session_id": id})
	var detail session.SessionDetail
	if err := json.Unmarshal([]byte(resultText(t, getRes)), &detail); err != nil {
		t.Fatalf("decode get: %v", err)
	}
	if detail.ID != id || detail.LastRun != nil {
		t.Fatalf("get before run: %+v", detail)
	}

	// list includes it.
	listRes := call(t, h, "ikigenba_ralph_session_list", nil)
	var listOut struct {
		Sessions []session.Session `json:"sessions"`
	}
	if err := json.Unmarshal([]byte(resultText(t, listRes)), &listOut); err != nil {
		t.Fatalf("decode list: %v", err)
	}
	if len(listOut.Sessions) != 1 || listOut.Sessions[0].ID != id {
		t.Fatalf("list: %+v", listOut.Sessions)
	}

	// fs_list on the empty sandbox returns [].
	fsRes := call(t, h, "ikigenba_ralph_session_fs_list", map[string]any{"session_id": id})
	var fsOut struct {
		Entries []sandbox.Entry `json:"entries"`
	}
	if err := json.Unmarshal([]byte(resultText(t, fsRes)), &fsOut); err != nil {
		t.Fatalf("decode fs_list: %v", err)
	}
	if len(fsOut.Entries) != 0 {
		t.Fatalf("fs_list empty: got %+v", fsOut.Entries)
	}

	// run flips to running; get.last_run reflects it.
	runRes := call(t, h, "ikigenba_ralph_session_run", map[string]any{"session_id": id})
	var runOut struct {
		Status string `json:"status"`
	}
	if err := json.Unmarshal([]byte(resultText(t, runRes)), &runOut); err != nil {
		t.Fatalf("decode run: %v", err)
	}
	if runOut.Status != "running" {
		t.Fatalf("run: want running, got %q", runOut.Status)
	}
	getRes = call(t, h, "ikigenba_ralph_session_get", map[string]any{"session_id": id})
	if err := json.Unmarshal([]byte(resultText(t, getRes)), &detail); err != nil {
		t.Fatalf("decode get2: %v", err)
	}
	if detail.Status != session.StatusRunning || detail.LastRun == nil || detail.LastRun.Status != session.RunRunning {
		t.Fatalf("get after run: %+v", detail)
	}

	// write a file into the sandbox dir, then fs_read returns it.
	if err := os.WriteFile(filepath.Join(sb.Root(id), "hello.txt"), []byte("line one\nline two\n"), 0o644); err != nil {
		t.Fatalf("write sandbox file: %v", err)
	}
	readRes := call(t, h, "ikigenba_ralph_session_fs_read", map[string]any{"session_id": id, "path": "hello.txt"})
	if got := resultText(t, readRes); got != "line one\nline two\n" {
		t.Fatalf("fs_read: got %q", got)
	}
}

func TestErrorMapping(t *testing.T) {
	h, _ := newTestHandler(t)

	// unknown id -> isError (not found).
	res := call(t, h, "ikigenba_ralph_session_get", map[string]any{"session_id": "nope"})
	if !isError(res) {
		t.Fatalf("get unknown: want isError, got %+v", res)
	}

	id := createSession(t, h)

	// run twice -> second is busy.
	if r := call(t, h, "ikigenba_ralph_session_run", map[string]any{"session_id": id}); isError(r) {
		t.Fatalf("first run: unexpected isError %+v", r)
	}
	busy := call(t, h, "ikigenba_ralph_session_run", map[string]any{"session_id": id})
	if !isError(busy) || !contains(resultText(t, busy), "flight") {
		t.Fatalf("second run: want busy isError, got %+v", busy)
	}

	// update/delete while running -> isError.
	upd := call(t, h, "ikigenba_ralph_session_update", map[string]any{
		"session_id": id, "prompt": "x", "config": map[string]any{"model": "haiku"},
	})
	if !isError(upd) {
		t.Fatalf("update while running: want isError, got %+v", upd)
	}
	del := call(t, h, "ikigenba_ralph_session_delete", map[string]any{"session_id": id})
	if !isError(del) {
		t.Fatalf("delete while running: want isError, got %+v", del)
	}

	// fs_read path escape -> isError.
	esc := call(t, h, "ikigenba_ralph_session_fs_read", map[string]any{"session_id": id, "path": "../escape"})
	if !isError(esc) {
		t.Fatalf("fs_read escape: want isError, got %+v", esc)
	}
}

func contains(s, sub string) bool { return bytes.Contains([]byte(s), []byte(sub)) }
