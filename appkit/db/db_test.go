package db_test

import (
	"context"
	"path/filepath"
	"strings"
	"testing"
	"testing/fstest"

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
	// Versions must be strictly ascending and unique; contiguity (no gaps) is
	// NOT required — timestamp migrations are sparse by design.
	for i := 1; i < len(migs); i++ {
		if migs[i].Version <= migs[i-1].Version {
			t.Errorf("migration %d version %d not strictly after prior %d", i, migs[i].Version, migs[i-1].Version)
		}
	}
}

// TestLoadMigrations_Timestamp confirms a 14-digit UTC timestamp filename loads
// and parses into the right Version (int is 64-bit on linux/amd64, so no
// overflow).
func TestLoadMigrations_Timestamp(t *testing.T) {
	fsys := fstest.MapFS{
		"m/20260607143022_add_widgets.sql": {Data: []byte("SELECT 1;")},
	}
	migs, err := db.LoadMigrations(fsys, "m")
	if err != nil {
		t.Fatalf("LoadMigrations: %v", err)
	}
	if len(migs) != 1 {
		t.Fatalf("len = %d, want 1", len(migs))
	}
	if migs[0].Version != 20260607143022 {
		t.Errorf("version = %d, want 20260607143022", migs[0].Version)
	}
}

// TestLoadMigrations_MixedIntegerAndTimestamp confirms a legacy-integer +
// timestamp set sorts into the correct order: integers sort strictly before any
// 14-digit timestamp.
func TestLoadMigrations_MixedIntegerAndTimestamp(t *testing.T) {
	fsys := fstest.MapFS{
		"m/002_b.sql":            {Data: []byte("SELECT 1;")},
		"m/20260607143022_d.sql": {Data: []byte("SELECT 1;")},
		"m/001_a.sql":            {Data: []byte("SELECT 1;")},
		"m/003_c.sql":            {Data: []byte("SELECT 1;")},
	}
	migs, err := db.LoadMigrations(fsys, "m")
	if err != nil {
		t.Fatalf("LoadMigrations: %v", err)
	}
	want := []int{1, 2, 3, 20260607143022}
	if len(migs) != len(want) {
		t.Fatalf("len = %d, want %d", len(migs), len(want))
	}
	for i, w := range want {
		if migs[i].Version != w {
			t.Errorf("migs[%d].Version = %d, want %d", i, migs[i].Version, w)
		}
	}
}

// TestLoadMigrations_RejectsDuplicates confirms the duplicate-version guard
// still fires (e.g. a cross-branch collision on the same prefix).
func TestLoadMigrations_RejectsDuplicates(t *testing.T) {
	fsys := fstest.MapFS{
		"m/004_a.sql": {Data: []byte("SELECT 1;")},
		"m/004_b.sql": {Data: []byte("SELECT 1;")},
	}
	_, err := db.LoadMigrations(fsys, "m")
	if err == nil {
		t.Fatal("expected duplicate-version error, got nil")
	}
	if !strings.Contains(err.Error(), "duplicated") {
		t.Fatalf("error = %v, want a duplication error", err)
	}
}

// TestLoadMigrations_RejectsNonPositive confirms the defensive Version > 0 check
// rejects a zero/negative prefix that would otherwise sort ahead of everything.
func TestLoadMigrations_RejectsNonPositive(t *testing.T) {
	fsys := fstest.MapFS{
		"m/000_zero.sql": {Data: []byte("SELECT 1;")},
	}
	_, err := db.LoadMigrations(fsys, "m")
	if err == nil {
		t.Fatal("expected non-positive version error, got nil")
	}
	if !strings.Contains(err.Error(), "positive") {
		t.Fatalf("error = %v, want a positive-integer error", err)
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
