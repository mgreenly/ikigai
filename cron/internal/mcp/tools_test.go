package mcp

import (
	"bytes"
	"context"
	"encoding/json"
	"io"
	"log/slog"
	"net/http"
	"net/http/httptest"
	"path/filepath"
	"strings"
	"testing"

	appkitdb "appkit/db"
	"appkit/server"

	"cron/internal/crontab"
	"cron/internal/db"
	"cron/internal/event"
)

func newHandler(t *testing.T) (http.Handler, *crontab.Store) {
	t.Helper()
	ctx := context.Background()
	conn, err := appkitdb.Open(filepath.Join(t.TempDir(), "test.db"))
	if err != nil {
		t.Fatalf("open: %v", err)
	}
	t.Cleanup(func() { conn.Close() })
	migs, err := appkitdb.LoadMigrations(db.FS, "migrations")
	if err != nil {
		t.Fatalf("load migrations: %v", err)
	}
	if err := appkitdb.Migrate(ctx, conn, migs); err != nil {
		t.Fatalf("migrate: %v", err)
	}
	store := crontab.NewStore(conn)
	var h http.Handler
	_, err = server.New(server.Options{
		Addr:       "127.0.0.1:0",
		Logger:     slog.New(slog.NewTextHandler(io.Discard, nil)),
		ResourceID: "https://example.test/srv/cron",
		AuthServer: "https://auth.example.test",
		Version:    "0.1.0",
		Service:    "cron",
		Publishes:  event.Publishes(store),
		DB:         conn,
		Register: func(rt *server.Router) error {
			var err error
			h, err = NewHandler(store, rt)
			return err
		},
	})
	if err != nil {
		t.Fatalf("build test router: %v", err)
	}
	if h == nil {
		t.Fatalf("NewHandler returned nil handler")
	}
	return h, store
}

func rpc(t *testing.T, h http.Handler, method string, params any) map[string]any {
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
	req.Header.Set("X-Owner-Email", "owner@example.com")
	req.Header.Set("X-Client-Id", "client-123")
	rec := httptest.NewRecorder()
	h.ServeHTTP(rec, req)
	if rec.Code != http.StatusOK {
		t.Fatalf("%s: status %d", method, rec.Code)
	}

	var resp struct {
		Result map[string]any `json:"result"`
		Error  any            `json:"error"`
	}
	if err := json.Unmarshal(rec.Body.Bytes(), &resp); err != nil {
		t.Fatalf("decode rpc response: %v\nbody: %s", err, rec.Body.String())
	}
	if resp.Error != nil {
		t.Fatalf("%s returned JSON-RPC error: %v", method, resp.Error)
	}
	return resp.Result
}

// call issues a tools/call and returns the parsed tool result (the
// result.content[0].text decoded as JSON, plus the raw isError flag).
func call(t *testing.T, h http.Handler, name string, args map[string]any) (map[string]any, bool) {
	t.Helper()
	result := rpc(t, h, "tools/call", map[string]any{"name": name, "arguments": args})
	var resp struct {
		Content []struct {
			Text string `json:"text"`
		} `json:"content"`
		IsError bool `json:"isError"`
	}
	raw, err := json.Marshal(result)
	if err != nil {
		t.Fatalf("marshal tool result: %v", err)
	}
	if err := json.Unmarshal(raw, &resp); err != nil {
		t.Fatalf("decode tool result: %v\nresult: %s", err, raw)
	}
	if len(resp.Content) == 0 {
		t.Fatalf("no content in result: %v", result)
	}
	var payload map[string]any
	if err := json.Unmarshal([]byte(resp.Content[0].Text), &payload); err != nil {
		t.Fatalf("decode tool text: %v\ntext: %s", err, resp.Content[0].Text)
	}
	return payload, resp.IsError
}

func TestToolsList_ComposesCronAndChassisTools(t *testing.T) {
	h, _ := newHandler(t)
	result := rpc(t, h, "tools/list", map[string]any{})
	tools, _ := result["tools"].([]any)
	// R-LS2J-73T5
	if len(tools) != 7 {
		t.Fatalf("tools/list returned %d tools, want exactly 7: %+v", len(tools), tools)
	}
	got := map[string]bool{}
	for _, raw := range tools {
		tool, _ := raw.(map[string]any)
		name, _ := tool["name"].(string)
		if got[name] {
			t.Fatalf("duplicate tool %q in tools/list: %+v", name, tools)
		}
		got[name] = true
		if tool["description"] == "" {
			t.Errorf("tool %q has an empty description", name)
		}
		schema, _ := tool["inputSchema"].(map[string]any)
		if schema == nil || schema["type"] != "object" {
			t.Errorf("tool %q inputSchema is not an object schema: %v", name, tool["inputSchema"])
		}
	}
	for _, want := range []string{"create", "list", "get", "update", "delete", "health", "reflection"} {
		if !got[want] {
			t.Errorf("tools/list missing %q: %+v", want, tools)
		}
	}
	for name := range got {
		switch name {
		case "create", "list", "get", "update", "delete", "health", "reflection":
		default:
			t.Errorf("unexpected tool %q in tools/list: %+v", name, tools)
		}
	}
}

// TestCreate_RejectsBadExpr: the MCP boundary parses the expr and fails loudly,
// naming the bad field, before touching the store.
func TestCreate_RejectsBadExpr(t *testing.T) {
	h, store := newHandler(t)

	payload, isErr := call(t, h, "create", map[string]any{
		"name": "broken", "expr": "0 99 * * *", // hour 99 out of range
	})
	if !isErr {
		t.Fatalf("bad expr should be a tool error, got success: %v", payload)
	}
	errObj, _ := payload["error"].(map[string]any)
	if errObj == nil {
		t.Fatalf("expected error envelope, got %v", payload)
	}
	if errObj["code"] != "validation" || errObj["field"] != "expr" {
		t.Fatalf("wrong error code/field: %v", errObj)
	}
	if msg, _ := errObj["message"].(string); !strings.Contains(msg, "hour") {
		t.Fatalf("error message should name the bad field 'hour': %q", msg)
	}
	// Nothing must have been persisted.
	if _, err := store.Get(context.Background(), "broken"); err == nil {
		t.Fatalf("bad-expr row must not be persisted")
	}
}

// TestCreate_WrongFieldCount also fails at the boundary.
func TestCreate_WrongFieldCount(t *testing.T) {
	h, _ := newHandler(t)
	payload, isErr := call(t, h, "create", map[string]any{
		"name": "short", "expr": "* * *",
	})
	if !isErr {
		t.Fatalf("expected validation error, got %v", payload)
	}
}

// TestCreateThenListGet: a valid expr round-trips through the store and the live
// family appears in reflection.
func TestCreateThenListGet(t *testing.T) {
	h, _ := newHandler(t)
	if _, isErr := call(t, h, "create", map[string]any{
		"name": "nightly", "expr": "0 3 * * *",
	}); isErr {
		t.Fatalf("valid create should succeed")
	}

	list, isErr := call(t, h, "list", map[string]any{})
	if isErr {
		t.Fatalf("list errored")
	}
	items, _ := list["items"].([]any)
	if len(items) != 1 {
		t.Fatalf("want 1 item, got %v", list)
	}
	get, isErr := call(t, h, "get", map[string]any{"name": "nightly"})
	if isErr {
		t.Fatalf("get errored")
	}
	if get["name"] != "nightly" || get["expr"] != "0 3 * * *" {
		t.Fatalf("get returned wrong entry: %v", get)
	}

	refl, isErr := call(t, h, "reflection", map[string]any{})
	if isErr {
		t.Fatalf("reflection errored")
	}
	pubs, _ := refl["publishes"].([]any)
	if len(pubs) != 1 {
		t.Fatalf("want 1 published family, got %v", refl["publishes"])
	}
	first, _ := pubs[0].(map[string]any)
	if first["kind"] != event.Kind || first["subject"] != "/<schedule name>" || !strings.Contains(first["description"].(string), "nightly") {
		t.Fatalf("wrong published family: %v", first)
	}
}

// R-PRP2-GJP7
func TestReflectionPublishesOneLiveTickFamily(t *testing.T) {
	h, _ := newHandler(t)
	for _, name := range []string{"nightly", "bill-sweep"} {
		if _, isErr := call(t, h, "create", map[string]any{"name": name, "expr": "0 3 * * *"}); isErr {
			t.Fatalf("create %q failed", name)
		}
	}
	index, isErr := call(t, h, "reflection", map[string]any{})
	if isErr {
		t.Fatal("reflection index errored")
	}
	families, _ := index["publishes"].([]any)
	if len(families) != 1 {
		t.Fatalf("families = %v, want exactly one", index["publishes"])
	}
	family, _ := families[0].(map[string]any)
	description, _ := family["description"].(string)
	if family["kind"] != event.Kind || family["subject"] != "/<schedule name>" || !strings.Contains(description, "bill-sweep") || !strings.Contains(description, "nightly") {
		t.Fatalf("unexpected reflected family: %v", family)
	}
	detail, isErr := call(t, h, "reflection", map[string]any{"kind": event.Kind})
	if isErr {
		t.Fatal("reflection detail errored")
	}
	schema, _ := detail["schema"].(map[string]any)
	properties, _ := schema["properties"].(map[string]any)
	example, _ := detail["example"].(map[string]any)
	for _, name := range []string{"name", "scheduled_for", "fired_at"} {
		if properties[name] == nil || example[name] == nil {
			t.Fatalf("schema/example disagree or omit %q: schema=%v example=%v", name, schema, example)
		}
	}
	if _, isErr := call(t, h, "delete", map[string]any{"name": "nightly"}); isErr {
		t.Fatal("delete nightly failed")
	}
	after, isErr := call(t, h, "reflection", map[string]any{})
	if isErr {
		t.Fatal("reflection after delete errored")
	}
	families, _ = after["publishes"].([]any)
	if len(families) != 1 {
		t.Fatalf("families after delete = %v, want exactly one", after["publishes"])
	}
	description, _ = families[0].(map[string]any)["description"].(string)
	if strings.Contains(description, "nightly") || !strings.Contains(description, "bill-sweep") {
		t.Fatalf("live description after delete = %q", description)
	}
}
