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
	"time"

	chassis "appkit/db"
	"appkit/server"

	"eventplane/outbox"

	"webhooks/internal/db"
	"webhooks/internal/webhooks"
)

const (
	testVersion = "test-1.2.3"
	testService = "webhooks"
	// testBaseURL carries the trailing slash the real wiring passes; trigger_url
	// is testBaseURL + "in/" + name.
	testBaseURL = "https://int.ikigenba.com/srv/webhooks/"

	ownerA = "alice@example.com"
	ownerB = "bob@example.com"
)

// fixedClock is the deterministic time seam injected into the Service so
// created_at is reproducible across runs.
type fixedClock struct{ t time.Time }

func (c fixedClock) Now() time.Time { return c.t }

// newTestHandler builds the assembled MCP handler through a real appkit/server
// Router over a real webhooks.Service backed by a fresh, migrated temp-file
// SQLite database (never :memory:), with a real *outbox.Outbox and a deterministic
// clock. It returns the handler and underlying Service so a test can also drive
// the public ingress handler to prove a secret still verifies end-to-end.
func newTestHandler(t *testing.T) (http.Handler, *webhooks.Service) {
	t.Helper()
	path := filepath.Join(t.TempDir(), "webhooks_test.db")
	conn, err := chassis.Open(path)
	if err != nil {
		t.Fatalf("open test db: %v", err)
	}
	t.Cleanup(func() { conn.Close() })
	migs, err := chassis.LoadMigrations(db.FS, "migrations")
	if err != nil {
		t.Fatalf("load test migrations: %v", err)
	}
	if err := chassis.Migrate(context.Background(), conn, migs); err != nil {
		t.Fatalf("migrate test db: %v", err)
	}
	clk := fixedClock{t: time.Date(2026, 6, 25, 12, 0, 0, 0, time.UTC)}
	svc := webhooks.NewService(conn, clk)
	ob, err := outbox.New(conn, outbox.Options{
		Source:   "webhooks",
		Registry: webhooks.Events,
		Now:      clk.Now,
	})
	if err != nil {
		t.Fatalf("outbox.New: %v", err)
	}
	svc.Outbox = ob
	var handler http.Handler
	_, err = server.New(server.Options{
		Addr:       "127.0.0.1:0",
		Logger:     slog.New(slog.NewTextHandler(io.Discard, nil)),
		ResourceID: testBaseURL + "mcp",
		AuthServer: "https://int.ikigenba.com",
		Version:    testVersion,
		Service:    testService,
		Events:     webhooks.Events,
		DB:         conn,
		Register: func(rt *server.Router) error {
			var err error
			handler, err = NewHandler(svc, rt)
			return err
		},
	})
	if err != nil {
		t.Fatalf("build test server: %v", err)
	}
	if handler == nil {
		t.Fatal("NewHandler returned nil handler")
	}
	return handler, svc
}

// ── JSON-RPC drivers ───────────────────────────────────────────────────────

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

type toolDescriptor struct {
	Name        string         `json:"name"`
	Description string         `json:"description"`
	InputSchema map[string]any `json:"inputSchema"`
}

// rpcAs drives a single JSON-RPC request through the real ServeHTTP seam with the
// nginx-injected X-Owner-Email identity header set, and returns the decoded
// response.
func rpcAs(t *testing.T, h http.Handler, owner, method string, params any) jsonRPCResponse {
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
	req.Header.Set("X-Owner-Email", owner)
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

// callAs drives a tools/call as owner and decodes the inner tool result.
func callAs(t *testing.T, h http.Handler, owner, name string, args any) toolResult {
	t.Helper()
	resp := rpcAs(t, h, owner, "tools/call", map[string]any{"name": name, "arguments": args})
	var tr toolResult
	if err := json.Unmarshal(resp.Result, &tr); err != nil {
		t.Fatalf("decode tool result for %s: %v (result=%s)", name, err, resp.Result)
	}
	return tr
}

// callOK asserts the success envelope (no isError) and decodes the text payload
// into a generic map.
func callOK(t *testing.T, h http.Handler, owner, name string, args any) map[string]any {
	t.Helper()
	tr := callAs(t, h, owner, name, args)
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
// `error` object (code, message, optional field).
func callErr(t *testing.T, h http.Handler, owner, name string, args any) map[string]any {
	t.Helper()
	tr := callAs(t, h, owner, name, args)
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

// listNames returns the set of webhook names owner currently sees via the real
// list tool.
func listNames(t *testing.T, h http.Handler, owner string) map[string]map[string]any {
	t.Helper()
	res := callOK(t, h, owner, "list", map[string]any{})
	rawItems, ok := res["items"].([]any)
	if !ok {
		t.Fatalf("list: items is not an array: %v", res["items"])
	}
	out := map[string]map[string]any{}
	for _, it := range rawItems {
		m, ok := it.(map[string]any)
		if !ok {
			t.Fatalf("list: item is not an object: %v", it)
		}
		name, _ := m["name"].(string)
		out[name] = m
	}
	return out
}

// str pulls a non-empty string field off a tool result map.
func str(t *testing.T, m map[string]any, key string) string {
	t.Helper()
	v, ok := m[key].(string)
	if !ok || v == "" {
		t.Fatalf("expected non-empty string %q, got %v", key, m[key])
	}
	return v
}

// secretVerifies POSTs to the real public ingress handler with the bearer secret
// and reports whether the trigger is accepted (202) — the end-to-end proof that a
// secret is still valid for a webhook.
func secretVerifies(t *testing.T, svc *webhooks.Service, name, secret string) bool {
	t.Helper()
	ing := webhooks.NewIngressHandler(svc, slog.New(slog.NewTextHandler(io.Discard, nil)))
	req := httptest.NewRequest(http.MethodPost, "/in/"+name, strings.NewReader(`{}`))
	req.Header.Set("Authorization", "Bearer "+secret)
	req.Header.Set("Content-Type", "application/json")
	rec := httptest.NewRecorder()
	ing.ServeHTTP(rec, req)
	return rec.Code == http.StatusAccepted
}

// ── tests ──────────────────────────────────────────────────────────────────

// R-0JBF-690N — tools/list through the assembled appkit handler exposes exactly
// webhooks's four domain tools plus the chassis health and reflection tools.
func TestAssembledHandlerListsExactlySixTools(t *testing.T) {
	h, _ := newTestHandler(t)

	resp := rpcAs(t, h, ownerA, "tools/list", nil)
	var result struct {
		Tools []toolDescriptor `json:"tools"`
	}
	if err := json.Unmarshal(resp.Result, &result); err != nil {
		t.Fatalf("decode tools/list result: %v (result=%s)", err, resp.Result)
	}
	if len(result.Tools) != 6 {
		t.Fatalf("tools/list returned %d tools, want 6: %+v", len(result.Tools), result.Tools)
	}

	got := map[string]bool{}
	for _, tool := range result.Tools {
		if tool.Name == "" {
			t.Fatalf("tools/list returned a tool with no name: %+v", tool)
		}
		if tool.Description == "" {
			t.Fatalf("tool %q has no description", tool.Name)
		}
		if tool.InputSchema == nil {
			t.Fatalf("tool %q has no inputSchema", tool.Name)
		}
		if got[tool.Name] {
			t.Fatalf("tools/list returned duplicate tool %q", tool.Name)
		}
		got[tool.Name] = true
	}
	for _, name := range []string{"create", "list", "delete", "rotate", "health", "reflection"} {
		if !got[name] {
			t.Fatalf("tools/list missing %q; got %v", name, got)
		}
	}
}

// R-A4N7-WVQ9 — the assembled MCP reflection surface exposes the sole received
// family, with its route-template subject and schema/example agreement.
func TestReflectionPublishesReceivedFamily(t *testing.T) {
	h, _ := newTestHandler(t)
	index := callOK(t, h, ownerA, "reflection", map[string]any{})
	publishes, ok := index["publishes"].([]any)
	if !ok || len(publishes) != 1 {
		t.Fatalf("reflection publishes = %v, want exactly one family", index["publishes"])
	}
	family, ok := publishes[0].(map[string]any)
	if !ok || family["kind"] != "received" || family["subject"] != "/<hook name>" {
		t.Fatalf("reflection family = %v, want received at /<hook name>", publishes[0])
	}

	detail := callOK(t, h, ownerA, "reflection", map[string]any{"kind": "received"})
	if detail["kind"] != "received" || detail["subject"] != "/<hook name>" {
		t.Fatalf("reflection detail route = %v", detail)
	}
	example, ok := detail["example"].(map[string]any)
	if !ok {
		t.Fatalf("reflection example = %T, want object", detail["example"])
	}
	schema, ok := detail["schema"].(map[string]any)
	if !ok {
		t.Fatalf("reflection schema = %T, want object", detail["schema"])
	}
	properties, ok := schema["properties"].(map[string]any)
	if !ok {
		t.Fatalf("reflection schema properties = %v", schema["properties"])
	}
	for field := range example {
		if _, ok := properties[field]; !ok {
			t.Errorf("example field %q absent from schema properties %v", field, properties)
		}
	}
	bad := callAs(t, h, ownerA, "reflection", map[string]any{"kind": "missing"})
	if !bad.IsError || len(bad.Content) != 1 || !strings.Contains(bad.Content[0].Text, `unknown event kind "missing"`) || !strings.Contains(bad.Content[0].Text, "known kinds: received") {
		t.Fatalf("unknown-kind response = %+v, want typed unknown-kind error naming received", bad)
	}
}

// R-5Z8J-Y0YP — create with no name mints an opaque name; trigger_url is
// baseURL+"in/"+name, the secret is show-once (ms_wh_ prefix), and the row is
// persisted owned by the X-Owner-Email caller (confirmed via a follow-up list).
func TestCreateGeneratesNamePersistsOwnerScoped(t *testing.T) {
	h, _ := newTestHandler(t)

	res := callOK(t, h, ownerA, "create", map[string]any{})
	name := str(t, res, "name")
	if want := testBaseURL + "in/" + name; res["trigger_url"] != want {
		t.Fatalf("trigger_url = %v, want %q", res["trigger_url"], want)
	}
	secret := str(t, res, "secret")
	if !strings.HasPrefix(secret, "ms_wh_") {
		t.Fatalf("secret = %q, want ms_wh_ prefix", secret)
	}

	// Persisted and owned by the caller — appears in ownerA's own list.
	if _, ok := listNames(t, h, ownerA)[name]; !ok {
		t.Fatalf("created webhook %q not found in owner's list", name)
	}
}

// R-60GG-BSPE — list is owner-scoped, secret-free, and complete: a caller sees
// exactly their own entries (name/created_at/last_triggered_at) and never the
// other owner's webhook or any secret material.
func TestListOwnerScopedSecretFree(t *testing.T) {
	h, _ := newTestHandler(t)

	callOK(t, h, ownerA, "create", map[string]any{"name": "a-one"})
	callOK(t, h, ownerA, "create", map[string]any{"name": "a-two"})
	callOK(t, h, ownerB, "create", map[string]any{"name": "b-one"})

	got := listNames(t, h, ownerA)
	if len(got) != 2 {
		t.Fatalf("ownerA list size = %d, want 2 (%v)", len(got), got)
	}
	if _, ok := got["b-one"]; ok {
		t.Fatalf("ownerA list leaked ownerB's webhook b-one")
	}
	for _, name := range []string{"a-one", "a-two"} {
		entry, ok := got[name]
		if !ok {
			t.Fatalf("ownerA list missing own webhook %q", name)
		}
		if _, ok := entry["created_at"]; !ok {
			t.Fatalf("%q list entry missing created_at", name)
		}
		if _, ok := entry["last_triggered_at"]; !ok {
			t.Fatalf("%q list entry missing last_triggered_at key", name)
		}
		if entry["last_triggered_at"] != nil {
			t.Fatalf("%q last_triggered_at = %v, want null before first trigger", name, entry["last_triggered_at"])
		}
		if _, leaked := entry["secret"]; leaked {
			t.Fatalf("%q list entry leaked a secret field", name)
		}
		if _, leaked := entry["secret_hash"]; leaked {
			t.Fatalf("%q list entry leaked a secret_hash field", name)
		}
	}
}

// R-61OC-PKG3 — delete returns {deleted:true} and the name no longer appears in
// the owner's list.
func TestDeleteRemovesFromList(t *testing.T) {
	h, _ := newTestHandler(t)
	callOK(t, h, ownerA, "create", map[string]any{"name": "gone-soon"})

	res := callOK(t, h, ownerA, "delete", map[string]any{"name": "gone-soon"})
	if res["deleted"] != true {
		t.Fatalf("delete result = %v, want {deleted:true}", res)
	}
	if _, ok := listNames(t, h, ownerA)["gone-soon"]; ok {
		t.Fatalf("deleted webhook still present in list")
	}
}

// R-62W9-3C6S — rotate returns a fresh show-once secret with the SAME trigger_url
// (name unchanged) and a secret differing from create's.
func TestRotateNewSecretSameURL(t *testing.T) {
	h, _ := newTestHandler(t)

	created := callOK(t, h, ownerA, "create", map[string]any{"name": "rotateme"})
	secret1 := str(t, created, "secret")
	url1 := str(t, created, "trigger_url")

	rotated := callOK(t, h, ownerA, "rotate", map[string]any{"name": "rotateme"})
	secret2 := str(t, rotated, "secret")
	url2 := str(t, rotated, "trigger_url")

	if url2 != url1 {
		t.Fatalf("rotate changed trigger_url: %q -> %q", url1, url2)
	}
	if secret2 == secret1 {
		t.Fatalf("rotate returned the same secret %q", secret2)
	}
	if !strings.HasPrefix(secret2, "ms_wh_") {
		t.Fatalf("rotated secret = %q, want ms_wh_ prefix", secret2)
	}
}

// R-6445-H3XH — a non-owner's delete and rotate each return a not_found envelope
// and mutate nothing: the webhook still exists in the owner's list and its
// original secret still verifies end-to-end through the public ingress.
func TestNonOwnerDeleteRotateNotFound(t *testing.T) {
	h, svc := newTestHandler(t)

	created := callOK(t, h, ownerA, "create", map[string]any{"name": "shared"})
	secret := str(t, created, "secret")

	if env := callErr(t, h, ownerB, "delete", map[string]any{"name": "shared"}); env["code"] != "not_found" {
		t.Fatalf("non-owner delete code = %v, want not_found", env["code"])
	}
	if env := callErr(t, h, ownerB, "rotate", map[string]any{"name": "shared"}); env["code"] != "not_found" {
		t.Fatalf("non-owner rotate code = %v, want not_found", env["code"])
	}

	// Nothing mutated: owner still sees it, and the ORIGINAL secret still verifies.
	if _, ok := listNames(t, h, ownerA)["shared"]; !ok {
		t.Fatalf("webhook vanished from owner's list after a non-owner mutation attempt")
	}
	if !secretVerifies(t, svc, "shared", secret) {
		t.Fatalf("original secret no longer verifies after non-owner mutation attempts")
	}
}

// R-65C1-UVO6 — create of an already-used name → duplicate; create of an invalid
// name → validation with field=="name".
func TestCreateDuplicateAndInvalid(t *testing.T) {
	h, _ := newTestHandler(t)

	callOK(t, h, ownerA, "create", map[string]any{"name": "dup"})
	if env := callErr(t, h, ownerA, "create", map[string]any{"name": "dup"}); env["code"] != "duplicate" {
		t.Fatalf("duplicate create code = %v, want duplicate", env["code"])
	}

	env := callErr(t, h, ownerA, "create", map[string]any{"name": "bad name"})
	if env["code"] != "validation" {
		t.Fatalf("invalid-name create code = %v, want validation", env["code"])
	}
	if env["field"] != "name" {
		t.Fatalf("invalid-name create field = %v, want name", env["field"])
	}
}
