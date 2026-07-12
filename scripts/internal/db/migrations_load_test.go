package db

import (
	"context"
	"crypto/sha256"
	"fmt"
	"os"
	"path/filepath"
	"testing"

	appkitdb "appkit/db"
)

// TestLoadMigrations asserts that this service's real embedded migration set
// loads through appkit's shared runner without error (versions parse, are
// unique, and order correctly). An in-service duplicate or malformed migration
// file fails this test (docs/adr-migration-timestamps.md).
func TestLoadMigrations(t *testing.T) {
	migs, err := appkitdb.LoadMigrations(FS, "migrations")
	if err != nil {
		t.Fatalf("LoadMigrations: %v", err)
	}
	if len(migs) == 0 {
		t.Fatal("no migrations embedded")
	}
}

func TestTriggerFilterMigrationShape(t *testing.T) {
	// R-7TR5-QSY4
	db, err := appkitdb.Open(filepath.Join(t.TempDir(), "fresh.db"))
	if err != nil {
		t.Fatal(err)
	}
	defer db.Close()
	migs, err := appkitdb.LoadMigrations(FS, "migrations")
	if err != nil {
		t.Fatal(err)
	}
	if err := appkitdb.Migrate(context.Background(), db, migs); err != nil {
		t.Fatal(err)
	}
	columns := func(table string) map[string]bool {
		rows, err := db.Query("PRAGMA table_info(" + table + ")")
		if err != nil {
			t.Fatal(err)
		}
		defer rows.Close()
		got := map[string]bool{}
		for rows.Next() {
			var cid int
			var name, typ string
			var notNull, pk int
			var def any
			if err := rows.Scan(&cid, &name, &typ, &notNull, &def, &pk); err != nil {
				t.Fatal(err)
			}
			got[name] = true
		}
		return got
	}
	triggers := columns("script_triggers")
	wantTriggers := map[string]bool{"script_id": true, "source": true, "filter": true, "created_at": true}
	if len(triggers) != len(wantTriggers) {
		t.Errorf("script_triggers columns = %v, want exactly %v", triggers, wantTriggers)
	}
	for name := range wantTriggers {
		if !triggers[name] {
			t.Errorf("script_triggers missing %s", name)
		}
	}
	var pkScriptID, pkFilter int
	if err := db.QueryRow(`SELECT pk FROM pragma_table_info('script_triggers') WHERE name = 'script_id'`).Scan(&pkScriptID); err != nil {
		t.Fatal(err)
	}
	if err := db.QueryRow(`SELECT pk FROM pragma_table_info('script_triggers') WHERE name = 'filter'`).Scan(&pkFilter); err != nil {
		t.Fatal(err)
	}
	if pkScriptID != 1 || pkFilter != 2 {
		t.Fatalf("script_triggers primary key positions = (%d, %d), want (1, 2)", pkScriptID, pkFilter)
	}
	runs := columns("runs")
	if !runs["trigger_kind"] || !runs["trigger_subject"] || runs["trigger_type"] {
		t.Fatalf("runs columns = %v", runs)
	}

	// 002 is frozen: this digest is its committed body, not merely a successful
	// migration result that could hide a retrospective edit to the old schema.
	frozen, err := os.ReadFile("migrations/002_scripts.sql")
	if err != nil {
		t.Fatal(err)
	}
	if got := fmt.Sprintf("%x", sha256.Sum256(frozen)); got != "b71fe8a87367ea55a253c6425fa9bbc457e56ce307442a8bf659658c9f9d07cd" {
		t.Fatalf("002_scripts.sql digest = %s; frozen migration was edited", got)
	}
}
