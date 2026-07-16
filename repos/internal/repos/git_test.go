package repos

import (
	"bufio"
	"bytes"
	"context"
	"fmt"
	"io"
	"net/http"
	"net/http/httptest"
	"os"
	"os/exec"
	"path/filepath"
	"strings"
	"testing"
	"time"
)

func TestFreshenMovesDriftedCanonicalCloneToAdvancedRemoteTip(t *testing.T) {
	// R-EZVJ-FECP
	remote := newBareRemote(t)
	stateRoot := filepath.Join(t.TempDir(), "repos")
	git := NewGit(stateRoot, &staticTokenSource{token: "fixture"})
	if err := git.Clone(context.Background(), fileURL(remote), "fixture"); err != nil {
		t.Fatalf("clone: %v", err)
	}
	canonical := filepath.Join(stateRoot, "fixture")
	gitRun(t, canonical, "config", "user.email", "fixture@example.com")
	gitRun(t, canonical, "config", "user.name", "Fixture")
	os.WriteFile(filepath.Join(canonical, "drift"), []byte("drift"), 0o644)
	gitRun(t, canonical, "add", "drift")
	gitRun(t, canonical, "commit", "-m", "local drift")
	remoteTip := advanceRemote(t, remote, "advanced")
	if err := git.Freshen(context.Background(), "fixture", "main"); err != nil {
		t.Fatalf("freshen: %v", err)
	}
	if got := gitOutput(t, canonical, "rev-parse", "main"); got != remoteTip {
		t.Fatalf("local main = %s, want remote tip %s", got, remoteTip)
	}
	if got := gitOutput(t, canonical, "rev-parse", "origin/main"); got != remoteTip {
		t.Fatalf("origin/main = %s, want remote tip %s", got, remoteTip)
	}
}

func TestPushUsesPeerTokenHeaderWithoutPersistingCredential(t *testing.T) {
	// R-F13F-T63E
	token := "literal-secret-installation-token"
	remote := newBareRemote(t)
	server, requests := gitHTTPServer(t, filepath.Dir(remote), token)
	defer server.Close()
	now := time.Date(2026, 7, 15, 12, 0, 0, 0, time.UTC)
	var tokenCalls int
	tokenServer := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		tokenCalls++
		fmt.Fprintf(w, `{"token":%q,"expires_at":%q}`, token, now.Add(time.Hour).Format(time.RFC3339))
	}))
	defer tokenServer.Close()
	source := &HTTPTokenSource{URL: tokenServer.URL, Client: tokenServer.Client(), Now: func() time.Time { return now }}
	stateRoot := filepath.Join(t.TempDir(), "repos")
	git := NewGit(stateRoot, source)
	cloneURL := server.URL + "/" + filepath.Base(remote)
	if err := git.Clone(context.Background(), cloneURL, "fixture"); err != nil {
		t.Fatalf("authenticated clone: %v", err)
	}
	worktree := filepath.Join(t.TempDir(), "worktree")
	if err := git.WorktreeAdd(context.Background(), "fixture", "agent/auth", worktree, "main"); err != nil {
		t.Fatalf("worktree add: %v", err)
	}
	commitFile(t, worktree, "authenticated", "authenticated push")
	if err := git.Push(context.Background(), worktree, "agent/auth"); err != nil {
		t.Fatalf("authenticated push: %v", err)
	}
	if tokenCalls == 0 || *requests == 0 {
		t.Fatalf("token calls = %d, authenticated git requests = %d", tokenCalls, *requests)
	}
	assertTreeLacks(t, stateRoot, token)
	assertTreeLacks(t, worktree, token)
}

func TestGitLifecyclePushesCommitAndRemovesWorktree(t *testing.T) {
	// R-F3J8-KPKS
	remote := newBareRemote(t)
	stateRoot := filepath.Join(t.TempDir(), "repos")
	git := NewGit(stateRoot, &staticTokenSource{token: "fixture"})
	ctx := context.Background()
	if err := git.Clone(ctx, fileURL(remote), "fixture"); err != nil {
		t.Fatalf("clone: %v", err)
	}
	if err := git.Freshen(ctx, "fixture", "main"); err != nil {
		t.Fatalf("freshen: %v", err)
	}
	worktree := filepath.Join(t.TempDir(), "session")
	if err := git.WorktreeAdd(ctx, "fixture", "agent/e2e", worktree, "main"); err != nil {
		t.Fatalf("worktree add: %v", err)
	}
	commitFile(t, worktree, "result", "session result")
	want := gitOutput(t, worktree, "rev-parse", "HEAD")
	if err := git.Push(ctx, worktree, "agent/e2e"); err != nil {
		t.Fatalf("push: %v", err)
	}
	if exists, err := git.BranchExists(ctx, "fixture", "agent/e2e"); err != nil || !exists {
		t.Fatalf("BranchExists = %v, %v", exists, err)
	}
	if got := gitOutput(t, "", "--git-dir", remote, "rev-parse", "refs/heads/agent/e2e"); got != want {
		t.Fatalf("remote branch = %s, want %s", got, want)
	}
	if err := git.WorktreeRemove(ctx, "fixture", worktree); err != nil {
		t.Fatalf("worktree remove: %v", err)
	}
	if _, err := os.Stat(worktree); !os.IsNotExist(err) {
		t.Fatalf("worktree still exists: %v", err)
	}
	if got := gitOutput(t, filepath.Join(stateRoot, "fixture"), "rev-parse", "main"); got == "" {
		t.Fatal("canonical clone is not intact")
	}
}

func TestResolveStateRootKeepsGitWorktreesIndependentOfProcessWorkingDirectory(t *testing.T) {
	// R-C9CO-ODYU
	originalWorkingDirectory, err := os.Getwd()
	if err != nil {
		t.Fatalf("get working directory: %v", err)
	}
	t.Cleanup(func() {
		if err := os.Chdir(originalWorkingDirectory); err != nil {
			t.Errorf("restore working directory: %v", err)
		}
	})

	workingDirectory := t.TempDir()
	if err := os.Chdir(workingDirectory); err != nil {
		t.Fatalf("change to fixture working directory: %v", err)
	}
	stateRoot, err := ResolveStateRoot(func(key string) string {
		if key != "REPOS_STATE_DIR" {
			t.Fatalf("getenv key = %q, want REPOS_STATE_DIR", key)
		}
		return ""
	})
	if err != nil {
		t.Fatalf("resolve state root: %v", err)
	}
	wantStateRoot := filepath.Join(workingDirectory, "state")
	if !filepath.IsAbs(stateRoot) {
		t.Fatalf("state root %q is not absolute", stateRoot)
	}
	if stateRoot != wantStateRoot {
		t.Fatalf("state root = %q, want %q", stateRoot, wantStateRoot)
	}

	remote := newBareRemote(t)
	ctx := context.Background()
	resolvedGit := NewGit(filepath.Join(stateRoot, "repos"), &staticTokenSource{token: "fixture"})
	if err := resolvedGit.Clone(ctx, fileURL(remote), "resolved"); err != nil {
		t.Fatalf("clone with resolved root: %v", err)
	}
	resolvedWorktree := filepath.Join(stateRoot, "worktrees", "resolved")
	if err := resolvedGit.WorktreeAdd(ctx, "resolved", "agent/resolved", resolvedWorktree, "main"); err != nil {
		t.Fatalf("add worktree with resolved root: %v", err)
	}

	unresolvedRoot := "relative-state"
	unresolvedGit := NewGit(filepath.Join(unresolvedRoot, "repos"), &staticTokenSource{token: "fixture"})
	if err := unresolvedGit.Clone(ctx, fileURL(remote), "unresolved"); err != nil {
		t.Fatalf("clone with unresolved root: %v", err)
	}
	unresolvedWorktree := filepath.Join(unresolvedRoot, "worktrees", "unresolved")
	if err := unresolvedGit.WorktreeAdd(ctx, "unresolved", "agent/unresolved", unresolvedWorktree, "main"); err != nil {
		t.Fatalf("add worktree with unresolved root: %v", err)
	}
	nestedWorktree := filepath.Join(workingDirectory, unresolvedRoot, "repos", "unresolved", unresolvedWorktree)
	if info, err := os.Stat(nestedWorktree); err != nil || !info.IsDir() {
		t.Fatalf("unresolved worktree was not nested at %q: %v", nestedWorktree, err)
	}

	otherWorkingDirectory := t.TempDir()
	if err := os.Chdir(otherWorkingDirectory); err != nil {
		t.Fatalf("change away from canonical clone: %v", err)
	}
	if hasCommits, err := resolvedGit.HasCommits(ctx, resolvedWorktree, "main"); err != nil || hasCommits {
		t.Fatalf("inspect resolved worktree = %v, %v; want false, nil", hasCommits, err)
	}
	if _, err := unresolvedGit.HasCommits(ctx, unresolvedWorktree, "main"); err == nil {
		t.Fatal("inspect unresolved worktree succeeded after changing working directory")
	}
}

func newBareRemote(t *testing.T) string {
	t.Helper()
	root := t.TempDir()
	remote := filepath.Join(root, "fixture.git")
	gitRun(t, "", "init", "--bare", "--initial-branch=main", remote)
	seed := filepath.Join(root, "seed")
	gitRun(t, "", "init", "--initial-branch=main", seed)
	commitFile(t, seed, "initial", "initial commit")
	gitRun(t, seed, "remote", "add", "origin", remote)
	gitRun(t, seed, "push", "origin", "main")
	return remote
}

func advanceRemote(t *testing.T, remote, contents string) string {
	t.Helper()
	dir := filepath.Join(t.TempDir(), "advance")
	gitRun(t, "", "clone", remote, dir)
	commitFile(t, dir, contents, "advance remote")
	gitRun(t, dir, "push", "origin", "main")
	return gitOutput(t, dir, "rev-parse", "HEAD")
}

func commitFile(t *testing.T, dir, contents, message string) {
	t.Helper()
	gitRun(t, dir, "config", "user.email", "fixture@example.com")
	gitRun(t, dir, "config", "user.name", "Fixture")
	path := filepath.Join(dir, strings.ReplaceAll(message, " ", "-")+".txt")
	if err := os.WriteFile(path, []byte(contents), 0o644); err != nil {
		t.Fatalf("write commit fixture: %v", err)
	}
	gitRun(t, dir, "add", filepath.Base(path))
	gitRun(t, dir, "commit", "-m", message)
}

func fileURL(path string) string { return "file://" + filepath.ToSlash(path) }

func gitRun(t *testing.T, dir string, args ...string) {
	t.Helper()
	command := exec.Command("git", args...)
	command.Dir = dir
	if output, err := command.CombinedOutput(); err != nil {
		t.Fatalf("git %v: %v\n%s", args, err, output)
	}
}

func gitOutput(t *testing.T, dir string, args ...string) string {
	t.Helper()
	command := exec.Command("git", args...)
	command.Dir = dir
	output, err := command.CombinedOutput()
	if err != nil {
		t.Fatalf("git %v: %v\n%s", args, err, output)
	}
	return strings.TrimSpace(string(output))
}

func writeGitConfig(t *testing.T, path, from, to string) {
	t.Helper()
	contents := fmt.Sprintf("[url %q]\n\tinsteadOf = %s\n", to, from)
	if err := os.WriteFile(path, []byte(contents), 0o600); err != nil {
		t.Fatalf("write git config: %v", err)
	}
}

func gitHTTPServer(t *testing.T, projectRoot, token string) (*httptest.Server, *int) {
	t.Helper()
	requests := 0
	wantAuthorization := "Basic " + basicCredentials(token)
	handler := http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		if r.Header.Get("Authorization") != wantAuthorization {
			http.Error(w, "authorization required", http.StatusUnauthorized)
			return
		}
		requests++
		body, err := io.ReadAll(r.Body)
		if err != nil {
			http.Error(w, err.Error(), http.StatusBadRequest)
			return
		}
		command := exec.Command("git", "http-backend")
		command.Env = append(os.Environ(),
			"GIT_PROJECT_ROOT="+projectRoot, "GIT_HTTP_EXPORT_ALL=1", "PATH_INFO="+r.URL.Path,
			"QUERY_STRING="+r.URL.RawQuery, "REQUEST_METHOD="+r.Method,
			"CONTENT_TYPE="+r.Header.Get("Content-Type"), fmt.Sprintf("CONTENT_LENGTH=%d", len(body)),
			"HTTP_GIT_PROTOCOL="+r.Header.Get("Git-Protocol"))
		command.Stdin = bytes.NewReader(body)
		output, err := command.Output()
		if err != nil {
			http.Error(w, err.Error(), http.StatusInternalServerError)
			return
		}
		reader := bufio.NewReader(bytes.NewReader(output))
		for {
			line, err := reader.ReadString('\n')
			if err != nil || line == "\n" || line == "\r\n" {
				break
			}
			name, value, ok := strings.Cut(strings.TrimSpace(line), ":")
			if ok {
				w.Header().Add(name, strings.TrimSpace(value))
			}
		}
		io.Copy(w, reader)
	})
	server := httptest.NewServer(handler)
	gitRun(t, "", "--git-dir", filepath.Join(projectRoot, "fixture.git"), "config", "http.receivepack", "true")
	return server, &requests
}

func basicCredentials(token string) string {
	return strings.TrimPrefix(authArgs(token)[1], "http.extraHeader=Authorization: Basic ")
}

func assertTreeLacks(t *testing.T, root, secret string) {
	t.Helper()
	err := filepath.Walk(root, func(path string, info os.FileInfo, err error) error {
		if err != nil {
			return err
		}
		if info.IsDir() {
			return nil
		}
		contents, err := os.ReadFile(path)
		if err != nil {
			return err
		}
		if bytes.Contains(contents, []byte(secret)) {
			return fmt.Errorf("secret persisted in %s", path)
		}
		return nil
	})
	if err != nil {
		t.Fatal(err)
	}
}
