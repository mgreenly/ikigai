package mcp

import (
	"context"
	"database/sql"
	"encoding/json"
	"net/http"
	"net/http/httptest"
	"strings"
	"testing"

	"ledger/internal/db"
	"ledger/internal/ledger"

	_ "modernc.org/sqlite"
)

func newHandler(t *testing.T) *Handler {
	t.Helper()
	conn, err := sql.Open("sqlite", ":memory:")
	if err != nil {
		t.Fatalf("open: %v", err)
	}
	if _, err := conn.Exec(`PRAGMA foreign_keys = ON`); err != nil {
		t.Fatalf("fk: %v", err)
	}
	t.Cleanup(func() { conn.Close() })
	if err := db.Migrate(context.Background(), conn); err != nil {
		t.Fatalf("migrate: %v", err)
	}
	return NewHandler(ledger.NewService(conn))
}

// rpc drives one JSON-RPC call through ServeHTTP and returns the decoded result
// object. params is the raw JSON for "params".
func rpc(t *testing.T, h *Handler, method, params string) map[string]any {
	t.Helper()
	body := `{"jsonrpc":"2.0","id":1,"method":"` + method + `","params":` + params + `}`
	req := httptest.NewRequest(http.MethodPost, "/mcp", strings.NewReader(body))
	req.Header.Set("X-Owner-Email", "me@example.com")
	req.Header.Set("X-Client-Id", "client-123")
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
func callTool(t *testing.T, h *Handler, name, args string) (map[string]any, bool) {
	t.Helper()
	res := rpc(t, h, "tools/call", `{"name":"`+name+`","arguments":`+args+`}`)
	isErr, _ := res["isError"].(bool)
	content, ok := res["content"].([]any)
	if !ok || len(content) == 0 {
		t.Fatalf("%s: no content: %v", name, res)
	}
	text := content[0].(map[string]any)["text"].(string)
	var payload map[string]any
	if err := json.Unmarshal([]byte(text), &payload); err != nil {
		t.Fatalf("%s: decode payload %q: %v", name, text, err)
	}
	return payload, isErr
}

func TestToolsList_HasEight(t *testing.T) {
	h := newHandler(t)
	res := rpc(t, h, "tools/list", `{}`)
	tools, _ := res["tools"].([]any)
	if len(tools) != 8 {
		t.Fatalf("tools/list returned %d tools, want 8", len(tools))
	}
	names := map[string]bool{}
	for _, tl := range tools {
		names[tl.(map[string]any)["name"].(string)] = true
	}
	for _, want := range []string{
		"ledger_record", "ledger_reverse", "ledger_reconcile", "ledger_balance",
		"ledger_register", "ledger_get", "ledger_describe", "ledger_whoami",
	} {
		if !names[want] {
			t.Errorf("tools/list missing %s", want)
		}
	}
}

func TestWhoami(t *testing.T) {
	h := newHandler(t)
	p, isErr := callTool(t, h, "ledger_whoami", `{}`)
	if isErr {
		t.Fatal("whoami isError")
	}
	if p["owner_email"] != "me@example.com" || p["client_id"] != "client-123" {
		t.Errorf("whoami = %v", p)
	}
}

func TestRecordGetReverseReconcile_EndToEnd(t *testing.T) {
	h := newHandler(t)

	// describe first (the recommended first call).
	d, _ := callTool(t, h, "ledger_describe", `{}`)
	if roots, _ := d["roots"].([]any); len(roots) != 5 {
		t.Fatalf("describe roots = %v", d["roots"])
	}

	// record with an elided residual.
	rec, isErr := callTool(t, h, "ledger_record", `{
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
	got, _ := callTool(t, h, "ledger_get", `{"id":"`+txnID+`"}`)
	if got["id"] != txnID {
		t.Errorf("get id = %v", got["id"])
	}

	// reconcile the first leg to cleared.
	rc, isErr := callTool(t, h, "ledger_reconcile", `{"posting_ids":["`+bankPosting+`"],"status":"cleared"}`)
	if isErr {
		t.Fatalf("reconcile isError: %v", rc)
	}
	txns := rc["transactions"].([]any)
	if len(txns) != 1 {
		t.Fatalf("reconcile affected = %v", txns)
	}

	// balance: whole ledger sums to zero.
	bal, _ := callTool(t, h, "ledger_balance", `{}`)
	if bal["total"].(float64) != 0 {
		t.Errorf("whole-ledger total = %v, want 0", bal["total"])
	}

	// register for the customer.
	reg, _ := callTool(t, h, "ledger_register", `{"query":"Assets:Receivable:Acme"}`)
	if lines := reg["lines"].([]any); len(lines) != 1 {
		t.Errorf("register lines = %v", lines)
	}

	// reverse the transaction.
	rev, isErr := callTool(t, h, "ledger_reverse", `{"id":"`+txnID+`"}`)
	if isErr {
		t.Fatalf("reverse isError: %v", rev)
	}
	if rev["reverses_id"] != txnID {
		t.Errorf("reverse reverses_id = %v", rev["reverses_id"])
	}
	// Double reverse blocked.
	_, isErr = callTool(t, h, "ledger_reverse", `{"id":"`+txnID+`"}`)
	if !isErr {
		t.Error("expected already_reversed error on double reverse")
	}
}

func TestRecord_ErrorsSurfaceAsToolErrors(t *testing.T) {
	h := newHandler(t)

	// Unknown root → bad_root.
	p, isErr := callTool(t, h, "ledger_record", `{"date":"2026-06-01","description":"x","postings":[{"account":"Bogus:Acct","amount_cents":1},{"account":"Assets:Bank","amount_cents":-1}]}`)
	if !isErr || errCode(p) != "bad_root" {
		t.Errorf("bad root: isErr=%v payload=%v", isErr, p)
	}

	// Unbalanced explicit postings → unbalanced.
	p, isErr = callTool(t, h, "ledger_record", `{"date":"2026-06-01","description":"x","postings":[{"account":"Assets:Bank","amount_cents":5000},{"account":"Income:Hosting","amount_cents":-4000}]}`)
	if !isErr || errCode(p) != "unbalanced" {
		t.Errorf("unbalanced: isErr=%v payload=%v", isErr, p)
	}

	// Fewer than two postings → validation.
	p, isErr = callTool(t, h, "ledger_record", `{"date":"2026-06-01","description":"x","postings":[{"account":"Assets:Bank","amount_cents":0}]}`)
	if !isErr || errCode(p) != "validation" {
		t.Errorf("one-posting: isErr=%v payload=%v", isErr, p)
	}

	// Bad date → validation.
	p, isErr = callTool(t, h, "ledger_record", `{"date":"2026-6-1","description":"x","postings":[{"account":"Assets:Bank","amount_cents":1},{"account":"Income:Hosting","amount_cents":-1}]}`)
	if !isErr || errCode(p) != "validation" {
		t.Errorf("bad date: isErr=%v payload=%v", isErr, p)
	}

	// Get of a missing id → not_found.
	p, isErr = callTool(t, h, "ledger_get", `{"id":"NOPE"}`)
	if !isErr || errCode(p) != "not_found" {
		t.Errorf("not_found: isErr=%v payload=%v", isErr, p)
	}
}

func TestBalance_PeriodBucketAndRange(t *testing.T) {
	h := newHandler(t)
	mustRecord(t, h, `{"date":"2026-06-15","description":"june","postings":[{"account":"Expenses:Office","amount_cents":1000},{"account":"Assets:Bank","amount_cents":-1000}]}`)
	mustRecord(t, h, `{"date":"2026-07-15","description":"july","postings":[{"account":"Expenses:Office","amount_cents":2000},{"account":"Assets:Bank","amount_cents":-2000}]}`)

	// Bucket "2026-06" → only June's 1000.
	bal, _ := callTool(t, h, "ledger_balance", `{"query":"Expenses","period":"2026-06"}`)
	if total := bal["total"].(float64); total != 1000 {
		t.Errorf("June expenses total = %v, want 1000", total)
	}
	// Range covering both months.
	bal, _ = callTool(t, h, "ledger_balance", `{"query":"Expenses","period":{"from":"2026-06-01","to":"2026-07-31"}}`)
	if total := bal["total"].(float64); total != 3000 {
		t.Errorf("range expenses total = %v, want 3000", total)
	}
}

func mustRecord(t *testing.T, h *Handler, args string) {
	t.Helper()
	if _, isErr := callTool(t, h, "ledger_record", args); isErr {
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
