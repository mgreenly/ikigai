package db

import (
	"strings"
	"testing"

	"eventplane/outbox"
)

// TestOutboxMigrationMatchesLibraryDDL guards the producer invariant: the outbox
// table DDL is OWNED by the eventplane library (outbox.SchemaSQL); cron's
// newest outbox migration only applies it. If the two drift, every producer's
// outbox is no longer identical — so this test fails loudly the moment they
// diverge (mirrors crm/ledger).
func TestOutboxMigrationMatchesLibraryDDL(t *testing.T) {
	body, err := migrationsFS.ReadFile("migrations/20260712160651_outbox_routing.sql")
	if err != nil {
		t.Fatalf("read newest outbox migration: %v", err)
	}
	if !strings.Contains(string(body), outbox.SchemaSQL) {
		t.Fatalf("newest outbox migration does not contain the library DDL verbatim.\n--- outbox.SchemaSQL ---\n%s\n--- migration file ---\n%s",
			outbox.SchemaSQL, string(body))
	}
}
