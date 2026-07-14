// Package mcp registers github's domain tools with the shared appkit MCP transport.
package mcp

import (
	"context"
	"net/http"

	"appkit"
	appkitmcp "appkit/mcp"

	gh "github/internal/gh"
)

const instructions = "Use these tools to read and update repositories in the configured GitHub organization."

// GitHubClient is the slice of the GitHub client driven by the MCP tools.
type GitHubClient interface {
	ReposList(ctx context.Context) ([]gh.Repo, error)
	RepoGet(ctx context.Context, repo string) (gh.Repo, error)
	PRList(ctx context.Context, repo, state string) ([]gh.PR, error)
	PRGet(ctx context.Context, repo string, number int) (gh.PRDetail, error)
	PRComment(ctx context.Context, repo string, number int, body string) (gh.Comment, error)
	PRReview(ctx context.Context, repo string, number int, event, body string) (gh.Review, error)
	PRMerge(ctx context.Context, repo string, number int, method string) (gh.MergeResult, error)
	IssueList(ctx context.Context, repo, state string) ([]gh.Issue, error)
	IssueGet(ctx context.Context, repo string, number int) (gh.Issue, error)
	IssueCreate(ctx context.Context, repo, title, body string) (gh.Issue, error)
	IssueComment(ctx context.Context, repo string, number int, body string) (gh.Comment, error)
	IssueUpdate(ctx context.Context, repo string, number int, patch gh.IssuePatch) (gh.Issue, error)
	FileGet(ctx context.Context, repo, path, ref string) (gh.FileContent, error)
	FilePut(ctx context.Context, repo, path string, in gh.FilePut) (gh.FileCommit, error)
}

// NewHandler builds the POST /mcp handler from the appkit Router seam.
func NewHandler(client GitHubClient, rt *appkit.Router) (http.Handler, error) {
	return appkitmcp.New(appkitmcp.Options{
		Service:       rt.Service(),
		Version:       rt.Version(),
		Instructions:  instructions,
		Tools:         Tools(client, rt.Logger()),
		Health:        rt.Health(),
		Events:        rt.Events(),
		Subscriptions: rt.Subscriptions(),
	})
}
