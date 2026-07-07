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

func migrateCron(ctx context.Context, conn *sql.DB) error {
	migs, err := appkitdb.LoadMigrations(FS, "migrations")
	if err != nil {
		return err
	}
	return appkitdb.Migrate(ctx, conn, migs)
}

// TestOpenAndMigrate smoke-checks that cron's embedded migration set applies
// cleanly through appkit's runner and stands up the crontab table.
func TestOpenAndMigrate(t *testing.T) {
	ctx := context.Background()
	conn, err := appkitdb.Open(tempDB(t))
	if err != nil {
		t.Fatalf("open: %v", err)
	}
	defer conn.Close()

	if err := migrateCron(ctx, conn); err != nil {
		t.Fatalf("first migrate: %v", err)
	}

	var n int
	if err := conn.QueryRowContext(ctx, `SELECT COUNT(*) FROM schema_migrations`).Scan(&n); err != nil {
		t.Fatalf("count migrations: %v", err)
	}
	if n < 2 {
		t.Fatalf("want >=2 applied migrations (schema_migrations + crontab), got %d", n)
	}

	// crontab table is queryable.
	if _, err := conn.ExecContext(ctx, `SELECT name, expr, created_at, updated_at, last_slot FROM crontab WHERE 0`); err != nil {
		t.Fatalf("crontab table missing/shape wrong: %v", err)
	}
}

// TestMigrate_IsIdempotent re-asserts cron's set is safe to re-apply.
func TestMigrate_IsIdempotent(t *testing.T) {
	ctx := context.Background()
	conn, err := appkitdb.Open(tempDB(t))
	if err != nil {
		t.Fatalf("open: %v", err)
	}
	defer conn.Close()

	if err := migrateCron(ctx, conn); err != nil {
		t.Fatalf("first migrate: %v", err)
	}
	var before int
	if err := conn.QueryRowContext(ctx, `SELECT COUNT(*) FROM schema_migrations`).Scan(&before); err != nil {
		t.Fatalf("count before: %v", err)
	}
	if err := migrateCron(ctx, conn); err != nil {
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

// TestCrontabNameCheck asserts the DB CHECK is the charset validation boundary:
// it rejects uppercase, spaces, underscores, and the empty name, and accepts the
// event-type-safe charset (lowercase, digits, hyphen).
func TestCrontabNameCheck(t *testing.T) {
	ctx := context.Background()
	conn, err := appkitdb.Open(tempDB(t))
	if err != nil {
		t.Fatalf("open: %v", err)
	}
	defer conn.Close()
	if err := migrateCron(ctx, conn); err != nil {
		t.Fatalf("migrate: %v", err)
	}

	good := []string{"daily", "nightly-report", "job-2", "a", "0"}
	for _, name := range good {
		if _, err := conn.ExecContext(ctx,
			`INSERT INTO crontab (name, expr, created_at, updated_at) VALUES (?, '* * * * *', '2026-06-06T00:00:00Z', '2026-06-06T00:00:00Z')`, name); err != nil {
			t.Errorf("expected name %q to be accepted, got: %v", name, err)
		}
	}

	bad := []string{"", "Daily", "has space", "under_score", "dot.name", "UPPER", "tab\tname"}
	for _, name := range bad {
		if _, err := conn.ExecContext(ctx,
			`INSERT INTO crontab (name, expr, created_at, updated_at) VALUES (?, '* * * * *', '2026-06-06T00:00:00Z', '2026-06-06T00:00:00Z')`, name); err == nil {
			t.Errorf("expected name %q to be rejected by CHECK, but insert succeeded", name)
		}
	}

	// Empty expr is rejected too.
	if _, err := conn.ExecContext(ctx,
		`INSERT INTO crontab (name, expr, created_at, updated_at) VALUES ('emptyexpr', '', '2026-06-06T00:00:00Z', '2026-06-06T00:00:00Z')`); err == nil {
		t.Errorf("expected empty expr to be rejected by CHECK, but insert succeeded")
	}
}
