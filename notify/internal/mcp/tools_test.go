package mcp

import (
	"bytes"
	"encoding/json"
	"net/http"
	"net/http/httptest"
	"testing"

	"eventplane/consumer"
	"eventplane/outbox"

	"notify/internal/push"
)

const (
	testOwner    = "owner@example.com"
	testClientID = "client-123"
	testVersion  = "test-1.2.3"
	testService  = "notify"
)

// newTestHandler builds a notify Handler with the consumer-only wiring: no domain
// service, an EMPTY published-event registry (notify produces nothing), and the
// live subscription provider returning notify's one declared in-edge — the same
// push.Subscription() the consumer Handler matches against.
func newTestHandler() *Handler {
	return NewHandler(testVersion, testService, nil,
		outbox.Registry{},
		func() []consumer.Subscription { return []consumer.Subscription{push.Subscription()} })
}

type jsonRPCResponse struct {
	JSONRPC string          `json:"jsonrpc"`
	ID      json.RawMessage `json:"id"`
	Result  json.RawMessage `json:"result"`
	Error   *struct {
		Code    int    `json:"code"`
		Message string `json:"message"`
	} `json:"error"`
}

type toolResult struct {
	IsError bool `json:"isError"`
	Content []struct {
		Type string `json:"type"`
		Text string `json:"text"`
	} `json:"content"`
}

// rpc drives a single JSON-RPC request through the real ServeHTTP seam with the
// nginx-injected identity headers set, and returns the decoded response.
func rpc(t *testing.T, h *Handler, method string, params any) jsonRPCResponse {
	t.Helper()
	body := map[string]any{"jsonrpc": "2.0", "id": 1, "method": method}
	if params != nil {
		body["params"] = params
	}
	raw, err := json.Marshal(body)
	if err != nil {
		t.Fatalf("marshal request: %v", err)
	}
	req := httptest.NewRequest(http.MethodPost, "/mcp", bytes.NewReader(raw))
	req.Header.Set("X-Owner-Email", testOwner)
	req.Header.Set("X-Client-Id", testClientID)
	rec := httptest.NewRecorder()
	h.ServeHTTP(rec, req)

	var resp jsonRPCResponse
	if err := json.Unmarshal(rec.Body.Bytes(), &resp); err != nil {
		t.Fatalf("decode response for %s: %v (body=%s)", method, err, rec.Body.String())
	}
	if resp.Error != nil {
		t.Fatalf("%s returned JSON-RPC error: %d %s", method, resp.Error.Code, resp.Error.Message)
	}
	return resp
}

// call drives a tools/call and decodes the inner tool result.
func call(t *testing.T, h *Handler, name string, args any) toolResult {
	t.Helper()
	resp := rpc(t, h, "tools/call", map[string]any{"name": name, "arguments": args})
	var tr toolResult
	if err := json.Unmarshal(resp.Result, &tr); err != nil {
		t.Fatalf("decode tool result for %s: %v (result=%s)", name, err, resp.Result)
	}
	return tr
}

// callOK asserts the success envelope (no isError) and decodes the text payload
// into a generic map.
func callOK(t *testing.T, h *Handler, name string, args any) map[string]any {
	t.Helper()
	tr := call(t, h, name, args)
	if tr.IsError {
		t.Fatalf("%s unexpectedly returned an error envelope: %s", name, payloadText(tr))
	}
	if len(tr.Content) != 1 || tr.Content[0].Type != "text" {
		t.Fatalf("%s: expected one text content block, got %+v", name, tr.Content)
	}
	var m map[string]any
	if err := json.Unmarshal([]byte(tr.Content[0].Text), &m); err != nil {
		t.Fatalf("%s: text payload is not a JSON object: %v (%s)", name, err, tr.Content[0].Text)
	}
	return m
}

// callErr asserts an error envelope (isError:true) and returns the rendered
// `error` object.
func callErr(t *testing.T, h *Handler, name string, args any) map[string]any {
	t.Helper()
	tr := call(t, h, name, args)
	if !tr.IsError {
		t.Fatalf("%s: expected an error envelope, got success: %s", name, payloadText(tr))
	}
	var env struct {
		Error map[string]any `json:"error"`
	}
	if err := json.Unmarshal([]byte(tr.Content[0].Text), &env); err != nil {
		t.Fatalf("%s: error payload is not the envelope shape: %v (%s)", name, err, tr.Content[0].Text)
	}
	if env.Error == nil {
		t.Fatalf("%s: error envelope missing the top-level \"error\" key: %s", name, tr.Content[0].Text)
	}
	return env.Error
}

func payloadText(tr toolResult) string {
	if len(tr.Content) == 0 {
		return "<no content>"
	}
	return tr.Content[0].Text
}

// TestToolsList asserts tools/list includes ikigenba_notify_reflection alongside
// the health tool, each with the required descriptor keys.
func TestToolsList(t *testing.T) {
	h := newTestHandler()
	resp := rpc(t, h, "tools/list", nil)

	var result struct {
		Tools []struct {
			Name        string         `json:"name"`
			Description string         `json:"description"`
			InputSchema map[string]any `json:"inputSchema"`
		} `json:"tools"`
	}
	if err := json.Unmarshal(resp.Result, &result); err != nil {
		t.Fatalf("decode tools/list: %v", err)
	}

	got := map[string]bool{}
	for _, tool := range result.Tools {
		got[tool.Name] = true
		if tool.Description == "" {
			t.Errorf("tool %q has an empty description", tool.Name)
		}
		if tool.InputSchema == nil || tool.InputSchema["type"] != "object" {
			t.Errorf("tool %q inputSchema is not an object schema: %v", tool.Name, tool.InputSchema)
		}
	}
	for _, name := range []string{"ikigenba_notify_health", "ikigenba_notify_reflection"} {
		if !got[name] {
			t.Errorf("missing expected tool %q: %+v", name, result.Tools)
		}
	}
}

// TestToolsCallReflection covers ikigenba_notify_reflection: the no-arg index
// shows empty publishes (notify produces nothing) and exactly one subscribes
// in-edge (crm/contact.created) with no Handler leaked; an event_type arg against
// the empty registry returns the corrective error with an empty valid list.
func TestToolsCallReflection(t *testing.T) {
	h := newTestHandler()

	idx := callOK(t, h, "ikigenba_notify_reflection", map[string]any{})

	// notify is a consumer: publishes is present and empty.
	publishes, ok := idx["publishes"].([]any)
	if !ok {
		t.Fatalf("reflection index missing publishes array: %+v", idx)
	}
	if len(publishes) != 0 {
		t.Fatalf("expected empty publishes for notify, got %+v", publishes)
	}

	// Exactly one subscribes in-edge: crm/contact.created.
	subscribes, ok := idx["subscribes"].([]any)
	if !ok {
		t.Fatalf("reflection index missing subscribes array: %+v", idx)
	}
	if len(subscribes) != 1 {
		t.Fatalf("expected exactly one subscribes entry, got %+v", subscribes)
	}
	sub := subscribes[0].(map[string]any)
	if sub["source"] != "crm" {
		t.Errorf("subscribes source = %v, want crm", sub["source"])
	}
	if sub["filter"] != "contact.created" {
		t.Errorf("subscribes filter = %v, want contact.created", sub["filter"])
	}
	if d, _ := sub["description"].(string); d == "" {
		t.Errorf("subscribes entry missing description: %+v", sub)
	}
	// The Handler must NOT leak into the rendered output (only the declared edge).
	for _, k := range []string{"handler", "Handler"} {
		if _, present := sub[k]; present {
			t.Errorf("subscribes entry leaked %q key: %+v", k, sub)
		}
	}

	// An event_type arg against the empty registry → corrective error with an
	// empty valid-type list.
	badErr := callErr(t, h, "ikigenba_notify_reflection", map[string]any{"event_type": "contact.created"})
	if badErr["code"] != "unknown_event_type" {
		t.Fatalf("expected unknown_event_type code, got %+v", badErr)
	}
	if msg, _ := badErr["message"].(string); msg == "" {
		t.Fatalf("corrective error missing message: %+v", badErr)
	}
}
