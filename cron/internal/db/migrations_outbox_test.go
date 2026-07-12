package db

import (
	"context"
	"path/filepath"
	"strings"
	"testing"

	appkitdb "appkit/db"

	"eventplane/outbox"
)

// TestOutboxMigrationMatchesLibraryDDL guards the producer invariant: the outbox
// table DDL is OWNED by the eventplane library (outbox.SchemaSQL); cron's
// newest outbox migration only applies it. If the two drift, every producer's
// outbox is no longer identical — so this test fails loudly the moment they
// diverge (mirrors crm/ledger).
// R-PSWY-UBFW
func TestOutboxRoutingMigrationMatchesLibraryDDLAndCreatesRoutingColumns(t *testing.T) {
	body, err := migrationsFS.ReadFile("migrations/20260712160651_outbox_routing.sql")
	if err != nil {
		t.Fatalf("read newest outbox migration: %v", err)
	}
	if !strings.Contains(string(body), outbox.SchemaSQL) {
		t.Fatalf("newest outbox migration does not contain the library DDL verbatim.\n--- outbox.SchemaSQL ---\n%s\n--- migration file ---\n%s",
			outbox.SchemaSQL, string(body))
	}
	conn, err := appkitdb.Open(filepath.Join(t.TempDir(), "cron.db"))
	if err != nil {
		t.Fatalf("open database: %v", err)
	}
	defer conn.Close()
	migrations, err := appkitdb.LoadMigrations(FS, "migrations")
	if err != nil {
		t.Fatalf("load migrations: %v", err)
	}
	if err := appkitdb.Migrate(context.Background(), conn, migrations); err != nil {
		t.Fatalf("apply embedded migrations: %v", err)
	}
	rows, err := conn.Query(`PRAGMA table_info(outbox)`)
	if err != nil {
		t.Fatalf("inspect outbox: %v", err)
	}
	defer rows.Close()
	columns := map[string]bool{}
	for rows.Next() {
		var cid int
		var name, typ string
		var notNull, primaryKey int
		var defaultValue any
		if err := rows.Scan(&cid, &name, &typ, &notNull, &defaultValue, &primaryKey); err != nil {
			t.Fatalf("scan column: %v", err)
		}
		columns[name] = true
	}
	if !columns["kind"] || !columns["subject"] || columns["type"] {
		t.Fatalf("outbox columns = %v, want kind and subject but no type", columns)
	}
}
