package crontab

import (
	"context"
	"errors"
	"path/filepath"
	"testing"
	"time"

	"cron/internal/db"
)

func newStore(t *testing.T) (*Store, context.Context) {
	t.Helper()
	ctx := context.Background()
	conn, err := db.Open(filepath.Join(t.TempDir(), "test.db"))
	if err != nil {
		t.Fatalf("open: %v", err)
	}
	t.Cleanup(func() { conn.Close() })
	if err := db.Migrate(ctx, conn); err != nil {
		t.Fatalf("migrate: %v", err)
	}
	return NewStore(conn), ctx
}

func TestStore_CreateGetListUpdateDelete(t *testing.T) {
	s, ctx := newStore(t)
	now := time.Date(2026, 6, 6, 12, 0, 0, 0, time.UTC)

	// Create
	e, err := s.Create(ctx, "nightly", "0 3 * * *", now)
	if err != nil {
		t.Fatalf("create: %v", err)
	}
	if e.Name != "nightly" || e.Expr != "0 3 * * *" {
		t.Fatalf("create round-trip wrong: %+v", e)
	}
	if e.LastSlot != nil {
		t.Fatalf("new entry should have nil last_slot, got %v", e.LastSlot)
	}
	if !e.CreatedAt.Equal(now) || !e.UpdatedAt.Equal(now) {
		t.Fatalf("timestamps wrong: created=%v updated=%v", e.CreatedAt, e.UpdatedAt)
	}

	// Get
	got, err := s.Get(ctx, "nightly")
	if err != nil {
		t.Fatalf("get: %v", err)
	}
	if got.Expr != "0 3 * * *" {
		t.Fatalf("get expr wrong: %q", got.Expr)
	}

	// List (add a second so ordering is exercised)
	if _, err := s.Create(ctx, "hourly", "0 * * * *", now); err != nil {
		t.Fatalf("create second: %v", err)
	}
	list, err := s.List(ctx)
	if err != nil {
		t.Fatalf("list: %v", err)
	}
	if len(list) != 2 || list[0].Name != "hourly" || list[1].Name != "nightly" {
		t.Fatalf("list ordering/contents wrong: %+v", list)
	}

	// Update bumps updated_at and changes expr, leaves last_slot
	later := now.Add(time.Hour)
	up, err := s.Update(ctx, "nightly", "30 4 * * *", later)
	if err != nil {
		t.Fatalf("update: %v", err)
	}
	if up.Expr != "30 4 * * *" {
		t.Fatalf("update expr wrong: %q", up.Expr)
	}
	if !up.UpdatedAt.Equal(later) {
		t.Fatalf("update did not bump updated_at: %v", up.UpdatedAt)
	}
	if !up.CreatedAt.Equal(now) {
		t.Fatalf("update changed created_at: %v", up.CreatedAt)
	}

	// Delete
	if err := s.Delete(ctx, "nightly"); err != nil {
		t.Fatalf("delete: %v", err)
	}
	if _, err := s.Get(ctx, "nightly"); !errors.Is(err, ErrNotFound) {
		t.Fatalf("get after delete: want ErrNotFound, got %v", err)
	}
}

func TestStore_CreateDuplicate(t *testing.T) {
	s, ctx := newStore(t)
	now := time.Now().UTC()
	if _, err := s.Create(ctx, "dup", "* * * * *", now); err != nil {
		t.Fatalf("first create: %v", err)
	}
	_, err := s.Create(ctx, "dup", "* * * * *", now)
	if !errors.Is(err, ErrExists) {
		t.Fatalf("duplicate create: want ErrExists, got %v", err)
	}
}

func TestStore_UpdateDeleteMissing(t *testing.T) {
	s, ctx := newStore(t)
	now := time.Now().UTC()
	if _, err := s.Update(ctx, "ghost", "* * * * *", now); !errors.Is(err, ErrNotFound) {
		t.Fatalf("update missing: want ErrNotFound, got %v", err)
	}
	if err := s.Delete(ctx, "ghost"); !errors.Is(err, ErrNotFound) {
		t.Fatalf("delete missing: want ErrNotFound, got %v", err)
	}
}

func TestStore_CreateInvalidName(t *testing.T) {
	s, ctx := newStore(t)
	now := time.Now().UTC()
	// The DB CHECK is the validation boundary; the store maps it to ErrInvalid.
	if _, err := s.Create(ctx, "Bad Name", "* * * * *", now); !errors.Is(err, ErrInvalid) {
		t.Fatalf("invalid name: want ErrInvalid, got %v", err)
	}
}
