package repos

import (
	"context"
	"encoding/base64"
	"encoding/json"
	"errors"
	"fmt"
	"net/http"
	"os"
	"os/exec"
	"path/filepath"
	"strings"
	"sync"
	"time"

	"registry"
)

// TokenSource supplies a short-lived GitHub installation token.
type TokenSource interface {
	Token(context.Context) (string, error)
}

// HTTPTokenSource obtains and caches installation tokens from the github peer.
type HTTPTokenSource struct {
	URL    string
	Client *http.Client
	Now    func() time.Time

	mu      sync.Mutex
	token   string
	expires time.Time
}

// NewHTTPTokenSource uses the registered github peer address.
func NewHTTPTokenSource(client *http.Client, now func() time.Time) *HTTPTokenSource {
	return &HTTPTokenSource{URL: registry.BaseURL("github"), Client: client, Now: now}
}

func (s *HTTPTokenSource) Token(ctx context.Context) (string, error) {
	s.mu.Lock()
	defer s.mu.Unlock()
	now := time.Now
	if s.Now != nil {
		now = s.Now
	}
	if s.token != "" && now().Add(time.Minute).Before(s.expires) {
		return s.token, nil
	}
	client := s.Client
	if client == nil {
		client = http.DefaultClient
	}
	req, err := http.NewRequestWithContext(ctx, http.MethodGet, strings.TrimRight(s.URL, "/")+"/token", nil)
	if err != nil {
		return "", fmt.Errorf("github token request: %w", err)
	}
	response, err := client.Do(req)
	if err != nil {
		return "", fmt.Errorf("github token request: %w", err)
	}
	defer response.Body.Close()
	if response.StatusCode != http.StatusOK {
		return "", fmt.Errorf("github token request: status %s", response.Status)
	}
	var payload struct {
		Token     string    `json:"token"`
		ExpiresAt time.Time `json:"expires_at"`
	}
	if err := json.NewDecoder(response.Body).Decode(&payload); err != nil {
		return "", fmt.Errorf("github token response: %w", err)
	}
	if payload.Token == "" || payload.ExpiresAt.IsZero() {
		return "", errors.New("github token response: missing token or expiry")
	}
	s.token, s.expires = payload.Token, payload.ExpiresAt
	return s.token, nil
}

// Git owns every git subprocess used by the service.
type Git struct {
	stateRoot string
	tokens    TokenSource
	runner    func(ctx context.Context, dir string, env []string, args ...string) ([]byte, error)
}

func NewGit(stateRoot string, tokens TokenSource) *Git {
	return &Git{stateRoot: stateRoot, tokens: tokens, runner: runGit}
}

func runGit(ctx context.Context, dir string, env []string, args ...string) ([]byte, error) {
	command := exec.CommandContext(ctx, "git", args...)
	command.Dir = dir
	command.Env = append(os.Environ(), env...)
	return command.CombinedOutput()
}

func (g *Git) Clone(ctx context.Context, cloneURL, name string) error {
	if err := os.MkdirAll(g.stateRoot, 0o755); err != nil {
		return fmt.Errorf("create repo state root: %w", err)
	}
	return g.authenticated(ctx, "", "clone", "clone", cloneURL, filepath.Join(g.stateRoot, name))
}

func (g *Git) Freshen(ctx context.Context, name, defaultBranch string) error {
	dir := g.repoPath(name)
	if err := g.authenticated(ctx, dir, "fetch", "fetch", "origin", defaultBranch); err != nil {
		return err
	}
	if err := g.run(ctx, dir, "reset", "reset", "--hard", "origin/"+defaultBranch); err != nil {
		return err
	}
	return nil
}

func (g *Git) WorktreeAdd(ctx context.Context, name, branch, path, defaultBranch string) error {
	return g.run(ctx, g.repoPath(name), "add worktree", "worktree", "add", "-b", branch, path, defaultBranch)
}

func (g *Git) WorktreeRemove(ctx context.Context, name, path string) error {
	return g.run(ctx, g.repoPath(name), "remove worktree", "worktree", "remove", "--force", path)
}

func (g *Git) Push(ctx context.Context, worktreePath, branch string) error {
	return g.authenticated(ctx, worktreePath, "push", "push", "-u", "origin", branch)
}

// HasCommits reports whether the worktree is ahead of its starting remote branch.
func (g *Git) HasCommits(ctx context.Context, worktreePath, defaultBranch string) (bool, error) {
	output, err := g.runner(ctx, worktreePath, nil, "rev-list", "--count", "origin/"+defaultBranch+"..HEAD")
	if err != nil {
		return false, fmt.Errorf("git inspect commits: %w: %s", err, strings.TrimSpace(string(output)))
	}
	return strings.TrimSpace(string(output)) != "0", nil
}

func (g *Git) BranchExists(ctx context.Context, name, branch string) (bool, error) {
	dir := g.repoPath(name)
	_, err := g.runner(ctx, dir, nil, "show-ref", "--verify", "--quiet", "refs/heads/"+branch)
	if err == nil {
		return true, nil
	}
	var exitError *exec.ExitError
	if !errors.As(err, &exitError) || exitError.ExitCode() != 1 {
		return false, fmt.Errorf("git inspect local branch: %w", err)
	}
	token, err := g.token(ctx)
	if err != nil {
		return false, err
	}
	output, err := g.runner(ctx, dir, nil, authArgs(token, "ls-remote", "--exit-code", "--heads", "origin", "refs/heads/"+branch)...)
	if err == nil {
		return len(output) > 0, nil
	}
	if errors.As(err, &exitError) && exitError.ExitCode() == 2 {
		return false, nil
	}
	return false, fmt.Errorf("git inspect remote branch: %w", err)
}

func (g *Git) authenticated(ctx context.Context, dir, operation string, args ...string) error {
	token, err := g.token(ctx)
	if err != nil {
		return err
	}
	return g.run(ctx, dir, operation, authArgs(token, args...)...)
}

func (g *Git) token(ctx context.Context) (string, error) {
	if g.tokens == nil {
		return "", errors.New("git authentication: token source is required")
	}
	token, err := g.tokens.Token(ctx)
	if err != nil {
		return "", fmt.Errorf("git authentication: %w", err)
	}
	return token, nil
}

func authArgs(token string, args ...string) []string {
	credentials := base64.StdEncoding.EncodeToString([]byte("x-access-token:" + token))
	return append([]string{"-c", "http.extraHeader=Authorization: Basic " + credentials}, args...)
}

func (g *Git) run(ctx context.Context, dir, operation string, args ...string) error {
	output, err := g.runner(ctx, dir, nil, args...)
	if err != nil {
		return fmt.Errorf("git %s: %w: %s", operation, err, strings.TrimSpace(string(output)))
	}
	return nil
}

func (g *Git) repoPath(name string) string { return filepath.Join(g.stateRoot, name) }
