package repos

import (
	"bytes"
	"context"
	"encoding/json"
	"fmt"
	"io"
	"net/http"
	"strings"

	"registry"
)

// Identity is asserted by loopback callers on every GitHub peer request.
type Identity struct {
	OwnerEmail string
	SessionID  string
}

type Issue struct {
	Number int    `json:"number"`
	Title  string `json:"title"`
	Body   string `json:"body"`
}

type Comment struct {
	Body string `json:"body"`
}

type PR struct {
	Number int    `json:"number"`
	URL    string `json:"url"`
}

// GitHubPeer is the sole GitHub-service client used by the repository runner.
type GitHubPeer struct {
	baseURL string
	http    *http.Client
}

func NewGitHubPeer(client *http.Client) *GitHubPeer {
	return NewGitHubPeerAt(registry.BaseURL("github"), client)
}

// NewGitHubPeerAt constructs a peer for a supplied endpoint, primarily for
// recording local integration servers.
func NewGitHubPeerAt(baseURL string, client *http.Client) *GitHubPeer {
	if client == nil {
		client = http.DefaultClient
	}
	return &GitHubPeer{baseURL: baseURL, http: client}
}

func (p *GitHubPeer) IssueGet(ctx context.Context, id Identity, repo string, n int) (Issue, error) {
	var issue Issue
	err := p.call(ctx, id, "issue_get", map[string]any{"repo": repo, "number": n}, &issue)
	return issue, err
}

func (p *GitHubPeer) IssueComments(ctx context.Context, id Identity, repo string, n int) ([]Comment, error) {
	var comments []Comment
	err := p.call(ctx, id, "issue_comments", map[string]any{"repo": repo, "number": n}, &comments)
	return comments, err
}

func (p *GitHubPeer) IssueComment(ctx context.Context, id Identity, repo string, n int, body string) error {
	return p.call(ctx, id, "issue_comment", map[string]any{"repo": repo, "number": n, "body": body}, nil)
}

func (p *GitHubPeer) LabelAdd(ctx context.Context, id Identity, repo string, n int, labels []string) error {
	return p.call(ctx, id, "label_add", map[string]any{"repo": repo, "number": n, "labels": labels}, nil)
}

func (p *GitHubPeer) LabelRemove(ctx context.Context, id Identity, repo string, n int, label string) error {
	return p.call(ctx, id, "label_remove", map[string]any{"repo": repo, "number": n, "label": label}, nil)
}

func (p *GitHubPeer) PRCreate(ctx context.Context, id Identity, repo, title, head, base, body string) (PR, error) {
	var pr PR
	err := p.call(ctx, id, "pr_create", map[string]any{
		"repo": repo, "title": title, "head": head, "base": base, "body": body,
	}, &pr)
	return pr, err
}

func (p *GitHubPeer) call(ctx context.Context, id Identity, name string, arguments map[string]any, target any) error {
	payload := map[string]any{
		"jsonrpc": "2.0", "id": 1, "method": "tools/call",
		"params": map[string]any{"name": name, "arguments": arguments},
	}
	body, err := json.Marshal(payload)
	if err != nil {
		return fmt.Errorf("github peer %s: encode: %w", name, err)
	}
	req, err := http.NewRequestWithContext(ctx, http.MethodPost, strings.TrimRight(p.baseURL, "/")+"/mcp", bytes.NewReader(body))
	if err != nil {
		return fmt.Errorf("github peer %s: request: %w", name, err)
	}
	req.Header.Set("Content-Type", "application/json")
	req.Header.Set("X-Owner-Email", id.OwnerEmail)
	req.Header.Set("X-Client-Id", "repos:"+id.SessionID)
	response, err := p.http.Do(req)
	if err != nil {
		return fmt.Errorf("github peer %s: %w", name, err)
	}
	defer response.Body.Close()
	data, err := io.ReadAll(response.Body)
	if err != nil {
		return fmt.Errorf("github peer %s: read: %w", name, err)
	}
	if response.StatusCode < 200 || response.StatusCode >= 300 {
		return fmt.Errorf("github peer %s: status %s: %s", name, response.Status, strings.TrimSpace(string(data)))
	}
	var envelope struct {
		Result json.RawMessage `json:"result"`
		Error  *struct {
			Message string `json:"message"`
		} `json:"error"`
	}
	if len(data) == 0 {
		return nil
	}
	if err := json.Unmarshal(data, &envelope); err != nil {
		return fmt.Errorf("github peer %s: decode: %w", name, err)
	}
	if envelope.Error != nil {
		return fmt.Errorf("github peer %s: %s", name, envelope.Error.Message)
	}
	if target == nil || len(envelope.Result) == 0 || string(envelope.Result) == "null" {
		return nil
	}
	result := envelope.Result
	var mcp struct {
		Content []struct {
			Text string `json:"text"`
		} `json:"content"`
	}
	if json.Unmarshal(result, &mcp) == nil && len(mcp.Content) > 0 && mcp.Content[0].Text != "" {
		result = json.RawMessage(mcp.Content[0].Text)
	}
	if err := json.Unmarshal(result, target); err != nil {
		return fmt.Errorf("github peer %s: decode result: %w", name, err)
	}
	return nil
}
