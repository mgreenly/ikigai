package db

import (
	"testing"

	appkitdb "appkit/db"
)

// TestLoadMigrations asserts that this service's real embedded migration set
// loads through appkit's shared runner without error (versions parse, are
// unique, and order correctly). An in-service duplicate or malformed migration
// file fails this test, complementing the repo-wide bin/check-migrations guard
// (docs/adr-migration-timestamps.md).
func TestLoadMigrations(t *testing.T) {
	migs, err := appkitdb.LoadMigrations(FS, "migrations")
	if err != nil {
		t.Fatalf("LoadMigrations: %v", err)
	}
	if len(migs) == 0 {
		t.Fatal("no migrations embedded")
	}
}
