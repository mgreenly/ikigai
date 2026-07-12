package db

import (
	"context"
	"path/filepath"
	"strings"
	"testing"

	chassis "appkit/db"
	"eventplane/outbox"
)

// R-A5V4-ANGY — the newest outbox migration owns the revised kind/subject DDL,
// while the frozen bootstrap migration remains untouched.
func TestOutboxMigrationByteIdenticalToLibraryDDL(t *testing.T) {
	body, err := migrationsFS.ReadFile("migrations/20260712201504_outbox_routing.sql")
	if err != nil {
		t.Fatalf("read newest outbox routing migration: %v", err)
	}
	if !strings.Contains(string(body), outbox.SchemaSQL) {
		t.Fatalf("outbox routing migration does not contain outbox.SchemaSQL verbatim.\n--- outbox.SchemaSQL (%d bytes) ---\n%q\n--- migration file (%d bytes) ---\n%q",
			len(outbox.SchemaSQL), outbox.SchemaSQL, len(body), string(body))
	}
}

// R-A5V4-ANGY — the full embedded migration set replaces the disposable
// bootstrap outbox with the library's kind/subject schema without rewriting the
// frozen 003 migration.
func TestMigrationsCreateRoutedOutboxAndPreserveFrozenBootstrap(t *testing.T) {
	conn, err := chassis.Open(filepath.Join(t.TempDir(), "webhooks.db"))
	if err != nil {
		t.Fatalf("open temp sqlite: %v", err)
	}
	defer conn.Close()
	migs, err := chassis.LoadMigrations(FS, "migrations")
	if err != nil {
		t.Fatalf("load embedded migrations: %v", err)
	}
	if err := chassis.Migrate(context.Background(), conn, migs); err != nil {
		t.Fatalf("apply embedded migrations: %v", err)
	}
	columns := map[string]bool{}
	rows, err := conn.Query(`PRAGMA table_info(outbox)`)
	if err != nil {
		t.Fatalf("table_info(outbox): %v", err)
	}
	defer rows.Close()
	for rows.Next() {
		var cid int
		var name, typ string
		var notNull, pk int
		var defaultValue any
		if err := rows.Scan(&cid, &name, &typ, &notNull, &defaultValue, &pk); err != nil {
			t.Fatalf("scan outbox column: %v", err)
		}
		columns[name] = true
	}
	if !columns["kind"] || !columns["subject"] || columns["type"] {
		t.Fatalf("outbox columns = %v, want kind/subject and no type", columns)
	}
	const frozen003 = "CREATE TABLE outbox (\n  seq        INTEGER PRIMARY KEY AUTOINCREMENT,\n  event_id   TEXT    NOT NULL,\n  type       TEXT    NOT NULL,\n  payload    TEXT    NOT NULL,\n  created_at TEXT    NOT NULL\n);\nCREATE INDEX idx_outbox_created_at ON outbox(created_at);\n"
	frozen, err := migrationsFS.ReadFile("migrations/003_outbox.sql")
	if err != nil {
		t.Fatalf("read frozen 003 migration: %v", err)
	}
	if string(frozen) != frozen003 {
		t.Fatal("003_outbox.sql changed; committed bootstrap migrations are immutable")
	}
}
