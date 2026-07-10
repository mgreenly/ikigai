package mcp

import (
	"context"
	"database/sql"
	"encoding/base64"
	"encoding/json"
	"io"
	"log/slog"
	"net/http"
	"net/http/httptest"
	"strings"
	"testing"

	appkitdatabase "appkit/db"
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
	return handler
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

// callTool invokes tools/call and returns the decoded JSON text payload plus the
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

func TestToolsListComposesDropboxToolsWithChassisTools(t *testing.T) {
	// R-QQJT-LKCV
	h := newHandler(t)
	res := rpc(t, h, "tools/list", `{}`)
	tools, _ := res["tools"].([]any)
	if len(tools) != 4 {
		t.Fatalf("tools/list returned %d tools, want exactly 4: %v", len(tools), tools)
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
	for _, want := range []string{"health", "reflection", "list", "get"} {
		if !names[want] {
			t.Errorf("tools/list missing %q (got %v)", want, names)
		}
	}
	for name := range names {
		switch name {
		case "health", "reflection", "list", "get":
		default:
			t.Errorf("unexpected tool %q in tools/list: %v", name, names)
		}
	}
}

// TestReflection covers the reflection tool: the no-arg index
// (the three published file.* types, empty subscribes — dropbox is a producer),
// the event_type detail (schema + example), and the corrective error for an
// unknown type.
func TestReflection(t *testing.T) {
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
		got[p["type"].(string)] = true
		if p["description"] == "" {
			t.Errorf("published type %v has empty description", p["type"])
		}
	}
	for _, want := range []string{"file.created", "file.modified", "file.deleted"} {
		if !got[want] {
			t.Errorf("publishes missing %q (got %v)", want, got)
		}
	}
	if len(publishes) != 3 {
		t.Errorf("expected exactly 3 published types, got %d: %v", len(publishes), publishes)
	}

	// dropbox is a producer: subscribes is present and empty.
	subscribes, ok := idx["subscribes"].([]any)
	if !ok {
		t.Fatalf("reflection index missing subscribes array: %v", idx)
	}
	if len(subscribes) != 0 {
		t.Fatalf("expected empty subscribes for dropbox, got %v", subscribes)
	}

	// event_type → the publish detail (schema + example).
	detail, isErr := callTool(t, h, "reflection", `{"event_type":"file.created"}`)
	if isErr {
		t.Fatalf("reflection detail isError: %v", detail)
	}
	if detail["type"] != "file.created" {
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

	// Unknown event_type -> chassis error envelope naming the unknown and known types.
	msg, isErr := callToolText(t, h, "reflection", `{"event_type":"file.nope"}`)
	if !isErr {
		t.Fatalf("expected error for unknown event_type, got %q", msg)
	}
	for _, want := range []string{"file.nope", "known types", "file.created", "file.modified", "file.deleted"} {
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
	em := p["error"].(map[string]any)
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
	em := p["error"].(map[string]any)
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
	em := p["error"].(map[string]any)
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
	em := p["error"].(map[string]any)
	if em["code"] != "validation" {
		t.Errorf("code = %v, want validation", em["code"])
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
