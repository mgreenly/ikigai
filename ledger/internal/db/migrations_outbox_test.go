package db

import (
	"context"
	"strings"
	"testing"

	appkitdb "appkit/db"
	"eventplane/outbox"
)

func TestOutboxRoutingMigrationMatchesSchemaAndPreservesFrozenMigration(t *testing.T) {
	// R-G184-OOBO
	const routingMigration = "migrations/20260712184833_outbox_routing.sql"
	const frozen003 = `-- Event-plane outbox (event-protocol.md §4.5). The DDL is OWNED by the
-- eventplane library (outbox.SchemaSQL); this file must stay byte-identical to
-- that constant — internal/db/migrations_outbox_test.go asserts it. ledger's own
-- migration runner applies it so there is a single migration authority per DB
-- file, even though the schema's source of truth lives in the library.
CREATE TABLE outbox (
  seq        INTEGER PRIMARY KEY AUTOINCREMENT,
  event_id   TEXT    NOT NULL,
  type       TEXT    NOT NULL,
  payload    TEXT    NOT NULL,
  created_at TEXT    NOT NULL
);
CREATE INDEX idx_outbox_created_at ON outbox(created_at);
`
	routing, err := migrationsFS.ReadFile(routingMigration)
	if err != nil {
		t.Fatalf("read %s: %v", routingMigration, err)
	}
	if !strings.Contains(string(routing), outbox.SchemaSQL) {
		t.Fatalf("%s does not contain outbox.SchemaSQL verbatim:\n%s", routingMigration, routing)
	}

	body, err := migrationsFS.ReadFile("migrations/003_outbox.sql")
	if err != nil {
		t.Fatalf("read 003_outbox.sql: %v", err)
	}
	if string(body) != frozen003 {
		t.Fatalf("003_outbox.sql changed from its frozen committed body:\n%s", body)
	}
	for _, want := range []string{"event_id", "type", "payload", "created_at"} {
		if !strings.Contains(string(body), want) {
			t.Fatalf("003_outbox.sql missing frozen legacy column %q:\n%s", want, string(body))
		}
	}

	conn, err := appkitdb.Open(tempDB(t))
	if err != nil {
		t.Fatal(err)
	}
	defer conn.Close()
	if err := migrateLedger(context.Background(), conn); err != nil {
		t.Fatalf("migrate fresh database: %v", err)
	}
	rows, err := conn.Query(`PRAGMA table_info(outbox)`)
	if err != nil {
		t.Fatal(err)
	}
	defer rows.Close()
	columns := map[string]bool{}
	for rows.Next() {
		var cid, notnull, pk int
		var name, typ string
		var dflt any
		if err := rows.Scan(&cid, &name, &typ, &notnull, &dflt, &pk); err != nil {
			t.Fatal(err)
		}
		columns[name] = true
	}
	if !columns["kind"] || !columns["subject"] || columns["type"] {
		t.Errorf("outbox columns = %v, want kind+subject and no type", columns)
	}
}
