package prompt

import (
	"context"
	"encoding/json"
	"errors"
	"os"
	"path/filepath"
	"strings"
	"sync"
	"testing"

	"prompts/internal/db"
	"prompts/internal/sandbox"
)

// fakeRunner records Spawn/Cancel calls and does NOT auto-complete runs, so a
// session stays 'running' after Run unless a test drives completion itself.
type fakeRunner struct {
	mu      sync.Mutex
	spawned []Run
	cancels []string
	found   bool // value Cancel returns
}

func (f *fakeRunner) Spawn(run Run) {
	f.mu.Lock()
	defer f.mu.Unlock()
	f.spawned = append(f.spawned, run)
}

func (f *fakeRunner) Cancel(promptID string) bool {
	f.mu.Lock()
	defer f.mu.Unlock()
	f.cancels = append(f.cancels, promptID)
	return f.found
}

func (f *fakeRunner) spawnCount() int {
	f.mu.Lock()
	defer f.mu.Unlock()
	return len(f.spawned)
}

func newTestService(t *testing.T) (*Service, *Store, *sandbox.Manager, string) {
	t.Helper()
	ctx := context.Background()

	conn, err := db.Open(filepath.Join(t.TempDir(), "prompts.db"))
	if err != nil {
		t.Fatalf("db.Open: %v", err)
	}
	t.Cleanup(func() { conn.Close() })
	if err := db.Migrate(ctx, conn); err != nil {
		t.Fatalf("db.Migrate: %v", err)
	}

	// The sandbox Manager is rooted at the runs dir (A2): a run's sandbox is
	// runs/<run_id>/sandbox, sharing the run directory with its output.jsonl.
	runsDir := t.TempDir()
	sb, err := sandbox.New(runsDir)
	if err != nil {
		t.Fatalf("sandbox.New: %v", err)
	}

	store := NewStore(conn)
	svc := NewService(store, sb, runsDir, &fakeRunner{})
	return svc, store, sb, runsDir
}

func validConfig() Config {
	return Config{Provider: "anthropic", Model: testAnthropicModel}
}

const ownerA = "a@example.com"
const ownerB = "b@example.com"

const (
	testAnthropicModel  = "claude-haiku-4-5"
	testAnthropicSonnet = "claude-sonnet-4-6"
	testOpenAIModel     = "gpt-5.5"
	testGoogleModel     = "gemini-3.1-pro-preview"
	testZaiModel        = "glm-5.2"
)

func mustCreate(t *testing.T, svc *Service, owner string) Prompt {
	t.Helper()
	sess, err := svc.Create(context.Background(), owner, CreateInput{
		Name:       "test",
		UserPrompt: "do a thing",
		Config:     validConfig(),
	})
	if err != nil {
		t.Fatalf("Create: %v", err)
	}
	return sess
}

// TestConcurrentRunsIsolatedSandboxes proves full concurrency (Phase 3): two
// runs of the SAME prompt are both accepted (no ErrBusy / single-flight) and
// each gets its own run-scoped directory + sandbox (runs/<run_id>/sandbox), so
// the two never collide on disk.
func TestConcurrentRunsIsolatedSandboxes(t *testing.T) {
	t.Setenv("ANTHROPIC_API_KEY", "sk-test")
	svc, _, sb, runsDir := newTestService(t)
	ctx := context.Background()

	sess := mustCreate(t, svc, ownerA)

	// Fire two runs of the one prompt concurrently. Both must be accepted.
	var (
		wg      sync.WaitGroup
		mu      sync.Mutex
		runs    []Run
		runErrs []error
	)
	for i := 0; i < 2; i++ {
		wg.Add(1)
		go func() {
			defer wg.Done()
			run, err := svc.Run(ctx, ownerA, sess.ID)
			mu.Lock()
			runs = append(runs, run)
			runErrs = append(runErrs, err)
			mu.Unlock()
		}()
	}
	wg.Wait()

	for _, err := range runErrs {
		if err != nil {
			t.Fatalf("concurrent Run rejected: %v", err)
		}
	}
	if runs[0].ID == runs[1].ID {
		t.Fatalf("two runs share a run_id: %s", runs[0].ID)
	}

	// Each run has its own sandbox + log path, both materialized on disk and
	// distinct from each other.
	for _, run := range runs {
		if _, err := os.Stat(sb.Root(run.ID)); err != nil {
			t.Fatalf("run %s sandbox missing: %v", run.ID, err)
		}
		wantLog := filepath.Join(runsDir, run.ID, "output.jsonl")
		if run.LogPath != wantLog {
			t.Fatalf("run %s log path = %q, want %q", run.ID, run.LogPath, wantLog)
		}
	}
	if sb.Root(runs[0].ID) == sb.Root(runs[1].ID) {
		t.Fatalf("two runs share a sandbox dir: %s", sb.Root(runs[0].ID))
	}

	// Both runs reached the runner.
	fr := svc.runner.(*fakeRunner)
	if got := fr.spawnCount(); got != 2 {
		t.Fatalf("spawn count: want 2, got %d", got)
	}
}

// TestRunByEvent_WritesEventJSON proves an event-triggered run pins the
// triggering event to runs/<run_id>/input/event.json, and its contents
// unmarshal to the canonical envelope (source/type/event_id/payload).
func TestRunByEvent_WritesEventJSON(t *testing.T) {
	t.Setenv("ANTHROPIC_API_KEY", "sk-test")
	svc, _, _, runsDir := newTestService(t)
	ctx := context.Background()

	sess := mustCreate(t, svc, ownerA)

	payload := json.RawMessage(`{"id":"c1"}`)
	run, err := svc.RunByEvent(ctx, sess.ID, "crm", "contact.created", "01JEVENT", payload)
	if err != nil {
		t.Fatalf("RunByEvent: %v", err)
	}

	b, err := os.ReadFile(filepath.Join(runsDir, run.ID, "input", "event.json"))
	if err != nil {
		t.Fatalf("read event.json: %v", err)
	}
	var env eventEnvelope
	if err := json.Unmarshal(b, &env); err != nil {
		t.Fatalf("unmarshal event.json: %v", err)
	}
	if env.Source != "crm" || env.Type != "contact.created" || env.EventID != "01JEVENT" {
		t.Fatalf("envelope context = %+v", env)
	}
	if string(env.Payload) != `{"id":"c1"}` {
		t.Fatalf("envelope payload = %s, want {\"id\":\"c1\"}", env.Payload)
	}
}

// TestRun_NoEventJSON proves a manual Run writes NO input/event.json.
func TestRun_NoEventJSON(t *testing.T) {
	t.Setenv("ANTHROPIC_API_KEY", "sk-test")
	svc, _, _, runsDir := newTestService(t)
	ctx := context.Background()

	sess := mustCreate(t, svc, ownerA)
	run, err := svc.Run(ctx, ownerA, sess.ID)
	if err != nil {
		t.Fatalf("Run: %v", err)
	}

	if _, err := os.Stat(filepath.Join(runsDir, run.ID, "input", "event.json")); !os.IsNotExist(err) {
		t.Fatalf("manual run should write no event.json, stat err = %v", err)
	}
}

func TestValidateConfigRejectsUnsupportedProviderFirst(t *testing.T) {
	// R-JVR2-WAUP
	err := validateConfig(Config{Provider: "bogus", Model: testAnthropicModel}, func(key string) string {
		t.Fatalf("getenv should not be called for unsupported provider, got %q", key)
		return ""
	})
	ve := requireValidationError(t, err)
	if !strings.Contains(ve.Error(), `provider "bogus"`) {
		t.Fatalf("error = %q, want unsupported provider detail", ve.Error())
	}
}

func TestValidateConfigChecksModelAgainstSelectedProvider(t *testing.T) {
	// R-JWYZ-A2LE
	env := fakeEnv(map[string]string{"ANTHROPIC_API_KEY": "sk-test", "OPENAI_API_KEY": "sk-test"})
	err := validateConfig(Config{Provider: "anthropic", Model: testOpenAIModel}, env)
	ve := requireValidationError(t, err)
	if !strings.Contains(ve.Error(), "does not support model") {
		t.Fatalf("error = %q, want provider/model mismatch detail", ve.Error())
	}

	if err := validateConfig(Config{Provider: "openai", Model: testOpenAIModel}, env); err != nil {
		t.Fatalf("openai %s should validate with OPENAI_API_KEY: %v", testOpenAIModel, err)
	}
}

func TestValidateConfigRejectsUnknownModelBeforeMissingAPIKey(t *testing.T) {
	// R-JWYZ-A2LE
	err := validateConfig(Config{Provider: "openai", Model: "not-a-real-model"}, func(key string) string {
		t.Fatalf("getenv should not be called for an unsupported model, got %q", key)
		return ""
	})
	ve := requireValidationError(t, err)
	if !strings.Contains(ve.Error(), `provider "openai" does not support model "not-a-real-model"`) {
		t.Fatalf("error = %q, want unsupported model detail", ve.Error())
	}
}

func TestValidateConfigRejectsShortModelAliasBeforeMissingAPIKey(t *testing.T) {
	// R-JWYZ-A2LE
	err := validateConfig(Config{Provider: "anthropic", Model: "sonnet"}, func(key string) string {
		t.Fatalf("getenv should not be called for a short model alias, got %q", key)
		return ""
	})
	ve := requireValidationError(t, err)
	if !strings.Contains(ve.Error(), `provider "anthropic" does not support model "sonnet"`) {
		t.Fatalf("error = %q, want short alias rejection detail", ve.Error())
	}
}

func TestValidateConfigAcceptsSupportedProvidersWithInjectedKeys(t *testing.T) {
	// R-JVR2-WAUP
	// R-JWYZ-A2LE
	// R-JY6V-NUC3
	env := fakeEnv(map[string]string{
		"ANTHROPIC_API_KEY": "sk-test",
		"OPENAI_API_KEY":    "sk-test",
		"GEMINI_API_KEY":    "sk-test",
		"ZAI_API_KEY":       "sk-test",
	})
	tests := []struct {
		name string
		cfg  Config
	}{
		{"anthropic", Config{Provider: "anthropic", Model: testAnthropicModel}},
		{"openai", Config{Provider: "openai", Model: testOpenAIModel}},
		{"google", Config{Provider: "google", Model: testGoogleModel}},
		{"zai", Config{Provider: "zai", Model: testZaiModel}},
	}
	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			if err := validateConfig(tt.cfg, env); err != nil {
				t.Fatalf("validateConfig(%+v): %v", tt.cfg, err)
			}
		})
	}
}

func TestValidateConfigUsesProviderSpecificEnvVar(t *testing.T) {
	// R-JY6V-NUC3
	tests := []struct {
		name   string
		cfg    Config
		envVar string
	}{
		{"anthropic", Config{Provider: "anthropic", Model: testAnthropicModel}, "ANTHROPIC_API_KEY"},
		{"openai", Config{Provider: "openai", Model: testOpenAIModel}, "OPENAI_API_KEY"},
		{"google", Config{Provider: "google", Model: testGoogleModel}, "GEMINI_API_KEY"},
		{"zai", Config{Provider: "zai", Model: testZaiModel}, "ZAI_API_KEY"},
	}
	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			var requested []string
			err := validateConfig(tt.cfg, func(key string) string {
				requested = append(requested, key)
				return ""
			})
			ve := requireValidationError(t, err)
			if len(requested) != 1 || requested[0] != tt.envVar {
				t.Fatalf("getenv calls = %v, want [%s]", requested, tt.envVar)
			}
			if !strings.Contains(ve.Error(), tt.envVar) {
				t.Fatalf("error = %q, want missing %s", ve.Error(), tt.envVar)
			}
		})
	}
}

func TestValidateConfigAcceptsKnownModelWithOnlyProviderKey(t *testing.T) {
	// R-JZES-1M2S
	tests := []struct {
		name   string
		cfg    Config
		envVar string
	}{
		{"anthropic", Config{Provider: "anthropic", Model: testAnthropicModel}, "ANTHROPIC_API_KEY"},
		{"openai", Config{Provider: "openai", Model: testOpenAIModel}, "OPENAI_API_KEY"},
		{"google", Config{Provider: "google", Model: testGoogleModel}, "GEMINI_API_KEY"},
		{"zai", Config{Provider: "zai", Model: testZaiModel}, "ZAI_API_KEY"},
	}
	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			err := validateConfig(tt.cfg, fakeEnv(map[string]string{tt.envVar: "sk-test"}))
			if err != nil {
				t.Fatalf("validateConfig(%+v) with %s set: %v", tt.cfg, tt.envVar, err)
			}
		})
	}
}

func TestServiceCreateAndUpdatePreserveValidatedProvider(t *testing.T) {
	// R-JZES-1M2S
	t.Setenv("GEMINI_API_KEY", "sk-test")
	t.Setenv("OPENAI_API_KEY", "sk-test")
	svc, _, _, _ := newTestService(t)
	ctx := context.Background()
	temp := 0.7

	created, err := svc.Create(ctx, ownerA, CreateInput{
		UserPrompt: "p",
		Config:     Config{Provider: "google", Model: testGoogleModel, Temperature: &temp},
	})
	if err != nil {
		t.Fatalf("Create google config: %v", err)
	}
	if created.Config.Provider != "google" || created.Config.Model != testGoogleModel {
		t.Fatalf("created config = %+v, want google/%s", created.Config, testGoogleModel)
	}

	updated, err := svc.Update(ctx, ownerA, created.ID, UpdateInput{
		UserPrompt: "p2",
		Config:     Config{Provider: "openai", Model: testOpenAIModel},
	})
	if err != nil {
		t.Fatalf("Update openai config: %v", err)
	}
	if updated.Config.Provider != "openai" || updated.Config.Model != testOpenAIModel {
		t.Fatalf("updated config = %+v, want openai/%s", updated.Config, testOpenAIModel)
	}
	got, err := svc.Get(ctx, ownerA, created.ID)
	if err != nil {
		t.Fatalf("Get updated prompt: %v", err)
	}
	if got.Config.Temperature != nil {
		t.Fatalf("updated config retained omitted temperature: %+v", got.Config)
	}
	b, err := json.Marshal(got.Config)
	if err != nil {
		t.Fatalf("Marshal updated config: %v", err)
	}
	if strings.Contains(string(b), "temperature") {
		t.Fatalf("updated config JSON retained omitted temperature: %s", b)
	}
}

func fakeEnv(values map[string]string) func(string) string {
	return func(key string) string { return values[key] }
}

func requireValidationError(t *testing.T, err error) *ValidationError {
	t.Helper()
	var ve *ValidationError
	if !errors.As(err, &ve) {
		t.Fatalf("want ValidationError, got %v", err)
	}
	return ve
}

// TestUpdateDeleteAllowedWhileRunning: with single-flight gone, Update and
// Delete are ALWAYS allowed — even with a live run in flight.
func TestUpdateDeleteAllowedWhileRunning(t *testing.T) {
	t.Setenv("ANTHROPIC_API_KEY", "sk-test")
	svc, _, _, _ := newTestService(t)
	ctx := context.Background()

	sess := mustCreate(t, svc, ownerA)
	if _, err := svc.Run(ctx, ownerA, sess.ID); err != nil {
		t.Fatalf("Run: %v", err)
	}

	// fakeRunner never completes the run, so it is still "in flight" — Update
	// and Delete must both still succeed.
	if _, err := svc.Update(ctx, ownerA, sess.ID, UpdateInput{UserPrompt: "new", Config: validConfig()}); err != nil {
		t.Fatalf("Update while running: want success, got %v", err)
	}
	if err := svc.Delete(ctx, ownerA, sess.ID); err != nil {
		t.Fatalf("Delete while running: want success, got %v", err)
	}
}

func TestOwnerScoping(t *testing.T) {
	t.Setenv("ANTHROPIC_API_KEY", "sk-test")
	svc, _, _, _ := newTestService(t)
	ctx := context.Background()

	sess := mustCreate(t, svc, ownerA)

	// List as B: empty.
	listB, err := svc.List(ctx, ownerB)
	if err != nil {
		t.Fatalf("List B: %v", err)
	}
	if len(listB) != 0 {
		t.Fatalf("List B: want 0, got %d", len(listB))
	}

	if _, err := svc.Get(ctx, ownerB, sess.ID); !errors.Is(err, ErrNotFound) {
		t.Fatalf("Get B: want ErrNotFound, got %v", err)
	}
	if _, err := svc.Update(ctx, ownerB, sess.ID, UpdateInput{UserPrompt: "x", Config: validConfig()}); !errors.Is(err, ErrNotFound) {
		t.Fatalf("Update B: want ErrNotFound, got %v", err)
	}
	if err := svc.Delete(ctx, ownerB, sess.ID); !errors.Is(err, ErrNotFound) {
		t.Fatalf("Delete B: want ErrNotFound, got %v", err)
	}
	if _, err := svc.Run(ctx, ownerB, sess.ID); !errors.Is(err, ErrNotFound) {
		t.Fatalf("Run B: want ErrNotFound, got %v", err)
	}

	// Owner A still sees it.
	if _, err := svc.Get(ctx, ownerA, sess.ID); err != nil {
		t.Fatalf("Get A: %v", err)
	}
}

func TestHappyCRUD(t *testing.T) {
	t.Setenv("ANTHROPIC_API_KEY", "sk-test")
	svc, _, sb, runsDir := newTestService(t)
	ctx := context.Background()

	sess := mustCreate(t, svc, ownerA)
	if sess.Config.Provider != "anthropic" {
		t.Fatalf("provider not normalized: %q", sess.Config.Provider)
	}

	// Create makes NO sandbox (per-run now): the prompt id has no folder.
	if _, err := os.Stat(sb.Root(sess.ID)); !os.IsNotExist(err) {
		t.Fatalf("no per-prompt sandbox should exist after Create, stat err = %v", err)
	}

	// Get before any run: LastRun nil.
	detail, err := svc.Get(ctx, ownerA, sess.ID)
	if err != nil {
		t.Fatalf("Get: %v", err)
	}
	if detail.LastRun != nil {
		t.Fatalf("LastRun: want nil, got %+v", detail.LastRun)
	}

	// Run; Get.LastRun reflects the running run.
	run, err := svc.Run(ctx, ownerA, sess.ID)
	if err != nil {
		t.Fatalf("Run: %v", err)
	}
	detail, err = svc.Get(ctx, ownerA, sess.ID)
	if err != nil {
		t.Fatalf("Get after run: %v", err)
	}
	if detail.LastRun == nil || detail.LastRun.ID != run.ID {
		t.Fatalf("LastRun: want %s, got %+v", run.ID, detail.LastRun)
	}
	if detail.LastRun.Status != RunRunning {
		t.Fatalf("LastRun status: want running, got %q", detail.LastRun.Status)
	}

	// The run-scoped sandbox (runs/<run_id>/sandbox) exists after Run, and the
	// log path points at runs/<run_id>/output.jsonl.
	if _, err := os.Stat(sb.Root(run.ID)); err != nil {
		t.Fatalf("run sandbox folder missing: %v", err)
	}
	wantLog := filepath.Join(runsDir, run.ID, "output.jsonl")
	if run.LogPath != wantLog {
		t.Fatalf("log path = %q, want %q", run.LogPath, wantLog)
	}
	// No per-session sandboxes/ tree is created anywhere under the data dir.
	if _, err := os.Stat(filepath.Join(runsDir, "sandboxes")); !os.IsNotExist(err) {
		t.Fatalf("no sandboxes/ tree should exist, stat err = %v", err)
	}

	// Delete is a TOMBSTONE and always allowed (no single-flight): it removes
	// only the prompt row, leaving the run row and its on-disk directory.
	if err := svc.Delete(ctx, ownerA, sess.ID); err != nil {
		t.Fatalf("Delete: %v", err)
	}
	if _, err := svc.Get(ctx, ownerA, sess.ID); !errors.Is(err, ErrNotFound) {
		t.Fatalf("Get after delete: want ErrNotFound, got %v", err)
	}
	// The run directory (sandbox + input/ + output) SURVIVES the tombstone.
	if _, err := os.Stat(filepath.Join(runsDir, run.ID)); err != nil {
		t.Fatalf("run dir should survive tombstone: %v", err)
	}
	if _, err := os.Stat(filepath.Join(runsDir, run.ID, "input")); err != nil {
		t.Fatalf("run input/ dir should survive tombstone: %v", err)
	}
	// The run is STILL readable by the owner-scoped run readers (authorized via
	// the run's denormalized owner_email, not the now-gone prompt).
	got, err := svc.RunGet(ctx, ownerA, run.ID)
	if err != nil {
		t.Fatalf("RunGet after tombstone: %v", err)
	}
	if got.ID != run.ID {
		t.Fatalf("RunGet: want %s, got %+v", run.ID, got)
	}
}

func TestUpdateRevalidatesAndPersists(t *testing.T) {
	t.Setenv("ANTHROPIC_API_KEY", "sk-test")
	svc, _, _, _ := newTestService(t)
	ctx := context.Background()

	sess := mustCreate(t, svc, ownerA)

	// Update with a bad model is rejected.
	if _, err := svc.Update(ctx, ownerA, sess.ID, UpdateInput{UserPrompt: "p", Config: Config{Provider: "anthropic", Model: testOpenAIModel}}); err == nil {
		t.Fatalf("Update with provider/model mismatch: want error, got nil")
	}

	// Valid update persists.
	updated, err := svc.Update(ctx, ownerA, sess.ID, UpdateInput{Name: "renamed", UserPrompt: "new prompt", Config: Config{Provider: "anthropic", Model: testAnthropicSonnet}})
	if err != nil {
		t.Fatalf("Update: %v", err)
	}
	if updated.Name != "renamed" || updated.UserPrompt != "new prompt" || updated.Config.Model != testAnthropicSonnet {
		t.Fatalf("update not applied: %+v", updated)
	}
	got, err := svc.Get(ctx, ownerA, sess.ID)
	if err != nil {
		t.Fatalf("Get: %v", err)
	}
	if got.UserPrompt != "new prompt" || got.Config.Model != testAnthropicSonnet {
		t.Fatalf("update not persisted: %+v", got.Prompt)
	}
}

func TestFsAndOutputOwnerScoping(t *testing.T) {
	t.Setenv("ANTHROPIC_API_KEY", "sk-test")
	svc, _, sb, runsDir := newTestService(t)
	ctx := context.Background()

	sess := mustCreate(t, svc, ownerA)

	// Run first: the run readers key by run_id (owner-scoped via the run's
	// denormalized owner_email).
	run, err := svc.Run(ctx, ownerA, sess.ID)
	if err != nil {
		t.Fatalf("Run: %v", err)
	}
	_ = runsDir

	// Foreign owner is rejected before any disk read.
	if _, err := svc.RunFsList(ctx, ownerB, run.ID, ""); !errors.Is(err, ErrNotFound) {
		t.Fatalf("RunFsList B: want ErrNotFound, got %v", err)
	}
	if _, err := svc.RunFsRead(ctx, ownerB, run.ID, "f.txt", 0, 0); !errors.Is(err, ErrNotFound) {
		t.Fatalf("RunFsRead B: want ErrNotFound, got %v", err)
	}
	if _, err := svc.RunOutput(ctx, ownerB, run.ID, 0, 0); !errors.Is(err, ErrNotFound) {
		t.Fatalf("RunOutput B: want ErrNotFound, got %v", err)
	}

	// Write a file into the run's sandbox (runs/<run_id>/sandbox) and read it
	// back via the run-keyed RunFs* readers.
	if err := os.WriteFile(filepath.Join(sb.Root(run.ID), "f.txt"), []byte("l1\nl2\nl3\n"), 0o644); err != nil {
		t.Fatalf("write sandbox file: %v", err)
	}
	entries, err := svc.RunFsList(ctx, ownerA, run.ID, "")
	if err != nil {
		t.Fatalf("RunFsList A: %v", err)
	}
	if len(entries) != 1 || entries[0].Name != "f.txt" {
		t.Fatalf("RunFsList: want [f.txt], got %+v", entries)
	}
	got, err := svc.RunFsRead(ctx, ownerA, run.ID, "f.txt", 2, 1)
	if err != nil {
		t.Fatalf("RunFsRead A: %v", err)
	}
	if got != "l2\n" {
		t.Fatalf("RunFsRead: want %q, got %q", "l2\n", got)
	}

	if err := os.MkdirAll(filepath.Dir(run.LogPath), 0o755); err != nil {
		t.Fatalf("mkdir log: %v", err)
	}
	if err := os.WriteFile(run.LogPath, []byte(`{"e":1}
{"e":2}
{"e":3}
`), 0o644); err != nil {
		t.Fatalf("write log: %v", err)
	}
	out, err := svc.RunOutput(ctx, ownerA, run.ID, 2, 0)
	if err != nil {
		t.Fatalf("RunOutput A: %v", err)
	}
	if out != "{\"e\":2}\n{\"e\":3}\n" {
		t.Fatalf("RunOutput: got %q", out)
	}
}

func TestCancelIdempotent(t *testing.T) {
	t.Setenv("ANTHROPIC_API_KEY", "sk-test")
	svc, _, _, _ := newTestService(t)
	ctx := context.Background()

	sess := mustCreate(t, svc, ownerA)

	fr := svc.runner.(*fakeRunner)

	// Start a run, then RunCancel by run_id (idempotent — fakeRunner reports no
	// in-flight run, not an error).
	run, err := svc.Run(ctx, ownerA, sess.ID)
	if err != nil {
		t.Fatalf("Run: %v", err)
	}
	// Foreign owner: ErrNotFound, before reaching the runner.
	if err := svc.RunCancel(ctx, ownerB, run.ID); !errors.Is(err, ErrNotFound) {
		t.Fatalf("RunCancel B: want ErrNotFound, got %v", err)
	}
	fr.mu.Lock()
	n0 := len(fr.cancels)
	fr.mu.Unlock()
	if n0 != 0 {
		t.Fatalf("cancel calls after foreign cancel: want 0, got %d", n0)
	}

	if err := svc.RunCancel(ctx, ownerA, run.ID); err != nil {
		t.Fatalf("RunCancel running: %v", err)
	}
	// Idempotent: a second cancel of the same run is not an error.
	if err := svc.RunCancel(ctx, ownerA, run.ID); err != nil {
		t.Fatalf("RunCancel second: %v", err)
	}
	fr.mu.Lock()
	cancels := append([]string(nil), fr.cancels...)
	fr.mu.Unlock()
	if len(cancels) != 2 || cancels[0] != run.ID || cancels[1] != run.ID {
		t.Fatalf("cancel calls: want [%s %s] (owner A by run_id), got %v", run.ID, run.ID, cancels)
	}
}

// TestRun_MaterializesFrozenInput_SurvivesMutation is the Phase 4 governing
// invariant: Run pins the prompt's changeable execution inputs to
// runs/<run_id>/input/ BEFORE spawn, and that on-disk record is what the run
// executes. Mutating (Update) the prompt AFTER the run started must NOT change
// the run's frozen input/ — the run reads from disk, not the live prompt row.
// (Delete-survival of input/ is Phase 5's tombstone change; Phase 2's Delete
// still removes a prompt's run dirs, so it is not exercised here.)
func TestRun_MaterializesFrozenInput_SurvivesMutation(t *testing.T) {
	t.Setenv("ANTHROPIC_API_KEY", "sk-test")
	ctx := context.Background()
	svc, _, _, runsDir := newTestService(t)

	sess, err := svc.Create(ctx, ownerA, CreateInput{
		Name:         "frozen",
		UserPrompt:   "ORIGINAL user prompt",
		SystemPrompt: "ORIGINAL system prompt",
		Config:       validConfig(),
	})
	if err != nil {
		t.Fatalf("Create: %v", err)
	}

	run, err := svc.Run(ctx, ownerA, sess.ID)
	if err != nil {
		t.Fatalf("Run: %v", err)
	}

	inputDir := filepath.Join(runsDir, run.ID, "input")
	readInput := func(name string) string {
		b, err := os.ReadFile(filepath.Join(inputDir, name))
		if err != nil {
			t.Fatalf("read %s: %v", name, err)
		}
		return string(b)
	}

	// input/ was written before spawn and pins the create-time definition.
	if got := readInput("user_prompt.txt"); got != "ORIGINAL user prompt" {
		t.Fatalf("user_prompt.txt = %q, want ORIGINAL", got)
	}
	if got := readInput("system_prompt.txt"); got != "ORIGINAL system prompt" {
		t.Fatalf("system_prompt.txt = %q, want ORIGINAL", got)
	}
	if got := readInput("config.json"); !strings.Contains(got, testAnthropicModel) {
		t.Fatalf("config.json = %q, want model %s", got, testAnthropicModel)
	}

	// Mutate the prompt mid-run.
	if _, err := svc.Update(ctx, ownerA, sess.ID, UpdateInput{
		Name:         "frozen",
		UserPrompt:   "MUTATED user prompt",
		SystemPrompt: "MUTATED system prompt",
		Config:       validConfig(),
	}); err != nil {
		t.Fatalf("Update: %v", err)
	}

	// The run's frozen input/ is unchanged by the Update — the run executes
	// what was pinned, not the mutated prompt row.
	if got := readInput("user_prompt.txt"); got != "ORIGINAL user prompt" {
		t.Fatalf("after mutation user_prompt.txt = %q, want still ORIGINAL", got)
	}
	if got := readInput("system_prompt.txt"); got != "ORIGINAL system prompt" {
		t.Fatalf("after mutation system_prompt.txt = %q, want still ORIGINAL", got)
	}
}

// stubFetcher is a ContentFetcher returning canned bytes/err, so Import is
// exercised with no live dropbox or network.
type stubFetcher struct {
	data []byte
	err  error
}

func (f stubFetcher) Fetch(ctx context.Context, path string) ([]byte, error) {
	return f.data, f.err
}

// TestImportHappyAndIdempotent: a first import writes a prompt whose user_prompt
// is the file body and whose name is the basename, with a valid (runnable)
// config; a second import of the SAME path updates that same row (same prompt_id,
// new user_prompt) with no duplicate, and the config is preserved.
func TestImportHappyAndIdempotent(t *testing.T) {
	t.Setenv("ANTHROPIC_API_KEY", "sk-test")
	svc, store, _, _ := newTestService(t)
	ctx := context.Background()
	svc.Fetcher = stubFetcher{data: []byte("summarize the inbox\n")}

	p, err := svc.Import(ctx, ownerA, "/prompts/triage.md", "")
	if err != nil {
		t.Fatalf("Import: %v", err)
	}
	if p.Name != "triage.md" {
		t.Fatalf("name not derived from basename: %q", p.Name)
	}
	if p.UserPrompt != "summarize the inbox\n" {
		t.Fatalf("user_prompt: %q", p.UserPrompt)
	}
	if p.SystemPrompt != "" {
		t.Fatalf("system_prompt should be empty: %q", p.SystemPrompt)
	}
	if p.SourcePath != "/prompts/triage.md" {
		t.Fatalf("source_path: %q", p.SourcePath)
	}
	// The imported prompt must be runnable: its config must validate exactly as
	// Create requires (model resolves to an Anthropic model, key present).
	if err := validateConfig(p.Config, os.Getenv); err != nil {
		t.Fatalf("imported prompt config does not validate (run would reject it): %v", err)
	}
	if p.Config.Provider != "anthropic" {
		t.Fatalf("provider not normalized: %q", p.Config.Provider)
	}

	// Re-import the same path with new bytes → same id, updated user_prompt, no dup.
	svc.Fetcher = stubFetcher{data: []byte("summarize and label the inbox\n")}
	p2, err := svc.Import(ctx, ownerA, "/prompts/triage.md", "")
	if err != nil {
		t.Fatalf("re-Import: %v", err)
	}
	if p2.ID != p.ID {
		t.Fatalf("re-import created a new row: %q != %q", p2.ID, p.ID)
	}
	if p2.UserPrompt != "summarize and label the inbox\n" {
		t.Fatalf("re-import did not update user_prompt: %q", p2.UserPrompt)
	}
	// Config survives the re-import (a re-pull is a body refresh, not a config change).
	if err := validateConfig(p2.Config, os.Getenv); err != nil {
		t.Fatalf("re-imported prompt config does not validate: %v", err)
	}
	list, err := store.ListPrompts(ctx, ownerA)
	if err != nil || len(list) != 1 {
		t.Fatalf("expected exactly one row after re-import: len=%d err=%v", len(list), err)
	}
}

// TestImportRejectsNonUTF8 asserts a binary blob is rejected as ErrValidation.
func TestImportRejectsNonUTF8(t *testing.T) {
	svc, _, _, _ := newTestService(t)
	svc.Fetcher = stubFetcher{data: []byte{0xff, 0xfe, 0x00}}
	if _, err := svc.Import(context.Background(), ownerA, "/prompts/blob.bin", ""); !errors.Is(err, ErrValidation) {
		t.Fatalf("non-UTF-8: want ErrValidation, got %v", err)
	}
}

// TestImportRejectsTooLarge asserts a body over 1 MiB is rejected as ErrValidation.
func TestImportRejectsTooLarge(t *testing.T) {
	svc, _, _, _ := newTestService(t)
	big := make([]byte, (1<<20)+1)
	for i := range big {
		big[i] = 'a'
	}
	svc.Fetcher = stubFetcher{data: big}
	if _, err := svc.Import(context.Background(), ownerA, "/prompts/huge.md", ""); !errors.Is(err, ErrValidation) {
		t.Fatalf("too-large: want ErrValidation, got %v", err)
	}
}

// TestImportRejectsEmptySourcePath asserts a missing source_path is ErrValidation
// (before any fetch).
func TestImportRejectsEmptySourcePath(t *testing.T) {
	svc, _, _, _ := newTestService(t)
	svc.Fetcher = stubFetcher{data: []byte("x")}
	if _, err := svc.Import(context.Background(), ownerA, "", ""); !errors.Is(err, ErrValidation) {
		t.Fatalf("empty source_path: want ErrValidation, got %v", err)
	}
}

// TestDeleteTombstoneRunStillReadable is the Phase 5 gate: create → run →
// delete prompt → the run is STILL readable by the owner-scoped run readers
// (RunGet/RunList/RunOutput/RunFs*), authorized via the run's denormalized
// owner_email (the prompt is gone), and its input/ dir survives on disk. A
// foreign owner cannot read it.
func TestDeleteTombstoneRunStillReadable(t *testing.T) {
	t.Setenv("ANTHROPIC_API_KEY", "sk-test")
	svc, _, sb, runsDir := newTestService(t)
	ctx := context.Background()

	sess := mustCreate(t, svc, ownerA)
	run, err := svc.Run(ctx, ownerA, sess.ID)
	if err != nil {
		t.Fatalf("Run: %v", err)
	}

	// Seed the run's output.jsonl and a sandbox file so the readers have content.
	if err := os.WriteFile(run.LogPath, []byte(`{"line":1}`+"\n"+`{"line":2}`+"\n"), 0o644); err != nil {
		t.Fatalf("seed output.jsonl: %v", err)
	}
	if err := os.WriteFile(filepath.Join(sb.Root(run.ID), "result.txt"), []byte("hello\nworld\n"), 0o644); err != nil {
		t.Fatalf("seed sandbox file: %v", err)
	}

	// Tombstone the prompt.
	if err := svc.Delete(ctx, ownerA, sess.ID); err != nil {
		t.Fatalf("Delete: %v", err)
	}
	if _, err := svc.Get(ctx, ownerA, sess.ID); !errors.Is(err, ErrNotFound) {
		t.Fatalf("prompt should be gone, got %v", err)
	}

	// input/ dir still on disk.
	if _, err := os.Stat(filepath.Join(runsDir, run.ID, "input", "user_prompt.txt")); err != nil {
		t.Fatalf("run input/ should survive tombstone: %v", err)
	}

	// RunGet — authorized via run.owner_email even though the prompt is gone.
	got, err := svc.RunGet(ctx, ownerA, run.ID)
	if err != nil {
		t.Fatalf("RunGet after tombstone: %v", err)
	}
	if got.ID != run.ID || got.PromptID != sess.ID {
		t.Fatalf("RunGet: unexpected run %+v", got)
	}

	// RunList scopes via the prompt, which is gone → ErrNotFound (the prompt-keyed
	// listing no longer resolves once tombstoned).
	if _, err := svc.RunList(ctx, ownerA, sess.ID); !errors.Is(err, ErrNotFound) {
		t.Fatalf("RunList on tombstoned prompt: want ErrNotFound, got %v", err)
	}

	// RunOutput — reads the surviving output.jsonl.
	out, err := svc.RunOutput(ctx, ownerA, run.ID, 0, 0)
	if err != nil {
		t.Fatalf("RunOutput after tombstone: %v", err)
	}
	if !strings.Contains(out, `{"line":1}`) || !strings.Contains(out, `{"line":2}`) {
		t.Fatalf("RunOutput missing content: %q", out)
	}

	// RunFsList / RunFsRead — read the surviving sandbox.
	entries, err := svc.RunFsList(ctx, ownerA, run.ID, ".")
	if err != nil {
		t.Fatalf("RunFsList after tombstone: %v", err)
	}
	var found bool
	for _, e := range entries {
		if e.Name == "result.txt" {
			found = true
		}
	}
	if !found {
		t.Fatalf("RunFsList missing result.txt: %+v", entries)
	}
	body, err := svc.RunFsRead(ctx, ownerA, run.ID, "result.txt", 0, 0)
	if err != nil {
		t.Fatalf("RunFsRead after tombstone: %v", err)
	}
	if !strings.Contains(body, "hello") {
		t.Fatalf("RunFsRead unexpected body: %q", body)
	}

	// A foreign owner cannot read the run (authorized via run.owner_email).
	if _, err := svc.RunGet(ctx, ownerB, run.ID); !errors.Is(err, ErrNotFound) {
		t.Fatalf("RunGet by foreign owner: want ErrNotFound, got %v", err)
	}
	if _, err := svc.RunOutput(ctx, ownerB, run.ID, 0, 0); !errors.Is(err, ErrNotFound) {
		t.Fatalf("RunOutput by foreign owner: want ErrNotFound, got %v", err)
	}

	// RunCancel is idempotent on the surviving run.
	if err := svc.RunCancel(ctx, ownerA, run.ID); err != nil {
		t.Fatalf("RunCancel after tombstone: %v", err)
	}
}
