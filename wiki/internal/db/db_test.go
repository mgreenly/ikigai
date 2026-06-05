package db

import (
	"context"
	"path/filepath"
	"testing"

	appkitdb "appkit/db"
)

func tempDB(t *testing.T) string {
	t.Helper()
	return filepath.Join(t.TempDir(), "test.db")
}

func TestOpenAndMigrate(t *testing.T) {
	ctx := context.Background()
	conn, err := Open(tempDB(t))
	if err != nil {
		t.Fatalf("open: %v", err)
	}
	defer conn.Close()

	if err := Migrate(ctx, conn); err != nil {
		t.Fatalf("first migrate: %v", err)
	}

	var n int
	if err := conn.QueryRowContext(ctx, `SELECT COUNT(*) FROM schema_migrations`).Scan(&n); err != nil {
		t.Fatalf("count: %v", err)
	}
	if n < 1 {
		t.Fatalf("want >=1 applied migration after first run, got %d", n)
	}
}

func TestMigrate_IsIdempotent(t *testing.T) {
	ctx := context.Background()
	conn, err := Open(tempDB(t))
	if err != nil {
		t.Fatalf("open: %v", err)
	}
	defer conn.Close()

	if err := Migrate(ctx, conn); err != nil {
		t.Fatalf("first migrate: %v", err)
	}
	var before int
	if err := conn.QueryRowContext(ctx, `SELECT COUNT(*) FROM schema_migrations`).Scan(&before); err != nil {
		t.Fatalf("count before: %v", err)
	}
	if err := Migrate(ctx, conn); err != nil {
		t.Fatalf("second migrate: %v", err)
	}
	var after int
	if err := conn.QueryRowContext(ctx, `SELECT COUNT(*) FROM schema_migrations`).Scan(&after); err != nil {
		t.Fatalf("count after: %v", err)
	}
	if before != after {
		t.Fatalf("idempotent migrate changed count: %d -> %d", before, after)
	}
}

func TestMigrate_RefusesDowngrade(t *testing.T) {
	ctx := context.Background()
	conn, err := Open(tempDB(t))
	if err != nil {
		t.Fatalf("open: %v", err)
	}
	defer conn.Close()

	if err := Migrate(ctx, conn); err != nil {
		t.Fatalf("baseline migrate: %v", err)
	}
	// Simulate a future migration having run against this DB.
	if _, err := conn.ExecContext(ctx,
		`INSERT INTO schema_migrations (version, applied_at) VALUES (?, ?)`,
		999, "2099-01-01T00:00:00Z",
	); err != nil {
		t.Fatalf("inject future version: %v", err)
	}

	err = Migrate(ctx, conn)
	if err == nil {
		t.Fatal("expected downgrade refusal, got nil")
	}
}

func TestMigrate_AppliesWikiSchemaOnFreshDB(t *testing.T) {
	ctx := context.Background()
	conn, err := Open(tempDB(t))
	if err != nil {
		t.Fatalf("open: %v", err)
	}
	defer conn.Close()

	if err := Migrate(ctx, conn); err != nil {
		t.Fatalf("migrate: %v", err)
	}

	// Versions 1, 2 and 3 must all be recorded on a fresh DB (001→002→003; 003 is
	// the event-plane consumer offset migration, Task 6.1).
	for _, v := range []int{1, 2, 3} {
		var got int
		if err := conn.QueryRowContext(ctx,
			`SELECT version FROM schema_migrations WHERE version=?`, v,
		).Scan(&got); err != nil {
			t.Fatalf("schema_migrations missing version %d: %v", v, err)
		}
	}

	// The real 002 tables + the 003 feed_offset table must exist (placeholder
	// wiki_meta must be gone).
	for _, table := range []string{"wiki_ingest", "wiki_jobs", "feed_offset"} {
		var name string
		if err := conn.QueryRowContext(ctx,
			`SELECT name FROM sqlite_master WHERE type='table' AND name=?`, table,
		).Scan(&name); err != nil {
			t.Fatalf("expected table %q after migrate: %v", table, err)
		}
	}

	// The job-record columns must back agentkit/job.Record + owner + collection.
	wantCols := map[string]bool{
		"id": false, "flight_key": false, "status": false, "started_at": false,
		"ended_at": false, "usage_json": false, "error": false,
		"owner": false, "collection": false,
	}
	rows, err := conn.QueryContext(ctx, `PRAGMA table_info(wiki_jobs)`)
	if err != nil {
		t.Fatalf("pragma table_info: %v", err)
	}
	defer rows.Close()
	for rows.Next() {
		var cid int
		var name, ctype string
		var notnull, pk int
		var dflt any
		if err := rows.Scan(&cid, &name, &ctype, &notnull, &dflt, &pk); err != nil {
			t.Fatalf("scan table_info: %v", err)
		}
		if _, ok := wantCols[name]; ok {
			wantCols[name] = true
		}
	}
	for col, present := range wantCols {
		if !present {
			t.Errorf("wiki_jobs missing required column %q", col)
		}
	}

	// The single-flight gate: a running row blocks a second running row for the
	// same flight_key, but a terminal row does not.
	ins := `INSERT INTO wiki_jobs (id, flight_key, status, started_at, owner) VALUES (?,?,?,?,?)`
	if _, err := conn.ExecContext(ctx, ins, "j1", "k", "running", "t0", "alice"); err != nil {
		t.Fatalf("insert running job: %v", err)
	}
	if _, err := conn.ExecContext(ctx, ins, "j2", "k", "running", "t0", "alice"); err == nil {
		t.Fatal("expected single-flight rejection for second running job on same key")
	}
	if _, err := conn.ExecContext(ctx,
		`UPDATE wiki_jobs SET status='succeeded' WHERE id='j1'`); err != nil {
		t.Fatalf("terminalize j1: %v", err)
	}
	if _, err := conn.ExecContext(ctx, ins, "j3", "k", "running", "t0", "alice"); err != nil {
		t.Fatalf("a key should run again after its prior run is terminal: %v", err)
	}

	// wiki_ingest enforces idempotent provenance: same (owner, collection, sha256).
	insIng := `INSERT INTO wiki_ingest (id, owner, sha256, raw_path, ingested_at) VALUES (?,?,?,?,?)`
	if _, err := conn.ExecContext(ctx, insIng, "i1", "alice", "abc", "raw/abc.md", "t0"); err != nil {
		t.Fatalf("insert ingest: %v", err)
	}
	if _, err := conn.ExecContext(ctx, insIng, "i2", "alice", "abc", "raw/abc.md", "t0"); err == nil {
		t.Fatal("expected UNIQUE rejection for duplicate (owner, collection, sha256)")
	}
}

// TestLoadMigrations_Order guards that wiki's embedded migration set (now applied
// through appkit's shared forward-only runner) is well-formed: contiguous 1..N
// versions with no gaps. The runner mechanism itself is tested in appkit/db; this
// asserts only that wiki's *.sql, exposed via FS for Spec.Migrations, parse and
// order correctly.
func TestLoadMigrations_Order(t *testing.T) {
	migs, err := appkitdb.LoadMigrations(FS, "migrations")
	if err != nil {
		t.Fatalf("LoadMigrations: %v", err)
	}
	if len(migs) == 0 {
		t.Fatal("no migrations embedded")
	}
	for i, m := range migs {
		if m.Version != i+1 {
			t.Errorf("migration %d has version %d (gaps not yet allowed)", i, m.Version)
		}
	}
}
