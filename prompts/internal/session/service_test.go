package session

import (
	"context"
	"errors"
	"os"
	"path/filepath"
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

func (f *fakeRunner) Spawn(sess Session, run Run) {
	f.mu.Lock()
	defer f.mu.Unlock()
	f.spawned = append(f.spawned, run)
}

func (f *fakeRunner) Cancel(sessionID string) bool {
	f.mu.Lock()
	defer f.mu.Unlock()
	f.cancels = append(f.cancels, sessionID)
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

	conn, err := db.Open(filepath.Join(t.TempDir(), "agent.db"))
	if err != nil {
		t.Fatalf("db.Open: %v", err)
	}
	t.Cleanup(func() { conn.Close() })
	if err := db.Migrate(ctx, conn); err != nil {
		t.Fatalf("db.Migrate: %v", err)
	}

	sb, err := sandbox.New(filepath.Join(t.TempDir(), "sandboxes"))
	if err != nil {
		t.Fatalf("sandbox.New: %v", err)
	}
	runsDir := t.TempDir()

	store := NewStore(conn)
	svc := NewService(store, sb, runsDir, &fakeRunner{})
	return svc, store, sb, runsDir
}

func validConfig() Config {
	return Config{Model: "haiku"}
}

const ownerA = "a@example.com"
const ownerB = "b@example.com"

func mustCreate(t *testing.T, svc *Service, owner string) Session {
	t.Helper()
	sess, err := svc.Create(context.Background(), owner, CreateInput{
		Name:   "test",
		Prompt: "do a thing",
		Config: validConfig(),
	})
	if err != nil {
		t.Fatalf("Create: %v", err)
	}
	return sess
}

func TestSingleFlight(t *testing.T) {
	t.Setenv("ANTHROPIC_API_KEY", "sk-test")
	svc, _, _, _ := newTestService(t)
	ctx := context.Background()

	sess := mustCreate(t, svc, ownerA)

	if _, err := svc.Run(ctx, ownerA, sess.ID); err != nil {
		t.Fatalf("first Run: %v", err)
	}
	// fakeRunner does not complete the run, so the session is still running.
	_, err := svc.Run(ctx, ownerA, sess.ID)
	if !errors.Is(err, ErrBusy) {
		t.Fatalf("second Run: want ErrBusy, got %v", err)
	}

	fr := svc.runner.(*fakeRunner)
	if got := fr.spawnCount(); got != 1 {
		t.Fatalf("spawn count: want 1, got %d", got)
	}
}

func TestConfigValidation(t *testing.T) {
	svc, _, _, _ := newTestService(t)
	ctx := context.Background()

	t.Run("unknown model", func(t *testing.T) {
		t.Setenv("ANTHROPIC_API_KEY", "sk-test")
		_, err := svc.Create(ctx, ownerA, CreateInput{Prompt: "p", Config: Config{Model: "totally-bogus"}})
		var ve *ValidationError
		if !errors.As(err, &ve) {
			t.Fatalf("want ValidationError, got %v", err)
		}
	})

	t.Run("non-anthropic model", func(t *testing.T) {
		t.Setenv("ANTHROPIC_API_KEY", "sk-test")
		for _, m := range []string{"gpt-4o", "gemini-3.1-pro-preview"} {
			_, err := svc.Create(ctx, ownerA, CreateInput{Prompt: "p", Config: Config{Model: m}})
			var ve *ValidationError
			if !errors.As(err, &ve) {
				t.Fatalf("model %q: want ValidationError, got %v", m, err)
			}
		}
	})

	t.Run("missing api key", func(t *testing.T) {
		t.Setenv("ANTHROPIC_API_KEY", "")
		_, err := svc.Create(ctx, ownerA, CreateInput{Prompt: "p", Config: validConfig()})
		var ve *ValidationError
		if !errors.As(err, &ve) {
			t.Fatalf("want ValidationError, got %v", err)
		}
	})
}

func TestRejectWhileRunning(t *testing.T) {
	t.Setenv("ANTHROPIC_API_KEY", "sk-test")
	svc, _, _, _ := newTestService(t)
	ctx := context.Background()

	sess := mustCreate(t, svc, ownerA)
	if _, err := svc.Run(ctx, ownerA, sess.ID); err != nil {
		t.Fatalf("Run: %v", err)
	}

	_, err := svc.Update(ctx, ownerA, sess.ID, UpdateInput{Prompt: "new", Config: validConfig()})
	if !errors.Is(err, ErrRunning) {
		t.Fatalf("Update while running: want ErrRunning, got %v", err)
	}
	if err := svc.Delete(ctx, ownerA, sess.ID); !errors.Is(err, ErrRunning) {
		t.Fatalf("Delete while running: want ErrRunning, got %v", err)
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
	if _, err := svc.Update(ctx, ownerB, sess.ID, UpdateInput{Prompt: "x", Config: validConfig()}); !errors.Is(err, ErrNotFound) {
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
	svc, _, sb, _ := newTestService(t)
	ctx := context.Background()

	sess := mustCreate(t, svc, ownerA)
	if sess.Status != StatusIdle {
		t.Fatalf("new session status: want idle, got %q", sess.Status)
	}
	if sess.Config.Provider != "anthropic" {
		t.Fatalf("provider not normalized: %q", sess.Config.Provider)
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
	if detail.Status != StatusRunning {
		t.Fatalf("session status after run: want running, got %q", detail.Status)
	}

	// Sandbox folder exists.
	if _, err := os.Stat(sb.Root(sess.ID)); err != nil {
		t.Fatalf("sandbox folder missing: %v", err)
	}

	// To delete, return to idle (simulate run completion).
	if err := svc.store.SetSessionStatus(ctx, sess.ID, StatusIdle); err != nil {
		t.Fatalf("reset status: %v", err)
	}
	if err := svc.Delete(ctx, ownerA, sess.ID); err != nil {
		t.Fatalf("Delete: %v", err)
	}
	if _, err := svc.Get(ctx, ownerA, sess.ID); !errors.Is(err, ErrNotFound) {
		t.Fatalf("Get after delete: want ErrNotFound, got %v", err)
	}
	if _, err := os.Stat(sb.Root(sess.ID)); !os.IsNotExist(err) {
		t.Fatalf("sandbox folder should be gone, stat err = %v", err)
	}
}

func TestUpdateRevalidatesAndPersists(t *testing.T) {
	t.Setenv("ANTHROPIC_API_KEY", "sk-test")
	svc, _, _, _ := newTestService(t)
	ctx := context.Background()

	sess := mustCreate(t, svc, ownerA)

	// Update with a bad model is rejected.
	if _, err := svc.Update(ctx, ownerA, sess.ID, UpdateInput{Prompt: "p", Config: Config{Model: "gpt-4o"}}); err == nil {
		t.Fatalf("Update with non-anthropic model: want error, got nil")
	}

	// Valid update persists.
	updated, err := svc.Update(ctx, ownerA, sess.ID, UpdateInput{Name: "renamed", Prompt: "new prompt", Config: Config{Model: "sonnet"}})
	if err != nil {
		t.Fatalf("Update: %v", err)
	}
	if updated.Name != "renamed" || updated.Prompt != "new prompt" || updated.Config.Model != "sonnet" {
		t.Fatalf("update not applied: %+v", updated)
	}
	got, err := svc.Get(ctx, ownerA, sess.ID)
	if err != nil {
		t.Fatalf("Get: %v", err)
	}
	if got.Prompt != "new prompt" || got.Config.Model != "sonnet" {
		t.Fatalf("update not persisted: %+v", got.Session)
	}
}

func TestFsAndOutputOwnerScoping(t *testing.T) {
	t.Setenv("ANTHROPIC_API_KEY", "sk-test")
	svc, _, sb, runsDir := newTestService(t)
	ctx := context.Background()

	sess := mustCreate(t, svc, ownerA)

	// Foreign owner is rejected before any disk read.
	if _, err := svc.FsList(ctx, ownerB, sess.ID, ""); !errors.Is(err, ErrNotFound) {
		t.Fatalf("FsList B: want ErrNotFound, got %v", err)
	}
	if _, err := svc.FsRead(ctx, ownerB, sess.ID, "f.txt", 0, 0); !errors.Is(err, ErrNotFound) {
		t.Fatalf("FsRead B: want ErrNotFound, got %v", err)
	}
	if _, err := svc.Output(ctx, ownerB, sess.ID, 0, 0); !errors.Is(err, ErrNotFound) {
		t.Fatalf("Output B: want ErrNotFound, got %v", err)
	}

	// Happy path: write a file into the sandbox and read it back as the owner.
	if err := os.WriteFile(filepath.Join(sb.Root(sess.ID), "f.txt"), []byte("l1\nl2\nl3\n"), 0o644); err != nil {
		t.Fatalf("write sandbox file: %v", err)
	}
	entries, err := svc.FsList(ctx, ownerA, sess.ID, "")
	if err != nil {
		t.Fatalf("FsList A: %v", err)
	}
	if len(entries) != 1 || entries[0].Name != "f.txt" {
		t.Fatalf("FsList: want [f.txt], got %+v", entries)
	}
	got, err := svc.FsRead(ctx, ownerA, sess.ID, "f.txt", 2, 1)
	if err != nil {
		t.Fatalf("FsRead A: %v", err)
	}
	if got != "l2\n" {
		t.Fatalf("FsRead: want %q, got %q", "l2\n", got)
	}

	// Output before any run: error (no run yet).
	if _, err := svc.Output(ctx, ownerA, sess.ID, 0, 0); err == nil {
		t.Fatalf("Output before run: want error, got nil")
	}

	// Run, then drop a fake log at the run's LogPath and read it back.
	run, err := svc.Run(ctx, ownerA, sess.ID)
	if err != nil {
		t.Fatalf("Run: %v", err)
	}
	_ = runsDir
	if err := os.MkdirAll(filepath.Dir(run.LogPath), 0o755); err != nil {
		t.Fatalf("mkdir log: %v", err)
	}
	if err := os.WriteFile(run.LogPath, []byte(`{"e":1}
{"e":2}
{"e":3}
`), 0o644); err != nil {
		t.Fatalf("write log: %v", err)
	}
	out, err := svc.Output(ctx, ownerA, sess.ID, 2, 0)
	if err != nil {
		t.Fatalf("Output A: %v", err)
	}
	if out != "{\"e\":2}\n{\"e\":3}\n" {
		t.Fatalf("Output: got %q", out)
	}
}

func TestCancelIdempotent(t *testing.T) {
	t.Setenv("ANTHROPIC_API_KEY", "sk-test")
	svc, _, _, _ := newTestService(t)
	ctx := context.Background()

	sess := mustCreate(t, svc, ownerA)
	// No in-flight run: Cancel is not an error.
	if err := svc.Cancel(ctx, ownerA, sess.ID); err != nil {
		t.Fatalf("Cancel idle: %v", err)
	}
	// Foreign owner: ErrNotFound.
	if err := svc.Cancel(ctx, ownerB, sess.ID); !errors.Is(err, ErrNotFound) {
		t.Fatalf("Cancel B: want ErrNotFound, got %v", err)
	}
	fr := svc.runner.(*fakeRunner)
	fr.mu.Lock()
	n := len(fr.cancels)
	fr.mu.Unlock()
	if n != 1 {
		t.Fatalf("cancel calls: want 1 (owner A only), got %d", n)
	}
}
