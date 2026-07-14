package mcp

import (
	"context"
	"encoding/json"
	"errors"
	"fmt"
	"log/slog"

	appkitmcp "appkit/mcp"
	"appkit/server"

	gh "github/internal/gh"
)

const toolPrefix = ""

func tool(verb string) string { return toolPrefix + verb }

// Tools declares github's domain surface. Transport-owned health and reflection
// tools are appended by appkit/mcp.
func Tools(client GitHubClient, logger *slog.Logger) []appkitmcp.Tool {
	if client == nil {
		panic("mcp: github client is required")
	}
	if logger == nil {
		logger = slog.New(slog.DiscardHandler)
	}

	return []appkitmcp.Tool{
		{
			Name: tool("repos_list"), Description: "List repositories in the configured GitHub organization.",
			InputSchema: obj(map[string]any{}), OutputSchema: listOutput(repoOutput()),
			Handler: func(ctx context.Context, _ json.RawMessage, _ server.Identity) (map[string]any, error) {
				repos, err := client.ReposList(ctx)
				return clientResult(map[string]any{"items": repos}, err)
			},
		},
		{
			Name: tool("repo_get"), Description: "Fetch one repository by name.",
			InputSchema: obj(map[string]any{"repo": descTyp("string", "repository name")}, "repo"), OutputSchema: repoOutput(),
			Handler: func(ctx context.Context, raw json.RawMessage, _ server.Identity) (map[string]any, error) {
				var a struct {
					Repo string `json:"repo"`
				}
				if err := decodeAndValidate(raw, &a, func() error { return requireString("repo", a.Repo) }); err != nil {
					return validationResult(err), nil
				}
				v, err := client.RepoGet(ctx, a.Repo)
				return clientResult(v, err)
			},
		},
		{
			Name: tool("pr_list"), Description: "List pull requests in a repository.",
			InputSchema: obj(map[string]any{"repo": descTyp("string", "repository name"), "state": descTyp("string", "optional state filter")}, "repo"), OutputSchema: listOutput(prOutput()),
			Handler: func(ctx context.Context, raw json.RawMessage, _ server.Identity) (map[string]any, error) {
				var a struct{ Repo, State string }
				if err := decodeAndValidate(raw, &a, func() error { return requireString("repo", a.Repo) }); err != nil {
					return validationResult(err), nil
				}
				v, err := client.PRList(ctx, a.Repo, a.State)
				return clientResult(map[string]any{"items": v}, err)
			},
		},
		{
			Name: tool("pr_get"), Description: "Fetch one pull request with changed files.",
			InputSchema: obj(map[string]any{"repo": descTyp("string", "repository name"), "number": descTyp("integer", "pull request number")}, "repo", "number"), OutputSchema: prDetailOutput(),
			Handler: func(ctx context.Context, raw json.RawMessage, _ server.Identity) (map[string]any, error) {
				var a repoNumberArgs
				if err := decodeAndValidate(raw, &a, func() error { return a.validate("number") }); err != nil {
					return validationResult(err), nil
				}
				v, err := client.PRGet(ctx, a.Repo, a.Number)
				return clientResult(v, err)
			},
		},
		{
			Name: tool("pr_comment"), Description: "Create a pull request comment.",
			InputSchema: obj(map[string]any{"repo": descTyp("string", "repository name"), "number": descTyp("integer", "pull request number"), "body": descTyp("string", "comment body")}, "repo", "number", "body"), OutputSchema: commentOutput(),
			Handler: func(ctx context.Context, raw json.RawMessage, id server.Identity) (map[string]any, error) {
				var a struct {
					Repo   string `json:"repo"`
					Number int    `json:"number"`
					Body   string `json:"body"`
				}
				if err := decodeAndValidate(raw, &a, func() error { return validateRepoNumberBody(a.Repo, a.Number, "body", a.Body) }); err != nil {
					return validationResult(err), nil
				}
				logWrite(ctx, logger, id, "pr_comment", a.Repo, a.Number, "")
				v, err := client.PRComment(ctx, a.Repo, a.Number, a.Body)
				return clientResult(v, err)
			},
		},
		{
			Name: tool("pr_review"), Description: "Create a pull request review.",
			InputSchema: obj(map[string]any{"repo": descTyp("string", "repository name"), "number": descTyp("integer", "pull request number"), "event": descTyp("string", "review event"), "body": descTyp("string", "optional review body")}, "repo", "number", "event"), OutputSchema: reviewOutput(),
			Handler: func(ctx context.Context, raw json.RawMessage, id server.Identity) (map[string]any, error) {
				var a struct {
					Repo   string `json:"repo"`
					Number int    `json:"number"`
					Event  string `json:"event"`
					Body   string `json:"body"`
				}
				validate := func() error {
					if err := (repoNumberArgs{a.Repo, a.Number}).validate("number"); err != nil {
						return err
					}
					return requireString("event", a.Event)
				}
				if err := decodeAndValidate(raw, &a, validate); err != nil {
					return validationResult(err), nil
				}
				logWrite(ctx, logger, id, "pr_review", a.Repo, a.Number, "")
				v, err := client.PRReview(ctx, a.Repo, a.Number, a.Event, a.Body)
				return clientResult(v, err)
			},
		},
		{
			Name: tool("pr_merge"), Description: "Merge a pull request.",
			InputSchema: obj(map[string]any{"repo": descTyp("string", "repository name"), "number": descTyp("integer", "pull request number"), "method": descTyp("string", "optional merge method")}, "repo", "number"), OutputSchema: mergeOutput(),
			Handler: func(ctx context.Context, raw json.RawMessage, id server.Identity) (map[string]any, error) {
				var a struct {
					Repo   string `json:"repo"`
					Number int    `json:"number"`
					Method string `json:"method"`
				}
				if err := decodeAndValidate(raw, &a, func() error { return (repoNumberArgs{a.Repo, a.Number}).validate("number") }); err != nil {
					return validationResult(err), nil
				}
				logWrite(ctx, logger, id, "pr_merge", a.Repo, a.Number, "")
				v, err := client.PRMerge(ctx, a.Repo, a.Number, a.Method)
				return clientResult(v, err)
			},
		},
		{
			Name: tool("issue_list"), Description: "List issues in a repository.",
			InputSchema: obj(map[string]any{"repo": descTyp("string", "repository name"), "state": descTyp("string", "optional state filter")}, "repo"), OutputSchema: listOutput(issueOutput()),
			Handler: func(ctx context.Context, raw json.RawMessage, _ server.Identity) (map[string]any, error) {
				var a struct{ Repo, State string }
				if err := decodeAndValidate(raw, &a, func() error { return requireString("repo", a.Repo) }); err != nil {
					return validationResult(err), nil
				}
				v, err := client.IssueList(ctx, a.Repo, a.State)
				return clientResult(map[string]any{"items": v}, err)
			},
		},
		{
			Name: tool("issue_get"), Description: "Fetch one issue.",
			InputSchema: obj(map[string]any{"repo": descTyp("string", "repository name"), "number": descTyp("integer", "issue number")}, "repo", "number"), OutputSchema: issueOutput(),
			Handler: func(ctx context.Context, raw json.RawMessage, _ server.Identity) (map[string]any, error) {
				var a repoNumberArgs
				if err := decodeAndValidate(raw, &a, func() error { return a.validate("number") }); err != nil {
					return validationResult(err), nil
				}
				v, err := client.IssueGet(ctx, a.Repo, a.Number)
				return clientResult(v, err)
			},
		},
		{
			Name: tool("issue_create"), Description: "Create an issue.",
			InputSchema: obj(map[string]any{"repo": descTyp("string", "repository name"), "title": descTyp("string", "issue title"), "body": descTyp("string", "optional issue body")}, "repo", "title"), OutputSchema: issueOutput(),
			Handler: func(ctx context.Context, raw json.RawMessage, id server.Identity) (map[string]any, error) {
				var a struct {
					Repo  string `json:"repo"`
					Title string `json:"title"`
					Body  string `json:"body"`
				}
				validate := func() error {
					if err := requireString("repo", a.Repo); err != nil {
						return err
					}
					return requireString("title", a.Title)
				}
				if err := decodeAndValidate(raw, &a, validate); err != nil {
					return validationResult(err), nil
				}
				logWrite(ctx, logger, id, "issue_create", a.Repo, 0, "")
				v, err := client.IssueCreate(ctx, a.Repo, a.Title, a.Body)
				return clientResult(v, err)
			},
		},
		{
			Name: tool("issue_comment"), Description: "Create an issue comment.",
			InputSchema: obj(map[string]any{"repo": descTyp("string", "repository name"), "number": descTyp("integer", "issue number"), "body": descTyp("string", "comment body")}, "repo", "number", "body"), OutputSchema: commentOutput(),
			Handler: func(ctx context.Context, raw json.RawMessage, id server.Identity) (map[string]any, error) {
				var a struct {
					Repo   string `json:"repo"`
					Number int    `json:"number"`
					Body   string `json:"body"`
				}
				if err := decodeAndValidate(raw, &a, func() error { return validateRepoNumberBody(a.Repo, a.Number, "body", a.Body) }); err != nil {
					return validationResult(err), nil
				}
				logWrite(ctx, logger, id, "issue_comment", a.Repo, a.Number, "")
				v, err := client.IssueComment(ctx, a.Repo, a.Number, a.Body)
				return clientResult(v, err)
			},
		},
		{
			Name: tool("issue_update"), Description: "Update issue state, labels, or assignees.",
			InputSchema: obj(map[string]any{"repo": descTyp("string", "repository name"), "number": descTyp("integer", "issue number"), "state": descTyp("string", "optional issue state"), "labels": arrayTyp("string", "optional full label set"), "assignees": arrayTyp("string", "optional full assignee set")}, "repo", "number"), OutputSchema: issueOutput(),
			Handler: func(ctx context.Context, raw json.RawMessage, id server.Identity) (map[string]any, error) {
				var a struct {
					Repo      string   `json:"repo"`
					Number    int      `json:"number"`
					State     string   `json:"state"`
					Labels    []string `json:"labels"`
					Assignees []string `json:"assignees"`
				}
				if err := decodeAndValidate(raw, &a, func() error { return (repoNumberArgs{a.Repo, a.Number}).validate("number") }); err != nil {
					return validationResult(err), nil
				}
				logWrite(ctx, logger, id, "issue_update", a.Repo, a.Number, "")
				v, err := client.IssueUpdate(ctx, a.Repo, a.Number, gh.IssuePatch{State: a.State, Labels: a.Labels, Assignees: a.Assignees})
				return clientResult(v, err)
			},
		},
		{
			Name: tool("file_get"), Description: "Fetch repository file metadata.",
			InputSchema: obj(map[string]any{"repo": descTyp("string", "repository name"), "path": descTyp("string", "file path"), "ref": descTyp("string", "optional git ref")}, "repo", "path"), OutputSchema: fileContentOutput(),
			Handler: func(ctx context.Context, raw json.RawMessage, _ server.Identity) (map[string]any, error) {
				var a struct {
					Repo string `json:"repo"`
					Path string `json:"path"`
					Ref  string `json:"ref"`
				}
				validate := func() error {
					if err := requireString("repo", a.Repo); err != nil {
						return err
					}
					return requireString("path", a.Path)
				}
				if err := decodeAndValidate(raw, &a, validate); err != nil {
					return validationResult(err), nil
				}
				v, err := client.FileGet(ctx, a.Repo, a.Path, a.Ref)
				return clientResult(v, err)
			},
		},
		{
			Name: tool("file_put"), Description: "Create or update repository file content.",
			InputSchema: obj(map[string]any{"repo": descTyp("string", "repository name"), "path": descTyp("string", "file path"), "message": descTyp("string", "commit message"), "content": descTyp("string", "UTF-8 file content"), "sha": descTyp("string", "optional current blob SHA")}, "repo", "path", "message", "content"), OutputSchema: fileCommitOutput(),
			Handler: func(ctx context.Context, raw json.RawMessage, id server.Identity) (map[string]any, error) {
				var a struct {
					Repo    string `json:"repo"`
					Path    string `json:"path"`
					Message string `json:"message"`
					Content string `json:"content"`
					SHA     string `json:"sha"`
				}
				validate := func() error {
					for _, p := range []struct{ name, value string }{{"repo", a.Repo}, {"path", a.Path}, {"message", a.Message}} {
						if err := requireString(p.name, p.value); err != nil {
							return err
						}
					}
					return nil
				}
				if err := decodeAndValidate(raw, &a, validate); err != nil {
					return validationResult(err), nil
				}
				logWrite(ctx, logger, id, "file_put", a.Repo, 0, a.Path)
				v, err := client.FilePut(ctx, a.Repo, a.Path, gh.FilePut{Message: a.Message, Content: []byte(a.Content), SHA: a.SHA})
				return clientResult(v, err)
			},
		},
	}
}

func obj(props map[string]any, required ...string) map[string]any {
	o := map[string]any{"type": "object", "properties": props}
	if len(required) > 0 {
		o["required"] = required
	}
	return o
}

func descTyp(t, description string) map[string]any {
	return map[string]any{"type": t, "description": description}
}
func arrayTyp(itemType, description string) map[string]any {
	return map[string]any{"type": "array", "items": map[string]any{"type": itemType}, "description": description}
}
func strProps(names ...string) map[string]any {
	p := map[string]any{}
	for _, name := range names {
		p[name] = map[string]any{"type": "string"}
	}
	return p
}
func objectOutput(props map[string]any, required ...string) map[string]any {
	return obj(props, required...)
}
func listOutput(element map[string]any) map[string]any {
	return obj(map[string]any{"items": map[string]any{"type": "array", "items": element}}, "items")
}

func repoOutput() map[string]any {
	p := strProps("name", "full_name", "default_branch")
	p["private"] = map[string]any{"type": "boolean"}
	return objectOutput(p, "name", "full_name", "private", "default_branch")
}
func prOutput() map[string]any {
	p := strProps("title", "state", "body", "html_url")
	p["number"] = map[string]any{"type": "integer"}
	return objectOutput(p, "number", "title", "state", "body", "html_url")
}
func prFileOutput() map[string]any {
	p := strProps("filename", "status", "patch")
	for _, n := range []string{"additions", "deletions", "changes"} {
		p[n] = map[string]any{"type": "integer"}
	}
	return objectOutput(p, "filename", "status", "additions", "deletions", "changes", "patch")
}
func prDetailOutput() map[string]any {
	p := prOutput()["properties"].(map[string]any)
	p["Files"] = map[string]any{"type": "array", "items": prFileOutput()}
	return objectOutput(p, "number", "title", "state", "body", "html_url", "Files")
}
func commentOutput() map[string]any {
	p := strProps("body", "html_url")
	p["id"] = map[string]any{"type": "integer"}
	return objectOutput(p, "id", "body", "html_url")
}
func reviewOutput() map[string]any {
	p := strProps("state", "body", "html_url")
	p["id"] = map[string]any{"type": "integer"}
	return objectOutput(p, "id", "state", "body", "html_url")
}
func mergeOutput() map[string]any {
	p := strProps("sha", "message")
	p["merged"] = map[string]any{"type": "boolean"}
	return objectOutput(p, "sha", "merged", "message")
}
func issueOutput() map[string]any {
	p := strProps("title", "state", "body", "html_url")
	p["number"] = map[string]any{"type": "integer"}
	p["labels"] = map[string]any{"type": "array", "items": objectOutput(strProps("name", "color"), "name", "color")}
	p["user"] = objectOutput(strProps("login"), "login")
	p["assignees"] = map[string]any{"type": "array", "items": objectOutput(strProps("login"), "login")}
	return objectOutput(p, "number", "title", "state", "body", "html_url")
}
func fileContentOutput() map[string]any {
	return objectOutput(strProps("path", "sha", "encoding"), "path", "sha", "encoding")
}
func fileCommitOutput() map[string]any {
	return objectOutput(map[string]any{"content": fileContentOutput(), "commit": objectOutput(strProps("sha", "message"), "sha", "message")}, "content", "commit")
}

type repoNumberArgs struct {
	Repo   string `json:"repo"`
	Number int    `json:"number"`
}

func (a repoNumberArgs) validate(numberName string) error {
	if err := requireString("repo", a.Repo); err != nil {
		return err
	}
	if a.Number <= 0 {
		return fmt.Errorf("%s is required", numberName)
	}
	return nil
}
func validateRepoNumberBody(repo string, number int, bodyName, body string) error {
	if err := (repoNumberArgs{repo, number}).validate("number"); err != nil {
		return err
	}
	return requireString(bodyName, body)
}
func requireString(name, value string) error {
	if value == "" {
		return fmt.Errorf("%s is required", name)
	}
	return nil
}
func decodeAndValidate(raw json.RawMessage, v any, validate func() error) error {
	if len(raw) != 0 {
		if err := json.Unmarshal(raw, v); err != nil {
			return fmt.Errorf("invalid arguments: %w", err)
		}
	}
	return validate()
}
func validationResult(err error) map[string]any {
	return appkitmcp.ErrorResult(appkitmcp.ErrValidation, err.Error())
}

func codeFor(err error) appkitmcp.ErrorCode {
	switch {
	case errors.Is(err, gh.ErrNotFound):
		return appkitmcp.ErrNotFound
	case errors.Is(err, gh.ErrInvalid):
		return appkitmcp.ErrValidation
	case errors.Is(err, gh.ErrAppAuth):
		return appkitmcp.ErrSourceUnavailable
	default:
		return appkitmcp.ErrSourceUnavailable
	}
}

func clientResult(v any, err error) (map[string]any, error) {
	if err != nil {
		return appkitmcp.ErrorResult(codeFor(err), err.Error()), nil
	}
	return appkitmcp.StructuredResult(v)
}

func logWrite(ctx context.Context, logger *slog.Logger, id server.Identity, verb, repo string, number int, path string) {
	args := []any{"owner_email", id.OwnerEmail, "client_id", id.ClientID, "verb", verb, "repo", repo}
	if number > 0 {
		args = append(args, "number", number)
	}
	if path != "" {
		args = append(args, "path", path)
	}
	logger.InfoContext(ctx, "github write", args...)
}
