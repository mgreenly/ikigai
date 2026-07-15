package db

import (
	"context"
	"database/sql"
	"path/filepath"
	"reflect"
	"strings"
	"testing"

	appdb "appkit/db"
	"eventplane/outbox"
)

func TestEmbeddedMigrationsCreateExactDomainAndOutboxSchemas(t *testing.T) {
	// R-EMGN-7X72
	migrations, err := Migrations()
	if err != nil {
		t.Fatalf("load migrations and drift guard: %v", err)
	}
	if !strings.Contains(migrations[len(migrations)-1].SQL, outbox.SchemaSQL) {
		t.Fatal("newest migration does not contain outbox.SchemaSQL verbatim")
	}

	conn, err := appdb.Open(filepath.Join(t.TempDir(), "repos.db"))
	if err != nil {
		t.Fatalf("open temp database: %v", err)
	}
	t.Cleanup(func() { conn.Close() })
	if err := appdb.Migrate(context.Background(), conn, migrations); err != nil {
		t.Fatalf("apply full embedded migration set: %v", err)
	}

	want := map[string][]string{
		"repos":    {"name", "owner_email", "clone_url", "default_branch", "created_at"},
		"sessions": {"id", "repo_name", "owner_email", "issue_number", "attempt", "branch", "instructions", "status", "error", "pr_url", "created_at", "started_at", "ended_at", "log_path"},
		"outbox":   {"seq", "event_id", "kind", "subject", "payload", "created_at"},
	}
	for table, expected := range want {
		if got := tableColumns(t, conn, table); !reflect.DeepEqual(got, expected) {
			t.Errorf("%s columns = %v, want %v", table, got, expected)
		}
	}
	for _, column := range tableColumns(t, conn, "outbox") {
		if column == "type" {
			t.Fatal("outbox unexpectedly has legacy type column")
		}
	}
}

func tableColumns(t *testing.T, conn *sql.DB, table string) []string {
	t.Helper()
	rows, err := conn.Query(`SELECT name FROM pragma_table_info(?) ORDER BY cid`, table)
	if err != nil {
		t.Fatalf("query %s columns: %v", table, err)
	}
	defer rows.Close()
	var columns []string
	for rows.Next() {
		var name string
		if err := rows.Scan(&name); err != nil {
			t.Fatalf("scan %s column: %v", table, err)
		}
		columns = append(columns, name)
	}
	if err := rows.Err(); err != nil {
		t.Fatalf("read %s columns: %v", table, err)
	}
	return columns
}
