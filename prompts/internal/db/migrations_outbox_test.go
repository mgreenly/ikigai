package db

import (
	"strings"
	"testing"
)

// TestOutboxKindSubjectMigrationPreservesTheImmutableBaseline ensures the
// forward-only upgrade maps legacy type rows to the eventplane kind/subject
// schema. The original 005 migration is immutable and deliberately remains the
// historical table definition.
func TestOutboxKindSubjectMigrationPreservesTheImmutableBaseline(t *testing.T) {
	body, err := migrationsFS.ReadFile("migrations/20260712191337_outbox_kind_subject.sql")
	if err != nil {
		t.Fatalf("read kind/subject migration: %v", err)
	}
	for _, want := range []string{
		"ALTER TABLE outbox RENAME TO outbox_old;",
		"kind       TEXT    NOT NULL",
		"subject    TEXT    NOT NULL DEFAULT ''",
		"SELECT seq, event_id, type, '', payload, created_at FROM outbox_old;",
	} {
		if !strings.Contains(string(body), want) {
			t.Fatalf("kind/subject migration missing %q", want)
		}
	}
}
