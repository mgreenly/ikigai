package mcp

import (
	"context"
	"database/sql"
	"encoding/base64"
	"encoding/json"
	"errors"
	"io"
	"log/slog"
	"net/http"
	"net/http/httptest"
	"net/url"
	"strconv"
	"strings"
	"testing"

	appkitdatabase "appkit/db"
	appkitmcp "appkit/mcp"
	"appkit/server"

	"dropbox/internal/db"
	"dropbox/internal/dropbox"

	_ "modernc.org/sqlite"
)

func discardLogger() *slog.Logger {
	return slog.New(slog.NewJSONHandler(io.Discard, nil))
}

func newHandler(t *testing.T) http.Handler {
	t.Helper()
	conn, err := sql.Open("sqlite", ":memory:")
	if err != nil {
		t.Fatalf("open: %v", err)
	}
	if _, err := conn.Exec(`PRAGMA foreign_keys = ON`); err != nil {
		t.Fatalf("fk: %v", err)
	}
	t.Cleanup(func() { conn.Close() })
	if err := migrateDropbox(context.Background(), conn); err != nil {
		t.Fatalf("migrate: %v", err)
	}
	return newHandlerWithService(t, dropbox.NewService(conn), nil)
}

func migrateDropbox(ctx context.Context, conn *sql.DB) error {
	migs, err := appkitdatabase.LoadMigrations(db.FS, "migrations")
	if err != nil {
		return err
	}
	return appkitdatabase.Migrate(ctx, conn, migs)
}

func newHandlerWithService(t testing.TB, svc *dropbox.Service, health func(context.Context) (map[string]any, error)) http.Handler {
	return newHandlerWithSourceAllowed(t, svc, health, func(int) bool { return false })
}

func newHandlerWithSourceAllowed(t testing.TB, svc *dropbox.Service, health func(context.Context) (map[string]any, error), sourcePortAllowed func(int) bool) http.Handler {
	t.Helper()
	var handler http.Handler
	_, err := server.New(server.Options{
		Addr:       "127.0.0.1:0",
		Logger:     discardLogger(),
		ResourceID: "https://int.ikigenba.com/srv/dropbox/mcp",
		AuthServer: "https://int.ikigenba.com",
		Version:    "v-test",
		Service:    "dropbox",
		Health:     health,
		Events:     dropbox.Events,
		Register: func(rt *server.Router) error {
			var err error
			handler, err = NewHandler(svc, sourcePortAllowed, rt)
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

type recordedEventSink struct {
	events []dropbox.FileEvent
}

func (s *recordedEventSink) AppendFileEvent(_ *sql.Tx, event dropbox.FileEvent) error {
	s.events = append(s.events, event)
	return nil
}

func (s *recordedEventSink) Ring() {}

func sourcePort(t *testing.T, rawURL string) int {
	t.Helper()
	u, err := url.Parse(rawURL)
	if err != nil {
		t.Fatalf("parse source URL: %v", err)
	}
	p, err := strconv.Atoi(u.Port())
	if err != nil {
		t.Fatalf("source URL port: %v", err)
	}
	return p
}

// rpc drives one JSON-RPC call through ServeHTTP and returns the decoded result
// object. params is the raw JSON for "params".
func rpc(t *testing.T, h http.Handler, method, params string) map[string]any {
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
func callToolText(t *testing.T, h http.Handler, name, args string) (string, bool) {
	t.Helper()
	res := rpc(t, h, "tools/call", `{"name":"`+name+`","arguments":`+args+`}`)
	isErr, _ := res["isError"].(bool)
	content, ok := res["content"].([]any)
	if !ok || len(content) == 0 {
		t.Fatalf("%s: no content: %v", name, res)
	}
	return content[0].(map[string]any)["text"].(string), isErr
}

// callTool invokes tools/call and returns its structured payload plus the
// isError flag.
func callTool(t *testing.T, h http.Handler, name, args string) (map[string]any, bool) {
	t.Helper()
	res := rpc(t, h, "tools/call", `{"name":"`+name+`","arguments":`+args+`}`)
	payload, ok := res["structuredContent"].(map[string]any)
	if !ok {
		t.Fatalf("%s: missing structuredContent object: %v", name, res)
	}
	isErr, _ := res["isError"].(bool)
	return payload, isErr
}

func TestToolsListComposesDropboxToolsWithChassisTools(t *testing.T) {
	// R-QQJT-LKCV
	// R-KRXK-IQE2
	h := newHandler(t)
	res := rpc(t, h, "tools/list", `{}`)
	tools, _ := res["tools"].([]any)
	if len(tools) != 8 {
		t.Fatalf("tools/list returned %d tools, want exactly 8: %v", len(tools), tools)
	}
	names := map[string]bool{}
	for _, tl := range tools {
		tool := tl.(map[string]any)
		name := tool["name"].(string)
		if names[name] {
			t.Fatalf("duplicate tool %q in tools/list: %v", name, tools)
		}
		names[name] = true
		if tool["description"] == "" {
			t.Errorf("tool %q has empty description", name)
		}
		schema, _ := tool["inputSchema"].(map[string]any)
		if schema == nil || schema["type"] != "object" {
			t.Errorf("tool %q inputSchema is not an object schema: %v", name, tool["inputSchema"])
		}
	}
	for _, want := range []string{"health", "reflection", "list", "get", "put", "mkdir", "delete", "move"} {
		if !names[want] {
			t.Errorf("tools/list missing %q (got %v)", want, names)
		}
	}
	for name := range names {
		switch name {
		case "health", "reflection", "list", "get", "put", "mkdir", "delete", "move":
		default:
			t.Errorf("unexpected tool %q in tools/list: %v", name, names)
		}
	}
}

func TestDomainToolsReturnMatchingStructuredAndTextResults(t *testing.T) {
	// R-7PKS-A5KE
	h, _ := newMirrorHandler(t)
	calls := []struct {
		name string
		args string
	}{
		{name: "put", args: `{"path":"/a.txt","content_base64":"YQ=="}`},
		{name: "get", args: `{"path":"/a.txt"}`},
		{name: "list", args: `{}`},
		{name: "mkdir", args: `{"path":"/work"}`},
		{name: "move", args: `{"from":"/a.txt","to":"/b.txt"}`},
		{name: "delete", args: `{"path":"/b.txt"}`},
	}
	for _, tc := range calls {
		t.Run(tc.name, func(t *testing.T) {
			res := rpc(t, h, "tools/call", `{"name":"`+tc.name+`","arguments":`+tc.args+`}`)
			if isErr, _ := res["isError"].(bool); isErr {
				t.Fatalf("result isError: %v", res)
			}
			structured, ok := res["structuredContent"].(map[string]any)
			if !ok {
				t.Fatalf("structuredContent is not an object: %v", res)
			}
			content, ok := res["content"].([]any)
			if !ok || len(content) != 1 {
				t.Fatalf("content = %v, want one text block", res["content"])
			}
			block, ok := content[0].(map[string]any)
			if !ok || block["type"] != "text" {
				t.Fatalf("content[0] = %v, want text block", content[0])
			}
			wantText, err := json.Marshal(structured)
			if err != nil {
				t.Fatal(err)
			}
			if block["text"] != string(wantText) {
				t.Fatalf("text = %q, want identical structured JSON %q", block["text"], wantText)
			}
		})
	}
}

func TestDomainToolOutputSchemasMatchEmittedKeys(t *testing.T) {
	// R-7QSO-NXB3
	h := newHandler(t)
	res := rpc(t, h, "tools/list", `{}`)
	descriptors := map[string]map[string]any{}
	for _, raw := range res["tools"].([]any) {
		d := raw.(map[string]any)
		descriptors[d["name"].(string)] = d
	}
	wants := map[string][]string{
		"list":   {"files"},
		"get":    {"path", "size", "content_hash", "rev", "updated_at", "content_base64"},
		"put":    {"path", "size", "content_hash", "rev"},
		"mkdir":  {"path"},
		"delete": {"removed"},
		"move":   {"from", "to"},
	}
	for name, want := range wants {
		schema, ok := descriptors[name]["outputSchema"].(map[string]any)
		if !ok || schema["type"] != "object" {
			t.Fatalf("%s outputSchema = %v, want object", name, descriptors[name]["outputSchema"])
		}
		assertStringSet(t, name+" required", schema["required"], want)
	}

	listSchema := descriptors["list"]["outputSchema"].(map[string]any)
	props := listSchema["properties"].(map[string]any)
	files := props["files"].(map[string]any)
	if files["type"] != "array" {
		t.Fatalf("list files schema = %v, want array", files)
	}
	item := files["items"].(map[string]any)
	if item["type"] != "object" {
		t.Fatalf("list files item schema = %v, want object", item)
	}
	assertStringSet(t, "list file required", item["required"], []string{"path", "kind", "size", "hash", "rev", "updated_at"})
}

func assertStringSet(t *testing.T, label string, got any, want []string) {
	t.Helper()
	values, ok := got.([]any)
	if !ok {
		t.Fatalf("%s = %T(%v), want array", label, got, got)
	}
	seen := map[string]bool{}
	for _, value := range values {
		s, ok := value.(string)
		if !ok || seen[s] {
			t.Fatalf("%s contains invalid or duplicate value %v", label, value)
		}
		seen[s] = true
	}
	if len(seen) != len(want) {
		t.Fatalf("%s = %v, want exactly %v", label, seen, want)
	}
	for _, value := range want {
		if !seen[value] {
			t.Fatalf("%s = %v, missing %q", label, seen, value)
		}
	}
}

func TestDomainToolErrorsUseFlatTypedChassisEnvelope(t *testing.T) {
	// R-7S0L-1P1S
	h, _ := newMirrorHandler(t)
	res := rpc(t, h, "tools/call", `{"name":"get","arguments":{"path":"/missing"}}`)
	if res["isError"] != true {
		t.Fatalf("isError = %v, want true: %v", res["isError"], res)
	}
	structured, ok := res["structuredContent"].(map[string]any)
	if !ok || len(structured) != 2 || structured["code"] != "not_found" {
		t.Fatalf("structuredContent = %v, want flat not_found code and message", res["structuredContent"])
	}
	if _, nested := structured["error"]; nested {
		t.Fatalf("structuredContent retained nested error body: %v", structured)
	}
	message, ok := structured["message"].(string)
	if !ok || message == "" {
		t.Fatalf("error message = %v, want non-empty string", structured["message"])
	}
	content := res["content"].([]any)
	block := content[0].(map[string]any)
	if block["type"] != "text" || block["text"] != message {
		t.Fatalf("content[0] = %v, want human message %q", block, message)
	}
}

func TestToolErrMapsDomainSentinelsToClosedCodes(t *testing.T) {
	// R-7T8H-FGSH
	cases := []struct {
		name string
		err  error
		want appkitmcp.ErrorCode
	}{
		{name: "not found", err: dropbox.ErrNotFound, want: appkitmcp.ErrNotFound},
		{name: "revision mismatch", err: dropbox.ErrRevMismatch, want: appkitmcp.ErrConflict},
		{name: "validation", err: dropbox.ErrValidation, want: appkitmcp.ErrValidation},
		{name: "path escape", err: dropbox.ErrPathEscape, want: appkitmcp.ErrValidation},
		{name: "other", err: errors.New("transport detail"), want: appkitmcp.ErrInternal},
	}
	for _, tc := range cases {
		t.Run(tc.name, func(t *testing.T) {
			result := toolErr(tc.err)
			if result["isError"] != true {
				t.Fatalf("isError = %v, want true", result["isError"])
			}
			structured := result["structuredContent"].(map[string]any)
			if structured["code"] != tc.want || structured["message"] != tc.err.Error() {
				t.Fatalf("structuredContent = %v, want code %q and sentinel message", structured, tc.want)
			}
		})
	}
}

// TestReflection covers the reflection tool: the no-arg index
// (the three published families, empty subscribes — dropbox is a producer),
// the kind detail (schema + example), and the corrective error for an unknown
// kind.
func TestReflection(t *testing.T) {
	// R-KQPO-4YND
	// R-QCDP-UD1V
	h := newHandler(t)

	// No-arg → the index {publishes, subscribes}.
	idx, isErr := callTool(t, h, "reflection", `{}`)
	if isErr {
		t.Fatalf("reflection index isError: %v", idx)
	}
	publishes, ok := idx["publishes"].([]any)
	if !ok {
		t.Fatalf("reflection index missing publishes array: %v", idx)
	}
	got := map[string]bool{}
	for _, pe := range publishes {
		p := pe.(map[string]any)
		got[p["kind"].(string)] = true
		if p["subject"] != "/<mirror path>" {
			t.Errorf("published kind %v subject = %v, want mirror-path description", p["kind"], p["subject"])
		}
		if p["description"] == "" {
			t.Errorf("published kind %v has empty description", p["kind"])
		}
	}
	for _, want := range []string{"create", "modify", "delete"} {
		if !got[want] {
			t.Errorf("publishes missing %q (got %v)", want, got)
		}
	}
	if len(publishes) != 3 {
		t.Errorf("expected exactly 3 published kinds, got %d: %v", len(publishes), publishes)
	}

	// dropbox is a producer: subscribes is present and empty.
	subscribes, ok := idx["subscribes"].([]any)
	if !ok {
		t.Fatalf("reflection index missing subscribes array: %v", idx)
	}
	if len(subscribes) != 0 {
		t.Fatalf("expected empty subscribes for dropbox, got %v", subscribes)
	}

	// kind → each publish detail includes origin in its schema and example.
	for _, kind := range []string{"create", "modify", "delete"} {
		detail, isErr := callTool(t, h, "reflection", `{"kind":"`+kind+`"}`)
		if isErr {
			t.Fatalf("reflection detail for %s isError: %v", kind, detail)
		}
		if detail["kind"] != kind || detail["subject"] != "/<mirror path>" {
			t.Fatalf("detail for %s = %v", kind, detail)
		}
		if detail["description"] == "" {
			t.Fatalf("detail for %s missing description: %v", kind, detail)
		}
		sch, ok := detail["schema"].(map[string]any)
		if !ok || sch["type"] != "object" {
			t.Fatalf("detail schema for %s is not an object schema: %v", kind, detail["schema"])
		}
		properties, ok := sch["properties"].(map[string]any)
		if !ok || properties["origin"] == nil {
			t.Fatalf("detail schema for %s missing origin: %v", kind, sch)
		}
		if properties["event"] != nil {
			t.Fatalf("detail schema for %s unexpectedly has event: %v", kind, sch)
		}
		example, ok := detail["example"].(map[string]any)
		if !ok || example["origin"] != dropbox.OriginDropbox {
			t.Fatalf("detail example for %s origin = %v, want %q", kind, example["origin"], dropbox.OriginDropbox)
		}
		if example["event"] != nil || len(example) != len(properties) {
			t.Fatalf("detail example for %s does not agree with schema: example=%v schema=%v", kind, example, properties)
		}
	}

	// Unknown kind -> chassis error envelope naming the unknown and known kinds.
	msg, isErr := callToolText(t, h, "reflection", `{"kind":"nope"}`)
	if !isErr {
		t.Fatalf("expected error for unknown event_type, got %q", msg)
	}
	for _, want := range []string{"nope", "known kinds", "create", "modify", "delete"} {
		if !strings.Contains(msg, want) {
			t.Errorf("corrective message missing %q: %q", want, msg)
		}
	}
}

func TestHealth_Envelope(t *testing.T) {
	h := newHandler(t)
	p, isErr := callTool(t, h, "health", `{}`)
	if isErr {
		t.Fatal("health isError")
	}
	// Envelope required top-level keys + identity (no reporter here → details {}).
	if p["status"] != "ok" || p["version"] != "v-test" || p["service"] != "dropbox" {
		t.Errorf("health envelope keys = %v", p)
	}
	if p["owner_email"] != "me@example.com" || p["client_id"] != "client-123" {
		t.Errorf("health identity = %v", p)
	}
	d, ok := p["details"].(map[string]any)
	if !ok {
		t.Fatalf("details missing or not an object: %v", p["details"])
	}
	if len(d) != 0 {
		t.Errorf("details = %v, want empty {} with no reporter", d)
	}
}

func TestHealth_ReporterPopulatesDetails(t *testing.T) {
	// Build a Service with a real DB + mirror and wire a Health reporter (the
	// Spec.Health path) so the telemetry lands UNDER details — not splatted at
	// the top level.
	conn, err := sql.Open("sqlite", "file:"+t.TempDir()+"/dropbox.db?_pragma=foreign_keys(ON)")
	if err != nil {
		t.Fatalf("open: %v", err)
	}
	conn.SetMaxOpenConns(1)
	t.Cleanup(func() { conn.Close() })
	if err := migrateDropbox(context.Background(), conn); err != nil {
		t.Fatalf("migrate: %v", err)
	}
	mirror, err := dropbox.NewMirror(t.TempDir())
	if err != nil {
		t.Fatalf("mirror: %v", err)
	}
	svc := dropbox.NewService(conn)
	svc.Mirror = mirror

	// The reporter mirrors cmd/dropbox/main.go's Spec.Health: telemetry only,
	// no identity.
	reporter := func(ctx context.Context) (map[string]any, error) {
		info, err := svc.Health("", "")
		if err != nil {
			return nil, err
		}
		return map[string]any{
			"mirror_bytes":     info.MirrorBytes,
			"disk_free_bytes":  info.DiskFreeBytes,
			"disk_total_bytes": info.DiskTotalBytes,
			"failed_files":     info.FailedFiles,
		}, nil
	}
	h := newHandlerWithService(t, svc, reporter)

	p, isErr := callTool(t, h, "health", `{}`)
	if isErr {
		t.Fatal("health isError")
	}
	if p["owner_email"] != "me@example.com" || p["client_id"] != "client-123" {
		t.Errorf("health identity = %v", p)
	}
	d, ok := p["details"].(map[string]any)
	if !ok {
		t.Fatalf("details missing or not an object: %v", p["details"])
	}
	for _, k := range []string{"mirror_bytes", "disk_free_bytes", "disk_total_bytes", "failed_files"} {
		if _, ok := d[k]; !ok {
			t.Errorf("details missing telemetry field %q (details %v)", k, d)
		}
		// Telemetry must NOT splat at the top level (DECISIONS §3).
		if _, top := p[k]; top {
			t.Errorf("telemetry field %q splatted at top level, want only under details", k)
		}
	}
	// Disk numbers must be plausible/non-zero for a real filesystem.
	if dt, _ := d["disk_total_bytes"].(float64); dt <= 0 {
		t.Errorf("details.disk_total_bytes = %v, want > 0", d["disk_total_bytes"])
	}
	if df, _ := d["disk_free_bytes"].(float64); df <= 0 {
		t.Errorf("details.disk_free_bytes = %v, want > 0", d["disk_free_bytes"])
	}
}

// newMirrorHandler builds a Handler over a Service with a real file-backed DB
// and a real mirror (the bare newHandler has no mirror, so it can't serve `get`
// bytes). It mirrors TestHealth_ReporterPopulatesDetails' wiring.
func newMirrorHandler(t *testing.T) (http.Handler, *dropbox.Service) {
	t.Helper()
	conn, err := sql.Open("sqlite", "file:"+t.TempDir()+"/dropbox.db?_pragma=foreign_keys(ON)")
	if err != nil {
		t.Fatalf("open: %v", err)
	}
	conn.SetMaxOpenConns(1)
	t.Cleanup(func() { conn.Close() })
	if err := migrateDropbox(context.Background(), conn); err != nil {
		t.Fatalf("migrate: %v", err)
	}
	mirror, err := dropbox.NewMirror(t.TempDir())
	if err != nil {
		t.Fatalf("mirror: %v", err)
	}
	svc := dropbox.NewService(conn)
	svc.Mirror = mirror
	h := newHandlerWithService(t, svc, nil)
	return h, svc
}

// seedFile writes bytes to the mirror and upserts the matching index row on its
// own write tx (the same path the sync engine takes, minus the event).
func seedFile(t *testing.T, svc *dropbox.Service, path, rev, hash string, data []byte) {
	t.Helper()
	if _, _, err := svc.Mirror.WriteFrom(path, strings.NewReader(string(data))); err != nil {
		t.Fatalf("mirror WriteFrom %s: %v", path, err)
	}
	tx, err := svc.DB.BeginTx(context.Background(), nil)
	if err != nil {
		t.Fatalf("begin: %v", err)
	}
	if err := svc.Store.UpsertFile(tx, path, rev, hash, int64(len(data)), "2026-06-09T00:00:00Z"); err != nil {
		tx.Rollback()
		t.Fatalf("upsert %s: %v", path, err)
	}
	if err := tx.Commit(); err != nil {
		t.Fatalf("commit %s: %v", path, err)
	}
}

func TestList_OrderPrefixHashAndPagination(t *testing.T) {
	h, svc := newMirrorHandler(t)
	// Seed out of order; a long hash so the 8-char truncation is observable.
	longHash := "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"
	seedFile(t, svc, "/notes/b.md", "rev-b", longHash, []byte("bbb"))
	seedFile(t, svc, "/notes/a.md", "rev-a", longHash, []byte("aaa"))
	seedFile(t, svc, "/notesxyz.md", "rev-x", longHash, []byte("x")) // must NOT match /notes prefix
	seedFile(t, svc, "/top.md", "rev-t", longHash, []byte("t"))

	// Full listing: path_lower ASC over all four.
	all, isErr := callTool(t, h, "list", `{}`)
	if isErr {
		t.Fatalf("list isError: %v", all)
	}
	files := all["files"].([]any)
	if len(files) != 4 {
		t.Fatalf("expected 4 files, got %d: %v", len(files), files)
	}
	gotOrder := []string{}
	for _, f := range files {
		gotOrder = append(gotOrder, f.(map[string]any)["path"].(string))
	}
	want := []string{"/notes/a.md", "/notes/b.md", "/notesxyz.md", "/top.md"}
	for i := range want {
		if gotOrder[i] != want[i] {
			t.Fatalf("order mismatch: got %v want %v", gotOrder, want)
		}
	}
	// 8-char abbreviated hash.
	if hsh := files[0].(map[string]any)["hash"].(string); hsh != "01234567" {
		t.Errorf("hash = %q, want first-8 %q", hsh, "01234567")
	}

	// Prefix scoping: /notes matches a.md + b.md, NOT /notesxyz.md.
	scoped, _ := callTool(t, h, "list", `{"path":"/notes"}`)
	sf := scoped["files"].([]any)
	if len(sf) != 2 {
		t.Fatalf("prefix /notes expected 2 files, got %d: %v", len(sf), sf)
	}
	for _, f := range sf {
		p := f.(map[string]any)["path"].(string)
		if !strings.HasPrefix(p, "/notes/") {
			t.Errorf("prefix leaked non-subtree path %q", p)
		}
	}

	// Pagination round-trip: limit 2 → full page + next_cursor; page 2 stitches.
	page1, _ := callTool(t, h, "list", `{"limit":2}`)
	p1 := page1["files"].([]any)
	if len(p1) != 2 {
		t.Fatalf("page1 expected 2 files, got %d", len(p1))
	}
	cursor, ok := page1["next_cursor"].(string)
	if !ok || cursor == "" {
		t.Fatalf("page1 missing next_cursor (full page): %v", page1)
	}
	page2, _ := callTool(t, h, "list", `{"limit":2,"cursor":"`+cursor+`"}`)
	p2 := page2["files"].([]any)
	if len(p2) != 2 {
		t.Fatalf("page2 expected 2 files, got %d", len(p2))
	}
	// Last page is full (4 total / 2) so next_cursor is present, but a third page
	// is empty and must omit it.
	page3Cursor := page2["next_cursor"].(string)
	page3, _ := callTool(t, h, "list", `{"limit":2,"cursor":"`+page3Cursor+`"}`)
	if len(page3["files"].([]any)) != 0 {
		t.Fatalf("page3 expected empty, got %v", page3["files"])
	}
	if _, present := page3["next_cursor"]; present {
		t.Errorf("empty page must omit next_cursor, got %v", page3["next_cursor"])
	}
	// Stitched union covers all four with no overlap.
	seen := map[string]bool{}
	for _, f := range append(append([]any{}, p1...), p2...) {
		seen[f.(map[string]any)["path"].(string)] = true
	}
	for _, w := range want {
		if !seen[w] {
			t.Errorf("pagination dropped %q (seen %v)", w, seen)
		}
	}
}

func TestGet_HappyPath(t *testing.T) {
	h, svc := newMirrorHandler(t)
	body := []byte("hello dropbox bytes")
	fullHash := "abc123def456abc123def456abc123def456abc123def456abc123def4560000"
	seedFile(t, svc, "/notes/a.md", "rev-7", fullHash, body)

	// Case-insensitive resolution through the index.
	p, isErr := callTool(t, h, "get", `{"path":"/NOTES/A.md"}`)
	if isErr {
		t.Fatalf("get isError: %v", p)
	}
	if p["path"] != "/notes/a.md" {
		t.Errorf("path = %v, want canonical display /notes/a.md", p["path"])
	}
	if p["content_hash"] != fullHash {
		t.Errorf("content_hash = %v, want full %q", p["content_hash"], fullHash)
	}
	if p["rev"] != "rev-7" {
		t.Errorf("rev = %v", p["rev"])
	}
	if p["updated_at"] != "2026-06-09T00:00:00Z" {
		t.Errorf("updated_at = %v", p["updated_at"])
	}
	b64 := p["content_base64"].(string)
	dec, err := base64.StdEncoding.DecodeString(b64)
	if err != nil {
		t.Fatalf("decode content_base64: %v", err)
	}
	if string(dec) != string(body) {
		t.Errorf("decoded bytes = %q, want %q", dec, body)
	}
}

func TestGet_NotFound(t *testing.T) {
	h, _ := newMirrorHandler(t)
	p, isErr := callTool(t, h, "get", `{"path":"/missing.md"}`)
	if !isErr {
		t.Fatalf("expected isError for unknown path, got %v", p)
	}
	em := p
	if em["code"] != "not_found" {
		t.Errorf("code = %v, want not_found", em["code"])
	}
}

func TestGet_Conflict(t *testing.T) {
	h, svc := newMirrorHandler(t)
	seedFile(t, svc, "/a.md", "rev-current", "hash00000000", []byte("x"))
	p, isErr := callTool(t, h, "get", `{"path":"/a.md","rev":"rev-old"}`)
	if !isErr {
		t.Fatalf("expected isError for rev mismatch, got %v", p)
	}
	em := p
	if em["code"] != "conflict" {
		t.Errorf("code = %v, want conflict", em["code"])
	}
}

func TestGet_TooLarge(t *testing.T) {
	h, svc := newMirrorHandler(t)
	// The guard is post-Content (we hold the bytes, then reject on row.Size), so a
	// faithful oversize case needs real mirror bytes just over the 25 MiB cap.
	big := make([]byte, (25<<20)+1)
	seedFile(t, svc, "/big.bin", "rev-big", "hashbigbigbig", big)
	p, isErr := callTool(t, h, "get", `{"path":"/big.bin"}`)
	if !isErr {
		t.Fatalf("expected isError for oversize, got %v", p)
	}
	em := p
	if em["code"] != "too_large" {
		t.Errorf("code = %v, want too_large", em["code"])
	}
}

func TestGet_MissingPath(t *testing.T) {
	h, _ := newMirrorHandler(t)
	p, isErr := callTool(t, h, "get", `{}`)
	if !isErr {
		t.Fatalf("expected isError for missing path, got %v", p)
	}
	em := p
	if em["code"] != "validation" {
		t.Errorf("code = %v, want validation", em["code"])
	}
}

func TestPut_WritesBase64BytesEnqueuesUploadAndRejectsOversize(t *testing.T) {
	// R-KT5G-WI4R
	h, svc := newMirrorHandler(t)
	body := []byte("MCP writes these exact bytes\n")
	put, isErr := callTool(t, h, "put", `{"path":"/notes/from-mcp.txt","content_base64":"`+base64.StdEncoding.EncodeToString(body)+`"}`)
	if isErr {
		t.Fatalf("put isError: %v", put)
	}
	if put["path"] != "/notes/from-mcp.txt" || put["size"] != float64(len(body)) {
		t.Fatalf("put result = %v, want canonical path and size", put)
	}
	got, isErr := callTool(t, h, "get", `{"path":"/notes/from-mcp.txt"}`)
	if isErr {
		t.Fatalf("get after put isError: %v", got)
	}
	decoded, err := base64.StdEncoding.DecodeString(got["content_base64"].(string))
	if err != nil || string(decoded) != string(body) {
		t.Fatalf("get after put bytes = %q, %v; want %q", decoded, err, body)
	}
	var op, origin string
	if err := svc.DB.QueryRow(`SELECT op, origin FROM upload_queue WHERE path = ?`, "/notes/from-mcp.txt").Scan(&op, &origin); err != nil {
		t.Fatalf("uploaded write not queued: %v", err)
	}
	if op != "put" || origin != "client-123" {
		t.Errorf("upload queue = op=%q origin=%q, want put/client-123", op, origin)
	}

	overstated := base64.StdEncoding.EncodeToString(make([]byte, maxGetBytes+1))
	tooLarge, isErr := callTool(t, h, "put", `{"path":"/too-large.bin","content_base64":"`+overstated+`"}`)
	if !isErr {
		t.Fatalf("oversize put should be an error: %v", tooLarge)
	}
	if code := tooLarge["code"]; code != "too_large" {
		t.Errorf("oversize put code = %v, want too_large", code)
	}
}

func TestPut_SourceURLFetchesServerSideWritesAndPreservesOrigin(t *testing.T) {
	// R-Q52B-JQLP
	body := []byte("bytes held by the source service\n")
	gets := 0
	source := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		if r.Method != http.MethodGet {
			t.Errorf("source method = %s, want GET", r.Method)
		}
		gets++
		_, _ = w.Write(body)
	}))
	defer source.Close()

	h, svc := newMirrorHandler(t)
	// Rebuild the real MCP transport with only this source server's port admitted.
	h = newHandlerWithSourceAllowed(t, svc, nil, func(port int) bool { return port == sourcePort(t, source.URL) })
	sink := &recordedEventSink{}
	svc.Outbox = sink

	put, isErr := callTool(t, h, "put", `{"path":"/references/source.txt","source_url":"`+source.URL+`/bytes"}`)
	if isErr {
		t.Fatalf("source put isError: %v", put)
	}
	if gets != 1 {
		t.Fatalf("source GETs = %d, want 1", gets)
	}
	written, err := svc.Stat("/references/source.txt")
	if err != nil {
		t.Fatalf("stat written source file: %v", err)
	}
	if put["path"] != written.Path || put["size"] != float64(written.Size) || put["content_hash"] != written.ContentHash || put["rev"] != written.Rev {
		t.Fatalf("source put result = %v", put)
	}
	got, isErr := callTool(t, h, "get", `{"path":"/references/source.txt"}`)
	if isErr {
		t.Fatalf("get after source put isError: %v", got)
	}
	decoded, err := base64.StdEncoding.DecodeString(got["content_base64"].(string))
	if err != nil || string(decoded) != string(body) {
		t.Fatalf("source bytes = %q, %v; want %q", decoded, err, body)
	}
	var origin string
	if err := svc.DB.QueryRow(`SELECT origin FROM upload_queue WHERE path = ?`, "/references/source.txt").Scan(&origin); err != nil || origin != "client-123" {
		t.Fatalf("upload origin = %q, %v; want client-123", origin, err)
	}
	if len(sink.events) != 1 || sink.events[0].Origin != "client-123" {
		t.Fatalf("emitted events = %+v, want one event with client origin", sink.events)
	}
}

func TestPut_SourceURLConfinementAndExclusiveArguments(t *testing.T) {
	// R-Q6A7-XICE
	gets := 0
	source := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		gets++
		_, _ = w.Write([]byte("ok"))
	}))
	defer source.Close()
	port := sourcePort(t, source.URL)
	h, svc := newMirrorHandler(t)
	h = newHandlerWithSourceAllowed(t, svc, nil, func(p int) bool { return p == port })
	for _, args := range []string{
		`{"path":"/bad","source_url":"http://10.0.0.5:3202/a"}`,
		`{"path":"/bad","source_url":"http://localhost:` + strconv.Itoa(port) + `/a"}`,
		`{"path":"/bad","source_url":"http://127.0.0.1:39999/a"}`,
		`{"path":"/bad","source_url":"` + source.URL + `/a","content_base64":"eA=="}`,
		`{"path":"/bad"}`,
	} {
		out, isErr := callTool(t, h, "put", args)
		if !isErr || out["code"] != "validation" {
			t.Errorf("put %s = %v, isError=%t; want validation", args, out, isErr)
		}
	}
	if gets != 0 {
		t.Fatalf("rejected source URLs made %d fetches, want none", gets)
	}
	if out, isErr := callTool(t, h, "put", `{"path":"/good","source_url":"`+source.URL+`/a"}`); isErr || out["size"] != float64(2) {
		t.Fatalf("allowed source URL = %v, isError=%t", out, isErr)
	}
	if gets != 1 {
		t.Fatalf("allowed source URL GETs = %d, want 1", gets)
	}
}

func TestPut_SourceURLFailureMappingLeavesNoMutation(t *testing.T) {
	// R-Q8Q0-P1TS
	for _, status := range []int{http.StatusNotFound, http.StatusConflict} {
		t.Run(http.StatusText(status), func(t *testing.T) {
			source := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, _ *http.Request) { w.WriteHeader(status) }))
			defer source.Close()
			h, svc := newMirrorHandler(t)
			h = newHandlerWithSourceAllowed(t, svc, nil, func(p int) bool { return p == sourcePort(t, source.URL) })
			out, isErr := callTool(t, h, "put", `{"path":"/failed.txt","source_url":"`+source.URL+`/missing"}`)
			want := map[int]string{http.StatusNotFound: "not_found", http.StatusConflict: "conflict"}[status]
			if !isErr || out["code"] != want {
				t.Fatalf("source status %d = %v, isError=%t; want %s", status, out, isErr, want)
			}
			assertNoFailedSourceMutation(t, svc)
		})
	}

	listener := httptest.NewUnstartedServer(http.NotFoundHandler())
	listener.Start()
	closedURL := listener.URL
	listener.Close()
	h, svc := newMirrorHandler(t)
	h = newHandlerWithSourceAllowed(t, svc, nil, func(p int) bool { return p == sourcePort(t, closedURL) })
	out, isErr := callTool(t, h, "put", `{"path":"/failed.txt","source_url":"`+closedURL+`/gone"}`)
	if !isErr || out["code"] != "source_unavailable" {
		t.Fatalf("closed source = %v, isError=%t; want source_unavailable", out, isErr)
	}
	assertNoFailedSourceMutation(t, svc)
}

func assertNoFailedSourceMutation(t *testing.T, svc *dropbox.Service) {
	t.Helper()
	if _, err := svc.Content("/failed.txt", nil); !errors.Is(err, dropbox.ErrNotFound) {
		t.Fatalf("failed source left mirror/index entry: %v", err)
	}
	var count int
	if err := svc.DB.QueryRow(`SELECT COUNT(*) FROM upload_queue`).Scan(&count); err != nil || count != 0 {
		t.Fatalf("failed source queue count = %d, %v; want zero", count, err)
	}
}

func TestMkdirDeleteMove_HaveLoopbackWriteSemanticsAndErrorEnvelope(t *testing.T) {
	// R-KUDD-A9VG
	h, _ := newMirrorHandler(t)
	if out, isErr := callTool(t, h, "mkdir", `{"path":"/work/nested"}`); isErr || out["path"] != "/work/nested" {
		t.Fatalf("mkdir result = %v, isError=%t", out, isErr)
	}
	listed, isErr := callTool(t, h, "list", `{"path":"/work"}`)
	if isErr {
		t.Fatalf("list after mkdir isError: %v", listed)
	}
	foundDir := false
	for _, entry := range listed["files"].([]any) {
		item := entry.(map[string]any)
		if item["path"] == "/work/nested" && item["kind"] == "dir" {
			foundDir = true
		}
	}
	if !foundDir {
		t.Fatalf("mkdir directory not listable: %v", listed)
	}
	for _, path := range []string{"/work/nested/a.txt", "/work/nested/deeper/b.txt"} {
		if _, isErr := callTool(t, h, "put", `{"path":"`+path+`","content_base64":"eA=="}`); isErr {
			t.Fatalf("seed put %s failed", path)
		}
	}
	if out, isErr := callTool(t, h, "move", `{"from":"/work/nested/a.txt","to":"/work/nested/moved.txt"}`); isErr || out["to"] != "/work/nested/moved.txt" {
		t.Fatalf("move result = %v, isError=%t", out, isErr)
	}
	if _, isErr := callTool(t, h, "get", `{"path":"/work/nested/moved.txt"}`); isErr {
		t.Fatal("moved file cannot be fetched")
	}
	deleted, isErr := callTool(t, h, "delete", `{"path":"/work"}`)
	if isErr || deleted["removed"].(float64) != 2 {
		t.Fatalf("recursive delete result = %v, isError=%t; want two files", deleted, isErr)
	}
	remaining, isErr := callTool(t, h, "list", `{"path":"/work"}`)
	if isErr || len(remaining["files"].([]any)) != 0 {
		t.Fatalf("recursive delete left entries: %v, isError=%t", remaining, isErr)
	}
	bad, isErr := callTool(t, h, "move", `{"from":"../escape","to":"/safe"}`)
	if !isErr {
		t.Fatalf("invalid move should be an error: %v", bad)
	}
	if code := bad["code"]; code != "validation" {
		t.Errorf("invalid move code = %v, want validation", code)
	}
}

func TestUnknownTool_IsTransportError(t *testing.T) {
	h := newHandler(t)
	body := `{"jsonrpc":"2.0","id":1,"method":"tools/call","params":{"name":"dropbox_bogus","arguments":{}}}`
	req := httptest.NewRequest(http.MethodPost, "/mcp", strings.NewReader(body))
	req.Header.Set("X-Owner-Email", "me@example.com")
	req.Header.Set("X-Client-Id", "client-123")
	rec := httptest.NewRecorder()
	h.ServeHTTP(rec, req)
	var env struct {
		Error map[string]any `json:"error"`
	}
	if err := json.Unmarshal(rec.Body.Bytes(), &env); err != nil {
		t.Fatalf("decode envelope: %v\n%s", err, rec.Body.String())
	}
	if env.Error["code"] != float64(-32602) {
		t.Fatalf("error code = %v, want -32602: %v", env.Error["code"], env.Error)
	}
	if msg, _ := env.Error["message"].(string); !strings.Contains(msg, "unknown tool: dropbox_bogus") {
		t.Errorf("error message = %q", msg)
	}
}
