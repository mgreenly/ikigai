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

func migrateLedger(ctx context.Context, conn *sql.DB) error {
	migs, err := appkitdb.LoadMigrations(FS, "migrations")
	if err != nil {
		return err
	}
	return appkitdb.Migrate(ctx, conn, migs)
}

// TestOpenAndMigrate smoke-checks that ledger's embedded migration set applies
// cleanly through appkit's runner (the runner's own behaviors — idempotency,
// downgrade guard, ordering — are covered by appkit/db's tests).
func TestOpenAndMigrate(t *testing.T) {
	ctx := context.Background()
	conn, err := appkitdb.Open(tempDB(t))
	if err != nil {
		t.Fatalf("open: %v", err)
	}
	defer conn.Close()

	if err := migrateLedger(ctx, conn); err != nil {
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
	conn, err := appkitdb.Open(tempDB(t))
	if err != nil {
		t.Fatalf("open: %v", err)
	}
	defer conn.Close()

	if err := migrateLedger(ctx, conn); err != nil {
		t.Fatalf("first migrate: %v", err)
	}
	var before int
	if err := conn.QueryRowContext(ctx, `SELECT COUNT(*) FROM schema_migrations`).Scan(&before); err != nil {
		t.Fatalf("count before: %v", err)
	}
	if err := migrateLedger(ctx, conn); err != nil {
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

func TestExternalRefMigration_EnforcesNonNullClaims(t *testing.T) {
	// R-FRGX-MIE4
	ctx := context.Background()
	conn, err := appkitdb.Open(tempDB(t))
	if err != nil {
		t.Fatal(err)
	}
	defer conn.Close()
	if err := migrateLedger(ctx, conn); err != nil {
		t.Fatal(err)
	}
	rows, err := conn.QueryContext(ctx, `PRAGMA table_info(transactions)`)
	if err != nil {
		t.Fatal(err)
	}
	defer rows.Close()
	var found bool
	for rows.Next() {
		var cid int
		var name, typ string
		var notnull, pk int
		var dflt any
		if err := rows.Scan(&cid, &name, &typ, &notnull, &dflt, &pk); err != nil {
			t.Fatal(err)
		}
		if name == "external_ref" {
			found = typ == "TEXT" && notnull == 0
		}
	}
	if !found {
		t.Fatal("transactions.external_ref is not nullable TEXT")
	}
	if _, err := conn.ExecContext(ctx, `INSERT INTO transactions (id,date,description,created_at,external_ref) VALUES ('a','2026-01-01','a','2026-01-01T00:00:00Z',NULL),('b','2026-01-01','b','2026-01-01T00:00:00Z',NULL)`); err != nil {
		t.Fatalf("NULL refs should both insert: %v", err)
	}
	if _, err := conn.ExecContext(ctx, `INSERT INTO transactions (id,date,description,created_at,external_ref) VALUES ('c','2026-01-01','c','2026-01-01T00:00:00Z','source:c')`); err != nil {
		t.Fatal(err)
	}
	if _, err := conn.ExecContext(ctx, `INSERT INTO transactions (id,date,description,created_at,external_ref) VALUES ('d','2026-01-01','d','2026-01-01T00:00:00Z','source:c')`); err == nil {
		t.Fatal("duplicate non-null ref inserted")
	}
}
