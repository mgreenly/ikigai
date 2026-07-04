package gh

import (
	"context"
	"encoding/base64"
	"encoding/json"
	"errors"
	"io"
	"net/http"
	"reflect"
	"strings"
	"testing"
	"time"
)

func TestClientReposListR_DVE4_ETB5(t *testing.T) {
	// R-DVE4-ETB5
	c := newRESTTestClient(t, func(req *http.Request) (*http.Response, error) {
		assertRESTRequest(t, req, http.MethodGet, "/orgs/acme/repos")
		if req.URL.RawQuery != "" {
			t.Fatalf("query = %q, want empty", req.URL.RawQuery)
		}
		return jsonResponse(http.StatusOK, `[{"name":"widgets","full_name":"acme/widgets","private":true,"default_branch":"main"}]`), nil
	})

	got, err := c.ReposList(context.Background())
	if err != nil {
		t.Fatalf("ReposList() error = %v", err)
	}
	want := []Repo{{Name: "widgets", FullName: "acme/widgets", Private: true, DefaultBranch: "main"}}
	if !reflect.DeepEqual(got, want) {
		t.Fatalf("ReposList() = %+v, want %+v", got, want)
	}
}

func TestClientRepoGetR_DWM0_SL1U(t *testing.T) {
	// R-DWM0-SL1U
	c := newRESTTestClient(t, func(req *http.Request) (*http.Response, error) {
		assertRESTRequest(t, req, http.MethodGet, "/repos/acme/widgets")
		return jsonResponse(http.StatusOK, `{"name":"widgets","full_name":"acme/widgets","private":false,"default_branch":"trunk"}`), nil
	})

	got, err := c.RepoGet(context.Background(), "widgets")
	if err != nil {
		t.Fatalf("RepoGet() error = %v", err)
	}
	if got.Name != "widgets" || got.FullName != "acme/widgets" || got.Private || got.DefaultBranch != "trunk" {
		t.Fatalf("RepoGet() = %+v, want decoded repo fields", got)
	}
}

func TestClientPRListR_DXTX_6CSJ(t *testing.T) {
	// R-DXTX-6CSJ
	c := newRESTTestClient(t, func(req *http.Request) (*http.Response, error) {
		assertRESTRequest(t, req, http.MethodGet, "/repos/acme/widgets/pulls")
		if got := req.URL.Query().Get("state"); got != "open" {
			t.Fatalf("state query = %q, want open", got)
		}
		return jsonResponse(http.StatusOK, `[{"number":7,"title":"Add thing","state":"open","body":"ready","html_url":"https://example.test/pr/7"}]`), nil
	})

	got, err := c.PRList(context.Background(), "widgets", "open")
	if err != nil {
		t.Fatalf("PRList() error = %v", err)
	}
	if len(got) != 1 || got[0].Number != 7 || got[0].Title != "Add thing" || got[0].State != "open" {
		t.Fatalf("PRList() = %+v, want decoded PR list", got)
	}
}

func TestClientPRGetR_DZ1T_K4J8(t *testing.T) {
	// R-DZ1T-K4J8
	var paths []string
	c := newRESTTestClient(t, func(req *http.Request) (*http.Response, error) {
		paths = append(paths, req.URL.Path)
		switch req.URL.Path {
		case "/repos/acme/widgets/pulls/7":
			assertRESTRequest(t, req, http.MethodGet, "/repos/acme/widgets/pulls/7")
			return jsonResponse(http.StatusOK, `{"number":7,"title":"Add thing","state":"open"}`), nil
		case "/repos/acme/widgets/pulls/7/files":
			assertRESTRequest(t, req, http.MethodGet, "/repos/acme/widgets/pulls/7/files")
			return jsonResponse(http.StatusOK, `[{"filename":"main.go","status":"modified","additions":3,"deletions":1,"changes":4},{"filename":"README.md","status":"added","additions":2,"deletions":0,"changes":2}]`), nil
		default:
			t.Fatalf("unexpected path %s", req.URL.Path)
			return nil, nil
		}
	})

	got, err := c.PRGet(context.Background(), "widgets", 7)
	if err != nil {
		t.Fatalf("PRGet() error = %v", err)
	}
	if !reflect.DeepEqual(paths, []string{"/repos/acme/widgets/pulls/7", "/repos/acme/widgets/pulls/7/files"}) {
		t.Fatalf("paths = %v, want PR then files endpoints", paths)
	}
	if got.Number != 7 || len(got.Files) != 2 || got.Files[0].Filename != "main.go" || got.Files[1].Filename != "README.md" {
		t.Fatalf("PRGet() = %+v, want PR detail with changed files", got)
	}
}

func TestClientPRCommentR_E09P_XW9X(t *testing.T) {
	// R-E09P-XW9X
	c := newRESTTestClient(t, func(req *http.Request) (*http.Response, error) {
		assertRESTRequest(t, req, http.MethodPost, "/repos/acme/widgets/issues/7/comments")
		body := readRequestObject(t, req)
		assertOnlyKeys(t, body, "body")
		if body["body"] != "exact comment" {
			t.Fatalf("body = %v, want exact supplied text", body["body"])
		}
		assertNoOwnerFields(t, body)
		return jsonResponse(http.StatusCreated, `{"id":12,"body":"exact comment","html_url":"https://example.test/comment/12"}`), nil
	})

	got, err := c.PRComment(context.Background(), "widgets", 7, "exact comment")
	if err != nil {
		t.Fatalf("PRComment() error = %v", err)
	}
	if got.ID != 12 || got.Body != "exact comment" {
		t.Fatalf("PRComment() = %+v, want decoded comment", got)
	}
}

func TestClientPRReviewR_E1HM_BO0M(t *testing.T) {
	// R-E1HM-BO0M
	c := newRESTTestClient(t, func(req *http.Request) (*http.Response, error) {
		assertRESTRequest(t, req, http.MethodPost, "/repos/acme/widgets/pulls/7/reviews")
		body := readRequestObject(t, req)
		assertOnlyKeys(t, body, "body", "event")
		if body["event"] != "REQUEST_CHANGES" {
			t.Fatalf("event = %v, want REQUEST_CHANGES", body["event"])
		}
		assertNoOwnerFields(t, body)
		return jsonResponse(http.StatusOK, `{"id":22,"state":"REQUEST_CHANGES","body":"needs work"}`), nil
	})

	got, err := c.PRReview(context.Background(), "widgets", 7, "REQUEST_CHANGES", "needs work")
	if err != nil {
		t.Fatalf("PRReview() error = %v", err)
	}
	if got.ID != 22 || got.State != "REQUEST_CHANGES" {
		t.Fatalf("PRReview() = %+v, want decoded review", got)
	}
}

func TestClientPRMergeR_E2PI_PFRB(t *testing.T) {
	// R-E2PI-PFRB
	c := newRESTTestClient(t, func(req *http.Request) (*http.Response, error) {
		assertRESTRequest(t, req, http.MethodPut, "/repos/acme/widgets/pulls/7/merge")
		body := readRequestObject(t, req)
		assertOnlyKeys(t, body, "merge_method")
		if body["merge_method"] != "squash" {
			t.Fatalf("merge_method = %v, want squash", body["merge_method"])
		}
		return jsonResponse(http.StatusOK, `{"sha":"abc123","merged":true,"message":"Pull Request successfully merged"}`), nil
	})

	got, err := c.PRMerge(context.Background(), "widgets", 7, "squash")
	if err != nil {
		t.Fatalf("PRMerge() error = %v", err)
	}
	if !got.Merged || got.SHA != "abc123" {
		t.Fatalf("PRMerge() = %+v, want decoded merge result", got)
	}
}

func TestClientIssueListR_E3XF_37I0(t *testing.T) {
	// R-E3XF-37I0
	c := newRESTTestClient(t, func(req *http.Request) (*http.Response, error) {
		assertRESTRequest(t, req, http.MethodGet, "/repos/acme/widgets/issues")
		if got := req.URL.Query().Get("state"); got != "all" {
			t.Fatalf("state query = %q, want all", got)
		}
		return jsonResponse(http.StatusOK, `[
			{"number":1,"title":"real issue","state":"open"},
			{"number":2,"title":"pr entry","state":"open","pull_request":{"url":"https://example.test/pr/2"}}
		]`), nil
	})

	got, err := c.IssueList(context.Background(), "widgets", "all")
	if err != nil {
		t.Fatalf("IssueList() error = %v", err)
	}
	if len(got) != 1 || got[0].Number != 1 || got[0].Title != "real issue" {
		t.Fatalf("IssueList() = %+v, want only non-PR issues", got)
	}
}

func TestClientIssueGetR_E55B_GZ8P(t *testing.T) {
	// R-E55B-GZ8P
	c := newRESTTestClient(t, func(req *http.Request) (*http.Response, error) {
		assertRESTRequest(t, req, http.MethodGet, "/repos/acme/widgets/issues/3")
		return jsonResponse(http.StatusOK, `{"number":3,"title":"bug","state":"open","body":"details"}`), nil
	})

	got, err := c.IssueGet(context.Background(), "widgets", 3)
	if err != nil {
		t.Fatalf("IssueGet() error = %v", err)
	}
	if got.Number != 3 || got.Title != "bug" || got.Body != "details" {
		t.Fatalf("IssueGet() = %+v, want decoded issue", got)
	}
}

func TestClientIssueCreateR_E6D7_UQZE(t *testing.T) {
	// R-E6D7-UQZE
	c := newRESTTestClient(t, func(req *http.Request) (*http.Response, error) {
		assertRESTRequest(t, req, http.MethodPost, "/repos/acme/widgets/issues")
		body := readRequestObject(t, req)
		assertOnlyKeys(t, body, "body", "title")
		if body["title"] != "new bug" || body["body"] != "details" {
			t.Fatalf("request body = %v, want supplied title and body", body)
		}
		assertNoOwnerFields(t, body)
		return jsonResponse(http.StatusCreated, `{"number":4,"title":"new bug","state":"open","body":"details"}`), nil
	})

	got, err := c.IssueCreate(context.Background(), "widgets", "new bug", "details")
	if err != nil {
		t.Fatalf("IssueCreate() error = %v", err)
	}
	if got.Number != 4 || got.Title != "new bug" {
		t.Fatalf("IssueCreate() = %+v, want decoded issue", got)
	}
}

func TestClientIssueCommentR_E7L4_8IQ3(t *testing.T) {
	// R-E7L4-8IQ3
	c := newRESTTestClient(t, func(req *http.Request) (*http.Response, error) {
		assertRESTRequest(t, req, http.MethodPost, "/repos/acme/widgets/issues/4/comments")
		body := readRequestObject(t, req)
		assertOnlyKeys(t, body, "body")
		if body["body"] != "exact issue comment" {
			t.Fatalf("body = %v, want exact supplied text", body["body"])
		}
		assertNoOwnerFields(t, body)
		return jsonResponse(http.StatusCreated, `{"id":44,"body":"exact issue comment"}`), nil
	})

	got, err := c.IssueComment(context.Background(), "widgets", 4, "exact issue comment")
	if err != nil {
		t.Fatalf("IssueComment() error = %v", err)
	}
	if got.ID != 44 || got.Body != "exact issue comment" {
		t.Fatalf("IssueComment() = %+v, want decoded comment", got)
	}
}

func TestClientIssueUpdateR_EA0X_027H(t *testing.T) {
	// R-EA0X-027H
	c := newRESTTestClient(t, func(req *http.Request) (*http.Response, error) {
		assertRESTRequest(t, req, http.MethodPatch, "/repos/acme/widgets/issues/4")
		body := readRequestObject(t, req)
		assertOnlyKeys(t, body, "assignees", "labels", "state")
		if body["state"] != "closed" {
			t.Fatalf("state = %v, want closed", body["state"])
		}
		if !reflect.DeepEqual(body["labels"], []any{"triaged", "bug"}) {
			t.Fatalf("labels = %#v, want supplied labels", body["labels"])
		}
		if !reflect.DeepEqual(body["assignees"], []any{"maintainer"}) {
			t.Fatalf("assignees = %#v, want supplied assignees", body["assignees"])
		}
		assertNoOwnerFields(t, body)
		return jsonResponse(http.StatusOK, `{"number":4,"title":"new bug","state":"closed"}`), nil
	})

	got, err := c.IssueUpdate(context.Background(), "widgets", 4, IssuePatch{
		State:     "closed",
		Labels:    []string{"triaged", "bug"},
		Assignees: []string{"maintainer"},
	})
	if err != nil {
		t.Fatalf("IssueUpdate() error = %v", err)
	}
	if got.Number != 4 || got.State != "closed" {
		t.Fatalf("IssueUpdate() = %+v, want decoded issue", got)
	}
}

func TestClientFileGetR_EB8T_DTY6(t *testing.T) {
	// R-EB8T-DTY6
	encoded := base64.StdEncoding.EncodeToString([]byte("hello\nworld\n"))
	c := newRESTTestClient(t, func(req *http.Request) (*http.Response, error) {
		assertRESTRequest(t, req, http.MethodGet, "/repos/acme/widgets/contents/docs/readme.md")
		if got := req.URL.Query().Get("ref"); got != "main" {
			t.Fatalf("ref query = %q, want main", got)
		}
		return jsonResponse(http.StatusOK, `{"path":"docs/readme.md","sha":"abc123","encoding":"base64","content":"`+encoded+`"}`), nil
	})

	got, err := c.FileGet(context.Background(), "widgets", "docs/readme.md", "main")
	if err != nil {
		t.Fatalf("FileGet() error = %v", err)
	}
	if got.Path != "docs/readme.md" || string(got.Content) != "hello\nworld\n" {
		t.Fatalf("FileGet() = %+v with content %q, want decoded file bytes", got, string(got.Content))
	}
}

func TestClientFilePutR_ECGP_RLOV(t *testing.T) {
	// R-ECGP-RLOV
	c := newRESTTestClient(t, func(req *http.Request) (*http.Response, error) {
		assertRESTRequest(t, req, http.MethodPut, "/repos/acme/widgets/contents/docs/readme.md")
		body := readRequestObject(t, req)
		assertOnlyKeys(t, body, "content", "message", "sha")
		if body["message"] != "update docs" || body["sha"] != "old-sha" {
			t.Fatalf("request body = %v, want message and optional sha", body)
		}
		if body["content"] != base64.StdEncoding.EncodeToString([]byte("new contents")) {
			t.Fatalf("content = %v, want base64-encoded file bytes", body["content"])
		}
		assertNoOwnerFields(t, body)
		return jsonResponse(http.StatusOK, `{"content":{"path":"docs/readme.md","sha":"new-sha"},"commit":{"sha":"commit-sha","message":"update docs"}}`), nil
	})

	got, err := c.FilePut(context.Background(), "widgets", "docs/readme.md", FilePut{Message: "update docs", Content: []byte("new contents"), SHA: "old-sha"})
	if err != nil {
		t.Fatalf("FilePut() error = %v", err)
	}
	if got.Content.SHA != "new-sha" || got.Commit.SHA != "commit-sha" {
		t.Fatalf("FilePut() = %+v, want decoded commit response", got)
	}
}

func TestClientStatusErrorsR_D0IM_VQ7H(t *testing.T) {
	// R-D0IM-VQ7H
	tests := []struct {
		name       string
		statusCode int
		want       error
	}{
		{name: "not found", statusCode: http.StatusNotFound, want: ErrNotFound},
		{name: "unprocessable", statusCode: http.StatusUnprocessableEntity, want: ErrInvalid},
		{name: "generic", statusCode: http.StatusInternalServerError},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			c := newRESTTestClient(t, func(req *http.Request) (*http.Response, error) {
				assertRESTRequest(t, req, http.MethodGet, "/repos/acme/widgets")
				return jsonResponse(tt.statusCode, `{"message":"failed status"}`), nil
			})

			_, err := c.RepoGet(context.Background(), "widgets")
			if err == nil {
				t.Fatal("RepoGet() error = nil, want non-2xx failure")
			}
			if tt.want != nil && !errors.Is(err, tt.want) {
				t.Fatalf("RepoGet() error = %v, want errors.Is %v", err, tt.want)
			}
			if tt.want == nil && (errors.Is(err, ErrNotFound) || errors.Is(err, ErrInvalid)) {
				t.Fatalf("RepoGet() error = %v, want generic status error", err)
			}
			if !strings.Contains(err.Error(), "failed status") {
				t.Fatalf("RepoGet() error = %v, want response status body included", err)
			}
		})
	}
}

func newRESTTestClient(t *testing.T, fn func(*http.Request) (*http.Response, error)) *Client {
	t.Helper()
	withAPIBase(t, "https://stub.github.test")
	now := time.Date(2026, 7, 4, 12, 0, 0, 0, time.UTC)
	return &Client{
		org: "acme",
		ts: &tokenSource{
			cached: "test-token",
			expiry: now.Add(time.Hour),
			now:    func() time.Time { return now },
		},
		http: stubClient(fn),
	}
}

func assertRESTRequest(t *testing.T, req *http.Request, method, path string) {
	t.Helper()
	if req.Method != method {
		t.Fatalf("method = %s, want %s", req.Method, method)
	}
	if req.URL.Path != path {
		t.Fatalf("path = %s, want %s", req.URL.Path, path)
	}
	if got := req.Header.Get("Authorization"); got != "Bearer test-token" {
		t.Fatalf("Authorization = %q, want bearer installation token", got)
	}
	if got := req.Header.Get("Accept"); got != "application/vnd.github+json" {
		t.Fatalf("Accept = %q, want GitHub JSON media type", got)
	}
	if got := req.Header.Get("X-GitHub-Api-Version"); got != "2022-11-28" {
		t.Fatalf("X-GitHub-Api-Version = %q, want 2022-11-28", got)
	}
}

func readRequestObject(t *testing.T, req *http.Request) map[string]any {
	t.Helper()
	data, err := io.ReadAll(req.Body)
	if err != nil {
		t.Fatalf("read request body: %v", err)
	}
	var out map[string]any
	if err := json.Unmarshal(data, &out); err != nil {
		t.Fatalf("decode request body %q: %v", string(data), err)
	}
	return out
}

func assertOnlyKeys(t *testing.T, body map[string]any, keys ...string) {
	t.Helper()
	want := make(map[string]bool)
	for _, key := range keys {
		want[key] = true
	}
	for key := range body {
		if !want[key] {
			t.Fatalf("request body key %q was not expected in %v", key, keys)
		}
	}
	for key := range want {
		if _, ok := body[key]; !ok {
			t.Fatalf("request body missing key %q in %v", key, body)
		}
	}
}

func assertNoOwnerFields(t *testing.T, body map[string]any) {
	t.Helper()
	for _, key := range []string{"author", "committer", "owner"} {
		if _, ok := body[key]; ok {
			t.Fatalf("request body contains owner-identifying key %q: %v", key, body)
		}
	}
}
