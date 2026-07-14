package mcp_test

import (
	"bytes"
	"context"
	"encoding/json"
	"errors"
	"net/http"
	"net/http/httptest"
	"reflect"
	"strings"
	"testing"

	"appkit"
	"appkit/mcp"
	"appkit/server"

	"eventplane/consumer"
	"eventplane/outbox"
)

func newHandler(t *testing.T, opts mcp.Options) *mcp.Handler {
	t.Helper()
	h, err := mcp.New(opts)
	if err != nil {
		t.Fatalf("New: %v", err)
	}
	return h
}

func rpc(t *testing.T, h http.Handler, body string, headers map[string]string) map[string]any {
	t.Helper()
	req := httptest.NewRequest(http.MethodPost, "/mcp", bytes.NewBufferString(body))
	for key, value := range headers {
		req.Header.Set(key, value)
	}
	rr := httptest.NewRecorder()
	h.ServeHTTP(rr, req)
	if rr.Code != http.StatusOK {
		t.Fatalf("status = %d, want 200; body: %s", rr.Code, rr.Body.String())
	}
	var resp map[string]any
	if err := json.Unmarshal(rr.Body.Bytes(), &resp); err != nil {
		t.Fatalf("decode response: %v; body: %s", err, rr.Body.String())
	}
	return resp
}

func resultObject(t *testing.T, resp map[string]any) map[string]any {
	t.Helper()
	result, ok := resp["result"].(map[string]any)
	if !ok {
		t.Fatalf("result missing or not object: %#v", resp["result"])
	}
	return result
}

func errorObject(t *testing.T, resp map[string]any) map[string]any {
	t.Helper()
	errObj, ok := resp["error"].(map[string]any)
	if !ok {
		t.Fatalf("error missing or not object: %#v", resp)
	}
	if _, hasResult := resp["result"]; hasResult {
		t.Fatalf("response has result despite error: %#v", resp)
	}
	return errObj
}

func resultText(t *testing.T, resp map[string]any) string {
	t.Helper()
	result := resultObject(t, resp)
	content, ok := result["content"].([]any)
	if !ok || len(content) != 1 {
		t.Fatalf("content = %#v, want one text item", result["content"])
	}
	item, ok := content[0].(map[string]any)
	if !ok {
		t.Fatalf("content[0] not object: %#v", content[0])
	}
	if item["type"] != "text" {
		t.Fatalf("content[0].type = %v, want text", item["type"])
	}
	text, ok := item["text"].(string)
	if !ok {
		t.Fatalf("content[0].text missing or not string: %#v", item)
	}
	return text
}

func resultTextJSON(t *testing.T, resp map[string]any) map[string]any {
	t.Helper()
	var v map[string]any
	if err := json.Unmarshal([]byte(resultText(t, resp)), &v); err != nil {
		t.Fatalf("decode text JSON: %v", err)
	}
	return v
}

func normalizeJSON(t *testing.T, v any) any {
	t.Helper()
	b, err := json.Marshal(v)
	if err != nil {
		t.Fatalf("marshal: %v", err)
	}
	var normalized any
	if err := json.Unmarshal(b, &normalized); err != nil {
		t.Fatalf("unmarshal: %v", err)
	}
	return normalized
}

func toolsByName(t *testing.T, resp map[string]any) map[string]map[string]any {
	t.Helper()
	result := resultObject(t, resp)
	tools, ok := result["tools"].([]any)
	if !ok {
		t.Fatalf("tools missing or not array: %#v", result["tools"])
	}
	byName := map[string]map[string]any{}
	for _, item := range tools {
		tool, ok := item.(map[string]any)
		if !ok {
			t.Fatalf("tool item not object: %#v", item)
		}
		name, _ := tool["name"].(string)
		byName[name] = tool
	}
	return byName
}

type staticEventSample struct {
	StaticID string `json:"static_id"`
}

type liveEventSample struct {
	LiveID string `json:"live_id"`
}

type customerEventSample struct {
	CustomerID string `json:"customer_id"`
	Email      string `json:"email,omitempty"`
}

func TestInitializeReturnsOptions(t *testing.T) {
	h := newHandler(t, mcp.Options{
		Service:      "ledger",
		Version:      "v1.2.3",
		Instructions: "Use ledger tools for accounting records.",
	})

	resp := rpc(t, h, `{"jsonrpc":"2.0","id":1,"method":"initialize"}`, nil)
	result := resultObject(t, resp)
	serverInfo, ok := result["serverInfo"].(map[string]any)
	if !ok {
		t.Fatalf("serverInfo missing or not object: %#v", result["serverInfo"])
	}

	// R-WPNN-6Q9E
	if result["protocolVersion"] != "2025-06-18" {
		t.Errorf("protocolVersion = %v, want 2025-06-18", result["protocolVersion"])
	}

	// R-MCJJ-NXJR
	if serverInfo["name"] != "ledger" {
		t.Errorf("serverInfo.name = %v, want ledger", serverInfo["name"])
	}
	if serverInfo["version"] != "v1.2.3" {
		t.Errorf("serverInfo.version = %v, want v1.2.3", serverInfo["version"])
	}
	if result["instructions"] != "Use ledger tools for accounting records." {
		t.Errorf("instructions = %v, want Options.Instructions", result["instructions"])
	}
}

func TestToolsListIncludesDeclaredTools(t *testing.T) {
	schema := map[string]any{
		"type": "object",
		"properties": map[string]any{
			"query": map[string]any{"type": "string"},
		},
		"required": []string{"query"},
	}
	outputSchema := map[string]any{
		"type": "object",
		"properties": map[string]any{
			"records": map[string]any{"type": "array"},
		},
	}
	h := newHandler(t, mcp.Options{
		Tools: []mcp.Tool{
			{
				Name:         "search",
				Description:  "Search records.",
				InputSchema:  schema,
				OutputSchema: outputSchema,
				Handler: func(ctx context.Context, args json.RawMessage, id server.Identity) (map[string]any, error) {
					return mcp.TextResult("ok"), nil
				},
			},
			{
				Name:        "save",
				Description: "Save records.",
				InputSchema: map[string]any{"type": "object", "properties": map[string]any{}},
				Handler: func(ctx context.Context, args json.RawMessage, id server.Identity) (map[string]any, error) {
					return mcp.TextResult("ok"), nil
				},
			},
		},
	})

	resp := rpc(t, h, `{"jsonrpc":"2.0","id":"list","method":"tools/list"}`, nil)
	byName := toolsByName(t, resp)

	// R-MDRG-1PAG
	gotSearch, ok := byName["search"]
	if !ok {
		t.Fatalf("search descriptor missing from %#v", byName)
	}
	if gotSearch["description"] != "Search records." {
		t.Errorf("search description = %v, want exact declared description", gotSearch["description"])
	}
	if !reflect.DeepEqual(gotSearch["inputSchema"], normalizeJSON(t, schema)) {
		t.Errorf("search schema = %#v, want %#v", gotSearch["inputSchema"], normalizeJSON(t, schema))
	}
	// R-WQVJ-KI03
	if !reflect.DeepEqual(gotSearch["outputSchema"], normalizeJSON(t, outputSchema)) {
		t.Errorf("search output schema = %#v, want %#v", gotSearch["outputSchema"], normalizeJSON(t, outputSchema))
	}
	gotSave, ok := byName["save"]
	if !ok {
		t.Fatalf("save descriptor missing from %#v", byName)
	}
	if gotSave["description"] != "Save records." {
		t.Errorf("save description = %v, want exact declared description", gotSave["description"])
	}
	if _, exists := gotSave["outputSchema"]; exists {
		t.Errorf("save descriptor has outputSchema for nil declaration: %#v", gotSave)
	}
}

func TestStructuredResultRenderingsMatchOnWire(t *testing.T) {
	want := map[string]any{
		"items": []any{"alpha", float64(2)},
		"meta":  map[string]any{"ready": true},
	}
	h := newHandler(t, mcp.Options{Tools: []mcp.Tool{{
		Name: "structured",
		Handler: func(ctx context.Context, args json.RawMessage, id server.Identity) (map[string]any, error) {
			return mcp.StructuredResult(want)
		},
	}}})

	resp := rpc(t, h, `{"jsonrpc":"2.0","id":"structured","method":"tools/call","params":{"name":"structured","arguments":{}}}`, nil)
	result := resultObject(t, resp)
	var textValue any
	if err := json.Unmarshal([]byte(resultText(t, resp)), &textValue); err != nil {
		t.Fatalf("decode text block: %v", err)
	}

	// R-WTBC-C1HH
	if !reflect.DeepEqual(textValue, result["structuredContent"]) {
		t.Fatalf("text rendering = %#v, structuredContent = %#v", textValue, result["structuredContent"])
	}
	if !reflect.DeepEqual(result["structuredContent"], normalizeJSON(t, want)) {
		t.Fatalf("structuredContent = %#v, want %#v", result["structuredContent"], normalizeJSON(t, want))
	}
}

func TestErrorResultCarriesTypedCodeAndMessageOnWire(t *testing.T) {
	h := newHandler(t, mcp.Options{Tools: []mcp.Tool{{
		Name: "lookup",
		Handler: func(ctx context.Context, args json.RawMessage, id server.Identity) (map[string]any, error) {
			return mcp.ErrorResult(mcp.ErrNotFound, "record missing"), nil
		},
	}}})

	resp := rpc(t, h, `{"jsonrpc":"2.0","id":"lookup","method":"tools/call","params":{"name":"lookup","arguments":{}}}`, nil)
	result := resultObject(t, resp)

	// R-WUJ8-PT86
	if result["isError"] != true {
		t.Fatalf("isError = %v, want true", result["isError"])
	}
	wantStructured := map[string]any{"code": "not_found", "message": "record missing"}
	if !reflect.DeepEqual(result["structuredContent"], wantStructured) {
		t.Fatalf("structuredContent = %#v, want %#v", result["structuredContent"], wantStructured)
	}
	if got := resultText(t, resp); got != "record missing" {
		t.Fatalf("text = %q, want record missing", got)
	}
}

func TestStandardToolsListAndHealthEnvelope(t *testing.T) {
	h := newHandler(t, mcp.Options{
		Service: "crm",
		Version: "v2.4.6",
		Health: func(ctx context.Context) (map[string]any, error) {
			return map[string]any{"queue": 3}, nil
		},
	})

	listResp := rpc(t, h, `{"jsonrpc":"2.0","id":"standard-list","method":"tools/list"}`, nil)
	byName := toolsByName(t, listResp)

	// R-ML2U-CBQM
	if _, ok := byName["health"]; !ok {
		t.Fatalf("health descriptor missing from zero-declared tool list: %#v", byName)
	}
	if _, ok := byName["reflection"]; !ok {
		t.Fatalf("reflection descriptor missing from zero-declared tool list: %#v", byName)
	}
	reflection := byName["reflection"]
	schema, ok := reflection["inputSchema"].(map[string]any)
	if !ok {
		t.Fatalf("reflection inputSchema missing or not object: %#v", reflection["inputSchema"])
	}
	properties, ok := schema["properties"].(map[string]any)
	if !ok {
		t.Fatalf("reflection inputSchema.properties missing or not object: %#v", schema["properties"])
	}
	// R-7I7V-DBB3
	if _, ok := properties["kind"]; !ok {
		t.Fatalf("reflection inputSchema.properties missing kind: %#v", properties)
	}
	staleParam := "event" + "_type"
	if _, ok := properties[staleParam]; ok {
		t.Fatalf("reflection inputSchema.properties still advertises stale detail parameter: %#v", properties)
	}

	callResp := rpc(t, h, `{"jsonrpc":"2.0","id":"health","method":"tools/call","params":{"name":"health"}}`, nil)
	got := resultTextJSON(t, callResp)
	wantEnvelope := appkit.Envelope("v2.4.6", "crm", map[string]any{"queue": 3})
	for _, key := range []string{"status", "service", "version"} {
		if got[key] != wantEnvelope[key] {
			t.Fatalf("health %s = %v, want %v", key, got[key], wantEnvelope[key])
		}
	}
	details, ok := got["details"].(map[string]any)
	if !ok {
		t.Fatalf("health details missing or not object: %#v", got["details"])
	}
	if details["queue"] != float64(3) {
		t.Fatalf("health details.queue = %v, want 3", details["queue"])
	}
}

func TestReflectionReturnsEventIndexAndLivePublishes(t *testing.T) {
	staticEvents := outbox.Registry{{
		Kind:        "delete",
		Subject:     "/<mirror path>",
		Description: "Static-only event.",
		Sample:      staticEventSample{StaticID: "s1"},
	}}
	liveEvents := outbox.Registry{
		{
			Kind:        "create",
			Subject:     "/<mirror path>",
			Description: "Created mirror path.",
			Sample:      liveEventSample{LiveID: "l1"},
		},
		{
			Kind:        "delete",
			Subject:     "/<mirror path>",
			Description: "Deleted mirror path.",
			Sample:      liveEventSample{LiveID: "l2"},
		},
	}
	eventsOnly := outbox.Registry{{
		Kind:        "events.only",
		Subject:     "/<mirror path>",
		Description: "Live-only event.",
		Sample:      liveEventSample{LiveID: "l1"},
	}}
	subscriptions := []consumer.Subscription{{
		Source:      "crm",
		Filter:      "crm:create/<mirror path>",
		Description: "Customer changes.",
	}}
	h := newHandler(t, mcp.Options{
		Events: append(staticEvents, eventsOnly...),
		Publishes: func() outbox.Registry {
			return liveEvents
		},
		Subscriptions: func() []consumer.Subscription {
			return subscriptions
		},
	})

	resp := rpc(t, h, `{"jsonrpc":"2.0","id":"reflection-index","method":"tools/call","params":{"name":"reflection"}}`, nil)
	got := resultTextJSON(t, resp)

	// R-7EK6-8030
	if !reflect.DeepEqual(got["publishes"], normalizeJSON(t, liveEvents.Index())) {
		t.Fatalf("publishes = %#v, want live provider index %#v", got["publishes"], normalizeJSON(t, liveEvents.Index()))
	}
	publishesJSON, err := json.Marshal(got["publishes"])
	if err != nil {
		t.Fatalf("marshal publishes: %v", err)
	}
	if !strings.Contains(string(publishesJSON), "create") {
		t.Fatalf("publishes missing live-provider kind: %s", publishesJSON)
	}
	if strings.Contains(string(publishesJSON), "events.only") {
		t.Fatalf("publishes included Events-only kind despite live provider: %s", publishesJSON)
	}
	wantSubscribes := []map[string]any{{
		"source":      "crm",
		"filter":      "crm:create/<mirror path>",
		"description": "Customer changes.",
	}}
	if !reflect.DeepEqual(got["subscribes"], normalizeJSON(t, wantSubscribes)) {
		t.Fatalf("subscribes = %#v, want %#v", got["subscribes"], normalizeJSON(t, wantSubscribes))
	}
}

func TestReflectionReturnsEventDetailSchema(t *testing.T) {
	events := outbox.Registry{{
		Kind:        "customer.created",
		Subject:     "/<customer id>",
		Description: "A customer was created.",
		Sample:      customerEventSample{CustomerID: "cus_123", Email: "owner@example.com"},
	}}
	h := newHandler(t, mcp.Options{Events: events})

	resp := rpc(t, h, `{"jsonrpc":"2.0","id":"reflection-detail","method":"tools/call","params":{"name":"reflection","arguments":{"kind":"customer.created"}}}`, nil)
	got := resultTextJSON(t, resp)
	want, err := events.Detail("customer.created")
	if err != nil {
		t.Fatalf("Detail: %v", err)
	}

	// R-7FS2-LRTP
	if !reflect.DeepEqual(got, normalizeJSON(t, want)) {
		t.Fatalf("detail = %#v, want %#v", got, normalizeJSON(t, want))
	}
	detailJSON, err := json.Marshal(got)
	if err != nil {
		t.Fatalf("marshal detail: %v", err)
	}
	if !strings.Contains(string(detailJSON), "customer_id") {
		t.Fatalf("detail payload fields missing from %s", detailJSON)
	}
}

func TestReflectionUnknownKindReturnsToolError(t *testing.T) {
	events := outbox.Registry{
		{
			Kind:        "customer.created",
			Subject:     "/<customer id>",
			Description: "A customer was created.",
			Sample:      customerEventSample{CustomerID: "cus_123"},
		},
		{
			Kind:        "customer.deleted",
			Subject:     "/<customer id>",
			Description: "A customer was deleted.",
			Sample:      customerEventSample{CustomerID: "cus_123"},
		},
	}
	h := newHandler(t, mcp.Options{Events: events})

	resp := rpc(t, h, `{"jsonrpc":"2.0","id":"reflection-unknown","method":"tools/call","params":{"name":"reflection","arguments":{"kind":"customer.missing"}}}`, nil)
	if errObj, ok := resp["error"]; ok {
		t.Fatalf("unknown kind returned JSON-RPC error %#v, want tool result", errObj)
	}
	result := resultObject(t, resp)

	// R-7GZY-ZJKE
	if result["isError"] != true {
		t.Fatalf("isError = %v, want true in tool result %#v", result["isError"], result)
	}
	msg := resultText(t, resp)
	if !strings.Contains(msg, "customer.created") || !strings.Contains(msg, "customer.deleted") {
		t.Fatalf("tool error message %q does not name known event kinds", msg)
	}
}

func TestToolsCallDispatchesRawArgumentsAndResult(t *testing.T) {
	var gotArgs json.RawMessage
	wantResult := map[string]any{
		"content": []map[string]any{{"type": "text", "text": "created"}},
		"meta":    map[string]any{"id": "abc123"},
	}
	h := newHandler(t, mcp.Options{
		Tools: []mcp.Tool{{
			Name:        "create",
			Description: "Create a record.",
			InputSchema: map[string]any{"type": "object"},
			Handler: func(ctx context.Context, args json.RawMessage, id server.Identity) (map[string]any, error) {
				gotArgs = append(json.RawMessage(nil), args...)
				return wantResult, nil
			},
		}},
	})

	resp := rpc(t, h, `{"jsonrpc":"2.0","id":7,"method":"tools/call","params":{"name":"create","arguments":{"alpha":1,"nested":true}}}`, nil)

	// R-MEZC-FH15
	if string(gotArgs) != `{"alpha":1,"nested":true}` {
		t.Fatalf("handler args = %s, want raw arguments bytes", gotArgs)
	}
	if !reflect.DeepEqual(resp["result"], normalizeJSON(t, wantResult)) {
		t.Fatalf("result = %#v, want handler map %#v", resp["result"], normalizeJSON(t, wantResult))
	}
}

func TestToolsCallPassesRequestIdentityHeaders(t *testing.T) {
	var gotID server.Identity
	h := newHandler(t, mcp.Options{
		Tools: []mcp.Tool{{
			Name:        "whoami",
			Description: "Return caller identity.",
			InputSchema: map[string]any{"type": "object"},
			Handler: func(ctx context.Context, args json.RawMessage, id server.Identity) (map[string]any, error) {
				gotID = id
				return mcp.TextResult("ok"), nil
			},
		}},
	})

	rpc(t, h, `{"jsonrpc":"2.0","id":8,"method":"tools/call","params":{"name":"whoami","arguments":{}}}`, map[string]string{
		"X-Owner-Email": "owner@example.com",
		"X-Client-Id":   "client-123",
	})

	// R-MG78-T8RU
	if gotID.OwnerEmail != "owner@example.com" {
		t.Errorf("OwnerEmail = %q, want request X-Owner-Email", gotID.OwnerEmail)
	}
	if gotID.ClientID != "client-123" {
		t.Errorf("ClientID = %q, want request X-Client-Id", gotID.ClientID)
	}
}

func TestErrorsForUnknownMethodAndUndeclaredTool(t *testing.T) {
	h := newHandler(t, mcp.Options{Tools: []mcp.Tool{{
		Name: "broken",
		Handler: func(ctx context.Context, args json.RawMessage, id server.Identity) (map[string]any, error) {
			return nil, errors.New("database disconnected")
		},
	}}})

	unknownMethod := rpc(t, h, `{"jsonrpc":"2.0","id":"bad-method","method":"missing"}`, nil)
	methodErr := errorObject(t, unknownMethod)

	// R-MHF5-70IJ
	if methodErr["code"] != float64(-32601) {
		t.Fatalf("unknown method code = %v, want -32601", methodErr["code"])
	}
	unknownTool := rpc(t, h, `{"jsonrpc":"2.0","id":"bad-tool","method":"tools/call","params":{"name":"absent","arguments":{}}}`, nil)
	toolErr := errorObject(t, unknownTool)
	declaredFault := rpc(t, h, `{"jsonrpc":"2.0","id":"broken-tool","method":"tools/call","params":{"name":"broken","arguments":{}}}`, nil)
	faultErr := errorObject(t, declaredFault)

	// R-WVR5-3KYV
	if toolErr["code"] != float64(-32602) {
		t.Fatalf("undeclared tool code = %v, want -32602", toolErr["code"])
	}
	if toolErr["message"] == "" {
		t.Fatalf("undeclared tool error missing message: %#v", toolErr)
	}
	if faultErr["code"] != float64(-32603) {
		t.Fatalf("declared handler fault code = %v, want -32603", faultErr["code"])
	}
	if faultErr["message"] != "database disconnected" {
		t.Fatalf("declared handler fault message = %v, want database disconnected", faultErr["message"])
	}
}

func TestMalformedBodyReturnsParseError(t *testing.T) {
	h := newHandler(t, mcp.Options{})

	resp := rpc(t, h, `not json`, nil)
	errObj := errorObject(t, resp)

	// R-MIN1-KS98
	if errObj["code"] != float64(-32700) {
		t.Fatalf("malformed body code = %v, want -32700", errObj["code"])
	}
}

func TestNewRejectsDuplicateAndReservedToolNames(t *testing.T) {
	handler := func(ctx context.Context, args json.RawMessage, id server.Identity) (map[string]any, error) {
		return nil, errors.New("unused")
	}

	// R-MJUX-YJZX
	if _, err := mcp.New(mcp.Options{Tools: []mcp.Tool{
		{Name: "dupe", Handler: handler},
		{Name: "dupe", Handler: handler},
	}}); err == nil {
		t.Fatal("New duplicate tool names error = nil, want non-nil")
	}
	if _, err := mcp.New(mcp.Options{Tools: []mcp.Tool{{Name: "health", Handler: handler}}}); err == nil {
		t.Fatal("New health reserved name error = nil, want non-nil")
	}
	if _, err := mcp.New(mcp.Options{Tools: []mcp.Tool{{Name: "reflection", Handler: handler}}}); err == nil {
		t.Fatal("New reflection reserved name error = nil, want non-nil")
	}
}
