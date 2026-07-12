package db

import (
	"context"
	"strings"
	"testing"

	appkitdb "appkit/db"

	"eventplane/outbox"
)

// TestOutboxRoutingMigrationMatchesLibraryDDL guards the newest routing
// migration against drift from eventplane's canonical outbox DDL. Historical
// migrations remain immutable records of their original deployments.
func TestOutboxRoutingMigrationMatchesLibraryDDL(t *testing.T) {
	// R-6VKR-5LCR
	body, err := migrationsFS.ReadFile("migrations/20260712194453_outbox_routing.sql")
	if err != nil {
		t.Fatalf("read outbox routing migration: %v", err)
	}
	if !strings.Contains(string(body), outbox.SchemaSQL) {
		t.Fatalf("routing migration does not contain outbox.SchemaSQL verbatim:\n%s", body)
	}

	conn, err := appkitdb.Open(tempDB(t))
	if err != nil {
		t.Fatalf("open: %v", err)
	}
	defer conn.Close()
	migs, err := appkitdb.LoadMigrations(FS, "migrations")
	if err != nil {
		t.Fatalf("load migrations: %v", err)
	}
	if err := appkitdb.Migrate(context.Background(), conn, migs); err != nil {
		t.Fatalf("migrate fresh database: %v", err)
	}
	rows, err := conn.Query(`PRAGMA table_info(outbox)`)
	if err != nil {
		t.Fatalf("outbox table_info: %v", err)
	}
	defer rows.Close()
	columns := map[string]bool{}
	for rows.Next() {
		var cid int
		var name, typ string
		var notNull int
		var defaultValue any
		var primaryKey int
		if err := rows.Scan(&cid, &name, &typ, &notNull, &defaultValue, &primaryKey); err != nil {
			t.Fatalf("scan table_info: %v", err)
		}
		columns[name] = true
	}
	if !columns["kind"] || !columns["subject"] || columns["type"] {
		t.Fatalf("outbox columns = %#v, want kind+subject and no type", columns)
	}
}
