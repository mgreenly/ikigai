package gh

import (
	"bytes"
	"context"
	"encoding/base64"
	"encoding/json"
	"errors"
	"fmt"
	"io"
	"net/http"
	"net/url"
	"strings"
)

var (
	// ErrNotFound marks GitHub resources that returned 404.
	ErrNotFound = errors.New("github: not found")

	// ErrInvalid marks GitHub validation or unprocessable-entity responses.
	ErrInvalid = errors.New("github: unprocessable")
)

// Client is a small org-scoped GitHub REST v3 client.
type Client struct {
	org  string
	ts   *tokenSource
	http *http.Client
}

// Repo is the repository shape exposed by the tool surface.
type Repo struct {
	Name          string `json:"name"`
	FullName      string `json:"full_name"`
	Private       bool   `json:"private"`
	DefaultBranch string `json:"default_branch"`
}

// PR is the pull request summary shape exposed by the tool surface.
type PR struct {
	Number  int    `json:"number"`
	Title   string `json:"title"`
	State   string `json:"state"`
	Body    string `json:"body"`
	HTMLURL string `json:"html_url"`
}

// PRFile is a changed file on a pull request.
type PRFile struct {
	Filename  string `json:"filename"`
	Status    string `json:"status"`
	Additions int    `json:"additions"`
	Deletions int    `json:"deletions"`
	Changes   int    `json:"changes"`
	Patch     string `json:"patch"`
}

// PRDetail contains a pull request and its changed files.
type PRDetail struct {
	PR
	Files []PRFile
}

// Issue is the issue shape exposed by the tool surface.
type Issue struct {
	Number    int     `json:"number"`
	Title     string  `json:"title"`
	State     string  `json:"state"`
	Body      string  `json:"body"`
	HTMLURL   string  `json:"html_url"`
	Labels    []Label `json:"labels,omitempty"`
	User      User    `json:"user,omitempty"`
	Assignees []User  `json:"assignees,omitempty"`
}

// Label is a GitHub issue label.
type Label struct {
	Name  string `json:"name"`
	Color string `json:"color"`
}

// User is the minimal GitHub user shape used in issue payloads.
type User struct {
	Login string `json:"login"`
}

// IssuePatch is the partial issue update payload.
type IssuePatch struct {
	State     string
	Labels    []string
	Assignees []string
}

// Comment is a GitHub issue or pull request comment.
type Comment struct {
	ID      int64  `json:"id"`
	Body    string `json:"body"`
	HTMLURL string `json:"html_url"`
}

// Review is a GitHub pull request review.
type Review struct {
	ID      int64  `json:"id"`
	State   string `json:"state"`
	Body    string `json:"body"`
	HTMLURL string `json:"html_url"`
}

// MergeResult is the response from merging a pull request.
type MergeResult struct {
	SHA     string `json:"sha"`
	Merged  bool   `json:"merged"`
	Message string `json:"message"`
}

// FileContent is a repository file returned from GitHub contents API.
type FileContent struct {
	Path     string `json:"path"`
	SHA      string `json:"sha"`
	Encoding string `json:"encoding"`
	Content  []byte `json:"-"`
}

// FilePut is the requested file write.
type FilePut struct {
	Message string
	Content []byte
	SHA     string
}

// FileCommit is the response from creating or updating a file.
type FileCommit struct {
	Content FileContent `json:"content"`
	Commit  Commit      `json:"commit"`
}

// Commit is the minimal commit response shape.
type Commit struct {
	SHA     string `json:"sha"`
	Message string `json:"message"`
}

// ReposList lists repositories in the configured organization.
func (c *Client) ReposList(ctx context.Context) ([]Repo, error) {
	var out []Repo
	err := c.get(ctx, []string{"orgs", c.org, "repos"}, nil, &out)
	return out, err
}

// RepoGet fetches one repository in the configured organization.
func (c *Client) RepoGet(ctx context.Context, repo string) (Repo, error) {
	var out Repo
	err := c.get(ctx, []string{"repos", c.org, repo}, nil, &out)
	return out, err
}

// PRList lists pull requests in a repository.
func (c *Client) PRList(ctx context.Context, repo, state string) ([]PR, error) {
	var out []PR
	err := c.get(ctx, []string{"repos", c.org, repo, "pulls"}, values("state", state), &out)
	return out, err
}

// PRGet fetches a pull request and its changed files.
func (c *Client) PRGet(ctx context.Context, repo string, number int) (PRDetail, error) {
	var pr PR
	if err := c.get(ctx, []string{"repos", c.org, repo, "pulls", fmt.Sprint(number)}, nil, &pr); err != nil {
		return PRDetail{}, err
	}
	var files []PRFile
	if err := c.get(ctx, []string{"repos", c.org, repo, "pulls", fmt.Sprint(number), "files"}, nil, &files); err != nil {
		return PRDetail{}, err
	}
	return PRDetail{PR: pr, Files: files}, nil
}

// PRComment creates a pull request comment through the issue comments endpoint.
func (c *Client) PRComment(ctx context.Context, repo string, number int, body string) (Comment, error) {
	var out Comment
	err := c.post(ctx, []string{"repos", c.org, repo, "issues", fmt.Sprint(number), "comments"}, map[string]string{"body": body}, &out)
	return out, err
}

// PRReview creates a pull request review.
func (c *Client) PRReview(ctx context.Context, repo string, number int, event, body string) (Review, error) {
	var out Review
	err := c.post(ctx, []string{"repos", c.org, repo, "pulls", fmt.Sprint(number), "reviews"}, map[string]string{"event": event, "body": body}, &out)
	return out, err
}

// PRMerge merges a pull request.
func (c *Client) PRMerge(ctx context.Context, repo string, number int, method string) (MergeResult, error) {
	var out MergeResult
	err := c.put(ctx, []string{"repos", c.org, repo, "pulls", fmt.Sprint(number), "merge"}, map[string]string{"merge_method": method}, &out)
	return out, err
}

// IssueList lists issues in a repository, excluding pull requests.
func (c *Client) IssueList(ctx context.Context, repo, state string) ([]Issue, error) {
	var payload []issueListItem
	if err := c.get(ctx, []string{"repos", c.org, repo, "issues"}, values("state", state), &payload); err != nil {
		return nil, err
	}
	issues := make([]Issue, 0, len(payload))
	for _, item := range payload {
		if item.PullRequest != nil {
			continue
		}
		issues = append(issues, item.Issue)
	}
	return issues, nil
}

// IssueGet fetches one issue.
func (c *Client) IssueGet(ctx context.Context, repo string, number int) (Issue, error) {
	var out Issue
	err := c.get(ctx, []string{"repos", c.org, repo, "issues", fmt.Sprint(number)}, nil, &out)
	return out, err
}

// IssueCreate creates an issue.
func (c *Client) IssueCreate(ctx context.Context, repo, title, body string) (Issue, error) {
	var out Issue
	err := c.post(ctx, []string{"repos", c.org, repo, "issues"}, map[string]string{"title": title, "body": body}, &out)
	return out, err
}

// IssueComment creates an issue comment.
func (c *Client) IssueComment(ctx context.Context, repo string, number int, body string) (Comment, error) {
	var out Comment
	err := c.post(ctx, []string{"repos", c.org, repo, "issues", fmt.Sprint(number), "comments"}, map[string]string{"body": body}, &out)
	return out, err
}

// IssueUpdate updates selected issue fields.
func (c *Client) IssueUpdate(ctx context.Context, repo string, number int, patch IssuePatch) (Issue, error) {
	body := make(map[string]any)
	if patch.State != "" {
		body["state"] = patch.State
	}
	if patch.Labels != nil {
		body["labels"] = patch.Labels
	}
	if patch.Assignees != nil {
		body["assignees"] = patch.Assignees
	}

	var out Issue
	err := c.patch(ctx, []string{"repos", c.org, repo, "issues", fmt.Sprint(number)}, body, &out)
	return out, err
}

// FileGet fetches and decodes repository file content.
func (c *Client) FileGet(ctx context.Context, repo, path, ref string) (FileContent, error) {
	var payload struct {
		Path     string `json:"path"`
		SHA      string `json:"sha"`
		Encoding string `json:"encoding"`
		Content  string `json:"content"`
	}
	if err := c.get(ctx, []string{"repos", c.org, repo, "contents", path}, values("ref", ref), &payload); err != nil {
		return FileContent{}, err
	}
	decoded, err := base64.StdEncoding.DecodeString(strings.ReplaceAll(payload.Content, "\n", ""))
	if err != nil {
		return FileContent{}, fmt.Errorf("github: decode file content: %v", err)
	}
	return FileContent{
		Path:     payload.Path,
		SHA:      payload.SHA,
		Encoding: payload.Encoding,
		Content:  decoded,
	}, nil
}

// FilePut writes repository file content as the GitHub App identity.
func (c *Client) FilePut(ctx context.Context, repo, path string, in FilePut) (FileCommit, error) {
	body := map[string]string{
		"message": in.Message,
		"content": base64.StdEncoding.EncodeToString(in.Content),
	}
	if in.SHA != "" {
		body["sha"] = in.SHA
	}
	var out FileCommit
	err := c.put(ctx, []string{"repos", c.org, repo, "contents", path}, body, &out)
	return out, err
}

type issueListItem struct {
	Issue
	PullRequest *json.RawMessage `json:"pull_request"`
}

func (c *Client) get(ctx context.Context, parts []string, query url.Values, out any) error {
	return c.doJSON(ctx, http.MethodGet, parts, query, nil, out)
}

func (c *Client) post(ctx context.Context, parts []string, in any, out any) error {
	return c.doJSON(ctx, http.MethodPost, parts, nil, in, out)
}

func (c *Client) put(ctx context.Context, parts []string, in any, out any) error {
	return c.doJSON(ctx, http.MethodPut, parts, nil, in, out)
}

func (c *Client) patch(ctx context.Context, parts []string, in any, out any) error {
	return c.doJSON(ctx, http.MethodPatch, parts, nil, in, out)
}

func (c *Client) doJSON(ctx context.Context, method string, parts []string, query url.Values, in any, out any) error {
	req, err := c.request(ctx, method, parts, query, in)
	if err != nil {
		return err
	}
	resp, err := c.do(req)
	if err != nil {
		return err
	}
	defer closeBody(resp)
	if err := githubStatusError(resp); err != nil {
		return err
	}
	if out == nil {
		return nil
	}
	if err := json.NewDecoder(resp.Body).Decode(out); err != nil {
		return fmt.Errorf("github: decode response: %v", err)
	}
	return nil
}

func (c *Client) request(ctx context.Context, method string, parts []string, query url.Values, in any) (*http.Request, error) {
	pathParts := append([]string{apiBase}, parts...)
	reqURL, err := url.JoinPath(pathParts[0], pathParts[1:]...)
	if err != nil {
		return nil, fmt.Errorf("github: build request URL: %v", err)
	}
	if len(query) > 0 {
		u, err := url.Parse(reqURL)
		if err != nil {
			return nil, fmt.Errorf("github: parse request URL: %v", err)
		}
		u.RawQuery = query.Encode()
		reqURL = u.String()
	}

	var body io.Reader
	if in != nil {
		data, err := json.Marshal(in)
		if err != nil {
			return nil, fmt.Errorf("github: encode request: %v", err)
		}
		body = bytes.NewReader(data)
	}
	req, err := http.NewRequestWithContext(ctx, method, reqURL, body)
	if err != nil {
		return nil, fmt.Errorf("github: build request: %v", err)
	}
	req.Header.Set("Accept", "application/vnd.github+json")
	req.Header.Set("X-GitHub-Api-Version", "2022-11-28")
	if in != nil {
		req.Header.Set("Content-Type", "application/json")
	}
	return req, nil
}

func (c *Client) do(req *http.Request) (*http.Response, error) {
	resp, err := c.doOnce(req, false)
	if err != nil {
		return nil, err
	}
	if resp.StatusCode != http.StatusUnauthorized {
		return resp, nil
	}
	closeBody(resp)

	resp, err = c.doOnce(req, true)
	if err != nil {
		return nil, err
	}
	if resp.StatusCode == http.StatusUnauthorized {
		closeBody(resp)
		return nil, fmt.Errorf("github: unauthorized after token refresh: %s", resp.Status)
	}
	return resp, nil
}

func (c *Client) doOnce(req *http.Request, force bool) (*http.Response, error) {
	if c.ts == nil {
		return nil, fmt.Errorf("%w: missing token source", ErrAppAuth)
	}
	token, err := c.ts.token(req.Context(), force)
	if err != nil {
		return nil, err
	}

	next := req.Clone(req.Context())
	if req.Body != nil && req.GetBody != nil {
		body, err := req.GetBody()
		if err != nil {
			return nil, err
		}
		next.Body = body
	}
	next.Header = req.Header.Clone()
	next.Header.Set("Authorization", "Bearer "+token)
	return c.client().Do(next)
}

func (c *Client) client() *http.Client {
	if c.http != nil {
		return c.http
	}
	if c.ts != nil {
		return c.ts.client()
	}
	return http.DefaultClient
}

func githubStatusError(resp *http.Response) error {
	if resp.StatusCode >= 200 && resp.StatusCode <= 299 {
		return nil
	}
	message := readErrorMessage(resp.Body)
	switch resp.StatusCode {
	case http.StatusNotFound:
		if message == "" {
			return fmt.Errorf("%w: %s", ErrNotFound, resp.Status)
		}
		return fmt.Errorf("%w: %s: %s", ErrNotFound, resp.Status, message)
	case http.StatusUnprocessableEntity:
		if message == "" {
			return fmt.Errorf("%w: %s", ErrInvalid, resp.Status)
		}
		return fmt.Errorf("%w: %s: %s", ErrInvalid, resp.Status, message)
	default:
		if message == "" {
			return fmt.Errorf("github: request failed: %s", resp.Status)
		}
		return fmt.Errorf("github: request failed: %s: %s", resp.Status, message)
	}
}

func values(key, value string) url.Values {
	if value == "" {
		return nil
	}
	out := make(url.Values)
	out.Set(key, value)
	return out
}
