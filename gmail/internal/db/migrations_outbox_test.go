package db

import (
	"context"
	"strings"
	"testing"

	appkitdb "appkit/db"
	"eventplane/outbox"
)

// TestOutboxMigrationsMatchLibrarySchema guards the forward migration from the
// frozen legacy outbox DDL to the eventplane library's current kind/subject
// contract.
func TestOutboxMigrationsMatchLibrarySchema(t *testing.T) {
	// R-X9ED-THOL
	legacy, err := migrationsFS.ReadFile("migrations/003_outbox.sql")
	if err != nil {
		t.Fatalf("read 003_outbox.sql: %v", err)
	}
	if !strings.Contains(string(legacy), "type       TEXT    NOT NULL") {
		t.Fatal("003_outbox.sql must retain the frozen legacy type column")
	}
	body, err := migrationsFS.ReadFile("migrations/20260712190007_outbox_routing.sql")
	if err != nil {
		t.Fatalf("read newest outbox migration: %v", err)
	}
	if !strings.Contains(string(body), outbox.SchemaSQL) {
		t.Fatal("newest outbox migration must contain outbox.SchemaSQL verbatim")
	}

	conn, err := appkitdb.Open(":memory:")
	if err != nil {
		t.Fatalf("open database: %v", err)
	}
	t.Cleanup(func() { conn.Close() })
	migs, err := appkitdb.LoadMigrations(FS, "migrations")
	if err != nil {
		t.Fatalf("load migrations: %v", err)
	}
	if err := appkitdb.Migrate(context.Background(), conn, migs); err != nil {
		t.Fatalf("migrate: %v", err)
	}
	columns := map[string]bool{}
	rows, err := conn.Query(`PRAGMA table_info(outbox)`)
	if err != nil {
		t.Fatalf("inspect outbox columns: %v", err)
	}
	defer rows.Close()
	for rows.Next() {
		var cid int
		var name, typ string
		var notNull, primaryKey int
		var defaultValue any
		if err := rows.Scan(&cid, &name, &typ, &notNull, &defaultValue, &primaryKey); err != nil {
			t.Fatalf("scan outbox column: %v", err)
		}
		columns[name] = true
	}
	if err := rows.Err(); err != nil {
		t.Fatalf("iterate outbox columns: %v", err)
	}
	for _, name := range []string{"seq", "event_id", "kind", "subject", "payload", "created_at"} {
		if !columns[name] || !strings.Contains(outbox.SchemaSQL, name) {
			t.Fatalf("migrated outbox and library schema must both contain %q: columns=%v", name, columns)
		}
	}
	if columns["type"] {
		t.Fatalf("legacy type column remains after migration: columns=%v", columns)
	}
	var kind, subject string
	if err := conn.QueryRow(`INSERT INTO outbox (event_id, kind, subject, payload, created_at)
		VALUES ('event', 'received', '', '{}', 'now')
		RETURNING kind, subject`).Scan(&kind, &subject); err != nil {
		t.Fatalf("insert current outbox shape: %v", err)
	}
	if kind != "received" || subject != "" {
		t.Fatalf("routed row = (%q, %q), want (received, empty subject)", kind, subject)
	}
}
