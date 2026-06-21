package prompt

import (
	"context"
	"errors"
	"path/filepath"
	"testing"

	"prompts/internal/db"
	"prompts/internal/ids"
)

func newTestStore(t *testing.T) *Store {
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
	return NewStore(conn)
}

func seedPrompt(t *testing.T, store *Store, owner string) Prompt {
	t.Helper()
	now := store.nowStr()
	sess := Prompt{
		ID:         ids.NewULID(),
		OwnerEmail: owner,
		Name:       "n",
		UserPrompt: "p",
		Config:     Config{Provider: "anthropic", Model: "claude-haiku-4-5"},
		CreatedAt:  now,
		UpdatedAt:  now,
	}
	if err := store.InsertPrompt(context.Background(), sess); err != nil {
		t.Fatalf("InsertPrompt: %v", err)
	}
	return sess
}

func TestStoreGetNotFound(t *testing.T) {
	store := newTestStore(t)
	ctx := context.Background()

	sess := seedPrompt(t, store, ownerA)

	if _, err := store.GetPrompt(ctx, "nobody@example.com", sess.ID); !errors.Is(err, ErrNotFound) {
		t.Fatalf("foreign owner: want ErrNotFound, got %v", err)
	}
	if _, err := store.GetPrompt(ctx, ownerA, "nonexistent"); !errors.Is(err, ErrNotFound) {
		t.Fatalf("missing id: want ErrNotFound, got %v", err)
	}

	got, err := store.GetPrompt(ctx, ownerA, sess.ID)
	if err != nil {
		t.Fatalf("GetPrompt: %v", err)
	}
	if got.Config.Model != "claude-haiku-4-5" {
		t.Fatalf("config round-trip: %+v", got.Config)
	}
}

func TestStoreLatestRunNilWhenNone(t *testing.T) {
	store := newTestStore(t)
	ctx := context.Background()
	sess := seedPrompt(t, store, ownerA)

	last, err := store.GetLatestRun(ctx, sess.ID)
	if err != nil {
		t.Fatalf("GetLatestRun: %v", err)
	}
	if last != nil {
		t.Fatalf("want nil, got %+v", last)
	}
}

func TestStoreLatestRunNewest(t *testing.T) {
	store := newTestStore(t)
	ctx := context.Background()
	sess := seedPrompt(t, store, ownerA)

	older := Run{ID: ids.NewULID(), PromptID: sess.ID, Status: RunSucceeded, StartedAt: "2026-01-01T00:00:00Z", LogPath: "a"}
	newer := Run{ID: ids.NewULID(), PromptID: sess.ID, Status: RunRunning, StartedAt: "2026-02-01T00:00:00Z", LogPath: "b"}
	if err := store.InsertRun(ctx, older); err != nil {
		t.Fatalf("InsertRun older: %v", err)
	}
	if err := store.InsertRun(ctx, newer); err != nil {
		t.Fatalf("InsertRun newer: %v", err)
	}

	last, err := store.GetLatestRun(ctx, sess.ID)
	if err != nil {
		t.Fatalf("GetLatestRun: %v", err)
	}
	if last == nil || last.ID != newer.ID {
		t.Fatalf("want newest %s, got %+v", newer.ID, last)
	}
}

func TestStoreUpdateRunTerminal(t *testing.T) {
	store := newTestStore(t)
	ctx := context.Background()
	sess := seedPrompt(t, store, ownerA)
	run := Run{ID: ids.NewULID(), PromptID: sess.ID, Status: RunRunning, StartedAt: store.nowStr(), LogPath: "x"}
	if err := store.InsertRun(ctx, run); err != nil {
		t.Fatalf("InsertRun: %v", err)
	}

	if err := store.UpdateRunTerminal(ctx, run.ID, RunSucceeded, store.nowStr(), `{"tokens":5}`, ""); err != nil {
		t.Fatalf("UpdateRunTerminal: %v", err)
	}
	got, err := store.GetLatestRun(ctx, sess.ID)
	if err != nil {
		t.Fatalf("GetLatestRun: %v", err)
	}
	if got.Status != RunSucceeded || got.UsageJSON != `{"tokens":5}` || got.EndedAt == "" {
		t.Fatalf("terminal not applied: %+v", got)
	}
}

func TestStoreDeleteIsTombstone(t *testing.T) {
	store := newTestStore(t)
	ctx := context.Background()
	sess := seedPrompt(t, store, ownerA)
	run := Run{ID: ids.NewULID(), PromptID: sess.ID, OwnerEmail: ownerA, Status: RunSucceeded, StartedAt: store.nowStr(), LogPath: "x"}
	if err := store.InsertRun(ctx, run); err != nil {
		t.Fatalf("InsertRun: %v", err)
	}

	if err := store.DeletePrompt(ctx, ownerA, sess.ID); err != nil {
		t.Fatalf("DeletePrompt: %v", err)
	}
	// Tombstone (A3): the prompt is gone but its run survives — there is no
	// cascade. The run is still addressable by run_id.
	if _, err := store.GetPrompt(ctx, ownerA, sess.ID); !errors.Is(err, ErrNotFound) {
		t.Fatalf("prompt should be gone, got err=%v", err)
	}
	got, err := store.GetRun(ctx, run.ID)
	if err != nil {
		t.Fatalf("GetRun after tombstone: %v", err)
	}
	if got.ID != run.ID || got.OwnerEmail != ownerA {
		t.Fatalf("run should survive tombstone, got %+v", got)
	}
	last, err := store.GetLatestRun(ctx, sess.ID)
	if err != nil {
		t.Fatalf("GetLatestRun: %v", err)
	}
	if last == nil || last.ID != run.ID {
		t.Fatalf("latest run should survive tombstone, got %+v", last)
	}
}

func TestSweepRunning(t *testing.T) {
	store := newTestStore(t)
	ctx := context.Background()

	withRun := seedPrompt(t, store, ownerA)

	run := Run{ID: ids.NewULID(), PromptID: withRun.ID, Status: RunRunning, StartedAt: store.nowStr(), LogPath: "x"}
	if err := store.InsertRun(ctx, run); err != nil {
		t.Fatalf("InsertRun: %v", err)
	}

	n, err := store.SweepRunning(ctx)
	if err != nil {
		t.Fatalf("SweepRunning: %v", err)
	}
	if n != 1 {
		t.Fatalf("swept count: want 1, got %d", n)
	}

	// The orphaned running run is marked failed. There is no prompt status to
	// flip — SweepRunning touches runs only.
	got, err := store.GetLatestRun(ctx, withRun.ID)
	if err != nil {
		t.Fatalf("GetLatestRun: %v", err)
	}
	if got.Status != RunFailed || got.Error != "interrupted by restart" || got.EndedAt == "" {
		t.Fatalf("swept run: %+v", got)
	}
}
