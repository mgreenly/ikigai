package mcp

import (
	"bytes"
	"context"
	"database/sql"
	"encoding/json"
	"fmt"
	"io"
	"log/slog"
	"net/http"
	"net/http/httptest"
	"os"
	"os/exec"
	"path/filepath"
	"reflect"
	"sort"
	"strings"
	"testing"
	"time"

	appdb "appkit/db"
	"appkit/server"
	"eventplane/outbox"

	reposdb "repos/internal/db"
	"repos/internal/repos"
	"repos/internal/runner"
)

const testOwner = "owner@example.com"

type fixedClock struct{ now time.Time }

func (c fixedClock) Now() time.Time { return c.now }

type tokenSource string

func (s tokenSource) Token(context.Context) (string, error) { return string(s), nil }

type domain struct {
	lifecycle *repos.Service
	store     *repos.Store
	runner    *runner.Runner
	reaper    *repos.Reaper
}

func (d *domain) CloneRepo(ctx context.Context, owner, name string) error {
	return d.lifecycle.CloneRepo(ctx, owner, name)
}
func (d *domain) GetRepo(ctx context.Context, name string) (repos.Repo, error) {
	return d.store.GetRepo(ctx, name)
}
func (d *domain) ListRepos(ctx context.Context, owner string) ([]repos.Repo, error) {
	return d.store.ListRepos(ctx, owner)
}
func (d *domain) DeleteRepo(ctx context.Context, name string) error {
	return d.reaper.DeleteRepo(ctx, name)
}
func (d *domain) Enqueue(ctx context.Context, request runner.SessionRequest) (repos.Session, error) {
	return d.runner.Enqueue(ctx, request)
}
func (d *domain) GetSession(ctx context.Context, id string) (repos.Session, error) {
	return d.store.GetSession(ctx, id)
}
func (d *domain) ListSessions(ctx context.Context, repo, owner string) ([]repos.Session, error) {
	return d.store.ListSessions(ctx, repo, owner)
}
func (d *domain) Cancel(id string) bool { return d.runner.Cancel(id) }

type fixture struct {
	h         http.Handler
	db        *sql.DB
	store     *repos.Store
	runner    *runner.Runner
	stateRoot string
	remoteDir string
}

func newFixture(t *testing.T, factory runner.AgentFactory) *fixture {
	t.Helper()
	root := t.TempDir()
	dbPath := filepath.Join(root, "repos.db")
	conn, err := appdb.Open(dbPath)
	if err != nil {
		t.Fatal(err)
	}
	t.Cleanup(func() { conn.Close() })
	migrations, err := reposdb.Migrations()
	if err != nil {
		t.Fatal(err)
	}
	if err := appdb.Migrate(context.Background(), conn, migrations); err != nil {
		t.Fatal(err)
	}
	stateRoot := filepath.Join(root, "state")
	remoteDir := filepath.Join(root, "remotes")
	if err := os.MkdirAll(remoteDir, 0o755); err != nil {
		t.Fatal(err)
	}
	configPath := filepath.Join(root, "gitconfig")
	config := fmt.Sprintf("[url \"file://%s/\"]\n\tinsteadOf = https://github.com/ikigenba/\n", filepath.ToSlash(remoteDir))
	if err := os.WriteFile(configPath, []byte(config), 0o600); err != nil {
		t.Fatal(err)
	}
	t.Setenv("GIT_CONFIG_GLOBAL", configPath)
	clock := fixedClock{now: time.Date(2026, 7, 15, 12, 0, 0, 0, time.UTC)}
	store := repos.NewStore(conn)
	git := repos.NewGit(filepath.Join(stateRoot, "repos"), tokenSource("fixture"))
	lifecycle := repos.NewService(store, git, clock, "ikigenba")
	producer, err := outbox.New(conn, outbox.Options{Source: "repos", Registry: repos.Events, Now: clock.Now})
	if err != nil {
		t.Fatal(err)
	}
	reaper, err := repos.NewReaper(store, git, clock, stateRoot, repos.DefaultWorktreeTTL)
	if err != nil {
		t.Fatal(err)
	}
	engine, err := runner.New(runner.Config{Store: store, Git: git, Clock: clock, StateRoot: stateRoot,
		Model: runner.DefaultModelConfig("fixture-key"), Factory: factory, Outbox: producer, Reaper: reaper})
	if err != nil {
		t.Fatal(err)
	}
	domain := &domain{lifecycle: lifecycle, store: store, runner: engine, reaper: reaper}
	var rt *server.Router
	_, err = server.New(server.Options{Addr: "127.0.0.1:0", Logger: slog.New(slog.NewTextHandler(io.Discard, nil)),
		ResourceID: "https://example.test/srv/repos", AuthServer: "https://auth.example.test", Version: "test-1", Service: "repos",
		Events: Events, DB: conn, Register: func(captured *server.Router) error { rt = captured; return nil }})
	if err != nil {
		t.Fatal(err)
	}
	h, err := NewHandler(domain, rt)
	if err != nil {
		t.Fatal(err)
	}
	return &fixture{h: h, db: conn, store: store, runner: engine, stateRoot: stateRoot, remoteDir: remoteDir}
}

func (f *fixture) addRemote(t *testing.T, name string) {
	t.Helper()
	work := filepath.Join(t.TempDir(), "work")
	runGit(t, "", "init", "-b", "main", work)
	runGit(t, work, "config", "user.email", "fixture@example.com")
	runGit(t, work, "config", "user.name", "Fixture")
	if err := os.WriteFile(filepath.Join(work, "README.md"), []byte("fixture\n"), 0o644); err != nil {
		t.Fatal(err)
	}
	runGit(t, work, "add", "README.md")
	runGit(t, work, "commit", "-m", "fixture")
	runGit(t, "", "clone", "--bare", work, filepath.Join(f.remoteDir, name+".git"))
}

func runGit(t *testing.T, dir string, args ...string) {
	t.Helper()
	command := exec.Command("git", args...)
	command.Dir = dir
	if output, err := command.CombinedOutput(); err != nil {
		t.Fatalf("git %v: %v: %s", args, err, output)
	}
}

type rpcResponse struct {
	Result json.RawMessage `json:"result"`
	Error  any             `json:"error"`
}

type toolResult struct {
	IsError           bool           `json:"isError"`
	StructuredContent map[string]any `json:"structuredContent"`
}

func rpc(t *testing.T, h http.Handler, method string, params any, owner string) rpcResponse {
	t.Helper()
	body := map[string]any{"jsonrpc": "2.0", "id": 1, "method": method}
	if params != nil {
		body["params"] = params
	}
	encoded, err := json.Marshal(body)
	if err != nil {
		t.Fatal(err)
	}
	request := httptest.NewRequest(http.MethodPost, "/mcp", bytes.NewReader(encoded))
	request.Header.Set("X-Owner-Email", owner)
	request.Header.Set("X-Client-Id", "test-client")
	recorder := httptest.NewRecorder()
	h.ServeHTTP(recorder, request)
	var response rpcResponse
	if err := json.Unmarshal(recorder.Body.Bytes(), &response); err != nil {
		t.Fatalf("decode response: %v: %s", err, recorder.Body.String())
	}
	if response.Error != nil {
		t.Fatalf("JSON-RPC error: %v", response.Error)
	}
	return response
}

func call(t *testing.T, h http.Handler, name string, args any, owner string) toolResult {
	t.Helper()
	response := rpc(t, h, "tools/call", map[string]any{"name": name, "arguments": args}, owner)
	var result toolResult
	if err := json.Unmarshal(response.Result, &result); err != nil {
		t.Fatalf("decode %s result: %v", name, err)
	}
	return result
}

func callOK(t *testing.T, h http.Handler, name string, args any) map[string]any {
	t.Helper()
	result := call(t, h, name, args, testOwner)
	if result.IsError || result.StructuredContent == nil {
		t.Fatalf("%s result = %#v", name, result)
	}
	return result.StructuredContent
}

func callError(t *testing.T, h http.Handler, name string, args any) string {
	t.Helper()
	result := call(t, h, name, args, testOwner)
	if !result.IsError {
		t.Fatalf("%s unexpectedly succeeded: %#v", name, result)
	}
	code, _ := result.StructuredContent["code"].(string)
	if code == "" {
		t.Fatalf("%s error has no code: %#v", name, result)
	}
	return code
}

func TestRepositoryToolsRoundTripAndScopeByOwner(t *testing.T) {
	// R-FN1M-P1FW
	f := newFixture(t, nil)
	f.addRemote(t, "alpha")
	repo := callOK(t, f.h, "clone", map[string]any{"name": "alpha"})["repo"].(map[string]any)
	if repo["name"] != "alpha" || repo["default_branch"] != "main" || !strings.Contains(repo["clone_url"].(string), "/alpha.git") {
		t.Fatalf("clone repo view = %#v", repo)
	}
	if code := callError(t, f.h, "clone", map[string]any{"name": "alpha"}); code != "conflict" {
		t.Fatalf("duplicate clone code = %q", code)
	}
	if code := callError(t, f.h, "get", map[string]any{"name": "missing"}); code != "not_found" {
		t.Fatalf("missing get code = %q", code)
	}
	if err := f.store.InsertRepo(context.Background(), repos.Repo{Name: "theirs", OwnerEmail: "other@example.com", CloneURL: "file:///fixture", DefaultBranch: "main", CreatedAt: time.Now()}); err != nil {
		t.Fatal(err)
	}
	items := callOK(t, f.h, "list", map[string]any{})["items"].([]any)
	if len(items) != 1 || items[0].(map[string]any)["name"] != "alpha" {
		t.Fatalf("owner-scoped list = %#v", items)
	}
	if deleted := callOK(t, f.h, "delete", map[string]any{"name": "alpha"})["deleted"]; deleted != true {
		t.Fatalf("deleted = %v", deleted)
	}
	if code := callError(t, f.h, "get", map[string]any{"name": "alpha"}); code != "not_found" {
		t.Fatalf("post-delete get code = %q", code)
	}
	if _, err := os.Stat(filepath.Join(f.stateRoot, "repos", "alpha")); !os.IsNotExist(err) {
		t.Fatalf("deleted state dir still exists: %v", err)
	}
}

type cancelAgent struct{ started chan struct{} }

func (a cancelAgent) Send(ctx context.Context, _ string) error {
	close(a.started)
	<-ctx.Done()
	return ctx.Err()
}

func TestSessionStartAndCancelLifecycle(t *testing.T) {
	// R-FO9J-2T6L
	started := make(chan struct{})
	f := newFixture(t, runner.AgentFactoryFunc(func(runner.ConversationConfig) runner.Agent { return cancelAgent{started: started} }))
	f.addRemote(t, "alpha")
	callOK(t, f.h, "clone", map[string]any{"name": "alpha"})
	if code := callError(t, f.h, "session_start", map[string]any{"repo": "alpha", "instructions": "work", "branch": "feature/nope"}); code != "validation" {
		t.Fatalf("invalid branch code = %q", code)
	}
	if code := callError(t, f.h, "session_start", map[string]any{"repo": "missing", "instructions": "work"}); code != "not_found" {
		t.Fatalf("unknown repo code = %q", code)
	}
	created := callOK(t, f.h, "session_start", map[string]any{"repo": "alpha", "instructions": "work"})["session"].(map[string]any)
	id := created["id"].(string)
	if created["status"] != "queued" || created["branch"] != "ikibot/session-"+id {
		t.Fatalf("created session = %#v", created)
	}
	ctx, cancel := context.WithCancel(context.Background())
	defer cancel()
	go f.runner.Dispatch(ctx)
	select {
	case <-started:
	case <-time.After(5 * time.Second):
		t.Fatal("session did not start")
	}
	result := callOK(t, f.h, "session_cancel", map[string]any{"id": id})
	if result["cancelled"] != true {
		t.Fatalf("cancel result = %#v", result)
	}
	waitSessionStatus(t, f.store, id, repos.StatusCancelled)
	terminal := callOK(t, f.h, "session_cancel", map[string]any{"id": id})
	if terminal["cancelled"] != false || terminal["session"].(map[string]any)["status"] != "cancelled" {
		t.Fatalf("terminal cancel = %#v", terminal)
	}
}

func waitSessionStatus(t *testing.T, store *repos.Store, id, want string) {
	t.Helper()
	deadline := time.Now().Add(5 * time.Second)
	for time.Now().Before(deadline) {
		session, err := store.GetSession(context.Background(), id)
		if err == nil && session.Status == want {
			return
		}
		time.Sleep(10 * time.Millisecond)
	}
	t.Fatalf("session %s did not reach %s", id, want)
}

func TestSessionReadToolsFilterAndWindowRealTranscript(t *testing.T) {
	// R-FPHF-GKXA
	f := newFixture(t, nil)
	now := time.Date(2026, 7, 15, 12, 0, 0, 0, time.UTC)
	logPath := filepath.Join(f.stateRoot, "sessions", "one", "output.jsonl")
	if err := os.MkdirAll(filepath.Dir(logPath), 0o755); err != nil {
		t.Fatal(err)
	}
	if err := os.WriteFile(logPath, []byte("first\nsecond\nthird\nfourth\n"), 0o644); err != nil {
		t.Fatal(err)
	}
	problem, pr := "check failed", "https://example.test/pull/7"
	rows := []repos.Session{
		{ID: "one", RepoName: "alpha", OwnerEmail: testOwner, Attempt: 1, Branch: "ikibot/session-one", Instructions: "full instructions", Status: repos.StatusFailed, Error: &problem, PRURL: &pr, CreatedAt: now, EndedAt: &now, LogPath: logPath},
		{ID: "two", RepoName: "beta", OwnerEmail: testOwner, Attempt: 1, Branch: "ikibot/session-two", Status: repos.StatusQueued, CreatedAt: now, LogPath: filepath.Join(f.stateRoot, "two.jsonl")},
		{ID: "other", RepoName: "alpha", OwnerEmail: "other@example.com", Attempt: 1, Branch: "ikibot/session-other", Status: repos.StatusQueued, CreatedAt: now, LogPath: filepath.Join(f.stateRoot, "other.jsonl")},
	}
	for _, row := range rows {
		if err := f.store.InsertSession(context.Background(), row); err != nil {
			t.Fatal(err)
		}
	}
	items := callOK(t, f.h, "session_list", map[string]any{"repo": "alpha"})["items"].([]any)
	if len(items) != 1 || items[0].(map[string]any)["id"] != "one" {
		t.Fatalf("filtered sessions = %#v", items)
	}
	if _, exists := items[0].(map[string]any)["instructions"]; exists {
		t.Fatalf("session_list echoed instructions: %#v", items[0])
	}
	full := callOK(t, f.h, "session_get", map[string]any{"id": "one"})["session"].(map[string]any)
	if full["instructions"] != "full instructions" || full["error"] != problem || full["pr_url"] != pr {
		t.Fatalf("full session = %#v", full)
	}
	window := callOK(t, f.h, "session_output", map[string]any{"id": "one", "offset": 1, "limit": 2})
	if !reflect.DeepEqual(window["lines"], []any{"second", "third"}) || window["total"] != float64(4) || window["offset"] != float64(1) {
		t.Fatalf("output window = %#v", window)
	}
	if code := callError(t, f.h, "session_output", map[string]any{"id": "unknown"}); code != "not_found" {
		t.Fatalf("unknown output code = %q", code)
	}
}

func TestMalformedArgumentsHaveNoSideEffectsAndErrorsUseClosedCodes(t *testing.T) {
	// R-FQPB-UCNZ
	f := newFixture(t, nil)
	beforeRepos, beforeSessions := rowCount(t, f.db, "repos"), rowCount(t, f.db, "sessions")
	if code := callError(t, f.h, "clone", map[string]any{"name": 42}); code != "validation" {
		t.Fatalf("malformed clone code = %q", code)
	}
	if got := rowCount(t, f.db, "repos"); got != beforeRepos {
		t.Fatalf("repos rows changed: %d -> %d", beforeRepos, got)
	}
	if got := rowCount(t, f.db, "sessions"); got != beforeSessions {
		t.Fatalf("session rows changed: %d -> %d", beforeSessions, got)
	}
	if _, err := os.Stat(f.stateRoot); !os.IsNotExist(err) {
		t.Fatalf("malformed call created state root: %v", err)
	}
	allowed := map[string]bool{"validation": true, "not_found": true, "conflict": true, "source_unavailable": true, "internal": true, "too_large": true}
	for _, test := range []struct {
		name string
		args map[string]any
	}{{"get", map[string]any{"name": "missing"}}, {"session_output", map[string]any{"id": "missing"}}, {"delete", map[string]any{"name": "missing"}}, {"session_start", map[string]any{"repo": "", "instructions": ""}}} {
		if code := callError(t, f.h, test.name, test.args); !allowed[code] {
			t.Errorf("%s returned code outside closed vocabulary: %q", test.name, code)
		}
	}
}

func rowCount(t *testing.T, db *sql.DB, table string) int {
	t.Helper()
	var count int
	if err := db.QueryRow("SELECT COUNT(*) FROM " + table).Scan(&count); err != nil {
		t.Fatal(err)
	}
	return count
}

func TestToolTableAndAssembledHandlerExposeExactNamesAndSchemas(t *testing.T) {
	// R-FRX8-84EO
	f := newFixture(t, nil)
	response := rpc(t, f.h, "tools/list", nil, testOwner)
	var result struct {
		Tools []struct {
			Name         string         `json:"name"`
			InputSchema  map[string]any `json:"inputSchema"`
			OutputSchema map[string]any `json:"outputSchema"`
		} `json:"tools"`
	}
	if err := json.Unmarshal(response.Result, &result); err != nil {
		t.Fatal(err)
	}
	var names []string
	domainNames := map[string]bool{"clone": true, "list": true, "get": true, "delete": true, "session_start": true, "session_list": true, "session_get": true, "session_output": true, "session_cancel": true}
	for _, descriptor := range result.Tools {
		names = append(names, descriptor.Name)
		if domainNames[descriptor.Name] && (descriptor.InputSchema == nil || descriptor.OutputSchema == nil) {
			t.Errorf("%s missing schema: input=%v output=%v", descriptor.Name, descriptor.InputSchema, descriptor.OutputSchema)
		}
	}
	sort.Strings(names)
	want := []string{"clone", "delete", "get", "health", "list", "reflection", "session_cancel", "session_get", "session_list", "session_output", "session_start"}
	if !reflect.DeepEqual(names, want) {
		t.Fatalf("tools/list names = %v, want %v", names, want)
	}
	if len(Tools(&domain{})) != 9 {
		t.Fatalf("domain tool table length = %d, want 9", len(Tools(&domain{})))
	}
}
