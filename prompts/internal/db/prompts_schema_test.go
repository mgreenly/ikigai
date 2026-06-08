package db

import (
	"context"
	"testing"
)

// TestMigrate_CreatesPromptsSchema verifies 002_prompts.sql lands the prompts
// and runs tables (and the run index) on a fresh DB, idempotently.
func TestMigrate_CreatesPromptsSchema(t *testing.T) {
	ctx := context.Background()
	conn, err := Open(tempDB(t))
	if err != nil {
		t.Fatalf("open: %v", err)
	}
	defer conn.Close()

	// Migrate twice to confirm idempotency of the full set including 002.
	if err := Migrate(ctx, conn); err != nil {
		t.Fatalf("first migrate: %v", err)
	}
	if err := Migrate(ctx, conn); err != nil {
		t.Fatalf("second migrate: %v", err)
	}

	for _, tbl := range []string{"prompts", "runs", "prompt_triggers", "feed_offset"} {
		var name string
		err := conn.QueryRowContext(ctx,
			`SELECT name FROM sqlite_master WHERE type='table' AND name=?`, tbl,
		).Scan(&name)
		if err != nil {
			t.Fatalf("table %q missing after migrate: %v", tbl, err)
		}
	}

	// The run index must exist.
	var idx string
	err = conn.QueryRowContext(ctx,
		`SELECT name FROM sqlite_master WHERE type='index' AND name='idx_runs_prompt'`,
	).Scan(&idx)
	if err != nil {
		t.Fatalf("expected idx_runs_prompt: %v", err)
	}

	// Tombstone semantics (A3): there is NO FK/cascade. Deleting a prompt row
	// leaves its runs in place (they stay owner-addressable by run_id), and the
	// prompt's trigger(s) must be removed EXPLICITLY, not by cascade.
	if _, err := conn.ExecContext(ctx,
		`INSERT INTO prompts (id, owner_email, user_prompt, config_json, created_at, updated_at)
		 VALUES ('s1', 'o@example.com', 'hi', '{}', '2026-01-01T00:00:00Z', '2026-01-01T00:00:00Z')`,
	); err != nil {
		t.Fatalf("insert prompt: %v", err)
	}
	if _, err := conn.ExecContext(ctx,
		`INSERT INTO runs (id, prompt_id, owner_email, status, started_at, log_path)
		 VALUES ('r1', 's1', 'o@example.com', 'running', '2026-01-01T00:00:00Z', 'data/runs/s1/r1.jsonl')`,
	); err != nil {
		t.Fatalf("insert run: %v", err)
	}
	if _, err := conn.ExecContext(ctx,
		`INSERT INTO prompt_triggers (prompt_id, source, event_filter, created_at)
		 VALUES ('s1', 'cron', 'cron.nightly', '2026-01-01T00:00:00Z')`,
	); err != nil {
		t.Fatalf("insert prompt_trigger: %v", err)
	}
	if _, err := conn.ExecContext(ctx, `DELETE FROM prompts WHERE id='s1'`); err != nil {
		t.Fatalf("delete prompt: %v", err)
	}
	var n int
	if err := conn.QueryRowContext(ctx, `SELECT COUNT(*) FROM runs WHERE prompt_id='s1'`).Scan(&n); err != nil {
		t.Fatalf("count runs after tombstone: %v", err)
	}
	if n != 1 {
		t.Fatalf("expected NO cascade: run should survive prompt delete, got %d", n)
	}
	if err := conn.QueryRowContext(ctx, `SELECT COUNT(*) FROM prompt_triggers WHERE prompt_id='s1'`).Scan(&n); err != nil {
		t.Fatalf("count triggers after tombstone: %v", err)
	}
	if n != 1 {
		t.Fatalf("expected NO cascade: trigger should survive prompt delete (removed explicitly by service), got %d", n)
	}

	// The (source, event_filter) lookup index must exist.
	var tidx string
	if err := conn.QueryRowContext(ctx,
		`SELECT name FROM sqlite_master WHERE type='index' AND name='idx_prompt_triggers_lookup'`,
	).Scan(&tidx); err != nil {
		t.Fatalf("expected idx_prompt_triggers_lookup: %v", err)
	}
}
