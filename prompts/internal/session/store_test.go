package session

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
	conn, err := db.Open(filepath.Join(t.TempDir(), "agent.db"))
	if err != nil {
		t.Fatalf("db.Open: %v", err)
	}
	t.Cleanup(func() { conn.Close() })
	if err := db.Migrate(ctx, conn); err != nil {
		t.Fatalf("db.Migrate: %v", err)
	}
	return NewStore(conn)
}

func seedSession(t *testing.T, store *Store, owner, status string) Session {
	t.Helper()
	now := store.nowStr()
	sess := Session{
		ID:         ids.NewULID(),
		OwnerEmail: owner,
		Name:       "n",
		Prompt:     "p",
		Config:     Config{Provider: "anthropic", Model: "haiku"},
		Status:     status,
		CreatedAt:  now,
		UpdatedAt:  now,
	}
	if err := store.InsertSession(context.Background(), sess); err != nil {
		t.Fatalf("InsertSession: %v", err)
	}
	return sess
}

func TestStoreGetNotFound(t *testing.T) {
	store := newTestStore(t)
	ctx := context.Background()

	sess := seedSession(t, store, ownerA, StatusIdle)

	if _, err := store.GetSession(ctx, "nobody@example.com", sess.ID); !errors.Is(err, ErrNotFound) {
		t.Fatalf("foreign owner: want ErrNotFound, got %v", err)
	}
	if _, err := store.GetSession(ctx, ownerA, "nonexistent"); !errors.Is(err, ErrNotFound) {
		t.Fatalf("missing id: want ErrNotFound, got %v", err)
	}

	got, err := store.GetSession(ctx, ownerA, sess.ID)
	if err != nil {
		t.Fatalf("GetSession: %v", err)
	}
	if got.Config.Model != "haiku" {
		t.Fatalf("config round-trip: %+v", got.Config)
	}
}

func TestStoreLatestRunNilWhenNone(t *testing.T) {
	store := newTestStore(t)
	ctx := context.Background()
	sess := seedSession(t, store, ownerA, StatusIdle)

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
	sess := seedSession(t, store, ownerA, StatusIdle)

	older := Run{ID: ids.NewULID(), SessionID: sess.ID, Status: RunSucceeded, StartedAt: "2026-01-01T00:00:00Z", LogPath: "a"}
	newer := Run{ID: ids.NewULID(), SessionID: sess.ID, Status: RunRunning, StartedAt: "2026-02-01T00:00:00Z", LogPath: "b"}
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
	sess := seedSession(t, store, ownerA, StatusRunning)
	run := Run{ID: ids.NewULID(), SessionID: sess.ID, Status: RunRunning, StartedAt: store.nowStr(), LogPath: "x"}
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

func TestStoreDeleteCascadesRuns(t *testing.T) {
	store := newTestStore(t)
	ctx := context.Background()
	sess := seedSession(t, store, ownerA, StatusIdle)
	run := Run{ID: ids.NewULID(), SessionID: sess.ID, Status: RunSucceeded, StartedAt: store.nowStr(), LogPath: "x"}
	if err := store.InsertRun(ctx, run); err != nil {
		t.Fatalf("InsertRun: %v", err)
	}

	if err := store.DeleteSession(ctx, ownerA, sess.ID); err != nil {
		t.Fatalf("DeleteSession: %v", err)
	}
	// Runs cascade-deleted: latest run is now nil.
	last, err := store.GetLatestRun(ctx, sess.ID)
	if err != nil {
		t.Fatalf("GetLatestRun: %v", err)
	}
	if last != nil {
		t.Fatalf("runs should have cascaded, got %+v", last)
	}
}

func TestSweepRunning(t *testing.T) {
	store := newTestStore(t)
	ctx := context.Background()

	running := seedSession(t, store, ownerA, StatusRunning)
	idle := seedSession(t, store, ownerA, StatusIdle)

	run := Run{ID: ids.NewULID(), SessionID: running.ID, Status: RunRunning, StartedAt: store.nowStr(), LogPath: "x"}
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

	got, err := store.GetLatestRun(ctx, running.ID)
	if err != nil {
		t.Fatalf("GetLatestRun: %v", err)
	}
	if got.Status != RunFailed || got.Error != "interrupted by restart" || got.EndedAt == "" {
		t.Fatalf("swept run: %+v", got)
	}

	sess, err := store.GetSession(ctx, ownerA, running.ID)
	if err != nil {
		t.Fatalf("GetSession: %v", err)
	}
	if sess.Status != StatusIdle {
		t.Fatalf("swept session status: want idle, got %q", sess.Status)
	}

	// The already-idle session is untouched.
	if s2, _ := store.GetSession(ctx, ownerA, idle.ID); s2.Status != StatusIdle {
		t.Fatalf("idle session disturbed: %q", s2.Status)
	}
}
