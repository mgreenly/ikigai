package db

import (
	"strings"
	"testing"
)

// TestOutboxMigrationRemainsFrozen guards the already-applied legacy outbox
// shape. Eventplane now writes kind/subject tables, while ledger's immutable
// 003 migration has type-only rows; the producer carries the compatibility
// branch rather than rewriting history.
func TestOutboxMigrationRemainsFrozen(t *testing.T) {
	body, err := migrationsFS.ReadFile("migrations/003_outbox.sql")
	if err != nil {
		t.Fatalf("read 003_outbox.sql: %v", err)
	}
	for _, want := range []string{"event_id", "type", "payload", "created_at"} {
		if !strings.Contains(string(body), want) {
			t.Fatalf("003_outbox.sql missing frozen legacy column %q:\n%s", want, string(body))
		}
	}
}
