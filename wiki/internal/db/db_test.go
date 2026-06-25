package db

import (
	"context"
	"strings"
	"testing"

	appdb "appkit/db"
)

func TestEmbeddedMigrationsApplyToTempSQLite(t *testing.T) {
	ctx := context.Background()
	conn, err := Open(t.TempDir() + "/wiki.db")
	if err != nil {
		t.Fatalf("Open: %v", err)
	}
	defer conn.Close()

	migs, err := appdb.LoadMigrations(FS, "migrations")
	if err != nil {
		t.Fatalf("LoadMigrations: %v", err)
	}
	if len(migs) == 0 {
		t.Fatal("len(migs) = 0, want at least one embedded migration")
	}

	if err := appdb.Migrate(ctx, conn, migs); err != nil {
		t.Fatalf("Migrate: %v", err)
	}
	got, err := appdb.AppliedVersion(ctx, conn)
	if err != nil {
		t.Fatalf("AppliedVersion: %v", err)
	}
	if want := appdb.MaxEmbedded(migs); got != want {
		t.Fatalf("AppliedVersion = %d, want %d", got, want)
	}
	for _, table := range []string{"wiki_ingest", "wiki_jobs", "feed_offset"} {
		var name string
		err := conn.QueryRowContext(ctx,
			`SELECT name FROM sqlite_master WHERE type = 'table' AND name = ?`, table,
		).Scan(&name)
		if err != nil {
			t.Fatalf("table %s was not created: %v", table, err)
		}
	}
}

func TestPhaseTwoDataModelSchema(t *testing.T) {
	// R-7SNG-0G9A
	ctx := context.Background()
	conn, err := Open(t.TempDir() + "/wiki.db")
	if err != nil {
		t.Fatalf("Open: %v", err)
	}
	defer conn.Close()

	if err := Migrate(ctx, conn); err != nil {
		t.Fatalf("Migrate: %v", err)
	}

	for _, table := range []string{"jobs", "subjects", "claims", "pages"} {
		var name string
		err := conn.QueryRowContext(ctx,
			`SELECT name FROM sqlite_master WHERE name = ?`, table,
		).Scan(&name)
		if err != nil {
			t.Fatalf("schema object %s was not created: %v", table, err)
		}
	}
	for _, index := range []string{"jobs_status", "claims_subject", "claims_job"} {
		var name string
		err := conn.QueryRowContext(ctx,
			`SELECT name FROM sqlite_master WHERE type = 'index' AND name = ?`, index,
		).Scan(&name)
		if err != nil {
			t.Fatalf("index %s was not created: %v", index, err)
		}
	}
	for _, table := range []string{"jobs", "subjects", "claims", "pages"} {
		rows, err := conn.QueryContext(ctx, `PRAGMA foreign_key_list(`+table+`)`)
		if err != nil {
			t.Fatalf("foreign_key_list(%s): %v", table, err)
		}
		if rows.Next() {
			rows.Close()
			t.Fatalf("table %s declares a foreign key, want comments-only relationships", table)
		}
		if err := rows.Close(); err != nil {
			t.Fatalf("close foreign_key_list(%s): %v", table, err)
		}
	}

	if _, err := conn.ExecContext(ctx,
		`INSERT INTO subjects (id, name, norm_name, type) VALUES ('s1', 'Alpha', 'alpha', 'entity')`); err != nil {
		t.Fatalf("insert valid subject: %v", err)
	}
	if _, err := conn.ExecContext(ctx,
		`INSERT INTO subjects (id, name, norm_name, type) VALUES ('s2', 'Alpha 2', 'alpha', 'entity')`); err == nil {
		t.Fatal("duplicate subjects.norm_name insert succeeded, want UNIQUE failure")
	}
	if _, err := conn.ExecContext(ctx,
		`INSERT INTO subjects (id, name, norm_name, type) VALUES ('s3', 'Bad', 'bad', 'person')`); err == nil {
		t.Fatal("invalid subject type insert succeeded, want CHECK failure")
	}
	if _, err := conn.ExecContext(ctx,
		`INSERT INTO pages (id, subject_id, title, body) VALUES ('p1', 's1', 'Too Long', ?)`,
		strings.Repeat("x", 12001)); err == nil {
		t.Fatal("oversized page body insert succeeded, want CHECK failure")
	}
}

func TestMigrationsCreatePagesFTSExternalContentAndBackfill(t *testing.T) {
	// R-203P-F1ET
	ctx := context.Background()
	conn, err := Open(t.TempDir() + "/wiki.db")
	if err != nil {
		t.Fatalf("Open: %v", err)
	}
	defer conn.Close()

	migs, err := appdb.LoadMigrations(FS, "migrations")
	if err != nil {
		t.Fatalf("LoadMigrations: %v", err)
	}
	var createVersion int
	var createSQL string
	for _, mig := range migs {
		if strings.Contains(mig.Name, "create_pages_fts") {
			createVersion = mig.Version
			createSQL = mig.SQL
			break
		}
	}
	if createVersion == 0 {
		t.Fatal("create_pages_fts migration not found in embedded migrations")
	}
	if !strings.Contains(createSQL, "content='pages'") || !strings.Contains(createSQL, "content_rowid='rowid'") {
		t.Fatalf("create_pages_fts migration SQL = %q, want external-content pages FTS", createSQL)
	}
	if !strings.Contains(createSQL, "INSERT INTO pages_fts(pages_fts) VALUES('rebuild')") {
		t.Fatalf("create_pages_fts migration SQL = %q, want rebuild backfill", createSQL)
	}

	var beforeCreate []appdb.Migration
	for _, mig := range migs {
		if mig.Version < createVersion {
			beforeCreate = append(beforeCreate, mig)
		}
	}
	if err := appdb.Migrate(ctx, conn, beforeCreate); err != nil {
		t.Fatalf("Migrate before create_pages_fts: %v", err)
	}
	if _, err := conn.ExecContext(ctx,
		`INSERT INTO subjects (id, name, norm_name, type) VALUES ('subject-1', 'Acme Robotics', 'acme-robotics', 'entity')`); err != nil {
		t.Fatalf("insert subject before create_pages_fts: %v", err)
	}
	if _, err := conn.ExecContext(ctx,
		`INSERT INTO pages (id, subject_id, title, body) VALUES ('page-1', 'subject-1', 'Acme Robotics', 'Tulsa launch notes')`); err != nil {
		t.Fatalf("insert page before create_pages_fts: %v", err)
	}

	if err := appdb.Migrate(ctx, conn, migs); err != nil {
		t.Fatalf("Migrate through create_pages_fts: %v", err)
	}

	var tableSQL string
	if err := conn.QueryRowContext(ctx,
		`SELECT sql FROM sqlite_master WHERE type = 'table' AND name = 'pages_fts'`).
		Scan(&tableSQL); err != nil {
		t.Fatalf("read pages_fts schema: %v", err)
	}
	if !strings.Contains(tableSQL, "content='pages'") || !strings.Contains(tableSQL, "content_rowid='rowid'") {
		t.Fatalf("pages_fts schema = %q, want external-content table over pages(rowid)", tableSQL)
	}

	var matches int
	if err := conn.QueryRowContext(ctx,
		`SELECT COUNT(*) FROM pages_fts WHERE pages_fts MATCH '"Tulsa"'`).
		Scan(&matches); err != nil {
		t.Fatalf("query rebuilt pages_fts: %v", err)
	}
	if matches != 1 {
		t.Fatalf("rebuilt pages_fts matches = %d, want 1", matches)
	}
}
