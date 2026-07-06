package db

import (
	"context"
	"database/sql"
	"path/filepath"
	"testing"

	appkitdb "appkit/db"
)

func tempDB(t *testing.T) string {
	t.Helper()
	return filepath.Join(t.TempDir(), "test.db")
}

func migrateNotify(ctx context.Context, conn *sql.DB) error {
	migs, err := appkitdb.LoadMigrations(FS, "migrations")
	if err != nil {
		return err
	}
	return appkitdb.Migrate(ctx, conn, migs)
}

// TestOpenAndMigrate smoke-checks that notify's embedded migration set applies
// cleanly through appkit's runner (the runner's own behaviors — idempotency,
// downgrade guard, ordering — are covered by appkit/db's tests).
func TestOpenAndMigrate(t *testing.T) {
	ctx := context.Background()
	conn, err := appkitdb.Open(tempDB(t))
	if err != nil {
		t.Fatalf("open: %v", err)
	}
	defer conn.Close()

	if err := migrateNotify(ctx, conn); err != nil {
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

// TestMigrate_IsIdempotent re-asserts that notify's set is safe to re-apply (the
// embedded set must declare each version once); the feed_offset row store is the
// consumer engine's, written only by the engine.
func TestMigrate_IsIdempotent(t *testing.T) {
	ctx := context.Background()
	conn, err := appkitdb.Open(tempDB(t))
	if err != nil {
		t.Fatalf("open: %v", err)
	}
	defer conn.Close()

	if err := migrateNotify(ctx, conn); err != nil {
		t.Fatalf("first migrate: %v", err)
	}
	var before int
	if err := conn.QueryRowContext(ctx, `SELECT COUNT(*) FROM schema_migrations`).Scan(&before); err != nil {
		t.Fatalf("count before: %v", err)
	}
	if err := migrateNotify(ctx, conn); err != nil {
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
