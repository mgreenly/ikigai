package db

import (
	"context"
	"database/sql"
	"strings"
	"testing"

	appkitdb "appkit/db"
	"eventplane/outbox"
	_ "modernc.org/sqlite"
)

// R-8JX3-TO9U
// TestOutboxMigrationMatchesLibraryDDL guards Decision 3: the outbox table DDL
// is OWNED by the eventplane library (outbox.SchemaSQL); crm's 003_outbox.sql
// migration only applies it. If the two drift, every producer's outbox is no
// longer identical — so this test fails loudly the moment they diverge.
func TestOutboxMigrationMatchesLibraryDDL(t *testing.T) {
	body, err := migrationsFS.ReadFile("migrations/20260712160534_outbox_routing.sql")
	if err != nil {
		t.Fatalf("read routing migration: %v", err)
	}
	if !strings.Contains(string(body), outbox.SchemaSQL) {
		t.Fatalf("routing migration does not contain the library DDL verbatim.\n--- outbox.SchemaSQL ---\n%s\n--- migration file ---\n%s",
			outbox.SchemaSQL, string(body))
	}
}

// R-8JX3-TO9U
func TestMigrationsCreateRoutedOutbox(t *testing.T) {
	conn, err := sql.Open("sqlite", ":memory:")
	if err != nil {
		t.Fatalf("open SQLite: %v", err)
	}
	defer conn.Close()
	migs, err := appkitdb.LoadMigrations(FS, "migrations")
	if err != nil {
		t.Fatalf("load migrations: %v", err)
	}
	if err := appkitdb.Migrate(context.Background(), conn, migs); err != nil {
		t.Fatalf("apply migrations: %v", err)
	}
	rows, err := conn.Query(`PRAGMA table_info(outbox)`)
	if err != nil {
		t.Fatalf("outbox columns: %v", err)
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
	if err := rows.Err(); err != nil {
		t.Fatalf("column rows: %v", err)
	}
	if !columns["kind"] || !columns["subject"] || columns["type"] {
		t.Fatalf("outbox columns = %v, want kind and subject without type", columns)
	}
}
