package mcp

import (
	"bufio"
	"bytes"
	"context"
	"encoding/json"
	"errors"
	"fmt"
	"io"
	"os"
	"strings"
	"time"

	appkitmcp "appkit/mcp"
	"appkit/server"
	"eventplane/outbox"

	"repos/internal/repos"
	"repos/internal/runner"
)

const toolPrefix = ""

// Events is the event registry reflected by the repos MCP surface.
var Events outbox.Registry = repos.Events

func tool(name string) string { return toolPrefix + name }

type toolHandlers struct{ svc Service }

// Tools returns exactly the nine repository domain tools. The shared transport
// appends health and reflection.
func Tools(svc Service) []appkitmcp.Tool {
	if svc == nil {
		panic("mcp: repos service is required")
	}
	h := toolHandlers{svc: svc}
	repoOutput := obj(map[string]any{"repo": repoSchema()}, "repo")
	sessionOutput := obj(map[string]any{"session": sessionSchema(false)}, "session")
	return []appkitmcp.Tool{
		{Name: tool("clone"), Description: "Clone and provision a GitHub repository.", InputSchema: obj(map[string]any{"name": typ("string")}, "name"), OutputSchema: repoOutput, Handler: h.clone},
		{Name: tool("list"), Description: "List repositories provisioned by the authenticated owner.", InputSchema: obj(map[string]any{}), OutputSchema: obj(map[string]any{"items": array(repoSchema())}, "items"), Handler: h.list},
		{Name: tool("get"), Description: "Get a repository by name.", InputSchema: obj(map[string]any{"name": typ("string")}, "name"), OutputSchema: repoOutput, Handler: h.get},
		{Name: tool("delete"), Description: "Delete a repository and its disposable git state.", InputSchema: obj(map[string]any{"name": typ("string")}, "name"), OutputSchema: obj(map[string]any{"deleted": typ("boolean")}, "deleted"), Handler: h.delete},
		{Name: tool("session_start"), Description: "Queue a manual repository session.", InputSchema: obj(map[string]any{"repo": typ("string"), "instructions": typ("string"), "branch": typ("string")}, "repo", "instructions"), OutputSchema: sessionOutput, Handler: h.sessionStart},
		{Name: tool("session_list"), Description: "List the authenticated owner's sessions, optionally filtered by repository.", InputSchema: obj(map[string]any{"repo": typ("string")}), OutputSchema: obj(map[string]any{"items": array(sessionSchema(false))}, "items"), Handler: h.sessionList},
		{Name: tool("session_get"), Description: "Get a full session record.", InputSchema: obj(map[string]any{"id": typ("string")}, "id"), OutputSchema: obj(map[string]any{"session": sessionSchema(true)}, "session"), Handler: h.sessionGet},
		{Name: tool("session_output"), Description: "Read a line window from a session transcript.", InputSchema: obj(map[string]any{"id": typ("string"), "offset": typ("integer"), "limit": typ("integer")}, "id"), OutputSchema: obj(map[string]any{"lines": array(typ("string")), "total": typ("integer"), "offset": typ("integer")}, "lines", "total", "offset"), Handler: h.sessionOutput},
		{Name: tool("session_cancel"), Description: "Cancel a queued or running session.", InputSchema: obj(map[string]any{"id": typ("string")}, "id"), OutputSchema: obj(map[string]any{"cancelled": typ("boolean"), "session": sessionSchema(false)}, "cancelled", "session"), Handler: h.sessionCancel},
	}
}

func (h toolHandlers) clone(ctx context.Context, raw json.RawMessage, id server.Identity) (map[string]any, error) {
	var a struct {
		Name string `json:"name"`
	}
	if err := decode(raw, &a); err != nil || blank(a.Name) || blank(id.OwnerEmail) {
		return validation(err, "name and authenticated owner are required"), nil
	}
	if err := h.svc.CloneRepo(ctx, id.OwnerEmail, a.Name); err != nil {
		return domainError(err, appkitmcp.ErrSourceUnavailable), nil
	}
	repo, err := h.svc.GetRepo(ctx, a.Name)
	if err != nil {
		return domainError(err, appkitmcp.ErrInternal), nil
	}
	return structured(map[string]any{"repo": repoView(repo)})
}

func (h toolHandlers) list(ctx context.Context, raw json.RawMessage, id server.Identity) (map[string]any, error) {
	var a struct{}
	if err := decode(raw, &a); err != nil {
		return validation(err, "arguments must be an object"), nil
	}
	items, err := h.svc.ListRepos(ctx, id.OwnerEmail)
	if err != nil {
		return domainError(err, appkitmcp.ErrInternal), nil
	}
	views := make([]repoJSON, 0, len(items))
	for _, item := range items {
		views = append(views, repoView(item))
	}
	return structured(map[string]any{"items": views})
}

func (h toolHandlers) get(ctx context.Context, raw json.RawMessage, _ server.Identity) (map[string]any, error) {
	var a struct {
		Name string `json:"name"`
	}
	if err := decode(raw, &a); err != nil || blank(a.Name) {
		return validation(err, "name is required"), nil
	}
	repo, err := h.svc.GetRepo(ctx, a.Name)
	if err != nil {
		return domainError(err, appkitmcp.ErrInternal), nil
	}
	return structured(map[string]any{"repo": repoView(repo)})
}

func (h toolHandlers) delete(ctx context.Context, raw json.RawMessage, _ server.Identity) (map[string]any, error) {
	var a struct {
		Name string `json:"name"`
	}
	if err := decode(raw, &a); err != nil || blank(a.Name) {
		return validation(err, "name is required"), nil
	}
	if err := h.svc.DeleteRepo(ctx, a.Name); err != nil {
		return domainError(err, appkitmcp.ErrInternal), nil
	}
	return structured(map[string]any{"deleted": true})
}

func (h toolHandlers) sessionStart(ctx context.Context, raw json.RawMessage, id server.Identity) (map[string]any, error) {
	var a struct {
		Repo         string `json:"repo"`
		Instructions string `json:"instructions"`
		Branch       string `json:"branch,omitempty"`
	}
	if err := decode(raw, &a); err != nil || blank(a.Repo) || blank(a.Instructions) || blank(id.OwnerEmail) {
		return validation(err, "repo, instructions, and authenticated owner are required"), nil
	}
	if a.Branch != "" && (!strings.HasPrefix(a.Branch, "ikibot/") || len(a.Branch) == len("ikibot/")) {
		return validation(nil, "branch must match ikibot/*"), nil
	}
	if _, err := h.svc.GetRepo(ctx, a.Repo); err != nil {
		return domainError(err, appkitmcp.ErrInternal), nil
	}
	session, err := h.svc.Enqueue(ctx, runner.SessionRequest{RepoName: a.Repo, OwnerEmail: id.OwnerEmail, Instructions: a.Instructions})
	if err != nil {
		return domainError(err, appkitmcp.ErrValidation), nil
	}
	return structured(map[string]any{"session": sessionView(session, false)})
}

func (h toolHandlers) sessionList(ctx context.Context, raw json.RawMessage, id server.Identity) (map[string]any, error) {
	var a struct {
		Repo string `json:"repo,omitempty"`
	}
	if err := decode(raw, &a); err != nil {
		return validation(err, "invalid session_list arguments"), nil
	}
	items, err := h.svc.ListSessions(ctx, a.Repo, id.OwnerEmail)
	if err != nil {
		return domainError(err, appkitmcp.ErrInternal), nil
	}
	views := make([]sessionJSON, 0, len(items))
	for _, item := range items {
		views = append(views, sessionView(item, false))
	}
	return structured(map[string]any{"items": views})
}

func (h toolHandlers) sessionGet(ctx context.Context, raw json.RawMessage, _ server.Identity) (map[string]any, error) {
	var a struct {
		ID string `json:"id"`
	}
	if err := decode(raw, &a); err != nil || blank(a.ID) {
		return validation(err, "id is required"), nil
	}
	session, err := h.svc.GetSession(ctx, a.ID)
	if err != nil {
		return domainError(err, appkitmcp.ErrInternal), nil
	}
	return structured(map[string]any{"session": sessionView(session, true)})
}

func (h toolHandlers) sessionOutput(ctx context.Context, raw json.RawMessage, _ server.Identity) (map[string]any, error) {
	var a struct {
		ID     string `json:"id"`
		Offset *int   `json:"offset,omitempty"`
		Limit  *int   `json:"limit,omitempty"`
	}
	if err := decode(raw, &a); err != nil || blank(a.ID) {
		return validation(err, "id is required"), nil
	}
	offset, limit := 0, 100
	if a.Offset != nil {
		offset = *a.Offset
	}
	if a.Limit != nil {
		limit = *a.Limit
	}
	if offset < 0 || limit < 1 {
		return validation(nil, "offset must be non-negative and limit must be positive"), nil
	}
	session, err := h.svc.GetSession(ctx, a.ID)
	if err != nil {
		return domainError(err, appkitmcp.ErrInternal), nil
	}
	lines, err := readLines(session.LogPath)
	if err != nil {
		return domainError(err, appkitmcp.ErrInternal), nil
	}
	start := min(offset, len(lines))
	end := min(start+limit, len(lines))
	return structured(map[string]any{"lines": lines[start:end], "total": len(lines), "offset": offset})
}

func (h toolHandlers) sessionCancel(ctx context.Context, raw json.RawMessage, _ server.Identity) (map[string]any, error) {
	var a struct {
		ID string `json:"id"`
	}
	if err := decode(raw, &a); err != nil || blank(a.ID) {
		return validation(err, "id is required"), nil
	}
	cancelled := h.svc.Cancel(a.ID)
	session, err := h.svc.GetSession(ctx, a.ID)
	if err != nil {
		return domainError(err, appkitmcp.ErrInternal), nil
	}
	return structured(map[string]any{"cancelled": cancelled, "session": sessionView(session, false)})
}

func decode(raw json.RawMessage, dst any) error {
	if len(raw) == 0 {
		raw = json.RawMessage(`{}`)
	}
	dec := json.NewDecoder(bytes.NewReader(raw))
	dec.DisallowUnknownFields()
	if err := dec.Decode(dst); err != nil {
		return err
	}
	if err := dec.Decode(&struct{}{}); !errors.Is(err, io.EOF) {
		return errors.New("arguments must contain one JSON object")
	}
	return nil
}

func readLines(path string) ([]string, error) {
	file, err := os.Open(path)
	if err != nil {
		return nil, fmt.Errorf("read session output: %w", err)
	}
	defer file.Close()
	var lines []string
	scanner := bufio.NewScanner(file)
	scanner.Buffer(make([]byte, 64*1024), 4*1024*1024)
	for scanner.Scan() {
		lines = append(lines, scanner.Text())
	}
	if err := scanner.Err(); err != nil {
		return nil, fmt.Errorf("read session output: %w", err)
	}
	return lines, nil
}

func structured(value any) (map[string]any, error) { return appkitmcp.StructuredResult(value) }

func validation(err error, fallback string) map[string]any {
	if err != nil {
		return appkitmcp.ErrorResult(appkitmcp.ErrValidation, err.Error())
	}
	return appkitmcp.ErrorResult(appkitmcp.ErrValidation, fallback)
}

func domainError(err error, fallback appkitmcp.ErrorCode) map[string]any {
	switch {
	case errors.Is(err, repos.ErrConflict):
		return appkitmcp.ErrorResult(appkitmcp.ErrConflict, err.Error())
	case errors.Is(err, repos.ErrNotFound):
		return appkitmcp.ErrorResult(appkitmcp.ErrNotFound, err.Error())
	default:
		message := "internal error"
		if fallback == appkitmcp.ErrValidation || fallback == appkitmcp.ErrSourceUnavailable {
			message = err.Error()
		}
		return appkitmcp.ErrorResult(fallback, message)
	}
}

func blank(value string) bool { return strings.TrimSpace(value) == "" }

func obj(properties map[string]any, required ...string) map[string]any {
	result := map[string]any{"type": "object", "properties": properties, "additionalProperties": false}
	if len(required) != 0 {
		result["required"] = required
	}
	return result
}

func typ(name string) map[string]any { return map[string]any{"type": name} }
func array(items map[string]any) map[string]any {
	return map[string]any{"type": "array", "items": items}
}

func repoSchema() map[string]any {
	return obj(map[string]any{"name": typ("string"), "clone_url": typ("string"), "default_branch": typ("string"), "created_at": typ("string")}, "name", "clone_url", "default_branch", "created_at")
}

func sessionSchema(full bool) map[string]any {
	properties := map[string]any{
		"id": typ("string"), "repo": typ("string"), "issue_number": typ("integer"), "attempt": typ("integer"),
		"branch": typ("string"), "status": typ("string"), "error": typ("string"), "pr_url": typ("string"),
		"created_at": typ("string"), "started_at": typ("string"), "ended_at": typ("string"),
	}
	if full {
		properties["instructions"] = typ("string")
	}
	return obj(properties, "id", "repo", "attempt", "branch", "status", "created_at")
}

type repoJSON struct {
	Name          string    `json:"name"`
	CloneURL      string    `json:"clone_url"`
	DefaultBranch string    `json:"default_branch"`
	CreatedAt     time.Time `json:"created_at"`
}

func repoView(repo repos.Repo) repoJSON {
	return repoJSON{Name: repo.Name, CloneURL: repo.CloneURL, DefaultBranch: repo.DefaultBranch, CreatedAt: repo.CreatedAt}
}

type sessionJSON struct {
	ID           string     `json:"id"`
	Repo         string     `json:"repo"`
	IssueNumber  *int       `json:"issue_number,omitempty"`
	Attempt      int        `json:"attempt"`
	Branch       string     `json:"branch"`
	Status       string     `json:"status"`
	Error        *string    `json:"error,omitempty"`
	PRURL        *string    `json:"pr_url,omitempty"`
	Instructions string     `json:"instructions,omitempty"`
	CreatedAt    time.Time  `json:"created_at"`
	StartedAt    *time.Time `json:"started_at,omitempty"`
	EndedAt      *time.Time `json:"ended_at,omitempty"`
}

func sessionView(session repos.Session, full bool) sessionJSON {
	view := sessionJSON{ID: session.ID, Repo: session.RepoName, IssueNumber: session.IssueNumber, Attempt: session.Attempt,
		Branch: session.Branch, Status: session.Status, Error: session.Error, PRURL: session.PRURL,
		CreatedAt: session.CreatedAt, StartedAt: session.StartedAt, EndedAt: session.EndedAt}
	if full {
		view.Instructions = session.Instructions
	}
	return view
}
