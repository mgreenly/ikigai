package mcp

import (
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

	"ledger/internal/db"
	"ledger/internal/ledger"
)

const (
	testOwner    = "owner@example.com"
	testClientID = "client-123"
	testVersion  = "test-1.2.3"
	testService  = "ledger"
)

func newTestHandler(t *testing.T) http.Handler {
	t.Helper()
	path := filepath.Join(t.TempDir(), "ledger_test.db")
	conn, err := appkitdb.Open(path)
	if err != nil {
		t.Fatalf("open test db: %v", err)
	}
	t.Cleanup(func() { conn.Close() })
	migs, err := appkitdb.LoadMigrations(db.FS, "migrations")
	if err != nil {
		t.Fatalf("load migrations: %v", err)
	}
	if err := appkitdb.Migrate(context.Background(), conn, migs); err != nil {
		t.Fatalf("migrate test db: %v", err)
	}
	svc := ledger.NewService(conn)
	var captured *server.Router
	_, err = server.New(server.Options{
		Addr:       "127.0.0.1:0",
		Logger:     slog.New(slog.NewTextHandler(io.Discard, nil)),
		ResourceID: "https://example.test/srv/ledger",
		AuthServer: "https://auth.example.test",
		Version:    testVersion,
		Service:    testService,
		Events:     ledger.Events,
		DB:         conn,
		Register: func(rt *server.Router) error {
			captured = rt
			return nil
		},
	})
	if err != nil {
		t.Fatalf("build test router: %v", err)
	}
	if captured == nil {
		t.Fatalf("server.New did not invoke Register")
	}
	h, err := NewHandler(svc, captured)
	if err != nil {
		t.Fatalf("build mcp handler: %v", err)
	}
	return h
}

// rpc drives one JSON-RPC call through ServeHTTP and returns the decoded result
// object. params is the raw JSON for "params".
func rpc(t *testing.T, h http.Handler, method, params string) map[string]any {
	t.Helper()
	body := `{"jsonrpc":"2.0","id":1,"method":"` + method + `","params":` + params + `}`
	req := httptest.NewRequest(http.MethodPost, "/mcp", strings.NewReader(body))
	req.Header.Set("X-Owner-Email", testOwner)
	req.Header.Set("X-Client-Id", testClientID)
	rec := httptest.NewRecorder()
	h.ServeHTTP(rec, req)
	if rec.Code != http.StatusOK {
		t.Fatalf("%s: status %d", method, rec.Code)
	}
	var env struct {
		Result map[string]any `json:"result"`
		Error  any            `json:"error"`
	}
	if err := json.Unmarshal(rec.Body.Bytes(), &env); err != nil {
		t.Fatalf("%s: decode envelope: %v\n%s", method, err, rec.Body.String())
	}
	if env.Error != nil {
		t.Fatalf("%s: transport error %v", method, env.Error)
	}
	return env.Result
}

// callTool invokes tools/call and returns the decoded text payload plus the
// isError flag.
func callTool(t *testing.T, h http.Handler, name, args string) (map[string]any, bool) {
	t.Helper()
	text, isErr := callToolText(t, h, name, args)
	var payload map[string]any
	if err := json.Unmarshal([]byte(text), &payload); err != nil {
		t.Fatalf("%s: decode payload %q: %v", name, text, err)
	}
	return payload, isErr
}

func callToolText(t *testing.T, h http.Handler, name, args string) (string, bool) {
	t.Helper()
	res := rpc(t, h, "tools/call", `{"name":"`+name+`","arguments":`+args+`}`)
	isErr, _ := res["isError"].(bool)
	content, ok := res["content"].([]any)
	if !ok || len(content) == 0 {
		t.Fatalf("%s: no content: %v", name, res)
	}
	text := content[0].(map[string]any)["text"].(string)
	return text, isErr
}

func TestToolsListIncludesDomainAndChassisTools(t *testing.T) {
	h := newTestHandler(t)
	res := rpc(t, h, "tools/list", `{}`)
	tools, _ := res["tools"].([]any)
	// R-52PA-OE6N
	if len(tools) != 9 {
		t.Fatalf("tools/list returned %d tools, want 9", len(tools))
	}
	names := map[string]bool{}
	for _, tl := range tools {
		names[tl.(map[string]any)["name"].(string)] = true
	}
	for _, want := range []string{
		"record", "reverse", "reconcile",
		"balance", "register", "get",
		"describe", "health", "reflection",
	} {
		if !names[want] {
			t.Errorf("tools/list missing %s", want)
		}
	}
}

// TestReflection covers the reflection tool: the no-arg index
// (the one published type, empty subscribes — ledger is a producer), the
// event_type detail (schema + example), and the corrective error for an unknown
// type.
func TestReflection(t *testing.T) {
	h := newTestHandler(t)

	// No-arg → the index {publishes, subscribes}.
	idx, isErr := callTool(t, h, "reflection", `{}`)
	if isErr {
		t.Fatalf("reflection index isError: %v", idx)
	}
	publishes, ok := idx["publishes"].([]any)
	if !ok {
		t.Fatalf("reflection index missing publishes array: %v", idx)
	}
	if len(publishes) != 1 {
		t.Fatalf("expected exactly 1 published type, got %d: %v", len(publishes), publishes)
	}
	p := publishes[0].(map[string]any)
	if p["type"] != "transaction.recorded" {
		t.Errorf("published type = %v, want transaction.recorded", p["type"])
	}
	if p["description"] == "" {
		t.Errorf("published type has empty description")
	}

	// ledger is a producer: subscribes is present and empty.
	subscribes, ok := idx["subscribes"].([]any)
	if !ok {
		t.Fatalf("reflection index missing subscribes array: %v", idx)
	}
	if len(subscribes) != 0 {
		t.Fatalf("expected empty subscribes for ledger, got %v", subscribes)
	}

	// event_type → the publish detail (schema + example).
	detail, isErr := callTool(t, h, "reflection", `{"event_type":"transaction.recorded"}`)
	if isErr {
		t.Fatalf("reflection detail isError: %v", detail)
	}
	if detail["type"] != "transaction.recorded" {
		t.Fatalf("detail type mismatch: %v", detail)
	}
	if detail["description"] == "" {
		t.Fatalf("detail missing description: %v", detail)
	}
	sch, ok := detail["schema"].(map[string]any)
	if !ok || sch["type"] != "object" {
		t.Fatalf("detail schema not an object schema: %v", detail["schema"])
	}
	if _, ok := sch["properties"].(map[string]any); !ok {
		t.Fatalf("detail schema missing properties: %v", sch)
	}
	if _, ok := detail["example"].(map[string]any); !ok {
		t.Fatalf("detail missing example object: %v", detail["example"])
	}

	// Unknown event_type → corrective error listing valid types.
	badErr, isErr := callToolText(t, h, "reflection", `{"event_type":"transaction.nope"}`)
	if !isErr {
		t.Fatalf("expected error for unknown event_type, got %v", badErr)
	}
	if !strings.Contains(badErr, "unknown event_type") ||
		!strings.Contains(badErr, "transaction.recorded") {
		t.Errorf("corrective message missing valid type: %q", badErr)
	}
}

func TestHealth(t *testing.T) {
	h := newTestHandler(t)
	p, isErr := callTool(t, h, "health", `{}`)
	if isErr {
		t.Fatal("health isError")
	}
	if p["status"] != "ok" || p["version"] != testVersion || p["service"] != testService {
		t.Errorf("health envelope = %v", p)
	}
	if p["owner_email"] != testOwner || p["client_id"] != testClientID {
		t.Errorf("health identity = %v", p)
	}
	details, ok := p["details"].(map[string]any)
	if !ok || len(details) != 0 {
		t.Errorf("health details = %v, want present and empty", p["details"])
	}
}

func TestRecordGetReverseReconcile_EndToEnd(t *testing.T) {
	h := newTestHandler(t)

	// describe first (the recommended first call).
	d, _ := callTool(t, h, "describe", `{}`)
	if roots, _ := d["roots"].([]any); len(roots) != 5 {
		t.Fatalf("describe roots = %v", d["roots"])
	}

	// record with an elided residual.
	rec, isErr := callTool(t, h, "record", `{
		"date":"2026-06-01","description":"Acme — June hosting",
		"postings":[
			{"account":"Assets:Receivable:Acme","amount_cents":5000},
			{"account":"Revenue:Hosting"}
		]}`)
	if isErr {
		t.Fatalf("record isError: %v", rec)
	}
	txnID := rec["id"].(string)
	postings := rec["postings"].([]any)
	if len(postings) != 2 {
		t.Fatalf("postings = %v", postings)
	}
	// Alias folded Revenue→Income; residual resolved to -5000.
	second := postings[1].(map[string]any)
	if second["account"] != "Income:Hosting" {
		t.Errorf("alias not folded: %v", second["account"])
	}
	if second["amount_cents"].(float64) != -5000 {
		t.Errorf("residual = %v, want -5000", second["amount_cents"])
	}
	bankPosting := postings[0].(map[string]any)["id"].(string)

	// get round-trips.
	got, _ := callTool(t, h, "get", `{"id":"`+txnID+`"}`)
	if got["id"] != txnID {
		t.Errorf("get id = %v", got["id"])
	}

	// reconcile the first leg to cleared.
	rc, isErr := callTool(t, h, "reconcile", `{"posting_ids":["`+bankPosting+`"],"status":"cleared"}`)
	if isErr {
		t.Fatalf("reconcile isError: %v", rc)
	}
	txns := rc["transactions"].([]any)
	if len(txns) != 1 {
		t.Fatalf("reconcile affected = %v", txns)
	}

	// balance: whole ledger sums to zero.
	bal, _ := callTool(t, h, "balance", `{}`)
	if bal["total"].(float64) != 0 {
		t.Errorf("whole-ledger total = %v, want 0", bal["total"])
	}

	// register for the customer.
	reg, _ := callTool(t, h, "register", `{"query":"Assets:Receivable:Acme"}`)
	if lines := reg["lines"].([]any); len(lines) != 1 {
		t.Errorf("register lines = %v", lines)
	}

	// reverse the transaction.
	rev, isErr := callTool(t, h, "reverse", `{"id":"`+txnID+`"}`)
	if isErr {
		t.Fatalf("reverse isError: %v", rev)
	}
	if rev["reverses_id"] != txnID {
		t.Errorf("reverse reverses_id = %v", rev["reverses_id"])
	}
	// Double reverse blocked.
	_, isErr = callTool(t, h, "reverse", `{"id":"`+txnID+`"}`)
	if !isErr {
		t.Error("expected already_reversed error on double reverse")
	}
}

func TestRecord_ErrorsSurfaceAsToolErrors(t *testing.T) {
	h := newTestHandler(t)

	// Unknown root → bad_root.
	p, isErr := callTool(t, h, "record", `{"date":"2026-06-01","description":"x","postings":[{"account":"Bogus:Acct","amount_cents":1},{"account":"Assets:Bank","amount_cents":-1}]}`)
	if !isErr || errCode(p) != "bad_root" {
		t.Errorf("bad root: isErr=%v payload=%v", isErr, p)
	}

	// Unbalanced explicit postings → unbalanced.
	p, isErr = callTool(t, h, "record", `{"date":"2026-06-01","description":"x","postings":[{"account":"Assets:Bank","amount_cents":5000},{"account":"Income:Hosting","amount_cents":-4000}]}`)
	if !isErr || errCode(p) != "unbalanced" {
		t.Errorf("unbalanced: isErr=%v payload=%v", isErr, p)
	}

	// Fewer than two postings → validation.
	p, isErr = callTool(t, h, "record", `{"date":"2026-06-01","description":"x","postings":[{"account":"Assets:Bank","amount_cents":0}]}`)
	if !isErr || errCode(p) != "validation" {
		t.Errorf("one-posting: isErr=%v payload=%v", isErr, p)
	}

	// Bad date → validation.
	p, isErr = callTool(t, h, "record", `{"date":"2026-6-1","description":"x","postings":[{"account":"Assets:Bank","amount_cents":1},{"account":"Income:Hosting","amount_cents":-1}]}`)
	if !isErr || errCode(p) != "validation" {
		t.Errorf("bad date: isErr=%v payload=%v", isErr, p)
	}

	// Get of a missing id → not_found.
	p, isErr = callTool(t, h, "get", `{"id":"NOPE"}`)
	if !isErr || errCode(p) != "not_found" {
		t.Errorf("not_found: isErr=%v payload=%v", isErr, p)
	}
}

func TestBalance_PeriodBucketAndRange(t *testing.T) {
	h := newTestHandler(t)
	mustRecord(t, h, `{"date":"2026-06-15","description":"june","postings":[{"account":"Expenses:Office","amount_cents":1000},{"account":"Assets:Bank","amount_cents":-1000}]}`)
	mustRecord(t, h, `{"date":"2026-07-15","description":"july","postings":[{"account":"Expenses:Office","amount_cents":2000},{"account":"Assets:Bank","amount_cents":-2000}]}`)

	// Bucket "2026-06" → only June's 1000.
	bal, _ := callTool(t, h, "balance", `{"query":"Expenses","period":"2026-06"}`)
	if total := bal["total"].(float64); total != 1000 {
		t.Errorf("June expenses total = %v, want 1000", total)
	}
	// Range covering both months.
	bal, _ = callTool(t, h, "balance", `{"query":"Expenses","period":{"from":"2026-06-01","to":"2026-07-31"}}`)
	if total := bal["total"].(float64); total != 3000 {
		t.Errorf("range expenses total = %v, want 3000", total)
	}
}

func mustRecord(t *testing.T, h http.Handler, args string) {
	t.Helper()
	if _, isErr := callTool(t, h, "record", args); isErr {
		t.Fatalf("record failed: %s", args)
	}
}

func errCode(payload map[string]any) string {
	e, ok := payload["error"].(map[string]any)
	if !ok {
		return ""
	}
	code, _ := e["code"].(string)
	return code
}
