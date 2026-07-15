package runner

import (
	"context"
	"crypto/sha256"
	"database/sql"
	"encoding/json"
	"fmt"
	"io"
	"net/http"
	"net/http/httptest"
	"os"
	"os/exec"
	"path/filepath"
	"reflect"
	"strings"
	"testing"
	"time"

	appkitdatabase "appkit/db"
	"eventplane/outbox"

	"scripts/internal/db"
	"scripts/internal/ids"
	"scripts/internal/script"
)

const owner = "a@example.com"

func nowStr() string { return time.Now().UTC().Format(time.RFC3339Nano) }

// newStore mirrors store_test's DB setup: a migrated temp SQLite DB with a real
// producer outbox attached so completion events land in the outbox table.
func newStore(t *testing.T) (*script.Store, *sql.DB) {
	t.Helper()
	ctx := context.Background()
	conn, err := appkitdatabase.Open(filepath.Join(t.TempDir(), "scripts.db"))
	if err != nil {
		t.Fatalf("appkitdatabase.Open: %v", err)
	}
	t.Cleanup(func() { conn.Close() })
	migs, err := appkitdatabase.LoadMigrations(db.FS, "migrations")
	if err != nil {
		t.Fatalf("appkitdatabase.LoadMigrations: %v", err)
	}
	if err := appkitdatabase.Migrate(ctx, conn, migs); err != nil {
		t.Fatalf("appkitdatabase.Migrate: %v", err)
	}
	st := script.NewStore(conn)
	ob, err := outbox.New(conn, outbox.Options{Source: "scripts", Registry: script.Events})
	if err != nil {
		t.Fatalf("outbox.New: %v", err)
	}
	st.Outbox = ob
	return st, conn
}

// seed inserts a script + a running run for it and returns the run.
func seed(t *testing.T, st *script.Store, body string) script.Run {
	t.Helper()
	ctx := context.Background()
	now := nowStr()
	sc := script.Script{
		ID:         ids.NewULID(),
		OwnerEmail: owner,
		Name:       "nightly",
		Body:       body,
		Config:     script.Config{Interpreter: "python3", TimeoutSecs: 30},
		CreatedAt:  now,
		UpdatedAt:  now,
	}
	if err := st.InsertScript(ctx, sc); err != nil {
		t.Fatalf("InsertScript: %v", err)
	}
	run := script.Run{
		ID:         ids.NewULID(),
		ScriptID:   sc.ID,
		Status:     script.RunRunning,
		StartedAt:  nowStr(),
		StdoutPath: "runs/x/stdout.log",
		StderrPath: "runs/x/stderr.log",
	}
	if err := st.InsertRun(ctx, run); err != nil {
		t.Fatalf("InsertRun: %v", err)
	}
	return run
}

func outboxCount(t *testing.T, conn *sql.DB) int {
	t.Helper()
	var n int
	if err := conn.QueryRow(`SELECT COUNT(*) FROM outbox`).Scan(&n); err != nil {
		t.Fatalf("outbox count: %v", err)
	}
	return n
}

func lastOutboxPayload(t *testing.T, conn *sql.DB) string {
	t.Helper()
	var p string
	if err := conn.QueryRow(`SELECT payload FROM outbox ORDER BY created_at DESC, event_id DESC LIMIT 1`).Scan(&p); err != nil {
		t.Fatalf("outbox payload: %v", err)
	}
	return p
}

func getRun(t *testing.T, st *script.Store, runID string) script.Run {
	t.Helper()
	r, err := st.GetRun(context.Background(), owner, runID)
	if err != nil {
		t.Fatalf("GetRun: %v", err)
	}
	return r
}

// waitTerminal polls the run row until it leaves 'running' (or fails the test).
func waitTerminal(t *testing.T, st *script.Store, runID string) script.Run {
	t.Helper()
	deadline := time.Now().Add(10 * time.Second)
	for time.Now().Before(deadline) {
		r := getRun(t, st, runID)
		if r.Status != script.RunRunning {
			return r
		}
		time.Sleep(10 * time.Millisecond)
	}
	t.Fatalf("run %s never reached a terminal status", runID)
	return script.Run{}
}

func requirePython(t *testing.T) {
	t.Helper()
	if _, err := exec.LookPath("python3"); err != nil {
		t.Skip("python3 not on PATH")
	}
}

func TestSpawnSucceeds(t *testing.T) {
	requirePython(t)
	st, conn := newStore(t)
	dataDir := t.TempDir()
	r := New(st, dataDir, 30*time.Second, nil)

	body := "open('out.txt', 'w').write('done')\nprint('hello')\n"
	run := seed(t, st, body)

	r.Spawn(run, []byte("{}"))
	got := waitTerminal(t, st, run.ID)

	if got.Status != script.RunSucceeded {
		t.Fatalf("status = %q, want succeeded", got.Status)
	}
	if got.ExitCode == nil || *got.ExitCode != 0 {
		t.Fatalf("exit code = %v, want 0", got.ExitCode)
	}
	if n := outboxCount(t, conn); n != 1 {
		t.Fatalf("outbox rows = %d, want 1", n)
	}

	dir := filepath.Join(dataDir, "runs", run.ID)
	for _, f := range []string{"main.py", "config.json", "stdout.log", "out.txt"} {
		if _, err := os.Stat(filepath.Join(dir, f)); err != nil {
			t.Fatalf("expected %s in run dir: %v", f, err)
		}
	}
	if b, _ := os.ReadFile(filepath.Join(dir, "main.py")); string(b) != body {
		t.Fatalf("main.py = %q, want materialized body", b)
	}
	if b, _ := os.ReadFile(filepath.Join(dir, "out.txt")); string(b) != "done" {
		t.Fatalf("out.txt = %q, want 'done'", b)
	}
}

func TestSpawnMaterializesEmbeddedSuiteBeforeExecution(t *testing.T) {
	// R-HVKP-FQRD
	st, _ := newStore(t)
	dataDir := t.TempDir()
	r := New(st, dataDir, 30*time.Second, nil)
	run := seed(t, st, "import suite\n")

	r.Spawn(run, []byte("{}"))
	got := waitTerminal(t, st, run.ID)
	if got.Status != script.RunSucceeded {
		t.Fatalf("status = %q, want succeeded; error = %q", got.Status, got.Error)
	}

	path := filepath.Join(dataDir, "runs", run.ID, "suite.py")
	b, err := os.ReadFile(path)
	if err != nil {
		t.Fatalf("read materialized suite.py: %v", err)
	}
	if string(b) != suitePy {
		t.Fatalf("materialized suite.py differs from embedded source")
	}
	info, err := os.Stat(path)
	if err != nil {
		t.Fatalf("stat materialized suite.py: %v", err)
	}
	if got := info.Mode().Perm(); got != 0o600 {
		t.Fatalf("suite.py mode = %o, want 600", got)
	}
}

func TestSpawnInjectsSuiteEnvironment(t *testing.T) {
	// R-HWSL-TII2
	st, _ := newStore(t)
	dataDir := t.TempDir()
	services := `{"crm":"http://127.0.0.1:3100","scripts":"http://127.0.0.1:3200"}`
	filesBase := "http://127.0.0.1:3300"
	r := New(st, dataDir, 30*time.Second, []string{
		"SUITE_SERVICES=" + services,
		"SUITE_FILES_BASE_URL=" + filesBase,
	})
	body := "import json, os\nprint(json.dumps([os.environ[k] for k in [\"SUITE_SERVICES\", \"SUITE_FILES_BASE_URL\", \"SUITE_SCRIPT_ID\", \"SUITE_RUN_ID\", \"SUITE_OWNER_EMAIL\"]]))\n"
	run := seed(t, st, body)

	r.Spawn(run, []byte("{}"))
	got := waitTerminal(t, st, run.ID)
	if got.Status != script.RunSucceeded {
		t.Fatalf("status = %q, want succeeded; error = %q", got.Status, got.Error)
	}

	b, err := os.ReadFile(filepath.Join(dataDir, "runs", run.ID, "stdout.log"))
	if err != nil {
		t.Fatalf("read stdout.log: %v", err)
	}
	var values []string
	if err := json.Unmarshal(b, &values); err != nil {
		t.Fatalf("decode environment probe %q: %v", b, err)
	}
	want := []string{services, filesBase, run.ScriptID, run.ID, owner}
	if !reflect.DeepEqual(values, want) {
		t.Fatalf("suite environment = %#v, want %#v", values, want)
	}
}

func TestSuiteEventReturnsTriggerPayloadVerbatim(t *testing.T) {
	// R-HY0I-7A8R
	tests := []struct {
		name  string
		input string
		want  any
	}{
		{name: "triggered", input: `{"a":{"b":[1,2]},"c":"x"}`, want: map[string]any{"a": map[string]any{"b": []any{float64(1), float64(2)}}, "c": "x"}},
		{name: "manual", input: `{}`, want: map[string]any{}},
	}
	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			st, _ := newStore(t)
			dataDir := t.TempDir()
			r := New(st, dataDir, 30*time.Second, nil)
			run := seed(t, st, "import json, suite\nprint(json.dumps(suite.event()))\n")

			r.Spawn(run, []byte(tt.input))
			got := waitTerminal(t, st, run.ID)
			if got.Status != script.RunSucceeded {
				t.Fatalf("status = %q, want succeeded; error = %q", got.Status, got.Error)
			}
			b, err := os.ReadFile(filepath.Join(dataDir, "runs", run.ID, "stdout.log"))
			if err != nil {
				t.Fatalf("read stdout.log: %v", err)
			}
			var payload any
			if err := json.Unmarshal(b, &payload); err != nil {
				t.Fatalf("decode event probe %q: %v", b, err)
			}
			if !reflect.DeepEqual(payload, tt.want) {
				t.Fatalf("suite.event() = %#v, want %#v", payload, tt.want)
			}
		})
	}
}

func TestSuiteRuntimeFactsAreRequiredOutsideRunner(t *testing.T) {
	// R-HZ8E-L1ZG
	dir := t.TempDir()
	if err := os.WriteFile(filepath.Join(dir, "suite.py"), []byte(suitePy), 0o600); err != nil {
		t.Fatalf("write suite.py probe: %v", err)
	}
	tests := []struct {
		name    string
		expr    string
		missing string
	}{
		{name: "event", expr: "suite.event()", missing: "EVENT_JSON"},
		{name: "mcp", expr: "suite.mcp('crm', 'probe')", missing: "SUITE_SERVICES"},
	}
	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			probe := "import suite\ntry:\n    " + tt.expr + "\nexcept Exception as exc:\n    print(type(exc).__name__)\n    print(str(exc))\n"
			cmd := exec.Command("python3", "-c", probe)
			cmd.Dir = dir
			cmd.Env = []string{}
			output, err := cmd.CombinedOutput()
			if err != nil {
				t.Fatalf("python probe failed: %v; output = %s", err, output)
			}
			lines := strings.Split(strings.TrimSpace(string(output)), "\n")
			if len(lines) != 2 || lines[0] != "RuntimeError" {
				t.Fatalf("raised output = %q, want RuntimeError and message", output)
			}
			if !strings.Contains(lines[1], "missing "+tt.missing) {
				t.Fatalf("error message = %q, want missing variable %s", lines[1], tt.missing)
			}
		})
	}
}

type recordedMCPRequest struct {
	Method string
	Path   string
	Header http.Header
	Body   map[string]any
}

func newMCPServer(t *testing.T, response string) (*httptest.Server, *[]recordedMCPRequest) {
	t.Helper()
	requests := []recordedMCPRequest{}
	server := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		body, err := io.ReadAll(r.Body)
		if err != nil {
			t.Errorf("read request body: %v", err)
			return
		}
		var decoded map[string]any
		if err := json.Unmarshal(body, &decoded); err != nil {
			t.Errorf("decode request body: %v", err)
			return
		}
		requests = append(requests, recordedMCPRequest{
			Method: r.Method,
			Path:   r.URL.Path,
			Header: r.Header.Clone(),
			Body:   decoded,
		})
		w.Header().Set("Content-Type", "application/json")
		_, _ = io.WriteString(w, response)
	}))
	t.Cleanup(server.Close)
	return server, &requests
}

func runSuiteMCPProbe(t *testing.T, services, body string) (script.Run, string) {
	t.Helper()
	requirePython(t)
	st, _ := newStore(t)
	dataDir := t.TempDir()
	r := New(st, dataDir, 30*time.Second, []string{
		"SUITE_SERVICES=" + services,
		"SUITE_FILES_BASE_URL=http://127.0.0.1:1",
	})
	run := seed(t, st, body)
	r.Spawn(run, []byte("{}"))
	got := waitTerminal(t, st, run.ID)
	stdout, err := os.ReadFile(filepath.Join(dataDir, "runs", run.ID, "stdout.log"))
	if err != nil {
		t.Fatalf("read stdout.log: %v", err)
	}
	return got, string(stdout)
}

func TestSuiteMcpHappyPath(t *testing.T) {
	// R-I0GA-YTQ5
	server, requests := newMCPServer(t, `{"jsonrpc":"2.0","id":1,"result":{"structuredContent":{"id":"x","nested":{"ok":true}},"isError":false}}`)
	services, _ := json.Marshal(map[string]string{"svc": server.URL})
	body := "import json, suite\nprint(json.dumps([suite.mcp('svc', 'get', {'id': 'x'}), suite.mcp('svc', 'list')], sort_keys=True))\n"
	got, stdout := runSuiteMCPProbe(t, string(services), body)
	if got.Status != script.RunSucceeded {
		t.Fatalf("status = %q, want succeeded; error = %q", got.Status, got.Error)
	}
	if strings.TrimSpace(stdout) != `[{"id": "x", "nested": {"ok": true}}, {"id": "x", "nested": {"ok": true}}]` {
		t.Fatalf("suite.mcp output = %q", stdout)
	}
	if len(*requests) != 2 {
		t.Fatalf("requests = %d, want 2 (one per call)", len(*requests))
	}
	wantArguments := []map[string]any{{"id": "x"}, {}}
	for i, request := range *requests {
		if request.Method != http.MethodPost || request.Path != "/mcp" {
			t.Fatalf("request %d = %s %s, want POST /mcp", i, request.Method, request.Path)
		}
		wantBody := map[string]any{
			"jsonrpc": "2.0", "id": float64(1), "method": "tools/call",
			"params": map[string]any{"name": map[bool]string{true: "list", false: "get"}[i == 1], "arguments": wantArguments[i]},
		}
		if !reflect.DeepEqual(request.Body, wantBody) {
			t.Fatalf("request %d body = %#v, want %#v", i, request.Body, wantBody)
		}
		if request.Header.Get("X-Owner-Email") != owner {
			t.Fatalf("X-Owner-Email = %q, want %q", request.Header.Get("X-Owner-Email"), owner)
		}
		if request.Header.Get("X-Client-Id") != "scripts:"+got.ScriptID {
			t.Fatalf("X-Client-Id = %q, want scripts:%s", request.Header.Get("X-Client-Id"), got.ScriptID)
		}
		if request.Header.Get("Content-Type") != "application/json" {
			t.Fatalf("Content-Type = %q, want application/json", request.Header.Get("Content-Type"))
		}
		if _, ok := request.Header["X-Forwarded-Proto"]; ok {
			t.Fatalf("X-Forwarded-Proto unexpectedly present")
		}
	}
}

func TestSuiteMcpProseFallback(t *testing.T) {
	// R-I1O7-CLGU
	server, _ := newMCPServer(t, `{"jsonrpc":"2.0","id":1,"result":{"content":[{"type":"text","text":"hello "},{"type":"text","text":"world"}],"isError":false}}`)
	services, _ := json.Marshal(map[string]string{"svc": server.URL})
	got, stdout := runSuiteMCPProbe(t, string(services), "import suite\nr = suite.mcp('svc', 'describe')\nprint(type(r).__name__)\nprint(r)\n")
	if got.Status != script.RunSucceeded {
		t.Fatalf("status = %q, want succeeded; error = %q", got.Status, got.Error)
	}
	if stdout != "str\nhello world\n" {
		t.Fatalf("suite.mcp prose output = %q, want str and concatenated text", stdout)
	}
}

func TestSuiteMcpIsErrorRaisesToolError(t *testing.T) {
	// R-I2W3-QD7J
	server, _ := newMCPServer(t, `{"jsonrpc":"2.0","id":1,"result":{"structuredContent":{"code":"not_found","message":"missing x"},"isError":true}}`)
	services, _ := json.Marshal(map[string]string{"svc": server.URL})
	body := "import suite\ntry:\n    suite.mcp('svc', 'get')\nexcept suite.ToolError as exc:\n    print(exc.code)\n    print(exc.message)\n"
	got, stdout := runSuiteMCPProbe(t, string(services), body)
	if got.Status != script.RunSucceeded {
		t.Fatalf("status = %q, want succeeded; error = %q", got.Status, got.Error)
	}
	if stdout != "not_found\nmissing x\n" {
		t.Fatalf("ToolError fields = %q", stdout)
	}
}

func TestSuiteMcpUnknownServiceRaisesValueError(t *testing.T) {
	// R-I5BW-HWOX
	server, requests := newMCPServer(t, `{}`)
	services, _ := json.Marshal(map[string]string{"svc": server.URL})
	body := "import suite\ntry:\n    suite.mcp('nonesuch', 'get')\nexcept Exception as exc:\n    print(type(exc).__name__)\n"
	got, stdout := runSuiteMCPProbe(t, string(services), body)
	if got.Status != script.RunSucceeded || stdout != "ValueError\n" {
		t.Fatalf("status = %q, error = %q, stdout = %q", got.Status, got.Error, stdout)
	}
	if len(*requests) != 0 {
		t.Fatalf("requests = %d, want zero", len(*requests))
	}
}

func TestSuiteMcpSourceUnavailableOnTransportFailure(t *testing.T) {
	// R-I6JS-VOFM
	closed := httptest.NewServer(http.HandlerFunc(func(http.ResponseWriter, *http.Request) {}))
	closedURL := closed.URL
	closed.Close()
	services, _ := json.Marshal(map[string]string{"svc": closedURL})
	body := "import suite\ntry:\n    suite.mcp('svc', 'get')\nexcept suite.ToolError as exc:\n    print(exc.code)\n"
	got, stdout := runSuiteMCPProbe(t, string(services), body)
	if got.Status != script.RunSucceeded || stdout != "source_unavailable\n" {
		t.Fatalf("status = %q, error = %q, stdout = %q", got.Status, got.Error, stdout)
	}
}

type recordedSuiteRequest struct {
	Method string
	Path   string
	Query  string
	Header http.Header
	Body   []byte
}

func newRecordedSuiteServer(t *testing.T, respond http.HandlerFunc) (*httptest.Server, *[]recordedSuiteRequest) {
	t.Helper()
	requests := []recordedSuiteRequest{}
	server := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		body, err := io.ReadAll(r.Body)
		if err != nil {
			t.Errorf("read request body: %v", err)
			return
		}
		requests = append(requests, recordedSuiteRequest{
			Method: r.Method,
			Path:   r.URL.Path,
			Query:  r.URL.RawQuery,
			Header: r.Header.Clone(),
			Body:   body,
		})
		respond(w, r)
	}))
	t.Cleanup(server.Close)
	return server, &requests
}

func runSuiteProbe(t *testing.T, services, filesBase, body string) (script.Run, string, string) {
	t.Helper()
	requirePython(t)
	st, _ := newStore(t)
	dataDir := t.TempDir()
	r := New(st, dataDir, 30*time.Second, []string{
		"SUITE_SERVICES=" + services,
		"SUITE_FILES_BASE_URL=" + filesBase,
	})
	run := seed(t, st, body)
	r.Spawn(run, []byte("{}"))
	got := waitTerminal(t, st, run.ID)
	runDir := filepath.Join(dataDir, "runs", run.ID)
	stdout, err := os.ReadFile(filepath.Join(runDir, "stdout.log"))
	if err != nil {
		t.Fatalf("read stdout.log: %v", err)
	}
	return got, string(stdout), runDir
}

func requireSucceededProbe(t *testing.T, got script.Run) {
	t.Helper()
	if got.Status != script.RunSucceeded {
		t.Fatalf("status = %q, want succeeded; error = %q", got.Status, got.Error)
	}
}

func TestSuiteFetchStreamsAllowedContentURL(t *testing.T) {
	// R-I7RP-9G6B
	content := []byte("known content\x00with bytes")
	server, requests := newRecordedSuiteServer(t, func(w http.ResponseWriter, _ *http.Request) {
		_, _ = w.Write(content)
	})
	services, _ := json.Marshal(map[string]string{"content": server.URL})
	body := fmt.Sprintf("import json, suite\nprint(json.dumps(suite.fetch(%q, 'landed.bin'), sort_keys=True))\n", server.URL+"/object")
	got, stdout, runDir := runSuiteProbe(t, string(services), server.URL, body)
	requireSucceededProbe(t, got)

	if len(*requests) != 1 || (*requests)[0].Method != http.MethodGet || (*requests)[0].Path != "/object" {
		t.Fatalf("requests = %#v, want one GET /object", *requests)
	}
	landed, err := os.ReadFile(filepath.Join(runDir, "landed.bin"))
	if err != nil || !reflect.DeepEqual(landed, content) {
		t.Fatalf("landed bytes = %q, err = %v, want %q", landed, err, content)
	}
	want := fmt.Sprintf(`{"content_hash": "%x", "path": "landed.bin", "size": %d}`+"\n", sha256.Sum256(content), len(content))
	if stdout != want {
		t.Fatalf("fetch result = %q, want %q", stdout, want)
	}
}

func TestSuiteFetchConfinesURLBeforeRequest(t *testing.T) {
	// R-I8ZL-N7X0
	server, requests := newRecordedSuiteServer(t, func(w http.ResponseWriter, _ *http.Request) {
		_, _ = io.WriteString(w, "ok")
	})
	services, _ := json.Marshal(map[string]string{"content": server.URL})
	port := strings.TrimPrefix(server.URL, "http://127.0.0.1:")
	body := fmt.Sprintf(`import json, suite
urls = [%q, %q, %q, %q]
codes = []
for i, url in enumerate(urls):
    try:
        suite.fetch(url, 'reject-%%d' %% i)
    except suite.ToolError as exc:
        codes.append(exc.code)
result = suite.fetch(%q, 'allowed')
print(json.dumps([codes, result['size']]))
`, "http://10.0.0.5:"+port+"/x", "http://localhost:"+port+"/x", "http://127.0.0.1:1/x", "https://127.0.0.1:"+port+"/x", server.URL+"/allowed")
	got, stdout, _ := runSuiteProbe(t, string(services), server.URL, body)
	requireSucceededProbe(t, got)
	if stdout != `[["validation", "validation", "validation", "validation"], 2]`+"\n" {
		t.Fatalf("confinement result = %q", stdout)
	}
	if len(*requests) != 1 || (*requests)[0].Path != "/allowed" {
		t.Fatalf("requests = %#v, want only the allowed fetch", *requests)
	}
}

func TestSuiteFetchMapsFailuresWithoutPartialFilesOrRedirects(t *testing.T) {
	// R-IA7I-0ZNP
	server, requests := newRecordedSuiteServer(t, func(w http.ResponseWriter, r *http.Request) {
		switch r.URL.Path {
		case "/missing":
			http.Error(w, "missing", http.StatusNotFound)
		case "/conflict":
			http.Error(w, "moved", http.StatusConflict)
		case "/redirect":
			http.Redirect(w, r, "/target", http.StatusFound)
		case "/torn":
			hijacker, ok := w.(http.Hijacker)
			if !ok {
				t.Error("response writer cannot hijack")
				return
			}
			conn, rw, err := hijacker.Hijack()
			if err != nil {
				t.Errorf("hijack: %v", err)
				return
			}
			_, _ = rw.WriteString("HTTP/1.1 200 OK\r\nContent-Length: 100\r\n\r\nshort")
			_ = rw.Flush()
			_ = conn.Close()
		default:
			_, _ = io.WriteString(w, "redirect followed")
		}
	})
	closed := httptest.NewServer(http.HandlerFunc(func(http.ResponseWriter, *http.Request) {}))
	closedURL := closed.URL
	closed.Close()
	services, _ := json.Marshal(map[string]string{"content": server.URL, "closed": closedURL})
	body := fmt.Sprintf(`import json, os, suite
urls = [%q, %q, %q, %q, %q]
out = []
for i, url in enumerate(urls):
    dest = 'failed-%%d' %% i
    try:
        suite.fetch(url, dest)
    except suite.ToolError as exc:
        out.append([exc.code, os.path.exists(dest)])
print(json.dumps(out))
`, server.URL+"/missing", server.URL+"/conflict", closedURL+"/refused", server.URL+"/redirect", server.URL+"/torn")
	got, stdout, _ := runSuiteProbe(t, string(services), server.URL, body)
	requireSucceededProbe(t, got)
	want := `[["not_found", false], ["conflict", false], ["source_unavailable", false], ["source_unavailable", false], ["source_unavailable", false]]` + "\n"
	if stdout != want {
		t.Fatalf("failure mapping = %q, want %q", stdout, want)
	}
	paths := []string{}
	for _, request := range *requests {
		paths = append(paths, request.Path)
	}
	if strings.Contains(strings.Join(paths, ","), "/target") {
		t.Fatalf("redirect target received request: %v", paths)
	}
}

func defaultFileShareResponse(w http.ResponseWriter, r *http.Request) {
	w.Header().Set("Content-Type", "application/json")
	switch {
	case r.Method == http.MethodGet && r.URL.Path == "/content":
		_, _ = io.WriteString(w, "shared bytes")
	case r.Method == http.MethodPut && r.URL.Path == "/content":
		_, _ = io.WriteString(w, `{"path":"/put","size":7,"content_hash":"server-hash","rev":"r1"}`)
	case r.URL.Path == "/list":
		_, _ = io.WriteString(w, `{"files":[{"path":"/a"}],"next_cursor":"next"}`)
	case r.URL.Path == "/stat":
		_, _ = io.WriteString(w, `{"path":"/a","size":3,"rev":"r2"}`)
	default:
		w.WriteHeader(http.StatusNoContent)
	}
}

func TestSuiteFilesAllVerbsCarryOnlyClientIdentity(t *testing.T) {
	// R-IBFE-EREE
	server, requests := newRecordedSuiteServer(t, defaultFileShareResponse)
	services, _ := json.Marshal(map[string]string{"svc": server.URL})
	body := `import suite
open('source', 'wb').write(b'payload')
suite.files.list()
suite.files.stat('/a')
suite.files.get('/a', 'dest')
suite.files.put('source', '/put')
suite.files.delete('/gone')
suite.files.move('/a', '/b')
suite.files.mkdir('/dir')
`
	got, _, _ := runSuiteProbe(t, string(services), server.URL, body)
	requireSucceededProbe(t, got)
	if len(*requests) != 7 {
		t.Fatalf("requests = %d, want seven", len(*requests))
	}
	for i, request := range *requests {
		if request.Header.Get("X-Client-Id") != "scripts:"+got.ScriptID {
			t.Fatalf("request %d X-Client-Id = %q", i, request.Header.Get("X-Client-Id"))
		}
		if _, ok := request.Header["X-Owner-Email"]; ok {
			t.Fatalf("request %d unexpectedly has X-Owner-Email", i)
		}
		if _, ok := request.Header["X-Forwarded-Proto"]; ok {
			t.Fatalf("request %d unexpectedly has X-Forwarded-Proto", i)
		}
	}
}

func TestSuiteFilesGetStreamsContent(t *testing.T) {
	// R-ICNA-SJ53
	content := []byte("share content\x00bytes")
	server, requests := newRecordedSuiteServer(t, func(w http.ResponseWriter, _ *http.Request) {
		_, _ = w.Write(content)
	})
	services, _ := json.Marshal(map[string]string{"svc": server.URL})
	body := "import json, suite\nprint(json.dumps(suite.files.get('/folder/a b', 'download'), sort_keys=True))\n"
	got, stdout, runDir := runSuiteProbe(t, string(services), server.URL, body)
	requireSucceededProbe(t, got)
	if len(*requests) != 1 || (*requests)[0].Method != http.MethodGet || (*requests)[0].Path != "/content" || (*requests)[0].Query != "path=%2Ffolder%2Fa+b" {
		t.Fatalf("request = %#v", *requests)
	}
	landed, err := os.ReadFile(filepath.Join(runDir, "download"))
	if err != nil || !reflect.DeepEqual(landed, content) {
		t.Fatalf("download = %q, err = %v", landed, err)
	}
	want := fmt.Sprintf(`{"content_hash": "%x", "path": "download", "size": %d}`+"\n", sha256.Sum256(content), len(content))
	if stdout != want {
		t.Fatalf("get result = %q, want %q", stdout, want)
	}
}

func TestSuiteFilesPutStreamsSourceAndReturnsServerJSON(t *testing.T) {
	// R-IDV7-6AVS
	server, requests := newRecordedSuiteServer(t, defaultFileShareResponse)
	services, _ := json.Marshal(map[string]string{"svc": server.URL})
	body := `import json, suite
open('source', 'wb').write(b'payload')
result = suite.files.put('source', '/folder/a b')
try:
    suite.files.put('missing', '/never')
except FileNotFoundError:
    missing = True
print(json.dumps([result, missing], sort_keys=True))
`
	got, stdout, _ := runSuiteProbe(t, string(services), server.URL, body)
	requireSucceededProbe(t, got)
	if len(*requests) != 1 || (*requests)[0].Method != http.MethodPut || (*requests)[0].Path != "/content" || (*requests)[0].Query != "path=%2Ffolder%2Fa+b" || string((*requests)[0].Body) != "payload" {
		t.Fatalf("request = %#v", *requests)
	}
	want := `[{"content_hash": "server-hash", "path": "/put", "rev": "r1", "size": 7}, true]` + "\n"
	if stdout != want {
		t.Fatalf("put result = %q, want %q", stdout, want)
	}
}

func TestSuiteFilesListAndStatPassQueriesAndJSONVerbatim(t *testing.T) {
	// R-IF33-K2MH
	server, requests := newRecordedSuiteServer(t, defaultFileShareResponse)
	services, _ := json.Marshal(map[string]string{"svc": server.URL})
	body := `import json, suite
print(json.dumps([suite.files.list(path='/a b', cursor='c+1', limit=25), suite.files.stat('/a b')], sort_keys=True))
`
	got, stdout, _ := runSuiteProbe(t, string(services), server.URL, body)
	requireSucceededProbe(t, got)
	if len(*requests) != 2 || (*requests)[0].Method != http.MethodGet || (*requests)[0].Path != "/list" || (*requests)[0].Query != "path=%2Fa+b&cursor=c%2B1&limit=25" || (*requests)[1].Method != http.MethodGet || (*requests)[1].Path != "/stat" || (*requests)[1].Query != "path=%2Fa+b" {
		t.Fatalf("requests = %#v", *requests)
	}
	want := `[{"files": [{"path": "/a"}], "next_cursor": "next"}, {"path": "/a", "rev": "r2", "size": 3}]` + "\n"
	if stdout != want {
		t.Fatalf("list/stat result = %q, want %q", stdout, want)
	}
}

func TestSuiteFilesMutationVerbsUseEscapedQueriesAndReturnNone(t *testing.T) {
	// R-IGAZ-XUD6
	server, requests := newRecordedSuiteServer(t, defaultFileShareResponse)
	services, _ := json.Marshal(map[string]string{"svc": server.URL})
	body := `import json, suite
print(json.dumps([suite.files.delete('/a b'), suite.files.mkdir('/new dir'), suite.files.move('/a b', '/c+d')]))
`
	got, stdout, _ := runSuiteProbe(t, string(services), server.URL, body)
	requireSucceededProbe(t, got)
	wantRequests := []recordedSuiteRequest{
		{Method: http.MethodDelete, Path: "/content", Query: "path=%2Fa+b"},
		{Method: http.MethodPost, Path: "/mkdir", Query: "path=%2Fnew+dir"},
		{Method: http.MethodPost, Path: "/move", Query: "from=%2Fa+b&to=%2Fc%2Bd"},
	}
	if len(*requests) != len(wantRequests) {
		t.Fatalf("requests = %#v", *requests)
	}
	for i, want := range wantRequests {
		gotRequest := (*requests)[i]
		if gotRequest.Method != want.Method || gotRequest.Path != want.Path || gotRequest.Query != want.Query {
			t.Fatalf("request %d = %s %s?%s, want %s %s?%s", i, gotRequest.Method, gotRequest.Path, gotRequest.Query, want.Method, want.Path, want.Query)
		}
	}
	if stdout != "[null, null, null]\n" {
		t.Fatalf("mutation results = %q", stdout)
	}
}

func TestSuiteFilesRootsEverySharePathAndPreservesAbsolutePaths(t *testing.T) {
	// R-ZECX-40UZ
	server, requests := newRecordedSuiteServer(t, defaultFileShareResponse)
	services, _ := json.Marshal(map[string]string{"svc": server.URL})
	body := `import suite
open('source', 'wb').write(b'payload')
suite.files.stat('a/b')
suite.files.get('a/b', 'relative-download')
suite.files.put('source', 'a/b')
suite.files.delete('a/b')
suite.files.mkdir('a/b')
suite.files.move('a/b', 'c/d')
suite.files.stat('/raw//path')
suite.files.get('/raw//path', 'absolute-download')
suite.files.put('source', '/raw//path')
suite.files.delete('/raw//path')
suite.files.mkdir('/raw//path')
suite.files.move('/raw//path', '/other//path')
`
	got, _, _ := runSuiteProbe(t, string(services), server.URL, body)
	requireSucceededProbe(t, got)

	want := []recordedSuiteRequest{
		{Method: http.MethodGet, Path: "/stat", Query: "path=%2Fa%2Fb"},
		{Method: http.MethodGet, Path: "/content", Query: "path=%2Fa%2Fb"},
		{Method: http.MethodPut, Path: "/content", Query: "path=%2Fa%2Fb"},
		{Method: http.MethodDelete, Path: "/content", Query: "path=%2Fa%2Fb"},
		{Method: http.MethodPost, Path: "/mkdir", Query: "path=%2Fa%2Fb"},
		{Method: http.MethodPost, Path: "/move", Query: "from=%2Fa%2Fb&to=%2Fc%2Fd"},
		{Method: http.MethodGet, Path: "/stat", Query: "path=%2Fraw%2F%2Fpath"},
		{Method: http.MethodGet, Path: "/content", Query: "path=%2Fraw%2F%2Fpath"},
		{Method: http.MethodPut, Path: "/content", Query: "path=%2Fraw%2F%2Fpath"},
		{Method: http.MethodDelete, Path: "/content", Query: "path=%2Fraw%2F%2Fpath"},
		{Method: http.MethodPost, Path: "/mkdir", Query: "path=%2Fraw%2F%2Fpath"},
		{Method: http.MethodPost, Path: "/move", Query: "from=%2Fraw%2F%2Fpath&to=%2Fother%2F%2Fpath"},
	}
	if len(*requests) != len(want) {
		t.Fatalf("requests = %#v, want %d", *requests, len(want))
	}
	for i, wantRequest := range want {
		gotRequest := (*requests)[i]
		if gotRequest.Method != wantRequest.Method || gotRequest.Path != wantRequest.Path || gotRequest.Query != wantRequest.Query {
			t.Errorf("request %d = %s %s?%s, want %s %s?%s", i, gotRequest.Method, gotRequest.Path, gotRequest.Query, wantRequest.Method, wantRequest.Path, wantRequest.Query)
		}
	}
}

func TestSuiteFilesListOmitsAbsentPathAndRootsGivenPath(t *testing.T) {
	// R-ZFKT-HSLO
	server, requests := newRecordedSuiteServer(t, defaultFileShareResponse)
	services, _ := json.Marshal(map[string]string{"svc": server.URL})
	body := `import suite
suite.files.list()
suite.files.list('a')
`
	got, _, _ := runSuiteProbe(t, string(services), server.URL, body)
	requireSucceededProbe(t, got)

	want := []recordedSuiteRequest{
		{Method: http.MethodGet, Path: "/list", Query: ""},
		{Method: http.MethodGet, Path: "/list", Query: "path=%2Fa"},
	}
	if len(*requests) != len(want) {
		t.Fatalf("requests = %#v, want %d", *requests, len(want))
	}
	for i, wantRequest := range want {
		gotRequest := (*requests)[i]
		if gotRequest.Method != wantRequest.Method || gotRequest.Path != wantRequest.Path || gotRequest.Query != wantRequest.Query {
			t.Errorf("request %d = %s %s?%s, want %s %s?%s", i, gotRequest.Method, gotRequest.Path, gotRequest.Query, wantRequest.Method, wantRequest.Path, wantRequest.Query)
		}
	}
}

func TestSuiteFilesMapsFailuresAndCleansFailedGet(t *testing.T) {
	// R-IHIW-BM3V
	server, _ := newRecordedSuiteServer(t, func(w http.ResponseWriter, r *http.Request) {
		switch r.URL.Query().Get("path") {
		case "/bad":
			w.WriteHeader(http.StatusBadRequest)
			_, _ = io.WriteString(w, "invalid display path")
		case "/missing":
			w.WriteHeader(http.StatusNotFound)
		case "/conflict":
			w.WriteHeader(http.StatusConflict)
		}
	})
	closed := httptest.NewServer(http.HandlerFunc(func(http.ResponseWriter, *http.Request) {}))
	closedURL := closed.URL
	closed.Close()
	services, _ := json.Marshal(map[string]string{"svc": server.URL})
	body := fmt.Sprintf(`import json, os, suite
out = []
for path in ['/bad', '/missing', '/conflict']:
    try:
        suite.files.get(path, 'failed')
    except suite.ToolError as exc:
        out.append([exc.code, exc.message, os.path.exists('failed')])
os.environ['SUITE_FILES_BASE_URL'] = %q
try:
    suite.files.stat('/a')
except suite.ToolError as exc:
    out.append([exc.code])
print(json.dumps(out))
`, closedURL)
	got, stdout, _ := runSuiteProbe(t, string(services), server.URL, body)
	requireSucceededProbe(t, got)
	var result []any
	if err := json.Unmarshal([]byte(stdout), &result); err != nil {
		t.Fatalf("decode failure output %q: %v", stdout, err)
	}
	if len(result) != 4 {
		t.Fatalf("failure results = %#v", result)
	}
	wantCodes := []string{"validation", "not_found", "conflict", "source_unavailable"}
	for i, wantCode := range wantCodes {
		row := result[i].([]any)
		if row[0] != wantCode {
			t.Fatalf("failure %d code = %#v, want %q", i, row[0], wantCode)
		}
		if i < 3 && row[2] != false {
			t.Fatalf("failure %d left destination: %#v", i, row)
		}
	}
	if !strings.Contains(result[0].([]any)[1].(string), "invalid display path") {
		t.Fatalf("validation message = %#v", result[0])
	}
}

func TestSpawnUsesRebuildableRunsDirOutsideState(t *testing.T) {
	// R-4LKF-FB23
	requirePython(t)
	st, _ := newStore(t)
	root := t.TempDir()
	stateDir := filepath.Join(root, "state")
	if err := os.MkdirAll(stateDir, 0o700); err != nil {
		t.Fatalf("mkdir state: %v", err)
	}
	r := New(st, root, 30*time.Second, nil)

	run := seed(t, st, "open('out.txt', 'w').write('artifact')\n")
	r.Spawn(run, []byte("{}"))
	got := waitTerminal(t, st, run.ID)

	if got.Status != script.RunSucceeded {
		t.Fatalf("status = %q, want succeeded", got.Status)
	}
	runDir := filepath.Join(root, "runs", run.ID)
	if _, err := os.Stat(filepath.Join(runDir, "out.txt")); err != nil {
		t.Fatalf("run artifact under non-state runs dir: %v", err)
	}
	if _, err := os.Stat(filepath.Join(stateDir, "runs")); !os.IsNotExist(err) {
		t.Fatalf("state/runs should not exist for rebuildable execution trees; stat err=%v", err)
	}
}

func TestSpawnFailsNonZero(t *testing.T) {
	requirePython(t)
	st, conn := newStore(t)
	r := New(st, t.TempDir(), 30*time.Second, nil)

	run := seed(t, st, "import sys\nsys.exit(3)\n")
	r.Spawn(run, []byte("{}"))
	got := waitTerminal(t, st, run.ID)

	if got.Status != script.RunFailed {
		t.Fatalf("status = %q, want failed", got.Status)
	}
	if got.ExitCode == nil || *got.ExitCode != 3 {
		t.Fatalf("exit code = %v, want 3", got.ExitCode)
	}
	if n := outboxCount(t, conn); n != 1 {
		t.Fatalf("outbox rows = %d, want 1", n)
	}
}

func TestSpawnStdoutTruncated(t *testing.T) {
	requirePython(t)
	st, conn := newStore(t)
	dataDir := t.TempDir()
	r := New(st, dataDir, 30*time.Second, nil)

	// Print 10000 'A' chars (no newline) → > 8KB; tail must be the last 8192 'A'.
	run := seed(t, st, "import sys\nsys.stdout.write('A' * 10000)\n")
	r.Spawn(run, []byte("{}"))
	got := waitTerminal(t, st, run.ID)
	if got.Status != script.RunSucceeded {
		t.Fatalf("status = %q, want succeeded", got.Status)
	}
	if n := outboxCount(t, conn); n != 1 {
		t.Fatalf("outbox rows = %d, want 1", n)
	}

	// Assert truncation via the emitted event payload.
	payload := lastOutboxPayload(t, conn)
	if !strings.Contains(payload, `"stdout_truncated":true`) {
		t.Fatalf("payload missing stdout_truncated=true: %s", payload)
	}
	want := strings.Repeat("A", tailMax)
	if !strings.Contains(payload, want) {
		t.Fatalf("payload tail not last 8KB of A's")
	}
	// The full file on disk keeps all 10000 bytes.
	b, _ := os.ReadFile(filepath.Join(dataDir, "runs", run.ID, "stdout.log"))
	if len(b) != 10000 {
		t.Fatalf("stdout.log len = %d, want 10000", len(b))
	}
}

func TestCancel(t *testing.T) {
	requirePython(t)
	st, conn := newStore(t)
	r := New(st, t.TempDir(), 30*time.Second, nil)

	// A sleeper that writes its pid so we can confirm the kill.
	run := seed(t, st, "import time\ntime.sleep(60)\n")
	r.Spawn(run, []byte("{}"))

	// Wait until the run is registered as in-flight, then cancel.
	deadline := time.Now().Add(5 * time.Second)
	for time.Now().Before(deadline) {
		if r.Cancel(run.ID) {
			break
		}
		time.Sleep(10 * time.Millisecond)
	}

	got := waitTerminal(t, st, run.ID)
	if got.Status != script.RunCancelled {
		t.Fatalf("status = %q, want cancelled", got.Status)
	}
	if n := outboxCount(t, conn); n != 0 {
		t.Fatalf("outbox rows = %d, want 0 (cancel emits no event)", n)
	}
	// Cancel on an unknown/finished run returns false.
	if r.Cancel(run.ID) {
		t.Fatalf("Cancel on finished run should return false")
	}
}

func TestTTL(t *testing.T) {
	requirePython(t)
	st, _ := newStore(t)
	r := New(st, t.TempDir(), 150*time.Millisecond, nil)

	run := seed(t, st, "import time\ntime.sleep(60)\n")
	start := time.Now()
	r.Spawn(run, []byte("{}"))
	got := waitTerminal(t, st, run.ID)

	if got.Status != script.RunFailed {
		t.Fatalf("status = %q, want failed", got.Status)
	}
	if got.Error != "run TTL exceeded" {
		t.Fatalf("error = %q, want 'run TTL exceeded'", got.Error)
	}
	if elapsed := time.Since(start); elapsed > 10*time.Second {
		t.Fatalf("TTL did not kill the process promptly (%v)", elapsed)
	}
}

func TestRecover(t *testing.T) {
	st, _ := newStore(t)
	r := New(st, t.TempDir(), 30*time.Second, nil)

	// Plant a running row directly (simulating a crash mid-run).
	run := seed(t, st, "print('x')")

	n, err := r.Recover(context.Background())
	if err != nil {
		t.Fatalf("Recover: %v", err)
	}
	if n != 1 {
		t.Fatalf("Recover count = %d, want 1", n)
	}
	got := getRun(t, st, run.ID)
	if got.Status != script.RunFailed {
		t.Fatalf("status = %q, want failed", got.Status)
	}
}

func TestSpawnTombstonedScript(t *testing.T) {
	st, conn := newStore(t)
	dataDir := t.TempDir()
	r := New(st, dataDir, 30*time.Second, nil)

	// Insert a script + its running run, then tombstone the script (the run row
	// survives with a dangling script_id, §7A). Spawning then hits the
	// ScriptForRun not-found path: fail the run, materialize nothing.
	run := seed(t, st, "print('never runs')")
	if err := st.DeleteScript(context.Background(), owner, run.ScriptID); err != nil {
		t.Fatalf("DeleteScript: %v", err)
	}

	r.Spawn(run, []byte("{}"))

	// The script row is gone, so GetRun's owner-scoped JOIN can't see this run;
	// poll the row directly instead.
	var status, errMsg string
	deadline := time.Now().Add(10 * time.Second)
	for time.Now().Before(deadline) {
		if err := conn.QueryRow(`SELECT status, COALESCE(error,'') FROM runs WHERE id = ?`, run.ID).Scan(&status, &errMsg); err != nil {
			t.Fatalf("scan run row: %v", err)
		}
		if status != script.RunRunning {
			break
		}
		time.Sleep(10 * time.Millisecond)
	}
	if status != script.RunFailed {
		t.Fatalf("status = %q, want failed", status)
	}
	if errMsg == "" {
		t.Fatalf("expected a failure error message")
	}
	// A failed run still emits the failed completion event.
	if n := outboxCount(t, conn); n != 1 {
		t.Fatalf("outbox rows = %d, want 1", n)
	}
	// Nothing materialized — no run dir.
	if _, err := os.Stat(filepath.Join(dataDir, "runs", run.ID)); err == nil {
		t.Fatalf("run dir should not exist for a tombstoned script")
	}
}
