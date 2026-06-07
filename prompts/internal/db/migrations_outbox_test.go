package db

import (
	"strings"
	"testing"

	"eventplane/outbox"
)

// TestOutboxMigrationMatchesLibraryDDL guards the producer invariant: the outbox
// table DDL is OWNED by the eventplane library (outbox.SchemaSQL); prompts'
// 005_outbox.sql migration only applies it. If the two drift, prompts' outbox is
// no longer identical to every other producer's — so this test fails loudly the
// moment they diverge (mirrors crm/ledger/cron). prompts is the event-plane
// producer of the static run.succeeded / run.failed outcome types
// (event-triggering decisions §3).
func TestOutboxMigrationMatchesLibraryDDL(t *testing.T) {
	body, err := migrationsFS.ReadFile("migrations/005_outbox.sql")
	if err != nil {
		t.Fatalf("read 005_outbox.sql: %v", err)
	}
	if !strings.Contains(string(body), outbox.SchemaSQL) {
		t.Fatalf("005_outbox.sql does not contain the library DDL verbatim.\n--- outbox.SchemaSQL ---\n%s\n--- migration file ---\n%s",
			outbox.SchemaSQL, string(body))
	}
}
