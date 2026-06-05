package db_test

import (
	"context"
	"path/filepath"
	"strings"
	"testing"

	"appkit/db"
	"appkit/internal/testmigrations"
)

func tempDB(t *testing.T) string {
	t.Helper()
	return filepath.Join(t.TempDir(), "test.db")
}

func loadMigs(t *testing.T) []db.Migration {
	t.Helper()
	migs, err := db.LoadMigrations(testmigrations.FS, "migrations")
	if err != nil {
		t.Fatalf("LoadMigrations: %v", err)
	}
	return migs
}

func TestOpenAndMigrate_Applies(t *testing.T) {
	ctx := context.Background()
	conn, err := db.Open(tempDB(t))
	if err != nil {
		t.Fatalf("open: %v", err)
	}
	defer conn.Close()

	migs := loadMigs(t)
	if err := db.Migrate(ctx, conn, migs); err != nil {
		t.Fatalf("migrate: %v", err)
	}
	v, err := db.AppliedVersion(ctx, conn)
	if err != nil {
		t.Fatalf("applied version: %v", err)
	}
	if v != 2 {
		t.Fatalf("applied version = %d, want 2", v)
	}
	if got := db.MaxEmbedded(migs); got != 2 {
		t.Fatalf("max embedded = %d, want 2", got)
	}
	// The 002 widgets table must exist (the migration body actually ran).
	if _, err := conn.ExecContext(ctx, `INSERT INTO widgets (name) VALUES ('x')`); err != nil {
		t.Fatalf("002 did not create widgets: %v", err)
	}
}

func TestMigrate_SkipsAlreadyApplied(t *testing.T) {
	ctx := context.Background()
	conn, err := db.Open(tempDB(t))
	if err != nil {
		t.Fatalf("open: %v", err)
	}
	defer conn.Close()

	migs := loadMigs(t)
	if err := db.Migrate(ctx, conn, migs); err != nil {
		t.Fatalf("first migrate: %v", err)
	}
	var before int
	if err := conn.QueryRowContext(ctx, `SELECT COUNT(*) FROM schema_migrations`).Scan(&before); err != nil {
		t.Fatalf("count before: %v", err)
	}
	if err := db.Migrate(ctx, conn, migs); err != nil {
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

func TestMigrate_RefusesDowngrade(t *testing.T) {
	ctx := context.Background()
	conn, err := db.Open(tempDB(t))
	if err != nil {
		t.Fatalf("open: %v", err)
	}
	defer conn.Close()

	migs := loadMigs(t)
	if err := db.Migrate(ctx, conn, migs); err != nil {
		t.Fatalf("baseline migrate: %v", err)
	}
	// Simulate a future migration having run against this DB — a version the
	// binary no longer embeds.
	if _, err := conn.ExecContext(ctx,
		`INSERT INTO schema_migrations (version, applied_at) VALUES (?, ?)`,
		999, "2099-01-01T00:00:00Z",
	); err != nil {
		t.Fatalf("inject future version: %v", err)
	}

	err = db.Migrate(ctx, conn, migs)
	if err == nil {
		t.Fatal("expected downgrade refusal, got nil")
	}
	if !strings.Contains(err.Error(), "refusing to downgrade") {
		t.Fatalf("error = %v, want a downgrade refusal", err)
	}
}

func TestLoadMigrations_OrderAndNaming(t *testing.T) {
	migs := loadMigs(t)
	if len(migs) != 2 {
		t.Fatalf("len = %d, want 2", len(migs))
	}
	for i, m := range migs {
		if m.Version != i+1 {
			t.Errorf("migration %d has version %d (gaps not allowed)", i, m.Version)
		}
	}
}

func TestAppliedVersion_EmptyDB(t *testing.T) {
	ctx := context.Background()
	conn, err := db.Open(tempDB(t))
	if err != nil {
		t.Fatalf("open: %v", err)
	}
	defer conn.Close()
	v, err := db.AppliedVersion(ctx, conn)
	if err != nil {
		t.Fatalf("applied version on empty db: %v", err)
	}
	if v != 0 {
		t.Fatalf("applied version on empty db = %d, want 0", v)
	}
}
