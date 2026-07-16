// Package runner owns queued repository sessions and their confined agents.
package runner

import (
	"context"
	"crypto/rand"
	"encoding/hex"
	"errors"
	"fmt"
	"io"
	"os"
	"os/exec"
	"path/filepath"
	"strings"
	"sync"
	"time"

	"eventplane/outbox"
	"github.com/ikigenba/agentkit"
	"github.com/ikigenba/agentkit/anthropic"

	"repos/internal/repos"
	toolset "repos/internal/tools"
)

const framingPrompt = `Work only in the supplied git worktree. Commit what you produce. Run .ikibot/check as the gate when it exists. Honor AGENTS.md when present. Your final message must be a concise summary suitable for the pull request body.`

// ModelConfig is service-wide model configuration read at the composition root.
type ModelConfig struct {
	Provider string
	Model    string
	APIKey   string
}

// DefaultModelConfig returns the v1 default pair.
func DefaultModelConfig(apiKey string) ModelConfig {
	return ModelConfig{Provider: "anthropic", Model: anthropic.ModelOpus48, APIKey: apiKey}
}

// ValidateModel checks both the API credential and the provider's real pricing
// registry. It is intended to run before the HTTP server is constructed.
func ValidateModel(config ModelConfig) (agentkit.Provider, error) {
	pair := fmt.Sprintf("%s/%s", config.Provider, config.Model)
	if strings.TrimSpace(config.APIKey) == "" {
		return nil, fmt.Errorf("model %s: empty provider API key", pair)
	}
	var provider agentkit.Provider
	switch config.Provider {
	case "anthropic":
		provider = anthropic.New(config.APIKey)
	default:
		return nil, fmt.Errorf("model %s: unsupported provider", pair)
	}
	if _, ok := provider.Pricing(config.Model); !ok {
		return nil, fmt.Errorf("model %s: unknown pricing pair", pair)
	}
	return provider, nil
}

// SessionRequest is the intake-owned request shared by webhook and MCP paths.
type SessionRequest = repos.SessionRequest

// IssueContent is the untrusted issue material fetched runner-side.
type IssueContent = repos.IssueContent

// Protocol is the narrow Phase 03 seam implemented by the GitHub protocol in
// the next phase.
type Protocol interface {
	FetchIssue(context.Context, repos.Session) (IssueContent, error)
	PostQueued(context.Context, repos.Session) error
}

type lifecycleProtocol interface {
	Admit(context.Context, repos.Session) error
	Success(context.Context, repos.Session, repos.Repo, string, string, string) (string, error)
	Failure(context.Context, repos.Session, string) error
}

// Agent is one conversation turn, drained to completion by Send.
type Agent interface {
	Send(context.Context, string) error
}

// ConversationConfig is handed to the swappable agent factory.
type ConversationConfig struct {
	Provider agentkit.Provider
	Model    string
	System   string
	Tools    []agentkit.Tool
	Log      io.Writer
}

// AgentFactory constructs a fresh, one-turn conversation for each session.
type AgentFactory interface {
	New(ConversationConfig) Agent
}

// AgentFactoryFunc adapts a function into AgentFactory.
type AgentFactoryFunc func(ConversationConfig) Agent

func (f AgentFactoryFunc) New(config ConversationConfig) Agent { return f(config) }

type agentkitFactory struct{}

func (agentkitFactory) New(config ConversationConfig) Agent {
	return &conversationAgent{conversation: &agentkit.Conversation{
		Provider: config.Provider,
		Model:    config.Model,
		System:   config.System,
		Tools:    config.Tools,
		Log:      config.Log,
	}}
}

type conversationAgent struct {
	conversation *agentkit.Conversation
	summary      string
}

func (a *conversationAgent) Send(ctx context.Context, text string) error {
	stream := a.conversation.Send(ctx, text)
	for event := range stream.Events() {
		var message *agentkit.Message
		switch done := event.(type) {
		case agentkit.MessageDone:
			message = &done.Message
		case *agentkit.MessageDone:
			message = &done.Message
		}
		if message != nil {
			var visible strings.Builder
			for _, block := range message.Blocks {
				switch text := block.(type) {
				case agentkit.TextBlock:
					visible.WriteString(text.Text)
				case *agentkit.TextBlock:
					visible.WriteString(text.Text)
				}
			}
			if value := strings.TrimSpace(visible.String()); value != "" {
				a.summary = value
			}
		}
	}
	return stream.Err()
}

func (a *conversationAgent) Summary() string { return a.summary }

// Clock is the deterministic time seam used for durable timestamps.
type Clock interface{ Now() time.Time }

// Config contains composition-root dependencies and runtime limits.
type Config struct {
	Store     *repos.Store
	Git       *repos.Git
	Protocol  Protocol
	Clock     Clock
	StateRoot string
	TTL       time.Duration
	MaxRun    int
	Model     ModelConfig
	Factory   AgentFactory
	Outbox    *outbox.Outbox
	Reaper    *repos.Reaper
}

// Runner admits durable queued sessions and owns their cancellation lifecycle.
type Runner struct {
	store      *repos.Store
	git        *repos.Git
	protocol   Protocol
	clock      Clock
	stateRoot  string
	ttl        time.Duration
	maxRun     int
	model      ModelConfig
	provider   agentkit.Provider
	factory    AgentFactory
	outbox     *outbox.Outbox
	reaper     *repos.Reaper
	mu         sync.Mutex
	cancels    map[string]context.CancelFunc
	userCancel map[string]bool
	wake       chan struct{}
}

// New validates boot configuration before returning a runnable engine.
func New(config Config) (*Runner, error) {
	provider, err := ValidateModel(config.Model)
	if err != nil {
		return nil, err
	}
	if config.Store == nil || config.Git == nil || config.Clock == nil || config.StateRoot == "" || config.Outbox == nil || config.Reaper == nil {
		return nil, errors.New("runner: store, git, clock, state root, outbox, and reaper are required")
	}
	if config.TTL <= 0 {
		config.TTL = 30 * time.Minute
	}
	if config.MaxRun <= 0 {
		config.MaxRun = 2
	}
	if config.Factory == nil {
		config.Factory = agentkitFactory{}
	}
	return &Runner{
		store: config.Store, git: config.Git, protocol: config.Protocol,
		clock: config.Clock, stateRoot: config.StateRoot, ttl: config.TTL,
		maxRun: config.MaxRun, model: config.Model, provider: provider,
		factory: config.Factory, cancels: make(map[string]context.CancelFunc),
		outbox: config.Outbox, reaper: config.Reaper,
		userCancel: make(map[string]bool), wake: make(chan struct{}, 1),
	}, nil
}

// Enqueue inserts a durable queued row and rings the dispatcher doorbell.
func (r *Runner) Enqueue(ctx context.Context, request SessionRequest) (repos.Session, error) {
	if request.RepoName == "" || request.OwnerEmail == "" {
		return repos.Session{}, errors.New("enqueue: repository and owner are required")
	}
	id := request.ID
	if id == "" {
		var err error
		id, err = sessionID()
		if err != nil {
			return repos.Session{}, err
		}
	}
	attempt := 1
	branch := "ikibot/session-" + id
	if request.IssueNumber != nil {
		max, err := r.store.MaxAttempt(ctx, request.RepoName, *request.IssueNumber)
		if err != nil {
			return repos.Session{}, err
		}
		attempt = max + 1
		for {
			branch = issueBranch(*request.IssueNumber, attempt)
			exists, err := r.git.BranchExists(ctx, request.RepoName, branch)
			if err != nil {
				return repos.Session{}, err
			}
			if !exists {
				break
			}
			attempt++
		}
	}
	queuedBehind := request.IssueNumber != nil && r.repoIsActive(ctx, request.RepoName)
	sessionDir := filepath.Join(r.stateRoot, "sessions", id)
	session := repos.Session{
		ID: id, RepoName: request.RepoName, OwnerEmail: request.OwnerEmail,
		IssueNumber: request.IssueNumber, Attempt: attempt, Branch: branch,
		Instructions: request.Instructions, Status: repos.StatusQueued,
		CreatedAt: r.clock.Now(), LogPath: filepath.Join(sessionDir, "output.jsonl"),
	}
	if err := r.store.InsertSession(ctx, session); err != nil {
		return repos.Session{}, err
	}
	if queuedBehind && r.protocol != nil {
		if err := r.protocol.PostQueued(ctx, session); err != nil {
			return repos.Session{}, fmt.Errorf("post queued comment: %w", err)
		}
	}
	r.ring()
	return session, nil
}

// Cancel cancels a running session, or terminally cancels a queued one.
func (r *Runner) Cancel(sessionID string) bool {
	r.mu.Lock()
	r.userCancel[sessionID] = true
	cancel := r.cancels[sessionID]
	r.mu.Unlock()
	if cancel != nil {
		cancel()
		return true
	}
	session, err := r.store.GetSession(context.Background(), sessionID)
	if err == nil && session.Status == repos.StatusRunning {
		// Admission and goroutine startup have a small handoff window. The run
		// observes this flag immediately after installing its cancel function.
		return true
	}
	if err != nil || session.Status != repos.StatusQueued {
		r.mu.Lock()
		delete(r.userCancel, sessionID)
		r.mu.Unlock()
		return false
	}
	if err := r.finish(context.Background(), sessionID, repos.StatusCancelled, nil, nil); err != nil {
		return false
	}
	r.ring()
	return true
}

// Recover marks sessions interrupted by restart and leaves queued work intact.
func (r *Runner) Recover(ctx context.Context) (int, error) {
	sessions, err := r.store.ListSessions(ctx, "", "")
	if err != nil {
		return 0, err
	}
	reason := "interrupted by restart"
	count := 0
	for _, session := range sessions {
		if session.Status != repos.StatusRunning {
			continue
		}
		if err := r.finish(ctx, session.ID, repos.StatusFailed, &reason, nil); err != nil {
			return count, err
		}
		count++
	}
	r.ring()
	return count, nil
}

// Dispatch continuously admits the oldest eligible sessions until ctx ends.
func (r *Runner) Dispatch(ctx context.Context) error {
	sweeps := time.NewTicker(r.reaper.SweepInterval())
	defer sweeps.Stop()
	for {
		admitted, err := r.admit(ctx)
		if err != nil {
			return err
		}
		if admitted {
			continue
		}
		select {
		case <-ctx.Done():
			return ctx.Err()
		case <-r.wake:
		case <-sweeps.C:
			if err := r.reaper.Sweep(ctx); err != nil {
				return err
			}
		}
	}
}

func (r *Runner) admit(ctx context.Context) (bool, error) {
	r.mu.Lock()
	defer r.mu.Unlock()
	running, err := r.store.CountRunning(ctx)
	if err != nil || running >= r.maxRun {
		return false, err
	}
	sessions, err := r.store.ListSessions(ctx, "", "")
	if err != nil {
		return false, err
	}
	runningRepos := make(map[string]bool)
	for _, session := range sessions {
		if session.Status == repos.StatusRunning {
			runningRepos[session.RepoName] = true
		}
	}
	for _, session := range sessions {
		if session.Status != repos.StatusQueued || runningRepos[session.RepoName] {
			continue
		}
		if err := r.store.MarkRunning(ctx, session.ID, r.clock.Now()); err != nil {
			return false, err
		}
		go r.run(ctx, session)
		return true, nil
	}
	return false, nil
}

func (r *Runner) run(parent context.Context, session repos.Session) {
	ctx, cancel := context.WithTimeout(parent, r.ttl)
	r.mu.Lock()
	r.cancels[session.ID] = cancel
	if r.userCancel[session.ID] {
		cancel()
	}
	r.mu.Unlock()
	defer func() {
		cancel()
		r.mu.Lock()
		delete(r.cancels, session.ID)
		delete(r.userCancel, session.ID)
		r.mu.Unlock()
		r.ring()
	}()

	result, err := r.execute(ctx, session)
	status := repos.StatusSucceeded
	var message *string
	var prURL *string
	r.mu.Lock()
	userCancelled := r.userCancel[session.ID]
	r.mu.Unlock()
	if userCancelled {
		status = repos.StatusCancelled
	} else if errors.Is(ctx.Err(), context.DeadlineExceeded) {
		status = repos.StatusFailed
		text := "session TTL exceeded"
		message = &text
	} else if err != nil {
		status = repos.StatusFailed
		text := err.Error()
		message = &text
	}
	if lifecycle, ok := r.protocol.(lifecycleProtocol); status != repos.StatusCancelled && ok {
		status, message, prURL = r.complete(context.Background(), ctx, lifecycle, session, result, status, message)
	}
	if status == repos.StatusSucceeded {
		if err := r.reaper.Success(context.Background(), session); err != nil {
			status = repos.StatusFailed
			text := err.Error()
			message, prURL = &text, nil
		}
	}
	_ = r.finish(context.Background(), session.ID, status, message, prURL)
}

type execution struct {
	repo     repos.Repo
	worktree string
	title    string
	summary  string
}

func (r *Runner) execute(ctx context.Context, session repos.Session) (execution, error) {
	if lifecycle, ok := r.protocol.(lifecycleProtocol); ok {
		if err := lifecycle.Admit(ctx, session); err != nil {
			return execution{}, fmt.Errorf("admit protocol: %w", err)
		}
	}
	repo, err := r.store.GetRepo(ctx, session.RepoName)
	if err != nil {
		return execution{}, err
	}
	if err := r.git.Freshen(ctx, repo.Name, repo.DefaultBranch); err != nil {
		return execution{}, err
	}
	sessionDir := filepath.Join(r.stateRoot, "sessions", session.ID)
	worktree := filepath.Join(sessionDir, "worktree")
	if err := os.MkdirAll(sessionDir, 0o755); err != nil {
		return execution{}, err
	}
	if err := r.git.WorktreeAdd(ctx, repo.Name, session.Branch, worktree, "origin/"+repo.DefaultBranch); err != nil {
		return execution{}, err
	}
	result := execution{repo: repo, worktree: worktree}
	instructions := session.Instructions
	if session.IssueNumber != nil {
		if r.protocol == nil {
			return result, errors.New("issue session: protocol is required")
		}
		issue, err := r.protocol.FetchIssue(ctx, session)
		if err != nil {
			return result, err
		}
		result.title = issue.Title
		instructions = formatIssue(issue)
	}
	instructionPath := filepath.Join(sessionDir, "instructions.md")
	if err := os.WriteFile(instructionPath, []byte(instructions), 0o600); err != nil {
		return result, err
	}
	logFile, err := os.OpenFile(session.LogPath, os.O_CREATE|os.O_WRONLY|os.O_TRUNC, 0o600)
	if err != nil {
		return result, err
	}
	defer logFile.Close()
	agent := r.factory.New(ConversationConfig{
		Provider: r.provider, Model: r.model.Model, System: framingPrompt,
		Tools: toolset.New(worktree), Log: logFile,
	})
	err = agent.Send(ctx, instructions)
	if summarizer, ok := agent.(interface{ Summary() string }); ok {
		result.summary = strings.TrimSpace(summarizer.Summary())
	}
	return result, err
}

func (r *Runner) complete(bg, runCtx context.Context, protocol lifecycleProtocol, session repos.Session, result execution, status string, message *string) (string, *string, *string) {
	fail := func(reason string) (string, *string, *string) {
		if err := protocol.Failure(bg, session, reason); err != nil {
			reason += "; failure protocol: " + err.Error()
		}
		return repos.StatusFailed, &reason, nil
	}
	if result.worktree == "" {
		reason := "session failed before worktree creation"
		if message != nil {
			reason = *message
		}
		return fail(reason)
	}
	committed, err := r.git.HasCommits(bg, result.worktree, result.repo.DefaultBranch)
	if err != nil {
		return fail(err.Error())
	}
	if !committed {
		return fail("no commits produced")
	}
	checkSummary := ""
	if status == repos.StatusSucceeded {
		var checkErr error
		checkSummary, checkErr = runCheck(runCtx, result.worktree, filepath.Join(r.stateRoot, "sessions", session.ID, "check.log"))
		if checkErr != nil {
			status = repos.StatusFailed
			reason := checkErr.Error()
			message = &reason
		}
	}
	if err := r.git.Push(bg, result.worktree, session.Branch); err != nil {
		return fail(err.Error())
	}
	if status != repos.StatusSucceeded {
		reason := "session failed"
		if message != nil {
			reason = *message
		}
		return fail(reason)
	}
	summary := result.summary
	if summary == "" {
		summary = "Agent completed the requested work."
	}
	url, err := protocol.Success(bg, session, result.repo, result.title, summary, checkSummary)
	if err != nil {
		return fail("success protocol: " + err.Error())
	}
	return repos.StatusSucceeded, nil, &url
}

func runCheck(ctx context.Context, worktree, logPath string) (string, error) {
	path := filepath.Join(worktree, ".ikibot", "check")
	info, err := os.Stat(path)
	if errors.Is(err, os.ErrNotExist) {
		return "no check declared", nil
	}
	if err != nil {
		return "", fmt.Errorf("inspect check: %w", err)
	}
	if info.Mode().Perm()&0o111 == 0 {
		return "no check declared", nil
	}
	command := exec.CommandContext(ctx, path)
	command.Dir = worktree
	output, commandErr := command.CombinedOutput()
	if writeErr := os.WriteFile(logPath, output, 0o600); writeErr != nil {
		return "", fmt.Errorf("write check log: %w", writeErr)
	}
	trimmed := strings.TrimSpace(string(output))
	if commandErr != nil {
		if len(trimmed) > 4000 {
			trimmed = trimmed[len(trimmed)-4000:]
		}
		return "", fmt.Errorf("check failed: %s", trimmed)
	}
	if trimmed == "" {
		return "check passed", nil
	}
	return trimmed, nil
}

func (r *Runner) finish(ctx context.Context, id, status string, message, prURL *string) error {
	err := r.store.FinishSession(ctx, id, status, message, prURL, r.clock.Now(), repos.AppendOutcome(r.outbox))
	if err == nil {
		r.outbox.Ring()
	}
	return err
}

func (r *Runner) ring() {
	select {
	case r.wake <- struct{}{}:
	default:
	}
}

func (r *Runner) repoIsActive(ctx context.Context, repoName string) bool {
	sessions, err := r.store.ListSessions(ctx, repoName, "")
	if err != nil {
		return false
	}
	for _, session := range sessions {
		if session.Status == repos.StatusRunning || session.Status == repos.StatusQueued {
			return true
		}
	}
	return false
}

func issueBranch(issue, attempt int) string {
	branch := fmt.Sprintf("ikibot/issue-%d", issue)
	if attempt > 1 {
		branch += fmt.Sprintf(".%d", attempt)
	}
	return branch
}

func formatIssue(issue IssueContent) string {
	var text strings.Builder
	fmt.Fprintf(&text, "# %s\n\n%s", issue.Title, issue.Body)
	if len(issue.Comments) > 0 {
		text.WriteString("\n\n## Comments")
		for _, comment := range issue.Comments {
			fmt.Fprintf(&text, "\n\n%s", comment)
		}
	}
	return text.String()
}

func sessionID() (string, error) {
	var bytes [12]byte
	if _, err := rand.Read(bytes[:]); err != nil {
		return "", fmt.Errorf("session id: %w", err)
	}
	return hex.EncodeToString(bytes[:]), nil
}
