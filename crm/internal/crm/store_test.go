package crm

import (
	"context"
	"database/sql"
	"path/filepath"
	"testing"
	"time"

	"crm/internal/db"
)

// txAlias lets entity test files reference *sql.Tx without each importing
// database/sql (withTx hands them one).
type txAlias = sql.Tx

// fixedClock is a deterministic, monotonically-increasing clock for tests so
// updated_at ordering is stable and reproducible.
type fixedClock struct{ t time.Time }

func (c *fixedClock) now() time.Time {
	c.t = c.t.Add(time.Millisecond)
	return c.t
}

// newTestStore returns a Service wired to a fresh, migrated SQLite database.
// PLAN.md §9 calls for :memory: with a temp-file fallback; we use a temp file
// directly — it is the robust path (WAL + the migration runner both work
// unchanged) and t.TempDir() cleans it up. The clock is deterministic.
func newTestStore(t *testing.T) *Service {
	t.Helper()
	path := filepath.Join(t.TempDir(), "crm_test.db")
	conn, err := db.Open(path)
	if err != nil {
		t.Fatalf("open test db: %v", err)
	}
	t.Cleanup(func() { conn.Close() })
	if err := db.Migrate(context.Background(), conn); err != nil {
		t.Fatalf("migrate test db: %v", err)
	}
	clk := &fixedClock{t: time.Date(2026, 6, 3, 12, 0, 0, 0, time.UTC)}
	s := NewService(conn)
	s.Now = clk.now
	return s
}

// withTx runs fn inside a transaction and commits it, failing the test on error.
// Entity-store tests use it to exercise tx-passed hooks directly.
func withTx(t *testing.T, s *Service, fn func(tx *txAlias)) {
	t.Helper()
	tx, err := s.DB.BeginTx(context.Background(), nil)
	if err != nil {
		t.Fatalf("begin tx: %v", err)
	}
	defer tx.Rollback()
	fn(tx)
	if err := tx.Commit(); err != nil {
		t.Fatalf("commit tx: %v", err)
	}
}

func sp(s string) *string { return &s }
func i64(n int64) *int64  { return &n }
