package mcp

import (
	"context"
	"encoding/json"
	"net/http"
	"net/http/httptest"
	"path/filepath"
	"strings"
	"testing"

	"cron/internal/crontab"
	"cron/internal/db"
	"cron/internal/event"
)

func newHandler(t *testing.T) (*Handler, *crontab.Store) {
	t.Helper()
	ctx := context.Background()
	conn, err := db.Open(filepath.Join(t.TempDir(), "test.db"))
	if err != nil {
		t.Fatalf("open: %v", err)
	}
	t.Cleanup(func() { conn.Close() })
	if err := db.Migrate(ctx, conn); err != nil {
		t.Fatalf("migrate: %v", err)
	}
	store := crontab.NewStore(conn)
	h := NewHandler(store, "0.1.0", "cron", nil, event.Publishes(store), nil)
	return h, store
}

// call issues a tools/call and returns the parsed tool result (the
// result.content[0].text decoded as JSON, plus the raw isError flag).
func call(t *testing.T, h *Handler, name string, args map[string]any) (map[string]any, bool) {
	t.Helper()
	argsRaw, _ := json.Marshal(args)
	body, _ := json.Marshal(map[string]any{
		"jsonrpc": "2.0", "id": 1, "method": "tools/call",
		"params": map[string]any{"name": name, "arguments": json.RawMessage(argsRaw)},
	})
	req := httptest.NewRequest(http.MethodPost, "/mcp", strings.NewReader(string(body)))
	req.Header.Set("X-Owner-Email", "owner@example.com")
	rec := httptest.NewRecorder()
	h.ServeHTTP(rec, req)

	var resp struct {
		Result struct {
			Content []struct {
				Text string `json:"text"`
			} `json:"content"`
			IsError bool `json:"isError"`
		} `json:"result"`
	}
	if err := json.Unmarshal(rec.Body.Bytes(), &resp); err != nil {
		t.Fatalf("decode rpc response: %v\nbody: %s", err, rec.Body.String())
	}
	if len(resp.Result.Content) == 0 {
		t.Fatalf("no content in result: %s", rec.Body.String())
	}
	var payload map[string]any
	if err := json.Unmarshal([]byte(resp.Result.Content[0].Text), &payload); err != nil {
		t.Fatalf("decode tool text: %v\ntext: %s", err, resp.Result.Content[0].Text)
	}
	return payload, resp.Result.IsError
}

// TestCreate_RejectsBadExpr: the MCP boundary parses the expr and fails loudly,
// naming the bad field, before touching the store.
func TestCreate_RejectsBadExpr(t *testing.T) {
	h, store := newHandler(t)

	payload, isErr := call(t, h, "ikigenba_cron_create", map[string]any{
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
	payload, isErr := call(t, h, "ikigenba_cron_create", map[string]any{
		"name": "short", "expr": "* * *",
	})
	if !isErr {
		t.Fatalf("expected validation error, got %v", payload)
	}
}

// TestCreateThenListGet: a valid expr round-trips through the store and the live
// type appears in reflection.
func TestCreateThenListGet(t *testing.T) {
	h, _ := newHandler(t)
	if _, isErr := call(t, h, "ikigenba_cron_create", map[string]any{
		"name": "nightly", "expr": "0 3 * * *",
	}); isErr {
		t.Fatalf("valid create should succeed")
	}

	list, isErr := call(t, h, "ikigenba_cron_list", map[string]any{})
	if isErr {
		t.Fatalf("list errored")
	}
	items, _ := list["items"].([]any)
	if len(items) != 1 {
		t.Fatalf("want 1 item, got %v", list)
	}

	refl, isErr := call(t, h, "ikigenba_cron_reflection", map[string]any{})
	if isErr {
		t.Fatalf("reflection errored")
	}
	pubs, _ := refl["publishes"].([]any)
	if len(pubs) != 1 {
		t.Fatalf("want 1 published type, got %v", refl["publishes"])
	}
	first, _ := pubs[0].(map[string]any)
	if first["type"] != "cron.nightly" {
		t.Fatalf("wrong published type: %v", first)
	}
}
