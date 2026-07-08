package mcp

import (
	"bytes"
	"context"
	"encoding/json"
	"errors"
	"go/parser"
	"go/token"
	"net/http"
	"net/http/httptest"
	"os"
	"path/filepath"
	"runtime"
	"strconv"
	"strings"
	"testing"

	sqlkit "appkit/db"
	appkitmcp "appkit/mcp"

	"sites/internal/db"
	"sites/internal/sites"
)

const (
	testOwner    = "owner@example.com"
	testClientID = "client-123"
	testVersion  = "test-1.2.3"
	testService  = "sites"
	testBaseURL  = "https://int.ikigenba.com/srv/sites/"
)

type testHandler struct {
	http.Handler
	store  *sites.Store
	layout sites.Layout
}

// newTestHandler stands up a temp DB (migrated) and a temp SITES_ROOT, returning
// a wired appkit MCP handler plus the root for filesystem assertions.
func newTestHandler(t *testing.T, mirror ...sites.MirrorClient) (*testHandler, string) {
	t.Helper()
	root := t.TempDir()
	dbPath := filepath.Join(t.TempDir(), "sites.db")
	conn, err := sqlkit.Open(dbPath)
	if err != nil {
		t.Fatalf("open db: %v", err)
	}
	t.Cleanup(func() { conn.Close() })
	migs, err := sqlkit.LoadMigrations(db.FS, "migrations")
	if err != nil {
		t.Fatalf("load migrations: %v", err)
	}
	if err := sqlkit.Migrate(context.Background(), conn, migs); err != nil {
		t.Fatalf("migrate: %v", err)
	}
	layout := sites.NewLayout(root)
	store := sites.NewStoreWithLayout(conn, layout)
	var mc sites.MirrorClient
	if len(mirror) > 0 {
		mc = mirror[0]
	}
	handler, err := appkitmcp.New(appkitmcp.Options{
		Service:      testService,
		Version:      testVersion,
		Instructions: Instructions,
		Tools:        Tools(store, layout, testBaseURL, mc),
	})
	if err != nil {
		t.Fatalf("new mcp handler: %v", err)
	}
	return &testHandler{Handler: handler, store: store, layout: layout}, root
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
	IsError bool `json:"isError"`
	Content []struct {
		Type string `json:"type"`
		Text string `json:"text"`
	} `json:"content"`
}

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

func call(t *testing.T, h http.Handler, name string, args any) toolResult {
	t.Helper()
	resp := rpc(t, h, "tools/call", map[string]any{"name": name, "arguments": args})
	var tr toolResult
	if err := json.Unmarshal(resp.Result, &tr); err != nil {
		t.Fatalf("decode tool result for %s: %v (result=%s)", name, err, resp.Result)
	}
	return tr
}

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
	return m
}

func callErr(t *testing.T, h http.Handler, name string, args any) map[string]any {
	t.Helper()
	tr := call(t, h, name, args)
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

func moduleRoot(t *testing.T) string {
	t.Helper()
	_, file, _, ok := runtime.Caller(0)
	if !ok {
		t.Fatal("runtime.Caller failed")
	}
	return filepath.Clean(filepath.Join(filepath.Dir(file), "..", ".."))
}

func TestNoAgentkitImportsRemain(t *testing.T) {
	root := moduleRoot(t)
	fset := token.NewFileSet()

	// R-0FMU-J775
	err := filepath.WalkDir(root, func(path string, d os.DirEntry, err error) error {
		if err != nil {
			return err
		}
		if d.IsDir() {
			switch d.Name() {
			case ".git", "vendor":
				return filepath.SkipDir
			}
			return nil
		}
		if filepath.Ext(path) != ".go" {
			return nil
		}
		file, err := parser.ParseFile(fset, path, nil, parser.ImportsOnly)
		if err != nil {
			return err
		}
		for _, imp := range file.Imports {
			importPath, err := strconv.Unquote(imp.Path.Value)
			if err != nil {
				return err
			}
			if importPath == "agentkit" || strings.HasPrefix(importPath, "agentkit/") {
				t.Errorf("%s imports %q", path, importPath)
			}
		}
		return nil
	})
	if err != nil {
		t.Fatalf("scan go imports: %v", err)
	}
}

func TestGoModHasNoAgentkitWiring(t *testing.T) {
	raw, err := os.ReadFile(filepath.Join(moduleRoot(t), "go.mod"))
	if err != nil {
		t.Fatalf("read go.mod: %v", err)
	}
	lines := strings.Split(string(raw), "\n")

	// R-0GUQ-WYXU
	for _, line := range lines {
		trimmed := strings.TrimSpace(line)
		if strings.HasPrefix(trimmed, "agentkit ") || strings.HasPrefix(trimmed, "require agentkit ") {
			t.Fatalf("go.mod still contains an agentkit require line: %q", line)
		}
		if strings.HasPrefix(trimmed, "replace agentkit =>") {
			t.Fatalf("go.mod still contains an agentkit replace line: %q", line)
		}
	}
}

// TestToolsList asserts tools/list returns exactly the lifecycle set with the
// correct prefixed names and well-formed descriptors.
func TestToolsList(t *testing.T) {
	h, _ := newTestHandler(t)
	resp := rpc(t, h, "tools/list", nil)

	var result struct {
		Tools []struct {
			Name        string         `json:"name"`
			Description string         `json:"description"`
			InputSchema map[string]any `json:"inputSchema"`
		} `json:"tools"`
	}
	if err := json.Unmarshal(resp.Result, &result); err != nil {
		t.Fatalf("decode tools/list: %v", err)
	}

	got := map[string]bool{}
	for _, tl := range result.Tools {
		got[tl.Name] = true
		if tl.Description == "" {
			t.Errorf("tool %q has an empty description", tl.Name)
		}
		if tl.InputSchema == nil || tl.InputSchema["type"] != "object" {
			t.Errorf("tool %q inputSchema is not an object schema: %v", tl.Name, tl.InputSchema)
		}
	}

	want := []string{
		"health",
		"reflection",
		"describe",
		"create",
		"list",
		"delete",
		"mkdir",
		"set_visibility",
		"sync",
		"file_write",
		"file_read",
		"file_edit",
		"file_glob",
		"file_grep",
		"file_list",
	}
	wantSet := map[string]bool{}
	for _, name := range want {
		wantSet[name] = true
		if !got[name] {
			t.Errorf("missing expected tool %q: %+v", name, result.Tools)
		}
	}
	// R-0KIG-2A5X
	for name := range got {
		if !wantSet[name] {
			t.Errorf("unexpected tool %q: %+v", name, result.Tools)
		}
	}
	// R-0UUY-N97T
	if len(result.Tools) != len(want) {
		t.Errorf("tools/list returned %d tools, want %d: %+v", len(result.Tools), len(want), result.Tools)
	}
	// R-RDBZ-AE4J
	if got["publish"] || got["unpublish"] {
		t.Fatalf("tools/list must not expose publish/unpublish after the visibility switch: %+v", result.Tools)
	}
}

func TestReflectionWithoutEventGraphReturnsEmptySets(t *testing.T) {
	h, _ := newTestHandler(t)

	// R-P21E-0285
	reflected := callOK(t, h, "reflection", nil)
	publishes, ok := reflected["publishes"].([]any)
	if !ok {
		t.Fatalf("publishes is not an array: %+v", reflected)
	}
	if len(publishes) != 0 {
		t.Fatalf("publishes = %+v, want empty array", publishes)
	}
	subscribes, ok := reflected["subscribes"].([]any)
	if !ok {
		t.Fatalf("subscribes is not an array: %+v", reflected)
	}
	if len(subscribes) != 0 {
		t.Fatalf("subscribes = %+v, want empty array", subscribes)
	}
}

// TestHealth covers the auth-proof envelope: identity from headers + the fixed
// envelope keys.
func TestHealth(t *testing.T) {
	h, _ := newTestHandler(t)
	env := callOK(t, h, "health", map[string]any{})
	if env["owner_email"] != testOwner {
		t.Errorf("owner_email = %v, want %v", env["owner_email"], testOwner)
	}
	if env["client_id"] != testClientID {
		t.Errorf("client_id = %v, want %v", env["client_id"], testClientID)
	}
	if env["service"] != testService {
		t.Errorf("service = %v, want %v", env["service"], testService)
	}
}

// TestCreateThenList is the end-to-end-ish happy path: create makes the row + the
// working dir, list shows it.
func TestCreateThenList(t *testing.T) {
	h, root := newTestHandler(t)

	created := callOK(t, h, "create", map[string]any{"name": "demo"})
	if created["name"] != "demo" {
		t.Fatalf("create returned %+v", created)
	}
	if created["public"] != false {
		t.Errorf("new site should be private: %+v", created)
	}
	if want := testBaseURL + "private/demo/"; created["url"] != want {
		t.Errorf("create url = %v, want %v", created["url"], want)
	}
	if created["created_by"] != testOwner {
		t.Errorf("create created_by = %v, want %v", created["created_by"], testOwner)
	}
	privateDir := filepath.Join(root, sites.PrivateSeg, "demo")
	if fi, err := os.Stat(privateDir); err != nil || !fi.IsDir() {
		t.Fatalf("private dir not created at %s: %v", privateDir, err)
	}

	listed := callOK(t, h, "list", map[string]any{})
	arr, ok := listed["sites"].([]any)
	if !ok || len(arr) != 1 {
		t.Fatalf("list should show one site: %+v", listed)
	}
	if arr[0].(map[string]any)["name"] != "demo" {
		t.Errorf("list entry = %+v", arr[0])
	}
	// R-RFRS-1XLX
	if arr[0].(map[string]any)["created_by"] != testOwner {
		t.Fatalf("list created_by = %v, want %v", arr[0].(map[string]any)["created_by"], testOwner)
	}
}

// TestCreateBadSlug asserts an invalid slug yields an MCP error result (not a
// transport error) with the stable code.
func TestCreateBadSlug(t *testing.T) {
	h, _ := newTestHandler(t)
	e := callErr(t, h, "create", map[string]any{"name": "Bad Slug!"})
	if e["code"] != "invalid_slug" {
		t.Fatalf("expected invalid_slug, got %+v", e)
	}
}

func TestSetVisibilityMovesBetweenPublicAndPrivate(t *testing.T) {
	h, _ := newTestHandler(t)
	callOK(t, h, "create", map[string]any{"name": "demo"})
	if err := os.WriteFile(filepath.Join(h.layout.SiteDir(false, "demo"), "index.html"), []byte("hello"), 0o644); err != nil {
		t.Fatalf("seed private file: %v", err)
	}

	pub := callOK(t, h, "set_visibility", map[string]any{"name": "demo", "public": true})
	if want := testBaseURL + "public/demo/"; pub["url"] != want {
		t.Errorf("public visibility url = %v, want %v", pub["url"], want)
	}
	site, err := h.store.Get(context.Background(), "demo")
	if err != nil {
		t.Fatalf("get public site: %v", err)
	}
	if !site.Public {
		t.Fatalf("stored Public = false, want true")
	}
	if _, err := os.Stat(filepath.Join(h.layout.SiteDir(true, "demo"), "index.html")); err != nil {
		t.Fatalf("public dir should contain moved file: %v", err)
	}
	if _, err := os.Stat(h.layout.SiteDir(false, "demo")); !os.IsNotExist(err) {
		t.Fatalf("private dir should be gone after public move: %v", err)
	}

	pvt := callOK(t, h, "set_visibility", map[string]any{"name": "demo", "public": false})
	if want := testBaseURL + "private/demo/"; pvt["url"] != want {
		t.Errorf("private visibility url = %v, want %v", pvt["url"], want)
	}
	site, err = h.store.Get(context.Background(), "demo")
	if err != nil {
		t.Fatalf("get private site: %v", err)
	}
	if site.Public {
		t.Fatalf("stored Public = true, want false")
	}
	if _, err := os.Stat(filepath.Join(h.layout.SiteDir(false, "demo"), "index.html")); err != nil {
		t.Fatalf("private dir should contain moved file: %v", err)
	}
	if _, err := os.Stat(h.layout.SiteDir(true, "demo")); !os.IsNotExist(err) {
		t.Fatalf("public dir should be gone after private move: %v", err)
	}

	// R-RGZO-FPCM
}

// TestDelete covers deleting the current visibility directory and row.
func TestDelete(t *testing.T) {
	h, _ := newTestHandler(t)
	callOK(t, h, "create", map[string]any{"name": "demo"})
	callOK(t, h, "set_visibility", map[string]any{"name": "demo", "public": true})
	if err := os.WriteFile(filepath.Join(h.layout.SiteDir(true, "demo"), "index.html"), []byte("hello"), 0o644); err != nil {
		t.Fatalf("seed public file: %v", err)
	}

	del := callOK(t, h, "delete", map[string]any{"name": "demo"})
	if del["deleted"] != "demo" {
		t.Fatalf("delete returned %+v", del)
	}
	if _, err := h.store.Get(context.Background(), "demo"); !errors.Is(err, sites.ErrNotFound) {
		t.Fatalf("store.Get after delete err = %v, want ErrNotFound", err)
	}
	if _, err := os.Stat(h.layout.SiteDir(true, "demo")); !os.IsNotExist(err) {
		t.Errorf("public dir should be removed: %v", err)
	}
	listed := callOK(t, h, "list", map[string]any{})
	if arr, _ := listed["sites"].([]any); len(arr) != 0 {
		t.Errorf("list should be empty after delete: %+v", listed)
	}
	// R-RJFH-78U0
}

// TestMkdirConfinement covers a valid nested mkdir and rejects an escape.
func TestMkdirConfinement(t *testing.T) {
	h, root := newTestHandler(t)
	callOK(t, h, "create", map[string]any{"name": "demo"})

	callOK(t, h, "mkdir", map[string]any{"name": "demo", "path": "a/b/c"})
	if fi, err := os.Stat(filepath.Join(root, sites.PrivateSeg, "demo", "a", "b", "c")); err != nil || !fi.IsDir() {
		t.Fatalf("nested dir not created: %v", err)
	}

	e := callErr(t, h, "mkdir", map[string]any{"name": "demo", "path": "../../escape"})
	if e["code"] != "path_escapes_working_dir" {
		t.Fatalf("expected path_escapes_working_dir, got %+v", e)
	}
	e2 := callErr(t, h, "mkdir", map[string]any{"name": "demo", "path": "/etc/evil"})
	if e2["code"] != "path_escapes_working_dir" {
		t.Fatalf("expected path_escapes_working_dir for absolute, got %+v", e2)
	}
}
