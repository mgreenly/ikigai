package mcp

import (
	"bytes"
	"encoding/json"
	"io"
	"log/slog"
	"net/http"
	"net/http/httptest"
	"testing"

	"appkit/server"

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

// discardLogger is a slog.Logger that drops every line (push must never log a
// secret; tests don't need the output either).
func discardLogger() *slog.Logger {
	return slog.New(slog.NewJSONHandler(io.Discard, nil))
}

// newTestHandler builds a notify Handler with the consumer-only wiring plus a
// push client whose base URL is unreachable (the read-only tests never call
// send). It has an EMPTY published-event registry (notify produces nothing) and
// the live subscription provider returning notify's one declared in-edge — the
// same push.Subscription() the consumer Handler matches against.
func newTestHandler(t testing.TB) http.Handler {
	t.Helper()
	return newHandlerWithClient(t, push.NewClient("http://127.0.0.1:1", "topic", "tok", discardLogger()))
}

// newHandlerWithClient builds a notify Handler around a specific push client so a
// send test can point it at a mock ntfy server.
func newHandlerWithClient(t testing.TB, c *push.Client) http.Handler {
	t.Helper()
	var handler http.Handler
	_, err := server.New(server.Options{
		Addr:       "127.0.0.1:0",
		Logger:     discardLogger(),
		ResourceID: "https://int.ikigenba.com/srv/notify/mcp",
		AuthServer: "https://int.ikigenba.com",
		Version:    testVersion,
		Service:    testService,
		Events:     outbox.Registry{},
		Subscriptions: func() []consumer.Subscription {
			return []consumer.Subscription{push.Subscription()}
		},
		Register: func(rt *server.Router) error {
			var err error
			handler, err = NewHandler(c, rt)
			return err
		},
	})
	if err != nil {
		t.Fatalf("build test server: %v", err)
	}
	if handler == nil {
		t.Fatal("NewHandler returned nil handler")
	}
	return handler
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
	IsError           bool           `json:"isError"`
	StructuredContent map[string]any `json:"structuredContent"`
	Content           []struct {
		Type string `json:"type"`
		Text string `json:"text"`
	} `json:"content"`
}

// rpc drives a single JSON-RPC request through the real ServeHTTP seam with the
// nginx-injected identity headers set, and returns the decoded response.
func rpc(t *testing.T, h http.Handler, method string, params any) jsonRPCResponse {
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
func call(t *testing.T, h http.Handler, name string, args any) toolResult {
	t.Helper()
	resp := rpc(t, h, "tools/call", map[string]any{"name": name, "arguments": args})
	var tr toolResult
	if err := json.Unmarshal(resp.Result, &tr); err != nil {
		t.Fatalf("decode tool result for %s: %v (result=%s)", name, err, resp.Result)
	}
	return tr
}

// callOK asserts that the success result has matching structured and text JSON.
func callOK(t *testing.T, h http.Handler, name string, args any) map[string]any {
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
	if tr.StructuredContent == nil {
		t.Fatalf("%s: result is missing structuredContent", name)
	}
	textJSON, err := json.Marshal(m)
	if err != nil {
		t.Fatalf("%s: marshal text object: %v", name, err)
	}
	structuredJSON, err := json.Marshal(tr.StructuredContent)
	if err != nil {
		t.Fatalf("%s: marshal structuredContent: %v", name, err)
	}
	if !bytes.Equal(textJSON, structuredJSON) {
		t.Fatalf("%s: text JSON %s does not mirror structuredContent %s", name, textJSON, structuredJSON)
	}
	return tr.StructuredContent
}

// callErr asserts an error result and returns its structured error object.
func callErr(t *testing.T, h http.Handler, name string, args any) map[string]any {
	t.Helper()
	tr := call(t, h, name, args)
	if !tr.IsError {
		t.Fatalf("%s: expected an error envelope, got success: %s", name, payloadText(tr))
	}
	if len(tr.Content) != 1 || tr.Content[0].Type != "text" || tr.Content[0].Text == "" {
		t.Fatalf("%s: expected one non-empty human text block, got %+v", name, tr.Content)
	}
	if tr.StructuredContent == nil {
		t.Fatalf("%s: error result missing structuredContent", name)
	}
	return tr.StructuredContent
}

func payloadText(tr toolResult) string {
	if len(tr.Content) == 0 {
		return "<no content>"
	}
	return tr.Content[0].Text
}

func TestToolsListComposesNotifyToolWithChassisTools(t *testing.T) {
	// R-4IBU-MT7Z
	h := newTestHandler(t)
	resp := rpc(t, h, "tools/list", nil)

	var result struct {
		Tools []struct {
			Name         string         `json:"name"`
			Description  string         `json:"description"`
			InputSchema  map[string]any `json:"inputSchema"`
			OutputSchema map[string]any `json:"outputSchema"`
		} `json:"tools"`
	}
	if err := json.Unmarshal(resp.Result, &result); err != nil {
		t.Fatalf("decode tools/list: %v", err)
	}
	if len(result.Tools) != 3 {
		t.Fatalf("tools/list returned %d tools, want exactly 3: %+v", len(result.Tools), result.Tools)
	}

	got := map[string]bool{}
	for _, tool := range result.Tools {
		if got[tool.Name] {
			t.Fatalf("duplicate tool %q in tools/list: %+v", tool.Name, result.Tools)
		}
		got[tool.Name] = true
		if tool.Description == "" {
			t.Errorf("tool %q has an empty description", tool.Name)
		}
		if tool.InputSchema == nil || tool.InputSchema["type"] != "object" {
			t.Errorf("tool %q inputSchema is not an object schema: %v", tool.Name, tool.InputSchema)
		}
	}
	for _, name := range []string{"send", "health", "reflection"} {
		if !got[name] {
			t.Errorf("missing expected tool %q: %+v", name, result.Tools)
		}
	}
	for name := range got {
		switch name {
		case "send", "health", "reflection":
		default:
			t.Errorf("unexpected tool %q in tools/list: %+v", name, result.Tools)
		}
	}

	// R-AA95-CPX6
	for _, declared := range result.Tools {
		if declared.Name != "send" {
			continue
		}
		if declared.OutputSchema["type"] != "object" {
			t.Fatalf("send outputSchema is not an object schema: %+v", declared.OutputSchema)
		}
		props, ok := declared.OutputSchema["properties"].(map[string]any)
		if !ok {
			t.Fatalf("send outputSchema has no properties object: %+v", declared.OutputSchema)
		}
		okSchema, ok := props["ok"].(map[string]any)
		if !ok || okSchema["type"] != "boolean" {
			t.Fatalf("send outputSchema ok property is not boolean: %+v", declared.OutputSchema)
		}
		required, ok := declared.OutputSchema["required"].([]any)
		if !ok || len(required) != 1 || required[0] != "ok" {
			t.Fatalf("send outputSchema required = %#v, want [ok]", declared.OutputSchema["required"])
		}
	}
}

// TestToolsCallReflection covers the chassis reflection index: notify produces
// nothing and subscribes to exactly one crm/contact.created in-edge with no
// Handler leaked.
func TestToolsCallReflection(t *testing.T) {
	h := newTestHandler(t)

	idx := callOK(t, h, "reflection", map[string]any{})

	// notify is a consumer: publishes is present and empty.
	publishes, ok := idx["publishes"].([]any)
	if !ok {
		t.Fatalf("reflection index missing publishes array: %+v", idx)
	}
	if len(publishes) != 0 {
		t.Fatalf("expected empty publishes for notify, got %+v", publishes)
	}

	// Exactly one subscribes in-edge: crm's canonical contact.created key.
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
	if sub["filter"] != "crm:contact.created/**" {
		t.Errorf("subscribes filter = %v, want crm:contact.created/**", sub["filter"])
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

}
