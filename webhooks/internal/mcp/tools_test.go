package mcp

import (
	"bytes"
	"context"
	"database/sql"
	"encoding/json"
	"io"
	"log/slog"
	"net/http"
	"net/http/httptest"
	"os/exec"
	"path/filepath"
	"reflect"
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
	h, svc, _ := newTestHandlerWithDB(t)
	return h, svc
}

func newTestHandlerWithDB(t *testing.T) (http.Handler, *webhooks.Service, *sql.DB) {
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
	return handler, svc, conn
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
	IsError           bool           `json:"isError"`
	StructuredContent map[string]any `json:"structuredContent"`
	Content           []struct {
		Type string `json:"type"`
		Text string `json:"text"`
	} `json:"content"`
}

type toolDescriptor struct {
	Name         string         `json:"name"`
	Description  string         `json:"description"`
	InputSchema  map[string]any `json:"inputSchema"`
	OutputSchema map[string]any `json:"outputSchema"`
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

// callErr asserts a structured error envelope (isError:true) and returns its
// machine-readable {code,message} object.
func callErr(t *testing.T, h http.Handler, owner, name string, args any) map[string]any {
	t.Helper()
	tr := callAs(t, h, owner, name, args)
	if !tr.IsError {
		t.Fatalf("%s: expected an error envelope, got success: %s", name, payloadText(tr))
	}
	if tr.StructuredContent == nil {
		t.Fatalf("%s: error envelope missing structuredContent: %+v", name, tr)
	}
	if len(tr.Content) != 1 || tr.Content[0].Type != "text" {
		t.Fatalf("%s: expected one text error content block, got %+v", name, tr.Content)
	}
	if tr.StructuredContent["message"] != tr.Content[0].Text {
		t.Fatalf("%s: text error message %q does not mirror structured message %v", name, tr.Content[0].Text, tr.StructuredContent["message"])
	}
	return tr.StructuredContent
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

// R-65C1-UVO6 — create of an already-used name → conflict; create of an invalid
// name → validation.
func TestCreateDuplicateAndInvalid(t *testing.T) {
	h, _ := newTestHandler(t)

	callOK(t, h, ownerA, "create", map[string]any{"name": "dup"})
	if env := callErr(t, h, ownerA, "create", map[string]any{"name": "dup"}); env["code"] != "conflict" {
		t.Fatalf("duplicate create code = %v, want conflict", env["code"])
	}

	env := callErr(t, h, ownerA, "create", map[string]any{"name": "bad name"})
	if env["code"] != "validation" {
		t.Fatalf("invalid-name create code = %v, want validation", env["code"])
	}
	if len(env) != 2 {
		t.Fatalf("invalid-name error keys = %v, want exactly code and message", env)
	}
}

// R-G8ZT-KWSE — create defaults to bearer, accepts github-hmac with a show-once
// secret and secret-free list projection, and rejects unknown schemes without a row.
func TestCreateVerificationSchemesAndValidation(t *testing.T) {
	h, _ := newTestHandler(t)
	github := callOK(t, h, ownerA, "create", map[string]any{"name": "github", "verification": "github-hmac"})
	if github["verification"] != "github-hmac" || !strings.HasPrefix(str(t, github, "secret"), "ms_wh_") {
		t.Fatalf("github create = %v", github)
	}
	defaulted := callOK(t, h, ownerA, "create", map[string]any{"name": "defaulted"})
	if defaulted["verification"] != "bearer" {
		t.Fatalf("default verification = %v, want bearer", defaulted["verification"])
	}
	err := callErr(t, h, ownerA, "create", map[string]any{"name": "invalid", "verification": "basic"})
	if err["code"] != "validation" {
		t.Fatalf("unknown scheme code = %v, want validation", err["code"])
	}
	items := listNames(t, h, ownerA)
	if len(items) != 2 || items["github"]["verification"] != "github-hmac" || items["defaulted"]["verification"] != "bearer" {
		t.Fatalf("list schemes = %v", items)
	}
	for name, item := range items {
		if _, leaked := item["secret"]; leaked {
			t.Fatalf("%s list item leaked secret: %v", name, item)
		}
	}
	if _, exists := items["invalid"]; exists {
		t.Fatal("invalid verification wrote a row")
	}
}

// R-DRUS-R3AP — every webhooks-owned domain tool advertises a non-nil output
// schema through the assembled handler's tools/list response.
func TestDomainToolsDeclareOutputSchemas(t *testing.T) {
	h, _ := newTestHandler(t)
	resp := rpcAs(t, h, ownerA, "tools/list", nil)
	var result struct {
		Tools []toolDescriptor `json:"tools"`
	}
	if err := json.Unmarshal(resp.Result, &result); err != nil {
		t.Fatalf("decode tools/list result: %v", err)
	}

	byName := make(map[string]toolDescriptor, len(result.Tools))
	for _, descriptor := range result.Tools {
		byName[descriptor.Name] = descriptor
	}
	for _, name := range []string{"create", "list", "delete", "rotate"} {
		descriptor, ok := byName[name]
		if !ok {
			t.Errorf("tools/list missing domain tool %q", name)
			continue
		}
		if descriptor.OutputSchema == nil {
			t.Errorf("domain tool %q has nil outputSchema", name)
		}
	}
}

// R-DT2P-4V1E — all four domain successes carry structuredContent and a text
// JSON rendering that decodes to exactly the same value.
func TestDomainSuccessesMirrorStructuredContentAsText(t *testing.T) {
	h, _ := newTestHandler(t)
	results := map[string]toolResult{}
	results["create"] = callAs(t, h, ownerA, "create", map[string]any{"name": "mirrored"})
	results["list"] = callAs(t, h, ownerA, "list", map[string]any{})
	results["rotate"] = callAs(t, h, ownerA, "rotate", map[string]any{"name": "mirrored"})
	results["delete"] = callAs(t, h, ownerA, "delete", map[string]any{"name": "mirrored"})

	for _, name := range []string{"create", "list", "delete", "rotate"} {
		tr := results[name]
		if tr.IsError {
			t.Errorf("%s returned isError:true", name)
			continue
		}
		if tr.StructuredContent == nil {
			t.Errorf("%s success has nil structuredContent", name)
			continue
		}
		if len(tr.Content) != 1 || tr.Content[0].Type != "text" {
			t.Errorf("%s content = %+v, want one text block", name, tr.Content)
			continue
		}
		var mirrored map[string]any
		if err := json.Unmarshal([]byte(tr.Content[0].Text), &mirrored); err != nil {
			t.Errorf("%s text is not JSON: %v", name, err)
			continue
		}
		if !reflect.DeepEqual(mirrored, tr.StructuredContent) {
			t.Errorf("%s text JSON = %#v, structuredContent = %#v", name, mirrored, tr.StructuredContent)
		}
	}
}

// R-DUAL-IMS3 — each domain success has exactly the keys its declared output
// schema describes, including the secret-free list item projection.
func TestDomainSuccessesConformToDeclaredOutputSchemas(t *testing.T) {
	h, _ := newTestHandler(t)
	resp := rpcAs(t, h, ownerA, "tools/list", nil)
	var listed struct {
		Tools []toolDescriptor `json:"tools"`
	}
	if err := json.Unmarshal(resp.Result, &listed); err != nil {
		t.Fatalf("decode tools/list: %v", err)
	}
	schemas := map[string]map[string]any{}
	for _, descriptor := range listed.Tools {
		schemas[descriptor.Name] = descriptor.OutputSchema
	}

	results := map[string]map[string]any{}
	results["create"] = callAs(t, h, ownerA, "create", map[string]any{"name": "schema-match"}).StructuredContent
	results["list"] = callAs(t, h, ownerA, "list", map[string]any{}).StructuredContent
	results["rotate"] = callAs(t, h, ownerA, "rotate", map[string]any{"name": "schema-match"}).StructuredContent
	results["delete"] = callAs(t, h, ownerA, "delete", map[string]any{"name": "schema-match"}).StructuredContent
	for _, name := range []string{"create", "list", "delete", "rotate"} {
		assertObjectMatchesSchema(t, name, results[name], schemas[name])
	}

	if str(t, results["create"], "secret") == "" {
		t.Fatal("create omitted its show-once secret")
	}
	if results["delete"]["deleted"] != true {
		t.Fatalf("delete structuredContent = %v, want deleted:true", results["delete"])
	}
	items, ok := results["list"]["items"].([]any)
	if !ok || len(items) != 1 {
		t.Fatalf("list items = %v, want one item", results["list"]["items"])
	}
	item, ok := items[0].(map[string]any)
	if !ok {
		t.Fatalf("list item = %T, want object", items[0])
	}
	listProps := schemas["list"]["properties"].(map[string]any)
	itemSchema := listProps["items"].(map[string]any)["items"].(map[string]any)
	assertObjectMatchesSchema(t, "list item", item, itemSchema)
	if _, leaked := item["secret"]; leaked {
		t.Fatal("list item leaked secret")
	}
	if _, leaked := item["secret_hash"]; leaked {
		t.Fatal("list item leaked secret_hash")
	}
}

func assertObjectMatchesSchema(t *testing.T, name string, value, schema map[string]any) {
	t.Helper()
	if value == nil || schema == nil {
		t.Fatalf("%s value/schema must be non-nil: value=%v schema=%v", name, value, schema)
	}
	props, ok := schema["properties"].(map[string]any)
	if !ok {
		t.Fatalf("%s schema properties = %T, want object", name, schema["properties"])
	}
	want := make(map[string]bool, len(props))
	for key := range props {
		want[key] = true
	}
	got := make(map[string]bool, len(value))
	for key := range value {
		got[key] = true
	}
	if !reflect.DeepEqual(got, want) {
		t.Fatalf("%s keys = %v, schema property keys = %v", name, got, want)
	}
	required, ok := schema["required"].([]any)
	if !ok || len(required) != len(want) {
		t.Fatalf("%s required = %v, want every property required", name, schema["required"])
	}
	for _, key := range required {
		if !want[key.(string)] {
			t.Fatalf("%s schema requires unknown key %v", name, key)
		}
	}
}

// R-DVIH-WEIS — a second create of the same name is a structured conflict,
// never the retired duplicate code.
func TestDuplicateCreateReturnsStructuredConflict(t *testing.T) {
	h, _ := newTestHandler(t)
	callOK(t, h, ownerA, "create", map[string]any{"name": "taken"})
	env := callErr(t, h, ownerA, "create", map[string]any{"name": "taken"})
	if env["code"] != "conflict" {
		t.Fatalf("duplicate create code = %v, want conflict", env["code"])
	}
}

// R-DWQE-A69H — domain errors are structured isError results whose codes stay
// inside appkit's closed vocabulary, including an unexpected closed-store error.
func TestDomainErrorsUseClosedStructuredVocabulary(t *testing.T) {
	h, _ := newTestHandler(t)
	callOK(t, h, ownerA, "create", map[string]any{"name": "owned"})

	internalHandler, _, conn := newTestHandlerWithDB(t)
	if err := conn.Close(); err != nil {
		t.Fatalf("close db to force store failure: %v", err)
	}

	cases := []struct {
		name string
		got  map[string]any
		want string
	}{
		{name: "invalid create", got: callErr(t, h, ownerA, "create", map[string]any{"name": "bad name"}), want: "validation"},
		{name: "non-owner delete", got: callErr(t, h, ownerB, "delete", map[string]any{"name": "owned"}), want: "not_found"},
		{name: "non-owner rotate", got: callErr(t, h, ownerB, "rotate", map[string]any{"name": "owned"}), want: "not_found"},
		{name: "unexpected store failure", got: callErr(t, internalHandler, ownerA, "list", map[string]any{}), want: "internal"},
	}
	closed := map[string]bool{
		"validation": true, "not_found": true, "conflict": true,
		"too_large": true, "source_unavailable": true, "internal": true,
	}
	for _, tc := range cases {
		if tc.got["code"] != tc.want {
			t.Errorf("%s code = %v, want %s", tc.name, tc.got["code"], tc.want)
		}
		code, _ := tc.got["code"].(string)
		if !closed[code] {
			t.Errorf("%s code %q is outside closed vocabulary", tc.name, code)
		}
		if len(tc.got) != 2 || tc.got["message"] == nil {
			t.Errorf("%s structuredContent = %v, want exactly code and message", tc.name, tc.got)
		}
	}
}

// R-DXYA-NY06 — no non-test service source retains the removed legacy result
// helper identifier.
func TestNonTestSourceContainsNoLegacyJSONResultCalls(t *testing.T) {
	cmd := exec.Command("bash", "-c", "grep -rn 'JSONResult' internal cmd --include='*.go' | grep -v _test.go || true")
	cmd.Dir = filepath.Join("..", "..")
	out, err := cmd.CombinedOutput()
	if err != nil {
		t.Fatalf("scan non-test Go source: %v (%s)", err, out)
	}
	if strings.TrimSpace(string(out)) != "" {
		t.Fatalf("legacy JSONResult remains in non-test source:\n%s", out)
	}
}
