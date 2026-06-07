package db

import (
	"strings"
	"testing"

	"eventplane/consumer"
)

// TestFeedOffsetMigrationMatchesLibraryDDL guards that prompts' 004_feed_offset.sql
// applies the eventplane library's feed_offset DDL (consumer.SchemaSQL) verbatim.
// prompts is a CONSUMER of cron's /feed; if the migration drifts from the library
// constant, prompts' offset store is no longer the schema the engine reads and
// writes. Mirrors notify's migrations_feed_offset_test.go.
func TestFeedOffsetMigrationMatchesLibraryDDL(t *testing.T) {
	body, err := migrationsFS.ReadFile("migrations/004_feed_offset.sql")
	if err != nil {
		t.Fatalf("read 004_feed_offset.sql: %v", err)
	}
	if !strings.Contains(string(body), consumer.SchemaSQL) {
		t.Fatalf("004_feed_offset.sql does not contain the library DDL verbatim.\n--- consumer.SchemaSQL ---\n%s\n--- migration file ---\n%s",
			consumer.SchemaSQL, string(body))
	}
}
