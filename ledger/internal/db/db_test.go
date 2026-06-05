package db

import (
	"context"
	"path/filepath"
	"testing"
)

func tempDB(t *testing.T) string {
	t.Helper()
	return filepath.Join(t.TempDir(), "test.db")
}

// TestOpenAndMigrate smoke-checks that ledger's embedded migration set applies
// cleanly through appkit's runner (the runner's own behaviors — idempotency,
// downgrade guard, ordering — are covered by appkit/db's tests).
func TestOpenAndMigrate(t *testing.T) {
	ctx := context.Background()
	conn, err := Open(tempDB(t))
	if err != nil {
		t.Fatalf("open: %v", err)
	}
	defer conn.Close()

	if err := Migrate(ctx, conn); err != nil {
		t.Fatalf("first migrate: %v", err)
	}

	var n int
	if err := conn.QueryRowContext(ctx, `SELECT COUNT(*) FROM schema_migrations`).Scan(&n); err != nil {
		t.Fatalf("count: %v", err)
	}
	if n < 1 {
		t.Fatalf("want >=1 applied migration after first run, got %d", n)
	}
}

// TestMigrate_IsIdempotent re-asserts that ledger's set is safe to re-apply
// (the embedded set must declare each version once).
func TestMigrate_IsIdempotent(t *testing.T) {
	ctx := context.Background()
	conn, err := Open(tempDB(t))
	if err != nil {
		t.Fatalf("open: %v", err)
	}
	defer conn.Close()

	if err := Migrate(ctx, conn); err != nil {
		t.Fatalf("first migrate: %v", err)
	}
	var before int
	if err := conn.QueryRowContext(ctx, `SELECT COUNT(*) FROM schema_migrations`).Scan(&before); err != nil {
		t.Fatalf("count before: %v", err)
	}
	if err := Migrate(ctx, conn); err != nil {
		t.Fatalf("second migrate: %v", err)
	}
	var after int
	if err := conn.QueryRowContext(ctx, `SELECT COUNT(*) FROM schema_migrations`).Scan(&after); err != nil {
		t.Fatalf("count after: %v", err)
	}
	if before != after {
		t.Fatalf("idempotent migrate changed count: %d -> %d", before, after)
	}
}
