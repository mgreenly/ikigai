package main

import (
	"context"
	"encoding/json"
	"io"
	"log/slog"
	"path/filepath"
	"reflect"
	"sync"
	"testing"
	"time"

	"appkit"
	"appkit/server"

	"eventplane/consumer"

	"prompts/internal/consume"
	"prompts/internal/db"
	"prompts/internal/prompt"
	"prompts/internal/sandbox"
)

func TestPromptsSpecDeclaresPerSourceConsumers(t *testing.T) {
	// R-DFV4-7W4Y
	spec := promptsSpec()
	wantSources := []string{"cron", "crm", "ledger", "dropbox", "scripts", "prompts"}

	if !reflect.DeepEqual(sources, wantSources) {
		t.Fatalf("sources = %#v, want %#v", sources, wantSources)
	}
	if got := len(spec.Consumers); got != len(wantSources) {
		t.Fatalf("len(spec.Consumers) = %d, want %d", got, len(wantSources))
	}
	if spec.Consumes != nil {
		t.Fatalf("spec.Consumes = %#v, want nil", spec.Consumes)
	}
	if spec.Subscriptions != nil {
		t.Fatal("spec.Subscriptions is set, want nil")
	}
	for i, entry := range spec.Consumers {
		if entry.Source != wantSources[i] {
			t.Fatalf("spec.Consumers[%d].Source = %q, want %q", i, entry.Source, wantSources[i])
		}
		wantSubs := consume.Subscriptions([]string{entry.Source})
		if !reflect.DeepEqual(entry.Subscriptions, wantSubs) {
			t.Fatalf("spec.Consumers[%d].Subscriptions = %#v, want %#v", i, entry.Subscriptions, wantSubs)
		}
		if entry.Handler == nil {
			t.Fatalf("spec.Consumers[%d].Handler is nil", i)
		}
	}
}

func TestPromptsSpecConsumerHandlerRunsMatchingPromptOnly(t *testing.T) {
	// R-DH30-LNVN
	t.Setenv("ANTHROPIC_API_KEY", "sk-test")
	ctx := context.Background()

	conn, err := db.Open(filepath.Join(t.TempDir(), "prompts.db"))
	if err != nil {
		t.Fatalf("db.Open: %v", err)
	}
	t.Cleanup(func() { _ = conn.Close() })
	if err := db.Migrate(ctx, conn); err != nil {
		t.Fatalf("db.Migrate: %v", err)
	}

	runsDir := t.TempDir()
	sb, err := sandbox.New(filepath.Join(t.TempDir(), "sandboxes"))
	if err != nil {
		t.Fatalf("sandbox.New: %v", err)
	}
	store := prompt.NewStore(conn)
	runner := &countingRunner{}
	svc := prompt.NewService(store, sb, runsDir, runner)

	previousSvc := svcRef
	svcRef = svc
	t.Cleanup(func() { svcRef = previousSvc })

	p, err := svc.Create(ctx, "owner@example.com", prompt.CreateInput{
		UserPrompt: "summarize the file",
		Config:     prompt.Config{Model: "claude-haiku-4-5"},
	})
	if err != nil {
		t.Fatalf("Create: %v", err)
	}
	if _, err := svc.SetTrigger(ctx, "owner@example.com", p.ID, "dropbox", "file.created"); err != nil {
		t.Fatalf("SetTrigger: %v", err)
	}

	rt := newTestRouter(t)
	entry := consumerEntry(t, promptsSpec(), "dropbox")
	h := entry.Handler(rt)
	if h == nil {
		t.Fatal("consumer Handler returned nil")
	}

	matching := consumer.Event{
		Type:    "file.created",
		ID:      "evt-match",
		Source:  "dropbox",
		Payload: json.RawMessage(`{"path":"/x"}`),
	}
	if err := h(ctx, matching); err != nil {
		t.Fatalf("matching handler: %v", err)
	}
	waitForSpawnCount(t, runner, 1)

	detail, err := svc.Get(ctx, "owner@example.com", p.ID)
	if err != nil {
		t.Fatalf("Get: %v", err)
	}
	if detail.LastRun == nil {
		t.Fatal("matching event did not create a run")
	}
	if lr := detail.LastRun; lr.TriggerSource != "dropbox" || lr.TriggerType != "file.created" || lr.TriggerEventID != "evt-match" {
		t.Fatalf("matching event run trigger context = %+v, want dropbox/file.created/evt-match", lr)
	}

	notMatching := consumer.Event{
		Type:    "file.deleted",
		ID:      "evt-miss",
		Source:  "dropbox",
		Payload: json.RawMessage(`{"path":"/x"}`),
	}
	if err := h(ctx, notMatching); err != nil {
		t.Fatalf("non-matching handler: %v", err)
	}
	if got := runner.count(); got != 1 {
		t.Fatalf("spawn count after non-matching event = %d, want 1", got)
	}
}

type countingRunner struct {
	mu     sync.Mutex
	spawns int
}

func (r *countingRunner) Spawn(prompt.Run) {
	r.mu.Lock()
	defer r.mu.Unlock()
	r.spawns++
}

func (r *countingRunner) Cancel(string) bool { return false }

func (r *countingRunner) count() int {
	r.mu.Lock()
	defer r.mu.Unlock()
	return r.spawns
}

func newTestRouter(t *testing.T) *server.Router {
	t.Helper()
	var rt *server.Router
	_, err := server.New(server.Options{
		Addr:    "127.0.0.1:0",
		Apex:    true,
		Logger:  slog.New(slog.NewTextHandler(io.Discard, nil)),
		Version: "test",
		Service: "prompts",
		Register: func(r *server.Router) error {
			rt = r
			return nil
		},
	})
	if err != nil {
		t.Fatalf("server.New: %v", err)
	}
	if rt == nil {
		t.Fatal("server.New did not invoke Register")
	}
	return rt
}

func consumerEntry(t *testing.T, spec appkit.Spec, source string) appkit.Consumer {
	t.Helper()
	for _, entry := range spec.Consumers {
		if entry.Source == source {
			return entry
		}
	}
	t.Fatalf("consumer source %q not found in %#v", source, spec.Consumers)
	return appkit.Consumer{}
}

func waitForSpawnCount(t *testing.T, runner *countingRunner, want int) {
	t.Helper()
	deadline := time.Now().Add(2 * time.Second)
	for time.Now().Before(deadline) {
		if runner.count() == want {
			return
		}
		time.Sleep(2 * time.Millisecond)
	}
	t.Fatalf("spawn count = %d, want %d", runner.count(), want)
}
