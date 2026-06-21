package suite

import (
	"context"
	"encoding/json"
	"net/http"
	"net/http/httptest"
	"net/url"
	"os"
	"path/filepath"
	"reflect"
	"strings"
	"sync"
	"testing"

	"github.com/ikigenba/agentkit"
)

// rpcRequest mirrors the JSON-RPC 2.0 request envelope mcpclient sends, so a peer
// can route on method and read params.
type rpcRequest struct {
	JSONRPC string          `json:"jsonrpc"`
	ID      json.RawMessage `json:"id"`
	Method  string          `json:"method"`
	Params  json.RawMessage `json:"params"`
}

// peer is a fake suite MCP service: it serves tools/list and tools/call over the
// JSON-RPC wire mcpclient speaks, recording the identity headers and tools/call
// names it saw so tests can assert routing and identity.
type peer struct {
	srv *httptest.Server

	mu          sync.Mutex
	listed      bool
	calledNames []string
	gotEmail    string
	gotClient   string

	tools    []map[string]any // tools/list payload
	listErr  bool             // tools/list returns a JSON-RPC error
	callText string           // text returned by tools/call
	callErr  bool             // isError returned by tools/call
}

func newPeer(t *testing.T, tools []map[string]any, callText string, callErr bool) *peer {
	t.Helper()
	p := &peer{tools: tools, callText: callText, callErr: callErr}
	p.srv = httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		if r.URL.Path != "/mcp" {
			http.Error(w, "not found", http.StatusNotFound)
			return
		}
		var req rpcRequest
		if err := json.NewDecoder(r.Body).Decode(&req); err != nil {
			t.Errorf("peer decode request: %v", err)
			return
		}

		p.mu.Lock()
		p.gotEmail = r.Header.Get("X-Owner-Email")
		p.gotClient = r.Header.Get("X-Client-Id")
		p.mu.Unlock()

		w.Header().Set("Content-Type", "application/json")
		switch req.Method {
		case "tools/list":
			p.mu.Lock()
			p.listed = true
			p.mu.Unlock()
			if p.listErr {
				writeError(t, w, req.ID, -32000, "list failed")
				return
			}
			writeResult(t, w, req.ID, map[string]any{"tools": p.tools})
		case "tools/call":
			var params struct {
				Name      string          `json:"name"`
				Arguments json.RawMessage `json:"arguments"`
			}
			if err := json.Unmarshal(req.Params, &params); err != nil {
				t.Errorf("peer unmarshal params: %v", err)
				return
			}
			p.mu.Lock()
			p.calledNames = append(p.calledNames, params.Name)
			p.mu.Unlock()
			writeResult(t, w, req.ID, map[string]any{
				"isError": p.callErr,
				"content": []map[string]any{{"type": "text", "text": p.callText}},
			})
		default:
			writeError(t, w, req.ID, -32601, "method not found")
		}
	}))
	t.Cleanup(p.srv.Close)
	return p
}

func newListErrorPeer(t *testing.T) *peer {
	t.Helper()
	p := newPeer(t, nil, "", false)
	p.listErr = true
	return p
}

func writeResult(t *testing.T, w http.ResponseWriter, id json.RawMessage, result any) {
	t.Helper()
	if err := json.NewEncoder(w).Encode(map[string]any{
		"jsonrpc": "2.0", "id": json.RawMessage(id), "result": result,
	}); err != nil {
		t.Fatalf("encode result: %v", err)
	}
}

func writeError(t *testing.T, w http.ResponseWriter, id json.RawMessage, code int, msg string) {
	t.Helper()
	if err := json.NewEncoder(w).Encode(map[string]any{
		"jsonrpc": "2.0", "id": json.RawMessage(id),
		"error": map[string]any{"code": code, "message": msg},
	}); err != nil {
		t.Fatalf("encode error: %v", err)
	}
}

// portOf extracts the TCP port from an httptest.Server URL (it binds 127.0.0.1,
// so http://127.0.0.1:<port>/mcp reaches it).
func portOf(t *testing.T, rawURL string) string {
	t.Helper()
	u, err := url.Parse(rawURL)
	if err != nil {
		t.Fatalf("parse %q: %v", rawURL, err)
	}
	return u.Port()
}

// writeManifest creates <root>/<svc>/etc/manifest.env with an MCP=true manifest
// pointing at the given port.
func writeManifest(t *testing.T, root, svc, port string) {
	t.Helper()
	dir := filepath.Join(root, svc, "etc")
	if err := os.MkdirAll(dir, 0o755); err != nil {
		t.Fatalf("mkdir %s: %v", dir, err)
	}
	contents := "APP=" + svc + "\nMOUNT=/srv/" + svc + "/\nPORT=" + port + "\nMCP=true\n"
	if err := os.WriteFile(filepath.Join(dir, "manifest.env"), []byte(contents), 0o644); err != nil {
		t.Fatalf("write manifest: %v", err)
	}
}

func tool(name string) map[string]any {
	return map[string]any{
		"name":        name,
		"description": name + " does a thing",
		"inputSchema": map[string]any{"type": "object"},
	}
}

func hasTool(tools []agentkit.Tool, name string) bool {
	_, ok := findTool(tools, name)
	return ok
}

func findTool(tools []agentkit.Tool, name string) (agentkit.Tool, bool) {
	for _, tool := range tools {
		if tool.Name() == name {
			return tool, true
		}
	}
	return nil, false
}

func assertJSONEqual(t *testing.T, got json.RawMessage, want any) {
	t.Helper()
	var gotValue any
	if err := json.Unmarshal(got, &gotValue); err != nil {
		t.Fatalf("unmarshal schema: %v", err)
	}
	if !reflect.DeepEqual(gotValue, want) {
		t.Fatalf("schema = %#v, want %#v", gotValue, want)
	}
}

// TestSelfExcluded: a prompts manifest in the root is not contacted and
// contributes no tools.
func TestSelfExcluded(t *testing.T) {
	root := t.TempDir()
	self := newPeer(t, []map[string]any{tool("run")}, "", false)
	crm := newPeer(t, []map[string]any{tool("list")}, "ok", false)
	writeManifest(t, root, "prompts", portOf(t, self.srv.URL))
	writeManifest(t, root, "crm", portOf(t, crm.srv.URL))

	tools := Discover(context.Background(), root, "owner@example.com", "p_123")

	if hasTool(tools, "ikigenba_prompts_run") {
		t.Error("self tool should not be owned")
	}
	if !hasTool(tools, "ikigenba_crm_list") {
		t.Error("crm tool should be owned")
	}
	self.mu.Lock()
	listed := self.listed
	self.mu.Unlock()
	if listed {
		t.Error("self peer (prompts) was contacted; it must be excluded before any call")
	}
}

// TestToolsListErrorPeerSkipped: a peer whose tools/list returns an error is
// skipped and discovery still succeeds with the live peer's tools.
func TestToolsListErrorPeerSkipped(t *testing.T) {
	// R-K32H-6XAV
	root := t.TempDir()
	live := newPeer(t, []map[string]any{tool("list")}, "ok", false)
	bad := newListErrorPeer(t)
	writeManifest(t, root, "crm", portOf(t, live.srv.URL))
	writeManifest(t, root, "ledger", portOf(t, bad.srv.URL))

	tools := Discover(context.Background(), root, "owner@example.com", "p_123")

	if !hasTool(tools, "ikigenba_crm_list") {
		t.Error("live peer's tool missing; list-error peer broke discovery")
	}
	if hasTool(tools, "ikigenba_ledger_list") {
		t.Error("list-error peer contributed a tool; want it skipped")
	}
	if got := len(tools); got != 1 {
		t.Errorf("Discover returned %d tools, want 1 (list-error peer must contribute nothing)", got)
	}
}

// TestIdentityHeaders: a live peer sees X-Owner-Email and X-Client-Id
// (prompts:<promptID>) on the tools/list request.
func TestIdentityHeaders(t *testing.T) {
	root := t.TempDir()
	crm := newPeer(t, []map[string]any{tool("list")}, "ok", false)
	writeManifest(t, root, "crm", portOf(t, crm.srv.URL))

	Discover(context.Background(), root, "alice@example.com", "p_abc")

	crm.mu.Lock()
	defer crm.mu.Unlock()
	if crm.gotEmail != "alice@example.com" {
		t.Errorf("X-Owner-Email = %q, want alice@example.com", crm.gotEmail)
	}
	if crm.gotClient != "prompts:p_abc" {
		t.Errorf("X-Client-Id = %q, want prompts:p_abc", crm.gotClient)
	}
}

// TestReachablePeerYieldsQualifiedToolAndDispatches: a reachable peer's listed
// tool becomes one service-qualified RawTool with its schema and call closure.
func TestReachablePeerYieldsQualifiedToolAndDispatches(t *testing.T) {
	// R-K4AD-KP1K
	root := t.TempDir()
	schema := map[string]any{
		"type":       "object",
		"properties": map[string]any{"q": map[string]any{"type": "string"}},
	}
	crm := newPeer(t, []map[string]any{{
		"name":        "list",
		"description": "List CRM records",
		"inputSchema": schema,
	}}, "crm-result", false)
	writeManifest(t, root, "crm", portOf(t, crm.srv.URL))

	tools := Discover(context.Background(), root, "owner@example.com", "p_123")

	if got := len(tools); got != 1 {
		t.Fatalf("Discover returned %d tools, want exactly 1", got)
	}
	got := tools[0]
	if got.Name() != "ikigenba_crm_list" {
		t.Fatalf("tool name = %q, want ikigenba_crm_list", got.Name())
	}
	if got.Description() != "List CRM records" {
		t.Fatalf("description = %q, want List CRM records", got.Description())
	}
	assertJSONEqual(t, got.JSONSchema(), schema)

	out, err := got.Call(context.Background(), json.RawMessage(`{"q":"x"}`))
	if err != nil {
		t.Fatalf("Call returned error: %v", err)
	}
	if out != "crm-result" {
		t.Errorf("Call output = %q, want crm-result", out)
	}

	crm.mu.Lock()
	crmCalls := append([]string(nil), crm.calledNames...)
	crm.mu.Unlock()
	if len(crmCalls) != 1 || crmCalls[0] != "list" {
		t.Errorf("crm calledNames = %v, want [list] (peer answers to the bare verb)", crmCalls)
	}
}

// TestSharedBareVerbReQualifiedPerService: peers now register BARE verbs, so two
// different services both expose the same verb (here `health`). The suite layer
// must re-qualify each to ikigenba_<svc>_<verb> so BOTH remain reachable under
// distinct advertised names, and each RawTool must route to the correct peer.
func TestSharedBareVerbReQualifiedPerService(t *testing.T) {
	root := t.TempDir()
	crm := newPeer(t, []map[string]any{tool("health")}, "crm-health", false)
	ledger := newPeer(t, []map[string]any{tool("health")}, "ledger-health", false)
	writeManifest(t, root, "crm", portOf(t, crm.srv.URL))
	writeManifest(t, root, "ledger", portOf(t, ledger.srv.URL))

	tools := Discover(context.Background(), root, "owner@example.com", "p_123")

	// Both services' health tools survive under distinct service-qualified names.
	crmTool, ok := findTool(tools, "ikigenba_crm_health")
	if !ok {
		t.Error("crm health tool was shadowed; want ikigenba_crm_health owned")
	}
	ledgerTool, ok := findTool(tools, "ikigenba_ledger_health")
	if !ok {
		t.Error("ledger health tool was shadowed; want ikigenba_ledger_health owned")
	}
	if got := len(tools); got != 2 {
		t.Errorf("Discover returned %d tools, want 2 (both health tools advertised)", got)
	}
	names := map[string]bool{}
	for _, tool := range tools {
		names[tool.Name()] = true
	}
	if !names["ikigenba_crm_health"] || !names["ikigenba_ledger_health"] {
		t.Errorf("advertised names = %v, want both ikigenba_crm_health and ikigenba_ledger_health", names)
	}

	// Call each qualified tool; each peer receives the BARE verb.
	crmContent, err := crmTool.Call(context.Background(), nil)
	if err != nil {
		t.Fatalf("crm tool call: %v", err)
	}
	ledgerContent, err := ledgerTool.Call(context.Background(), nil)
	if err != nil {
		t.Fatalf("ledger tool call: %v", err)
	}
	if crmContent != "crm-health" {
		t.Errorf("crm content = %q, want crm-health (routed to wrong peer)", crmContent)
	}
	if ledgerContent != "ledger-health" {
		t.Errorf("ledger content = %q, want ledger-health (routed to wrong peer)", ledgerContent)
	}

	crm.mu.Lock()
	crmCalls := append([]string(nil), crm.calledNames...)
	crm.mu.Unlock()
	ledger.mu.Lock()
	ledgerCalls := append([]string(nil), ledger.calledNames...)
	ledger.mu.Unlock()
	if len(crmCalls) != 1 || crmCalls[0] != "health" {
		t.Errorf("crm calledNames = %v, want [health] (bare verb)", crmCalls)
	}
	if len(ledgerCalls) != 1 || ledgerCalls[0] != "health" {
		t.Errorf("ledger calledNames = %v, want [health] (bare verb)", ledgerCalls)
	}
}

// TestWithinServiceDuplicateKeepsFirst: the dup guard still fires for a genuine
// within-service duplicate — one service listing the same bare verb twice yields a
// single advertised tool (first wins), not a panic or a double entry.
func TestWithinServiceDuplicateKeepsFirst(t *testing.T) {
	root := t.TempDir()
	crm := newPeer(t, []map[string]any{tool("health"), tool("health")}, "ok", false)
	writeManifest(t, root, "crm", portOf(t, crm.srv.URL))

	tools := Discover(context.Background(), root, "owner@example.com", "p_123")

	if !hasTool(tools, "ikigenba_crm_health") {
		t.Error("want ikigenba_crm_health owned")
	}
	if got := len(tools); got != 1 {
		t.Errorf("Discover returned %d tools, want 1 (within-service duplicate collapsed)", got)
	}
}

// TestDispatchDownstreamIsError: a downstream isError result yields a
// non-terminal Go error from the RawTool call.
func TestDispatchDownstreamIsError(t *testing.T) {
	root := t.TempDir()
	crm := newPeer(t, []map[string]any{tool("list")}, "boom", true)
	writeManifest(t, root, "crm", portOf(t, crm.srv.URL))

	tools := Discover(context.Background(), root, "owner@example.com", "p_123")
	tool, ok := findTool(tools, "ikigenba_crm_list")
	if !ok {
		t.Fatal("missing ikigenba_crm_list")
	}

	out, err := tool.Call(context.Background(), nil)
	if err == nil {
		t.Fatal("Call returned nil error, want error for downstream isError")
	}
	if out != "boom" {
		t.Errorf("Call output = %q, want boom", out)
	}
	if !strings.Contains(err.Error(), "boom") {
		t.Errorf("Call error = %q, want it to include downstream text", err.Error())
	}
}

// TestDispatchTransportFailureIsError: a Call against a peer that died after
// discovery yields a non-terminal Go error.
func TestDispatchTransportFailureIsError(t *testing.T) {
	root := t.TempDir()
	crm := newPeer(t, []map[string]any{tool("list")}, "ok", false)
	writeManifest(t, root, "crm", portOf(t, crm.srv.URL))

	tools := Discover(context.Background(), root, "owner@example.com", "p_123")
	tool, ok := findTool(tools, "ikigenba_crm_list")
	if !ok {
		t.Fatal("missing ikigenba_crm_list")
	}
	crm.srv.Close() // kill the peer after the snapshot

	if _, err := tool.Call(context.Background(), nil); err == nil {
		t.Fatal("Call returned nil error, want transport error")
	}
}

// TestInventoryErrorEmptySource: an inventory read error degrades to a non-nil,
// empty source (Discover never returns nil, never panics).
func TestInventoryErrorEmptySource(t *testing.T) {
	// An unclosed '[' in the root makes inventory.Read's filepath.Glob return a
	// bad-pattern error, exercising the inventory-error branch (not the empty
	// match path).
	tools := Discover(context.Background(), "bad[root", "owner@example.com", "p_123")
	if tools == nil {
		t.Fatal("Discover returned nil, want a non-nil empty slice")
	}
	if got := len(tools); got != 0 {
		t.Errorf("Discover returned %d tools, want 0", got)
	}
}
