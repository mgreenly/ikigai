package db

import (
	"strings"
	"testing"

	"eventplane/outbox"
)

// TestOutboxMigrationMatchesLibraryDDL guards the event-plane decision: the
// outbox table DDL is OWNED by the eventplane library (outbox.SchemaSQL);
// ledger's 003_outbox.sql migration only applies it. If the two drift, every
// producer's outbox is no longer identical — so this test fails loudly the
// moment they diverge.
func TestOutboxMigrationMatchesLibraryDDL(t *testing.T) {
	body, err := migrationsFS.ReadFile("migrations/003_outbox.sql")
	if err != nil {
		t.Fatalf("read 003_outbox.sql: %v", err)
	}
	if !strings.Contains(string(body), outbox.SchemaSQL) {
		t.Fatalf("003_outbox.sql does not contain the library DDL verbatim.\n--- outbox.SchemaSQL ---\n%s\n--- migration file ---\n%s",
			outbox.SchemaSQL, string(body))
	}
}
