package db

import (
	"slices"
	"strings"
	"testing"

	"eventplane/consumer"
)

// TestFeedOffsetMigrationMatchesLibraryDDL guards decision 3: the feed_offset
// table DDL is OWNED by the eventplane library (consumer.SchemaSQL); notify's
// 002_feed_offset.sql migration only applies it. If the two drift, notify's
// offset store is no longer the schema the engine reads and writes — so this test
// fails loudly the moment they diverge. It mirrors crm's
// migrations_outbox_test.go on the producer side.
// R-ZHCF-YJ8D
func TestFeedOffsetMigrationMatchesLibraryDDL(t *testing.T) {
	entries, err := migrationsFS.ReadDir("migrations")
	if err != nil {
		t.Fatalf("read embedded migrations: %v", err)
	}
	var names []string
	for _, entry := range entries {
		names = append(names, entry.Name())
	}
	if !slices.Equal(names, []string{"001_schema_migrations.sql", "002_feed_offset.sql"}) {
		t.Fatalf("embedded migrations = %v, want exactly schema_migrations and feed_offset", names)
	}
	body, err := migrationsFS.ReadFile("migrations/002_feed_offset.sql")
	if err != nil {
		t.Fatalf("read 002_feed_offset.sql: %v", err)
	}
	if !strings.Contains(string(body), consumer.SchemaSQL) {
		t.Fatalf("002_feed_offset.sql does not contain the library DDL verbatim.\n--- consumer.SchemaSQL ---\n%s\n--- migration file ---\n%s",
			consumer.SchemaSQL, string(body))
	}
}
