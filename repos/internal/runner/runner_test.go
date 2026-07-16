package runner

import (
	"context"
	"database/sql"
	"encoding/base64"
	"encoding/json"
	"fmt"
	"net/http"
	"net/http/httptest"
	"os"
	"os/exec"
	"path/filepath"
	"reflect"
	"strings"
	"sync"
	"testing"
	"time"

	appdb "appkit/db"
	"eventplane/consumer"
	"eventplane/outbox"
	reposdb "repos/internal/db"
	"repos/internal/repos"
)

func TestWebhookIntakeEnqueuesRunnableSessionAndRingsDispatcher(t *testing.T) {
	// R-2U0F-NNXH
	t.Run("runner enqueuer", func(t *testing.T) {
		fixture := newFixture(t, 1, time.Minute)
		_, remote := fixture.addRepo(t, "fixture")
		recorder := newGitHubRecorder(t)
		defer recorder.Close()
		fixture.config.Protocol = repos.NewProtocol(repos.NewGitHubPeerAt(recorder.URL, recorder.Client()))
		sessionIDs := make(chan string, 1)
		fixture.config.Factory = AgentFactoryFunc(func(config ConversationConfig) Agent {
			return agentFunc(func(context.Context, string) error {
				if _, err := config.Log.Write([]byte("scripted transcript\n")); err != nil {
					return err
				}
				id := <-sessionIDs
				return commitScriptedChange(filepath.Join(fixture.stateRoot, "sessions", id, "worktree"))
			})
		})
		engine := fixture.runner(t)
		service := repos.NewService(fixture.store, fixture.git, fixture.clock, "fixture-org")
		intake := repos.NewIntake(fixture.store, service, engine, "", nil)

		ctx, cancel := context.WithCancel(context.Background())
		defer cancel()
		engine.ring() // Prime the select so the test can prove Dispatch is already waiting.
		go engine.Dispatch(ctx)
		waitForDoorbell(t, engine, false)

		if err := intake.Handle(context.Background(), webhookIssueEvent(t, remote)); err != nil {
			t.Fatalf("Handle: %v", err)
		}
		sessions, err := fixture.store.ListSessions(context.Background(), "fixture", "")
		if err != nil || len(sessions) != 1 {
			t.Fatalf("sessions = %#v, %v", sessions, err)
		}
		session := sessions[0]
		wantLog := filepath.Join(fixture.stateRoot, "sessions", session.ID, "output.jsonl")
		if session.LogPath == "" || session.LogPath != wantLog {
			t.Fatalf("webhook session = %#v, want log %q", session, wantLog)
		}
		sessionIDs <- session.ID
		finished := waitStatus(t, fixture.store, session.ID, repos.StatusSucceeded)
		transcript, err := os.ReadFile(finished.LogPath)
		if err != nil || len(transcript) == 0 {
			t.Fatalf("transcript at %q = %q, %v", finished.LogPath, transcript, err)
		}
		if recorder.count("issue_get") != 1 {
			t.Fatalf("github issue fetches = %d, want 1", recorder.count("issue_get"))
		}
	})

	t.Run("hand rolled insert is malformed and silent", func(t *testing.T) {
		fixture := newFixture(t, 1, time.Minute)
		fixture.addRepo(t, "fixture")
		engine := fixture.runner(t)
		issue := 42
		handRolled := repos.Session{
			ID: "hand-rolled", RepoName: "fixture", OwnerEmail: "owner@example.com",
			IssueNumber: &issue, Attempt: 1, Branch: "ikibot/issue-42",
			Instructions: "Resolve GitHub issue #42.", Status: repos.StatusQueued,
			CreatedAt: fixture.clock.Now(),
		}
		if err := fixture.store.InsertSession(context.Background(), handRolled); err != nil {
			t.Fatal(err)
		}
		stored, err := fixture.store.GetSession(context.Background(), handRolled.ID)
		if err != nil {
			t.Fatal(err)
		}
		if stored.LogPath != "" || len(engine.wake) != 0 {
			t.Fatalf("hand-rolled session log = %q, doorbell depth = %d; want empty and silent", stored.LogPath, len(engine.wake))
		}
	})
}

func commitScriptedChange(worktree string) error {
	commands := [][]string{
		{"config", "user.email", "agent@example.com"},
		{"config", "user.name", "Fixture Agent"},
	}
	for _, args := range commands {
		command := exec.Command("git", args...)
		command.Dir = worktree
		if output, err := command.CombinedOutput(); err != nil {
			return fmt.Errorf("git %v: %w: %s", args, err, output)
		}
	}
	if err := os.WriteFile(filepath.Join(worktree, "scripted.txt"), []byte("done\n"), 0o644); err != nil {
		return err
	}
	for _, args := range [][]string{{"add", "scripted.txt"}, {"commit", "-m", "scripted change"}} {
		command := exec.Command("git", args...)
		command.Dir = worktree
		if output, err := command.CombinedOutput(); err != nil {
			return fmt.Errorf("git %v: %w: %s", args, err, output)
		}
	}
	return nil
}

func webhookIssueEvent(t *testing.T, remote string) consumer.Event {
	t.Helper()
	delivery, err := json.Marshal(map[string]any{
		"action": "labeled",
		"issue":  map[string]any{"number": 42, "state": "open"},
		"label":  map[string]any{"name": "execute"},
		"sender": map[string]any{"login": "alice"},
		"repository": map[string]any{
			"name": "fixture", "clone_url": "file://" + filepath.ToSlash(remote), "default_branch": "main",
		},
	})
	if err != nil {
		t.Fatal(err)
	}
	payload, err := json.Marshal(map[string]any{
		"name": "fixture", "owner": "owner@example.com",
		"content_type": "application/json", "body": base64.StdEncoding.EncodeToString(delivery),
		"headers": map[string]string{"x-github-event": "issues"},
	})
	if err != nil {
		t.Fatal(err)
	}
	return consumer.Event{Payload: payload}
}

func waitForDoorbell(t *testing.T, engine *Runner, want bool) {
	t.Helper()
	deadline := time.Now().Add(5 * time.Second)
	wantDepth := 0
	if want {
		wantDepth = 1
	}
	for time.Now().Before(deadline) {
		if len(engine.wake) == wantDepth {
			return
		}
		time.Sleep(time.Millisecond)
	}
	t.Fatalf("doorbell depth = %d, want %d", len(engine.wake), wantDepth)
}

func TestIssueSessionCreatesFreshWorktreeAndPinsInstructionsBeforeSend(t *testing.T) {
	// R-F4R4-YHBH
	// R-F76X-Q0SV
	fixture := newFixture(t, 1, time.Minute)
	canonical, remote := fixture.addRepo(t, "alpha")
	wantTip := advanceRemote(t, remote)
	issue := 41
	protocol := &protocolStub{issue: IssueContent{
		Title: "Broken widget", Body: "Repair the widget.",
		Comments: []string{"First comment", "Second comment"},
	}}
	fixture.config.Protocol = protocol
	instructionPath := filepath.Join(fixture.stateRoot, "sessions", "issue-one", "instructions.md")
	var sent string
	var seenTip, seenBranch string
	firstSend := true
	fixture.config.Factory = AgentFactoryFunc(func(config ConversationConfig) Agent {
		return agentFunc(func(_ context.Context, text string) error {
			if firstSend {
				firstSend = false
				contents, err := os.ReadFile(instructionPath)
				if err != nil {
					t.Errorf("instructions did not exist before Send: %v", err)
				}
				if string(contents) != text {
					t.Errorf("pinned instructions = %q, Send text = %q", contents, text)
				}
				sent = text
				worktree := filepath.Join(fixture.stateRoot, "sessions", "issue-one", "worktree")
				seenTip = gitOutput(t, worktree, "rev-parse", "HEAD")
				seenBranch = gitOutput(t, worktree, "branch", "--show-current")
			}
			return nil
		})
	})
	runner := fixture.runner(t)
	session, err := runner.Enqueue(context.Background(), SessionRequest{
		ID: "issue-one", RepoName: "alpha", OwnerEmail: "owner@example.com", IssueNumber: &issue,
	})
	if err != nil {
		t.Fatalf("enqueue: %v", err)
	}
	if session.Branch != "ikibot/issue-41" {
		t.Fatalf("branch = %q", session.Branch)
	}
	ctx, cancel := context.WithCancel(context.Background())
	defer cancel()
	go runner.Dispatch(ctx)
	waitStatus(t, fixture.store, session.ID, repos.StatusSucceeded)
	worktree := filepath.Join(fixture.stateRoot, "sessions", session.ID, "worktree")
	if seenTip != wantTip {
		t.Fatalf("worktree tip during run = %s, want fresh origin tip %s", seenTip, wantTip)
	}
	if seenBranch != "ikibot/issue-41" {
		t.Fatalf("worktree branch during run = %q", seenBranch)
	}
	if _, err := os.Stat(worktree); !os.IsNotExist(err) {
		t.Fatalf("successful worktree remains after finish: %v", err)
	}
	if got := gitOutput(t, canonical, "branch", "--show-current"); got != "main" {
		t.Fatalf("canonical branch changed to %q", got)
	}
	for _, want := range []string{"Broken widget", "Repair the widget.", "First comment", "Second comment"} {
		if !strings.Contains(sent, want) {
			t.Errorf("instructions missing %q: %q", want, sent)
		}
	}
	if protocol.fetches != 1 {
		t.Fatalf("github peer fetches = %d, want 1", protocol.fetches)
	}

	fixture.addRepo(t, "beta")
	failed := "inspected failure"
	if err := fixture.store.InsertSession(context.Background(), repos.Session{
		ID: "old-failure", RepoName: "beta", OwnerEmail: "owner@example.com",
		IssueNumber: &issue, Attempt: 1, Branch: "ikibot/issue-41", Instructions: "old",
		Status: repos.StatusFailed, Error: &failed, CreatedAt: fixture.clock.Now(), LogPath: "old.jsonl",
	}); err != nil {
		t.Fatal(err)
	}
	next, err := runner.Enqueue(context.Background(), SessionRequest{
		ID: "issue-two", RepoName: "beta", OwnerEmail: "owner@example.com", IssueNumber: &issue,
	})
	if err != nil || next.Attempt != 2 || next.Branch != "ikibot/issue-41.2" {
		t.Fatalf("next attempt = %#v, %v", next, err)
	}

	manualText := "manual text\nverbatim\n"
	manual, err := runner.Enqueue(context.Background(), SessionRequest{
		ID: "manual", RepoName: "alpha", OwnerEmail: "owner@example.com", Instructions: manualText,
	})
	if err != nil {
		t.Fatal(err)
	}
	waitStatus(t, fixture.store, manual.ID, repos.StatusSucceeded)
	contents, err := os.ReadFile(filepath.Join(fixture.stateRoot, "sessions", manual.ID, "instructions.md"))
	if err != nil || string(contents) != manualText {
		t.Fatalf("manual instructions = %q, %v", contents, err)
	}
}

func TestDispatcherEnforcesGlobalAndPerRepoCapsInFIFOOrder(t *testing.T) {
	// R-F8EU-3SJK
	fixture := newFixture(t, 2, time.Minute)
	for _, name := range []string{"one", "two", "three"} {
		fixture.addRepo(t, name)
	}
	started := make(chan string, 10)
	release := make(chan struct{}, 10)
	var mu sync.Mutex
	active, peak := 0, 0
	fixture.config.Factory = AgentFactoryFunc(func(ConversationConfig) Agent {
		return agentFunc(func(ctx context.Context, text string) error {
			mu.Lock()
			active++
			if active > peak {
				peak = active
			}
			mu.Unlock()
			started <- text
			select {
			case <-release:
			case <-ctx.Done():
				return ctx.Err()
			}
			mu.Lock()
			active--
			mu.Unlock()
			return nil
		})
	})
	runner := fixture.runner(t)
	for i, name := range []string{"one", "two", "three"} {
		fixture.clock.Advance(time.Second)
		if _, err := runner.Enqueue(context.Background(), SessionRequest{
			ID: fmt.Sprintf("fifo-%d", i), RepoName: name, OwnerEmail: "owner@example.com", Instructions: name,
		}); err != nil {
			t.Fatal(err)
		}
	}
	ctx, cancel := context.WithCancel(context.Background())
	defer cancel()
	go runner.Dispatch(ctx)
	waitForStarts(t, started, map[string]bool{"one": true, "two": true})
	assertStatus(t, fixture.store, "fifo-0", repos.StatusRunning)
	assertStatus(t, fixture.store, "fifo-1", repos.StatusRunning)
	assertStatus(t, fixture.store, "fifo-2", repos.StatusQueued)
	release <- struct{}{}
	if got := waitStart(t, started); got != "three" {
		t.Fatalf("third admission = %q, want FIFO session three", got)
	}
	release <- struct{}{}
	release <- struct{}{}
	for i := range 3 {
		waitStatus(t, fixture.store, fmt.Sprintf("fifo-%d", i), repos.StatusSucceeded)
	}

	for i := range 2 {
		fixture.clock.Advance(time.Second)
		if _, err := runner.Enqueue(context.Background(), SessionRequest{
			ID: fmt.Sprintf("same-%d", i), RepoName: "one", OwnerEmail: "owner@example.com", Instructions: fmt.Sprintf("same-%d", i),
		}); err != nil {
			t.Fatal(err)
		}
	}
	if got := waitStart(t, started); got != "same-0" {
		t.Fatalf("same-repo first = %q", got)
	}
	assertStatus(t, fixture.store, "same-1", repos.StatusQueued)
	release <- struct{}{}
	if got := waitStart(t, started); got != "same-1" {
		t.Fatalf("same-repo second = %q", got)
	}
	release <- struct{}{}
	waitStatus(t, fixture.store, "same-1", repos.StatusSucceeded)
	mu.Lock()
	defer mu.Unlock()
	if peak != 2 {
		t.Fatalf("peak concurrency = %d, want exactly 2", peak)
	}
}

func TestTTLAndUserCancellationAreClassifiedAndReleaseRepo(t *testing.T) {
	// R-F9MQ-HKA9
	fixture := newFixture(t, 2, time.Second)
	fixture.addRepo(t, "alpha")
	started := make(chan string, 5)
	fixture.config.Factory = AgentFactoryFunc(func(ConversationConfig) Agent {
		return agentFunc(func(ctx context.Context, text string) error {
			started <- text
			if text == "after-ttl" {
				return nil
			}
			<-ctx.Done()
			return ctx.Err()
		})
	})
	runner := fixture.runner(t)
	ctx, cancelDispatch := context.WithCancel(context.Background())
	defer cancelDispatch()
	go runner.Dispatch(ctx)
	for index, id := range []string{"ttl", "after-ttl"} {
		if index > 0 {
			fixture.clock.Advance(time.Second)
		}
		if _, err := runner.Enqueue(context.Background(), SessionRequest{
			ID: id, RepoName: "alpha", OwnerEmail: "owner@example.com", Instructions: id,
		}); err != nil {
			t.Fatal(err)
		}
	}
	if got := waitStart(t, started); got != "ttl" {
		t.Fatalf("first start = %q", got)
	}
	timed := waitStatus(t, fixture.store, "ttl", repos.StatusFailed)
	if timed.Error == nil || *timed.Error != "session TTL exceeded" || timed.EndedAt == nil {
		t.Fatalf("TTL terminal fields = %#v", timed)
	}
	if got := waitStart(t, started); got != "after-ttl" {
		t.Fatalf("repo was not released after TTL; start = %q", got)
	}
	waitStatus(t, fixture.store, "after-ttl", repos.StatusSucceeded)

	// User cancellation is a separate assertion from the deliberately short
	// TTL above. Give it enough time that scheduler load cannot classify the
	// session as an expiry between observing its start and calling Cancel.
	runner.ttl = time.Minute

	if _, err := runner.Enqueue(context.Background(), SessionRequest{
		ID: "cancel", RepoName: "alpha", OwnerEmail: "owner@example.com", Instructions: "cancel",
	}); err != nil {
		t.Fatal(err)
	}
	if got := waitStart(t, started); got != "cancel" {
		t.Fatalf("cancel start = %q", got)
	}
	if !runner.Cancel("cancel") {
		t.Fatal("Cancel returned false for running session")
	}
	cancelled := waitStatus(t, fixture.store, "cancel", repos.StatusCancelled)
	if cancelled.EndedAt == nil {
		t.Fatal("cancelled session has no ended_at")
	}
}

func TestRecoverSweepsRunningAndPreservesQueuedForDispatch(t *testing.T) {
	// R-FAUM-VC0Y
	fixture := newFixture(t, 1, time.Minute)
	fixture.addRepo(t, "alpha")
	now := fixture.clock.Now()
	for _, session := range []repos.Session{
		{ID: "orphan", RepoName: "alpha", OwnerEmail: "owner@example.com", Attempt: 1, Branch: "orphan", Instructions: "old", Status: repos.StatusRunning, CreatedAt: now, LogPath: "old.jsonl"},
		{ID: "survivor", RepoName: "alpha", OwnerEmail: "owner@example.com", Attempt: 1, Branch: "ikibot/session-survivor", Instructions: "queued", Status: repos.StatusQueued, CreatedAt: now.Add(time.Second), LogPath: filepath.Join(fixture.stateRoot, "sessions", "survivor", "output.jsonl")},
	} {
		if err := fixture.store.InsertSession(context.Background(), session); err != nil {
			t.Fatal(err)
		}
	}
	started := make(chan string, 1)
	fixture.config.Factory = AgentFactoryFunc(func(ConversationConfig) Agent {
		return agentFunc(func(_ context.Context, text string) error { started <- text; return nil })
	})
	runner := fixture.runner(t)
	count, err := runner.Recover(context.Background())
	if err != nil || count != 1 {
		t.Fatalf("Recover = %d, %v; want 1, nil", count, err)
	}
	orphan := waitStatus(t, fixture.store, "orphan", repos.StatusFailed)
	if orphan.Error == nil || *orphan.Error != "interrupted by restart" {
		t.Fatalf("orphan = %#v", orphan)
	}
	assertStatus(t, fixture.store, "survivor", repos.StatusQueued)
	ctx, cancel := context.WithCancel(context.Background())
	defer cancel()
	go runner.Dispatch(ctx)
	if got := waitStart(t, started); got != "queued" {
		t.Fatalf("recovered queued instructions = %q", got)
	}
	waitStatus(t, fixture.store, "survivor", repos.StatusSucceeded)
}

func TestModelValidationRejectsBadBootConfigurationAndAcceptsDefaultPricing(t *testing.T) {
	// R-FC2J-93RN
	for _, config := range []ModelConfig{
		{Provider: "anthropic", Model: "unknown-model", APIKey: "key"},
		{Provider: "anthropic", Model: "claude-opus-4-8", APIKey: ""},
	} {
		if _, err := ValidateModel(config); err == nil || !strings.Contains(err.Error(), config.Provider+"/"+config.Model) {
			t.Fatalf("ValidateModel(%#v) error = %v; want named pair", config, err)
		}
	}
	bad := ModelConfig{Provider: "anthropic", Model: "unknown-model", APIKey: "key"}
	if _, err := New(Config{Model: bad}); err == nil || !strings.Contains(err.Error(), "anthropic/unknown-model") {
		t.Fatalf("runner construction reached dependency/route setup before model validation: %v", err)
	}
	provider, err := ValidateModel(DefaultModelConfig("fixture-key"))
	if err != nil {
		t.Fatalf("default model did not pass real pricing registry: %v", err)
	}
	if _, ok := provider.Pricing("claude-opus-4-8"); !ok {
		t.Fatal("default model absent from real provider pricing table")
	}
}

func TestPassingCheckPushesBranchCreatesPRAndPersistsURL(t *testing.T) {
	// R-FEIC-0N91
	// R-FUD0-ZNW2
	// R-FWST-R7DG
	fixture := newFixture(t, 1, time.Minute)
	canonical, remote := fixture.addRepo(t, "passing")
	installCheck(t, canonical, "#!/bin/sh\necho gate-passed\n")
	recorder := newGitHubRecorder(t)
	defer recorder.Close()
	fixture.config.Protocol = repos.NewProtocol(repos.NewGitHubPeerAt(recorder.URL, recorder.Client()))
	issue := 23
	fixture.config.Factory = committingFactory(filepath.Join(fixture.stateRoot, "sessions", "passing", "worktree"), "agent-pass")
	runner := fixture.runner(t)
	session := enqueueAndDispatch(t, runner, SessionRequest{ID: "passing", RepoName: "passing", OwnerEmail: "owner@example.com", IssueNumber: &issue})
	ended := waitStatus(t, fixture.store, session.ID, repos.StatusSucceeded)
	if ended.PRURL == nil || *ended.PRURL != "https://example.test/pull/1" {
		t.Fatalf("persisted PR URL = %#v", ended.PRURL)
	}
	if !remoteHasBranch(t, remote, "ikibot/issue-23") {
		t.Fatal("passing branch was not pushed")
	}
	sessionDir := filepath.Join(fixture.stateRoot, "sessions", session.ID)
	if _, err := os.Stat(filepath.Join(sessionDir, "worktree")); !os.IsNotExist(err) {
		t.Fatalf("successful worktree remains: %v", err)
	}
	for _, name := range []string{"instructions.md", "output.jsonl", "check.log"} {
		if _, err := os.Stat(filepath.Join(sessionDir, name)); err != nil {
			t.Fatalf("durable %s missing after success: %v", name, err)
		}
	}
	var kind, subject string
	if err := fixture.db.QueryRow(`SELECT kind, subject FROM outbox WHERE seq = 1`).Scan(&kind, &subject); err != nil || kind != "session.succeeded" || subject != "/passing" {
		t.Fatalf("runner outcome = %q %q, %v", kind, subject, err)
	}
	pr := recorder.only(t, "pr_create")
	if pr.string("head") != "ikibot/issue-23" || pr.string("base") != "main" ||
		!strings.Contains(pr.string("body"), "Fixes #23") || !strings.Contains(pr.string("body"), "passing") {
		t.Fatalf("PR arguments = %#v", pr.arguments)
	}
	if got := recorder.namesAfter("pr_create"); !reflect.DeepEqual(got[:2], []string{"issue_comment", "label_remove"}) {
		t.Fatalf("calls after PR = %v", got)
	}
}

func TestFailingCheckPushesBranchWithoutPRAndPersistsFullLog(t *testing.T) {
	// R-FFQ8-EEZQ
	// R-FWST-R7DG
	fixture := newFixture(t, 1, time.Minute)
	canonical, remote := fixture.addRepo(t, "failing")
	installCheck(t, canonical, "#!/bin/sh\necho first-line\necho final-tail\nexit 7\n")
	recorder := newGitHubRecorder(t)
	defer recorder.Close()
	fixture.config.Protocol = repos.NewProtocol(repos.NewGitHubPeerAt(recorder.URL, recorder.Client()))
	issue := 24
	fixture.config.Factory = committingFactory(filepath.Join(fixture.stateRoot, "sessions", "failing", "worktree"), "agent-fail")
	runner := fixture.runner(t)
	session := enqueueAndDispatch(t, runner, SessionRequest{ID: "failing", RepoName: "failing", OwnerEmail: "owner@example.com", IssueNumber: &issue})
	ended := waitStatus(t, fixture.store, session.ID, repos.StatusFailed)
	if ended.Error == nil || !strings.Contains(*ended.Error, "final-tail") || recorder.count("pr_create") != 0 {
		t.Fatalf("failed outcome = %#v, PR calls = %d", ended, recorder.count("pr_create"))
	}
	if !remoteHasBranch(t, remote, "ikibot/issue-24") {
		t.Fatal("failed branch was not pushed")
	}
	if _, err := os.Stat(filepath.Join(fixture.stateRoot, "sessions", session.ID, "worktree")); err != nil {
		t.Fatalf("failed worktree was not retained: %v", err)
	}
	log, err := os.ReadFile(filepath.Join(fixture.stateRoot, "sessions", "failing", "check.log"))
	if err != nil || string(log) != "first-line\nfinal-tail\n" {
		t.Fatalf("check.log = %q, %v", log, err)
	}
	comment := recorder.last(t, "issue_comment")
	if !strings.Contains(comment.string("body"), "final-tail") || !recorder.hasLabel("failed") {
		t.Fatalf("failure calls = %#v", recorder.calls)
	}
}

func TestMissingCheckCreatesPRWithNoCheckDeclaration(t *testing.T) {
	// R-FGY4-S6QF
	fixture := newFixture(t, 1, time.Minute)
	fixture.addRepo(t, "unchecked")
	recorder := newGitHubRecorder(t)
	defer recorder.Close()
	fixture.config.Protocol = repos.NewProtocol(repos.NewGitHubPeerAt(recorder.URL, recorder.Client()))
	issue := 25
	fixture.config.Factory = committingFactory(filepath.Join(fixture.stateRoot, "sessions", "unchecked", "worktree"), "agent-unchecked")
	runner := fixture.runner(t)
	session := enqueueAndDispatch(t, runner, SessionRequest{ID: "unchecked", RepoName: "unchecked", OwnerEmail: "owner@example.com", IssueNumber: &issue})
	waitStatus(t, fixture.store, session.ID, repos.StatusSucceeded)
	if body := recorder.only(t, "pr_create").string("body"); !strings.Contains(body, "no check declared") {
		t.Fatalf("PR body = %q", body)
	}
	if _, err := os.Stat(filepath.Join(fixture.stateRoot, "sessions", "unchecked", "check.log")); !os.IsNotExist(err) {
		t.Fatalf("undeclared check unexpectedly created a log: %v", err)
	}
}

func TestNoCommitFailsWithoutPushOrPR(t *testing.T) {
	// R-FI61-5YH4
	fixture := newFixture(t, 1, time.Minute)
	_, remote := fixture.addRepo(t, "empty")
	recorder := newGitHubRecorder(t)
	defer recorder.Close()
	fixture.config.Protocol = repos.NewProtocol(repos.NewGitHubPeerAt(recorder.URL, recorder.Client()))
	fixture.config.Factory = AgentFactoryFunc(func(ConversationConfig) Agent {
		return agentFunc(func(context.Context, string) error { return nil })
	})
	issue := 26
	runner := fixture.runner(t)
	session := enqueueAndDispatch(t, runner, SessionRequest{ID: "empty", RepoName: "empty", OwnerEmail: "owner@example.com", IssueNumber: &issue})
	ended := waitStatus(t, fixture.store, session.ID, repos.StatusFailed)
	if ended.Error == nil || *ended.Error != "no commits produced" || recorder.count("pr_create") != 0 || remoteHasBranch(t, remote, session.Branch) {
		t.Fatalf("no-commit outcome = %#v, calls = %#v", ended, recorder.calls)
	}
}

func TestRetryPushesAttemptTwoBranch(t *testing.T) {
	// R-FKLT-XHYI
	fixture := newFixture(t, 1, time.Minute)
	_, remote := fixture.addRepo(t, "retry")
	issue := 27
	reason := "old failure"
	if err := fixture.store.InsertSession(context.Background(), repos.Session{
		ID: "old", RepoName: "retry", OwnerEmail: "owner@example.com", IssueNumber: &issue,
		Attempt: 1, Branch: "ikibot/issue-27", Status: repos.StatusFailed, Error: &reason,
		CreatedAt: fixture.clock.Now(), LogPath: "old.log",
	}); err != nil {
		t.Fatal(err)
	}
	recorder := newGitHubRecorder(t)
	defer recorder.Close()
	fixture.config.Protocol = repos.NewProtocol(repos.NewGitHubPeerAt(recorder.URL, recorder.Client()))
	fixture.config.Factory = committingFactory(filepath.Join(fixture.stateRoot, "sessions", "retry-2", "worktree"), "agent-retry")
	runner := fixture.runner(t)
	session := enqueueAndDispatch(t, runner, SessionRequest{ID: "retry-2", RepoName: "retry", OwnerEmail: "owner@example.com", IssueNumber: &issue})
	waitStatus(t, fixture.store, session.ID, repos.StatusSucceeded)
	if session.Branch != "ikibot/issue-27.2" || !remoteHasBranch(t, remote, session.Branch) {
		t.Fatalf("retry branch = %q, pushed = %v", session.Branch, remoteHasBranch(t, remote, session.Branch))
	}
}

func TestManualSessionSkipsIssueTrafficAndCreatesPRWithoutFixes(t *testing.T) {
	// R-FLTQ-B9P7
	fixture := newFixture(t, 1, time.Minute)
	canonical, _ := fixture.addRepo(t, "manual")
	installCheck(t, canonical, "#!/bin/sh\necho manual-check-passed\n")
	recorder := newGitHubRecorder(t)
	defer recorder.Close()
	fixture.config.Protocol = repos.NewProtocol(repos.NewGitHubPeerAt(recorder.URL, recorder.Client()))
	fixture.config.Factory = committingFactory(filepath.Join(fixture.stateRoot, "sessions", "manual-protocol", "worktree"), "agent-manual")
	runner := fixture.runner(t)
	session := enqueueAndDispatch(t, runner, SessionRequest{ID: "manual-protocol", RepoName: "manual", OwnerEmail: "owner@example.com", Instructions: "manual work"})
	waitStatus(t, fixture.store, session.ID, repos.StatusSucceeded)
	if recorder.count("label_add") != 0 || recorder.count("label_remove") != 0 || recorder.count("issue_comment") != 0 {
		t.Fatalf("manual issue traffic = %#v", recorder.calls)
	}
	body := recorder.only(t, "pr_create").string("body")
	if strings.Contains(body, "Fixes #") || !strings.Contains(body, "manual-check-passed") {
		t.Fatalf("manual PR body = %q", body)
	}
}

type fixture struct {
	store     *repos.Store
	db        *sql.DB
	git       *repos.Git
	clock     *fakeClock
	stateRoot string
	config    Config
}

func newFixture(t *testing.T, maxRun int, ttl time.Duration) *fixture {
	t.Helper()
	db, err := appdb.Open(filepath.Join(t.TempDir(), "repos.db"))
	if err != nil {
		t.Fatal(err)
	}
	t.Cleanup(func() { db.Close() })
	migrations, err := reposdb.Migrations()
	if err != nil {
		t.Fatal(err)
	}
	if err := appdb.Migrate(context.Background(), db, migrations); err != nil {
		t.Fatal(err)
	}
	stateRoot := t.TempDir()
	clock := &fakeClock{now: time.Date(2026, 7, 15, 12, 0, 0, 0, time.UTC)}
	f := &fixture{store: repos.NewStore(db), db: db, clock: clock, stateRoot: stateRoot}
	f.git = repos.NewGit(filepath.Join(stateRoot, "repos"), staticToken("fixture"))
	producer, err := outbox.New(db, outbox.Options{Source: "repos", Registry: repos.Events, Now: clock.Now})
	if err != nil {
		t.Fatal(err)
	}
	reaper, err := repos.NewReaper(f.store, f.git, clock, stateRoot, repos.DefaultWorktreeTTL)
	if err != nil {
		t.Fatal(err)
	}
	f.config = Config{Store: f.store, Git: f.git, Clock: clock, StateRoot: stateRoot,
		MaxRun: maxRun, TTL: ttl, Model: DefaultModelConfig("fixture-key"), Outbox: producer, Reaper: reaper}
	return f
}

func (f *fixture) runner(t *testing.T) *Runner {
	t.Helper()
	runner, err := New(f.config)
	if err != nil {
		t.Fatal(err)
	}
	return runner
}

func (f *fixture) addRepo(t *testing.T, name string) (string, string) {
	t.Helper()
	remote := newBareRemote(t, name)
	if err := f.git.Clone(context.Background(), "file://"+filepath.ToSlash(remote), name); err != nil {
		t.Fatalf("clone %s: %v", name, err)
	}
	if err := f.store.InsertRepo(context.Background(), repos.Repo{
		Name: name, OwnerEmail: "owner@example.com", CloneURL: "file://" + filepath.ToSlash(remote),
		DefaultBranch: "main", CreatedAt: f.clock.Now(),
	}); err != nil {
		t.Fatal(err)
	}
	return filepath.Join(f.stateRoot, "repos", name), remote
}

type staticToken string

func (s staticToken) Token(context.Context) (string, error) { return string(s), nil }

type fakeClock struct {
	mu  sync.Mutex
	now time.Time
}

func (c *fakeClock) Now() time.Time { c.mu.Lock(); defer c.mu.Unlock(); return c.now }
func (c *fakeClock) Advance(d time.Duration) {
	c.mu.Lock()
	c.now = c.now.Add(d)
	c.mu.Unlock()
}

type agentFunc func(context.Context, string) error

func (f agentFunc) Send(ctx context.Context, text string) error { return f(ctx, text) }

type protocolStub struct {
	issue   IssueContent
	fetches int
}

func (p *protocolStub) FetchIssue(context.Context, repos.Session) (IssueContent, error) {
	p.fetches++
	return p.issue, nil
}
func (*protocolStub) PostQueued(context.Context, repos.Session) error { return nil }

func waitStatus(t *testing.T, store *repos.Store, id, status string) repos.Session {
	t.Helper()
	deadline := time.Now().Add(5 * time.Second)
	for time.Now().Before(deadline) {
		session, err := store.GetSession(context.Background(), id)
		if err == nil && session.Status == status {
			return session
		}
		time.Sleep(5 * time.Millisecond)
	}
	session, err := store.GetSession(context.Background(), id)
	t.Fatalf("session %s = %#v, %v; never reached %s", id, session, err, status)
	return repos.Session{}
}

func assertStatus(t *testing.T, store *repos.Store, id, status string) {
	t.Helper()
	session, err := store.GetSession(context.Background(), id)
	if err != nil || session.Status != status {
		t.Fatalf("session %s status = %q, %v; want %q", id, session.Status, err, status)
	}
}

func waitStart(t *testing.T, started <-chan string) string {
	t.Helper()
	select {
	case value := <-started:
		return value
	case <-time.After(5 * time.Second):
		t.Fatal("timed out waiting for agent start")
		return ""
	}
}

func waitForStarts(t *testing.T, started <-chan string, want map[string]bool) {
	t.Helper()
	got := map[string]bool{}
	for range len(want) {
		got[waitStart(t, started)] = true
	}
	if !reflect.DeepEqual(got, want) {
		t.Fatalf("started = %v, want %v", got, want)
	}
}

func newBareRemote(t *testing.T, name string) string {
	t.Helper()
	root := t.TempDir()
	remote := filepath.Join(root, name+".git")
	gitRun(t, "", "init", "--bare", "--initial-branch=main", remote)
	seed := filepath.Join(root, "seed")
	gitRun(t, "", "init", "--initial-branch=main", seed)
	commitFile(t, seed, "initial")
	gitRun(t, seed, "remote", "add", "origin", remote)
	gitRun(t, seed, "push", "origin", "main")
	return remote
}

func advanceRemote(t *testing.T, remote string) string {
	t.Helper()
	dir := filepath.Join(t.TempDir(), "advance")
	gitRun(t, "", "clone", remote, dir)
	commitFile(t, dir, "advanced")
	gitRun(t, dir, "push", "origin", "main")
	return gitOutput(t, dir, "rev-parse", "HEAD")
}

func commitFile(t *testing.T, dir, contents string) {
	t.Helper()
	gitRun(t, dir, "config", "user.email", "fixture@example.com")
	gitRun(t, dir, "config", "user.name", "Fixture")
	if err := os.WriteFile(filepath.Join(dir, contents+".txt"), []byte(contents), 0o644); err != nil {
		t.Fatal(err)
	}
	gitRun(t, dir, "add", ".")
	gitRun(t, dir, "commit", "-m", contents)
}

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

func installCheck(t *testing.T, canonical, script string) {
	t.Helper()
	path := filepath.Join(canonical, ".ikibot", "check")
	if err := os.MkdirAll(filepath.Dir(path), 0o755); err != nil {
		t.Fatal(err)
	}
	if err := os.WriteFile(path, []byte(script), 0o755); err != nil {
		t.Fatal(err)
	}
	gitRun(t, canonical, "add", ".ikibot/check")
	gitRun(t, canonical, "commit", "-m", "add check")
	gitRun(t, canonical, "push", "origin", "main")
}

func committingFactory(worktree, contents string) AgentFactory {
	return AgentFactoryFunc(func(ConversationConfig) Agent {
		return agentFunc(func(context.Context, string) error {
			command := exec.Command("git", "config", "user.email", "agent@example.com")
			command.Dir = worktree
			if output, err := command.CombinedOutput(); err != nil {
				return fmt.Errorf("git config email: %w: %s", err, output)
			}
			command = exec.Command("git", "config", "user.name", "Fixture Agent")
			command.Dir = worktree
			if output, err := command.CombinedOutput(); err != nil {
				return fmt.Errorf("git config name: %w: %s", err, output)
			}
			if err := os.WriteFile(filepath.Join(worktree, contents+".txt"), []byte(contents), 0o644); err != nil {
				return err
			}
			command = exec.Command("git", "add", ".")
			command.Dir = worktree
			if output, err := command.CombinedOutput(); err != nil {
				return fmt.Errorf("git add: %w: %s", err, output)
			}
			command = exec.Command("git", "commit", "-m", contents)
			command.Dir = worktree
			if output, err := command.CombinedOutput(); err != nil {
				return fmt.Errorf("git commit: %w: %s", err, output)
			}
			return nil
		})
	})
}

func enqueueAndDispatch(t *testing.T, runner *Runner, request SessionRequest) repos.Session {
	t.Helper()
	session, err := runner.Enqueue(context.Background(), request)
	if err != nil {
		t.Fatal(err)
	}
	ctx, cancel := context.WithCancel(context.Background())
	t.Cleanup(cancel)
	go runner.Dispatch(ctx)
	return session
}

func remoteHasBranch(t *testing.T, remote, branch string) bool {
	t.Helper()
	command := exec.Command("git", "ls-remote", "--heads", remote, "refs/heads/"+branch)
	output, err := command.CombinedOutput()
	if err != nil {
		t.Fatalf("git ls-remote: %v\n%s", err, output)
	}
	return len(strings.TrimSpace(string(output))) > 0
}

type recordedCall struct {
	name      string
	arguments map[string]any
}

func (c recordedCall) string(name string) string {
	value, _ := c.arguments[name].(string)
	return value
}

type githubRecorder struct {
	*httptest.Server
	mu    sync.Mutex
	calls []recordedCall
}

func newGitHubRecorder(t *testing.T) *githubRecorder {
	t.Helper()
	recorder := &githubRecorder{}
	recorder.Server = httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		var request struct {
			Params struct {
				Name      string         `json:"name"`
				Arguments map[string]any `json:"arguments"`
			} `json:"params"`
		}
		if err := json.NewDecoder(r.Body).Decode(&request); err != nil {
			t.Error(err)
		}
		recorder.mu.Lock()
		recorder.calls = append(recorder.calls, recordedCall{name: request.Params.Name, arguments: request.Params.Arguments})
		recorder.mu.Unlock()
		result := any(map[string]any{})
		switch request.Params.Name {
		case "issue_get":
			result = map[string]any{"number": 1, "title": "Fixture issue", "body": "Do the work."}
		case "issue_comments":
			result = []any{}
		case "pr_create":
			result = map[string]any{"number": 1, "url": "https://example.test/pull/1"}
		}
		if err := json.NewEncoder(w).Encode(map[string]any{"jsonrpc": "2.0", "id": 1, "result": result}); err != nil {
			t.Error(err)
		}
	}))
	return recorder
}

func (r *githubRecorder) snapshot() []recordedCall {
	r.mu.Lock()
	defer r.mu.Unlock()
	return append([]recordedCall(nil), r.calls...)
}

func (r *githubRecorder) count(name string) int {
	count := 0
	for _, call := range r.snapshot() {
		if call.name == name {
			count++
		}
	}
	return count
}

func (r *githubRecorder) only(t *testing.T, name string) recordedCall {
	t.Helper()
	var found []recordedCall
	for _, call := range r.snapshot() {
		if call.name == name {
			found = append(found, call)
		}
	}
	if len(found) != 1 {
		t.Fatalf("%s calls = %d; all calls = %#v", name, len(found), r.snapshot())
	}
	return found[0]
}

func (r *githubRecorder) last(t *testing.T, name string) recordedCall {
	t.Helper()
	calls := r.snapshot()
	for i := len(calls) - 1; i >= 0; i-- {
		if calls[i].name == name {
			return calls[i]
		}
	}
	t.Fatalf("no %s call; all calls = %#v", name, calls)
	return recordedCall{}
}

func (r *githubRecorder) namesAfter(name string) []string {
	calls := r.snapshot()
	for i, call := range calls {
		if call.name == name {
			var names []string
			for _, next := range calls[i+1:] {
				names = append(names, next.name)
			}
			return names
		}
	}
	return nil
}

func (r *githubRecorder) hasLabel(label string) bool {
	for _, call := range r.snapshot() {
		if call.name != "label_add" {
			continue
		}
		for _, value := range call.arguments["labels"].([]any) {
			if value == label {
				return true
			}
		}
	}
	return false
}
