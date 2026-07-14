package mcp

import (
	"context"
	"encoding/json"
	"errors"
	"log/slog"
	"net/http"
	"net/http/httptest"
	"reflect"
	"strings"
	"sync"
	"testing"

	"appkit/server"

	gh "github/internal/gh"
)

type fakeClient struct {
	calls []string
	err   error

	lastRepo   string
	lastNumber int
	lastPath   string
	lastBody   string
	lastEvent  string
	lastMethod string
	lastTitle  string
	lastPatch  gh.IssuePatch
	lastFile   gh.FilePut
}

func (f *fakeClient) record(call string) {
	f.calls = append(f.calls, call)
}

func (f *fakeClient) ReposList(context.Context) ([]gh.Repo, error) {
	f.record("repos_list")
	if f.err != nil {
		return nil, f.err
	}
	return []gh.Repo{{Name: "repo"}}, nil
}

func (f *fakeClient) RepoGet(_ context.Context, repo string) (gh.Repo, error) {
	f.record("repo_get")
	f.lastRepo = repo
	return gh.Repo{Name: repo}, f.err
}

func (f *fakeClient) PRList(_ context.Context, repo, state string) ([]gh.PR, error) {
	f.record("pr_list")
	f.lastRepo = repo
	return []gh.PR{{Number: 7, State: state}}, f.err
}

func (f *fakeClient) PRGet(_ context.Context, repo string, number int) (gh.PRDetail, error) {
	f.record("pr_get")
	f.lastRepo, f.lastNumber = repo, number
	return gh.PRDetail{PR: gh.PR{Number: number}}, f.err
}

func (f *fakeClient) PRComment(_ context.Context, repo string, number int, body string) (gh.Comment, error) {
	f.record("pr_comment")
	f.lastRepo, f.lastNumber, f.lastBody = repo, number, body
	return gh.Comment{ID: 1, Body: body}, f.err
}

func (f *fakeClient) PRReview(_ context.Context, repo string, number int, event, body string) (gh.Review, error) {
	f.record("pr_review")
	f.lastRepo, f.lastNumber, f.lastEvent, f.lastBody = repo, number, event, body
	return gh.Review{ID: 2, State: event, Body: body}, f.err
}

func (f *fakeClient) PRMerge(_ context.Context, repo string, number int, method string) (gh.MergeResult, error) {
	f.record("pr_merge")
	f.lastRepo, f.lastNumber, f.lastMethod = repo, number, method
	return gh.MergeResult{SHA: "abc", Merged: true}, f.err
}

func (f *fakeClient) IssueList(_ context.Context, repo, state string) ([]gh.Issue, error) {
	f.record("issue_list")
	f.lastRepo = repo
	return []gh.Issue{{Number: 3, State: state}}, f.err
}

func (f *fakeClient) IssueGet(_ context.Context, repo string, number int) (gh.Issue, error) {
	f.record("issue_get")
	f.lastRepo, f.lastNumber = repo, number
	return gh.Issue{Number: number}, f.err
}

func (f *fakeClient) IssueCreate(_ context.Context, repo, title, body string) (gh.Issue, error) {
	f.record("issue_create")
	f.lastRepo, f.lastTitle, f.lastBody = repo, title, body
	return gh.Issue{Number: 4, Title: title, Body: body}, f.err
}

func (f *fakeClient) IssueComment(_ context.Context, repo string, number int, body string) (gh.Comment, error) {
	f.record("issue_comment")
	f.lastRepo, f.lastNumber, f.lastBody = repo, number, body
	return gh.Comment{ID: 5, Body: body}, f.err
}

func (f *fakeClient) IssueUpdate(_ context.Context, repo string, number int, patch gh.IssuePatch) (gh.Issue, error) {
	f.record("issue_update")
	f.lastRepo, f.lastNumber, f.lastPatch = repo, number, patch
	return gh.Issue{Number: number, State: patch.State}, f.err
}

func (f *fakeClient) FileGet(_ context.Context, repo, path, ref string) (gh.FileContent, error) {
	f.record("file_get")
	f.lastRepo, f.lastPath = repo, path
	return gh.FileContent{Path: path, SHA: ref}, f.err
}

func (f *fakeClient) FilePut(_ context.Context, repo, path string, in gh.FilePut) (gh.FileCommit, error) {
	f.record("file_put")
	f.lastRepo, f.lastPath, f.lastFile = repo, path, in
	return gh.FileCommit{Content: gh.FileContent{Path: path}, Commit: gh.Commit{Message: in.Message}}, f.err
}

type captureHandler struct {
	mu      sync.Mutex
	records []map[string]any
}

func (h *captureHandler) Enabled(context.Context, slog.Level) bool { return true }

func (h *captureHandler) Handle(_ context.Context, r slog.Record) error {
	if r.Message != "github write" {
		return nil
	}
	attrs := map[string]any{"msg": r.Message}
	r.Attrs(func(a slog.Attr) bool {
		attrs[a.Key] = a.Value.Any()
		return true
	})
	h.mu.Lock()
	defer h.mu.Unlock()
	h.records = append(h.records, attrs)
	return nil
}

func (h *captureHandler) WithAttrs([]slog.Attr) slog.Handler { return h }
func (h *captureHandler) WithGroup(string) slog.Handler      { return h }

func newTestHandler(fc *fakeClient, cap *captureHandler, health func(context.Context) (map[string]any, error)) http.Handler {
	logger := slog.New(cap)
	srv, err := server.New(server.Options{
		Addr:    "127.0.0.1:0",
		Logger:  logger,
		Apex:    true,
		Version: "v-test",
		Service: "github",
		Health:  health,
		Register: func(rt *server.Router) error {
			h, err := NewHandler(fc, rt)
			if err != nil {
				return err
			}
			rt.Handle("POST /mcp", rt.RequireIdentity(h))
			return nil
		},
	})
	if err != nil {
		panic(err)
	}
	return srv.Handler
}

func rpc(t *testing.T, h http.Handler, method, params string) (map[string]any, any, int) {
	t.Helper()
	body := `{"jsonrpc":"2.0","id":1,"method":"` + method + `","params":` + params + `}`
	req := httptest.NewRequest(http.MethodPost, "/mcp", strings.NewReader(body))
	req.Header.Set("X-Owner-Email", "owner@example.com")
	req.Header.Set("X-Client-Id", "client-123")
	req.Header.Set("Authorization", "Bearer ignored-by-service")
	rec := httptest.NewRecorder()
	h.ServeHTTP(rec, req)
	var env struct {
		Result map[string]any `json:"result"`
		Error  any            `json:"error"`
	}
	if err := json.Unmarshal(rec.Body.Bytes(), &env); err != nil {
		t.Fatalf("%s: decode envelope: %v\n%s", method, err, rec.Body.String())
	}
	return env.Result, env.Error, rec.Code
}

func callToolText(t *testing.T, h http.Handler, name, args string) (string, bool, any, int) {
	t.Helper()
	res, rpcErr, code := rpc(t, h, "tools/call", `{"name":"`+name+`","arguments":`+args+`}`)
	if rpcErr != nil {
		return "", false, rpcErr, code
	}
	isErr, _ := res["isError"].(bool)
	content, ok := res["content"].([]any)
	if !ok || len(content) == 0 {
		t.Fatalf("%s: missing content: %v", name, res)
	}
	return content[0].(map[string]any)["text"].(string), isErr, nil, code
}

func callTool(t *testing.T, h http.Handler, name, args string) (map[string]any, bool, any, int) {
	t.Helper()
	text, isErr, rpcErr, code := callToolText(t, h, name, args)
	if rpcErr != nil {
		return nil, isErr, rpcErr, code
	}
	var payload map[string]any
	if json.Unmarshal([]byte(text), &payload) != nil {
		payload = map[string]any{"_text": text}
	}
	return payload, isErr, nil, code
}

func TestInitializeAndToolsListR_EEWI_J569(t *testing.T) {
	// R-EEWI-J569
	// R-FI1O-9E44
	h := newTestHandler(&fakeClient{}, &captureHandler{}, nil)

	init, rpcErr, code := rpc(t, h, "initialize", `{}`)
	if code != http.StatusOK || rpcErr != nil {
		t.Fatalf("initialize status=%d error=%v", code, rpcErr)
	}
	if init["protocolVersion"] != "2025-06-18" {
		t.Fatalf("protocolVersion = %v, want 2025-06-18", init["protocolVersion"])
	}
	serverInfo, _ := init["serverInfo"].(map[string]any)
	if serverInfo["name"] != "github" || serverInfo["version"] != "v-test" {
		t.Fatalf("serverInfo = %v", serverInfo)
	}

	res, rpcErr, code := rpc(t, h, "tools/list", `{}`)
	if code != http.StatusOK || rpcErr != nil {
		t.Fatalf("tools/list status=%d error=%v", code, rpcErr)
	}
	tools, _ := res["tools"].([]any)
	names := map[string]bool{}
	for _, raw := range tools {
		tl := raw.(map[string]any)
		name := tl["name"].(string)
		if strings.Contains(name, "ikigenba") || strings.Contains(name, "github_") {
			t.Fatalf("tool name is not bare: %q", name)
		}
		schema, ok := tl["inputSchema"].(map[string]any)
		if !ok || len(schema) == 0 || schema["type"] != "object" {
			t.Fatalf("%s has empty/non-object schema: %v", name, tl["inputSchema"])
		}
		names[name] = true
	}
	want := []string{
		"repos_list", "repo_get",
		"pr_list", "pr_get", "pr_comment", "pr_review", "pr_merge",
		"issue_list", "issue_get", "issue_create", "issue_comment", "issue_update",
		"file_get", "file_put",
		"health", "reflection",
	}
	for _, name := range want {
		if !names[name] {
			t.Errorf("missing tool %q", name)
		}
	}
	if len(names) != len(want) {
		t.Fatalf("got %d tools, want %d: %v", len(names), len(want), names)
	}
}

func TestMissingOrMalformedArgsDoNotCallClientR_EHCB_AONN(t *testing.T) {
	// R-EHCB-AONN
	fc := &fakeClient{}
	h := newTestHandler(fc, &captureHandler{}, nil)

	if _, isErr, rpcErr, _ := callTool(t, h, "pr_get", `{"repo":"repo"}`); rpcErr != nil || !isErr {
		t.Fatalf("missing number should be tool error, rpcErr=%v isErr=%v", rpcErr, isErr)
	}
	if len(fc.calls) != 0 {
		t.Fatalf("client called for missing number: %v", fc.calls)
	}
	if _, isErr, rpcErr, _ := callTool(t, h, "pr_get", `{"repo":"repo","number":"bad"}`); rpcErr != nil || !isErr {
		t.Fatalf("malformed number should be tool error, rpcErr=%v isErr=%v", rpcErr, isErr)
	}
	if len(fc.calls) != 0 {
		t.Fatalf("client called for malformed number: %v", fc.calls)
	}
}

func TestIdentityFromHeadersAndNoBearerParsingR_EIK7_OGEC(t *testing.T) {
	// R-EIK7-OGEC
	fc := &fakeClient{}
	cap := &captureHandler{}
	h := newTestHandler(fc, cap, nil)
	if _, isErr, rpcErr, _ := callTool(t, h, "issue_comment", `{"repo":"repo","number":9,"body":"hi"}`); rpcErr != nil || isErr {
		t.Fatalf("issue_comment failed rpcErr=%v isErr=%v", rpcErr, isErr)
	}
	if len(cap.records) != 1 {
		t.Fatalf("expected one provenance log, got %d", len(cap.records))
	}
	rec := cap.records[0]
	if rec["owner_email"] != "owner@example.com" || rec["client_id"] != "client-123" {
		t.Fatalf("identity not read from headers: %v", rec)
	}
	if rec["verb"] != "issue_comment" || rec["repo"] != "repo" || rec["number"] != int64(9) && rec["number"] != 9 {
		t.Fatalf("dispatch target not logged: %v", rec)
	}
}

func TestWriteProvenanceAndNoOwnerInClientRequestR_EJS4_2851(t *testing.T) {
	// R-EJS4-2851
	writes := []struct {
		name string
		args string
		path string
	}{
		{"pr_comment", `{"repo":"repo","number":1,"body":"body"}`, ""},
		{"pr_review", `{"repo":"repo","number":2,"event":"APPROVE","body":"body"}`, ""},
		{"pr_merge", `{"repo":"repo","number":3,"method":"squash"}`, ""},
		{"issue_create", `{"repo":"repo","title":"title","body":"body"}`, ""},
		{"issue_comment", `{"repo":"repo","number":4,"body":"body"}`, ""},
		{"issue_update", `{"repo":"repo","number":5,"state":"closed","labels":["bug"],"assignees":["octo"]}`, ""},
		{"file_put", `{"repo":"repo","path":"README.md","message":"msg","content":"hello","sha":"old"}`, "README.md"},
	}
	for _, tc := range writes {
		t.Run(tc.name, func(t *testing.T) {
			fc := &fakeClient{}
			cap := &captureHandler{}
			h := newTestHandler(fc, cap, nil)
			if _, isErr, rpcErr, _ := callTool(t, h, tc.name, tc.args); rpcErr != nil || isErr {
				t.Fatalf("%s failed rpcErr=%v isErr=%v", tc.name, rpcErr, isErr)
			}
			if len(cap.records) != 1 {
				t.Fatalf("expected exactly one log record, got %d", len(cap.records))
			}
			rec := cap.records[0]
			if rec["owner_email"] != "owner@example.com" || rec["verb"] != tc.name || rec["repo"] != "repo" {
				t.Fatalf("provenance attrs wrong: %v", rec)
			}
			if tc.path != "" && rec["path"] != tc.path {
				t.Fatalf("path target missing: %v", rec)
			}
			for _, field := range []string{fc.lastBody, fc.lastEvent, fc.lastMethod, fc.lastTitle, string(fc.lastFile.Content)} {
				if strings.Contains(field, "owner@example.com") || strings.Contains(field, "client-123") {
					t.Fatalf("owner identity leaked into client request field %q", field)
				}
			}
			if fc.lastPatch.State == "owner@example.com" {
				t.Fatal("owner identity leaked into issue patch")
			}
		})
	}

	fc := &fakeClient{}
	cap := &captureHandler{}
	h := newTestHandler(fc, cap, nil)
	if _, isErr, rpcErr, _ := callTool(t, h, "pr_get", `{"repo":"repo","number":1}`); rpcErr != nil || isErr {
		t.Fatalf("pr_get failed rpcErr=%v isErr=%v", rpcErr, isErr)
	}
	if len(cap.records) != 0 {
		t.Fatalf("read verb emitted provenance log: %v", cap.records)
	}
}

func TestHealthEnvelopeReflectsAuthCallR_EL00_FZVQ(t *testing.T) {
	// R-EL00-FZVQ
	calls := 0
	h := newTestHandler(&fakeClient{}, &captureHandler{}, func(context.Context) (map[string]any, error) {
		calls++
		return map[string]any{"github_auth": "ok"}, nil
	})
	payload, isErr, rpcErr, _ := callTool(t, h, "health", `{}`)
	if rpcErr != nil || isErr {
		t.Fatalf("health failed rpcErr=%v isErr=%v", rpcErr, isErr)
	}
	if calls != 1 || payload["status"] != "ok" || payload["service"] != "github" {
		t.Fatalf("health did not use faked auth reporter: calls=%d payload=%v", calls, payload)
	}
	details := payload["details"].(map[string]any)
	if details["github_auth"] != "ok" {
		t.Fatalf("health details = %v", details)
	}

	h = newTestHandler(&fakeClient{}, &captureHandler{}, func(context.Context) (map[string]any, error) {
		return nil, gh.ErrAppAuth
	})
	payload, isErr, rpcErr, _ = callTool(t, h, "health", `{}`)
	if rpcErr != nil || isErr {
		t.Fatalf("health auth failure should be an envelope, rpcErr=%v isErr=%v", rpcErr, isErr)
	}
	details = payload["details"].(map[string]any)
	if !strings.Contains(details["error"].(string), gh.ErrAppAuth.Error()) {
		t.Fatalf("auth error not surfaced: %v", payload)
	}
}

func TestRepoGetStructuredSuccessR_FJ9K_N5UT(t *testing.T) {
	// R-FJ9K-N5UT
	h := newTestHandler(&fakeClient{}, &captureHandler{}, nil)
	res, rpcErr, code := rpc(t, h, "tools/call", `{"name":"repo_get","arguments":{"repo":"known"}}`)
	if code != http.StatusOK || rpcErr != nil || res["isError"] == true {
		t.Fatalf("repo_get status=%d rpcErr=%v result=%v", code, rpcErr, res)
	}
	want := map[string]any{"name": "known", "full_name": "", "private": false, "default_branch": ""}
	if got := res["structuredContent"]; !mapsEqualJSON(got, want) {
		t.Fatalf("structuredContent = %v, want %v", got, want)
	}
	content := res["content"].([]any)
	var textValue any
	if err := json.Unmarshal([]byte(content[0].(map[string]any)["text"].(string)), &textValue); err != nil {
		t.Fatal(err)
	}
	if !mapsEqualJSON(textValue, want) {
		t.Fatalf("text JSON = %v, want %v", textValue, want)
	}
}

func TestAllToolsDeclareOutputSchemasR_FKHH_0XLI(t *testing.T) {
	// R-FKHH-0XLI
	h := newTestHandler(&fakeClient{}, &captureHandler{}, nil)
	res, rpcErr, _ := rpc(t, h, "tools/list", `{}`)
	if rpcErr != nil {
		t.Fatalf("tools/list error: %v", rpcErr)
	}
	tools := res["tools"].([]any)
	want := map[string]bool{
		"repos_list": false, "repo_get": false, "pr_list": false, "pr_get": false,
		"pr_comment": false, "pr_review": false, "pr_merge": false,
		"issue_list": false, "issue_get": false, "issue_create": false,
		"issue_comment": false, "issue_update": false, "file_get": false, "file_put": false,
		"health": false, "reflection": false,
	}
	for _, raw := range tools {
		got := raw.(map[string]any)
		name := got["name"].(string)
		if _, expected := want[name]; !expected {
			t.Fatalf("unexpected tool %q", name)
		}
		if got["outputSchema"] == nil {
			t.Errorf("%s has nil outputSchema", name)
		}
		want[name] = true
	}
	for name, found := range want {
		if !found {
			t.Errorf("missing tool %q", name)
		}
	}
}

func TestListAndFileShapesR_FLPD_EPC7(t *testing.T) {
	// R-FLPD-EPC7
	h := newTestHandler(&fakeClient{}, &captureHandler{}, nil)
	listed, rpcErr, _ := rpc(t, h, "tools/list", `{}`)
	if rpcErr != nil {
		t.Fatal(rpcErr)
	}
	schemas := map[string]map[string]any{}
	for _, raw := range listed["tools"].([]any) {
		entry := raw.(map[string]any)
		schemas[entry["name"].(string)] = entry["outputSchema"].(map[string]any)
	}
	for _, name := range []string{"repos_list", "pr_list", "issue_list"} {
		schema := schemas[name]
		if schema["type"] != "object" || !mapsEqualJSON(schema["required"], []any{"items"}) {
			t.Errorf("%s outputSchema = %v", name, schema)
		}
		res, callErr, _ := rpc(t, h, "tools/call", `{"name":"`+name+`","arguments":{"repo":"repo"}}`)
		if callErr != nil {
			t.Fatalf("%s error: %v", name, callErr)
		}
		structured, ok := res["structuredContent"].(map[string]any)
		if !ok || structured["items"] == nil {
			t.Errorf("%s structuredContent = %v, want object with items", name, res["structuredContent"])
		}
	}

	fileSchema := schemas["file_get"]
	props := fileSchema["properties"].(map[string]any)
	if len(props) != 3 || props["path"] == nil || props["sha"] == nil || props["encoding"] == nil || props["content"] != nil {
		t.Fatalf("file_get properties = %v, want exactly path/sha/encoding", props)
	}
	res, callErr, _ := rpc(t, h, "tools/call", `{"name":"file_get","arguments":{"repo":"repo","path":"README.md","ref":"abc"}}`)
	if callErr != nil {
		t.Fatal(callErr)
	}
	structured := res["structuredContent"].(map[string]any)
	if structured["content"] != nil || len(structured) != 3 {
		t.Fatalf("file_get structuredContent = %v, want metadata only", structured)
	}
}

func TestTypedClientErrorCodes(t *testing.T) {
	tests := []struct {
		name string
		err  error
		want string
		id   string
	}{
		{"not found", gh.ErrNotFound, "not_found", "R-FMX9-SH2W"},
		{"invalid", gh.ErrInvalid, "validation", "R-FO56-68TL"},
		{"app auth", gh.ErrAppAuth, "source_unavailable", "R-FPD2-K0KA"},
		{"transport", errors.New("transport failed"), "source_unavailable", "R-FPD2-K0KA"},
	}
	// R-FMX9-SH2W
	// R-FO56-68TL
	// R-FPD2-K0KA
	for _, tc := range tests {
		t.Run(tc.name, func(t *testing.T) {
			h := newTestHandler(&fakeClient{err: tc.err}, &captureHandler{}, nil)
			res, rpcErr, code := rpc(t, h, "tools/call", `{"name":"repos_list","arguments":{}}`)
			if code != http.StatusOK || rpcErr != nil || res["isError"] != true {
				t.Fatalf("%s status=%d rpcErr=%v result=%v", tc.id, code, rpcErr, res)
			}
			structured := res["structuredContent"].(map[string]any)
			if structured["code"] != tc.want {
				t.Fatalf("%s code = %v, want %s", tc.id, structured["code"], tc.want)
			}
		})
	}
}

func TestValidationCodeAndNoClientCallR_FQKY_XSAZ(t *testing.T) {
	// R-FQKY-XSAZ
	for _, args := range []string{`{"repo":"repo"}`, `{"repo":"repo","number":"bad"}`} {
		fc := &fakeClient{}
		h := newTestHandler(fc, &captureHandler{}, nil)
		res, rpcErr, _ := rpc(t, h, "tools/call", `{"name":"pr_get","arguments":`+args+`}`)
		if rpcErr != nil || res["isError"] != true {
			t.Fatalf("arguments %s result=%v rpcErr=%v", args, res, rpcErr)
		}
		structured := res["structuredContent"].(map[string]any)
		if structured["code"] != "validation" || len(fc.calls) != 0 {
			t.Fatalf("arguments %s code=%v calls=%v", args, structured["code"], fc.calls)
		}
	}
}

func mapsEqualJSON(a, b any) bool {
	ab, _ := json.Marshal(a)
	bb, _ := json.Marshal(b)
	var av, bv any
	_ = json.Unmarshal(ab, &av)
	_ = json.Unmarshal(bb, &bv)
	return reflect.DeepEqual(av, bv)
}

func TestReflectionReportsEmptyGraphR_EM7W_TRMF(t *testing.T) {
	// R-EM7W-TRMF
	h := newTestHandler(&fakeClient{}, &captureHandler{}, nil)
	payload, isErr, rpcErr, _ := callTool(t, h, "reflection", `{}`)
	if rpcErr != nil || isErr {
		t.Fatalf("reflection failed rpcErr=%v isErr=%v", rpcErr, isErr)
	}
	if publishes := payload["publishes"].([]any); len(publishes) != 0 {
		t.Fatalf("publishes not empty: %v", publishes)
	}
	if subscribes := payload["subscribes"].([]any); len(subscribes) != 0 {
		t.Fatalf("subscribes not empty: %v", subscribes)
	}
}

func TestClientErrorsBecomeToolResultsR_ENFT_7JD4(t *testing.T) {
	// R-ENFT-7JD4
	errs := []error{
		gh.ErrNotFound,
		gh.ErrInvalid,
		gh.ErrAppAuth,
		errors.New("github: transport failed"),
	}
	for _, err := range errs {
		t.Run(err.Error(), func(t *testing.T) {
			h := newTestHandler(&fakeClient{err: err}, &captureHandler{}, nil)
			payload, isErr, rpcErr, code := callTool(t, h, "repos_list", `{}`)
			if code != http.StatusOK || rpcErr != nil {
				t.Fatalf("transport not up: status=%d rpcErr=%v", code, rpcErr)
			}
			if !isErr {
				t.Fatalf("expected isError result, got %v", payload)
			}
			if !strings.Contains(payload["_text"].(string), err.Error()) {
				t.Fatalf("tool error not descriptive: %v", payload)
			}
		})
	}
}
