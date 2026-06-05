// Package db owns the SQLite handle and the forward-only migration runner with
// its downgrade guard — the uniform half lifted from every service's
// internal/db (appkit extraction, PLAN §B).
//
// The runner is parameterised on a caller-supplied migrations fs.FS (the
// service embeds its own migrations/*.sql via Spec.Migrations) rather than an
// embed.FS baked into this package, so one runner serves every app while each
// app keeps its own migration set app-side (PLAN §B1 map: appkit/db owns the
// mechanism, the *.sql stay app-side).
package db

import (
	"context"
	"database/sql"
	"fmt"
	"io/fs"
	"path"
	"sort"
	"strconv"
	"strings"
	"time"

	_ "modernc.org/sqlite"
)

// Open opens the SQLite database at dbPath with the pragmas the spec requires.
// A single connection enforces single-writer discipline for the embedded
// SQLite store.
func Open(dbPath string) (*sql.DB, error) {
	dsn := fmt.Sprintf(
		"file:%s?_pragma=journal_mode(WAL)&_pragma=foreign_keys(ON)&_pragma=busy_timeout(5000)",
		dbPath,
	)
	conn, err := sql.Open("sqlite", dsn)
	if err != nil {
		return nil, fmt.Errorf("open sqlite %s: %w", dbPath, err)
	}
	conn.SetMaxOpenConns(1)
	if err := conn.Ping(); err != nil {
		conn.Close()
		return nil, fmt.Errorf("ping sqlite %s: %w", dbPath, err)
	}
	return conn, nil
}

// Migration is one parsed, numbered migration. Exported so callers (and the
// install preflight) can inspect the embedded set.
type Migration struct {
	Version int
	Name    string
	SQL     string
}

// LoadMigrations reads and orders the migrations under dir in fsys (the
// service's embedded migrations/*.sql). It enforces the NNN_name.sql naming and
// rejects duplicate version numbers.
func LoadMigrations(fsys fs.FS, dir string) ([]Migration, error) {
	entries, err := fs.ReadDir(fsys, dir)
	if err != nil {
		return nil, fmt.Errorf("read migrations dir: %w", err)
	}
	var out []Migration
	for _, e := range entries {
		if e.IsDir() || !strings.HasSuffix(e.Name(), ".sql") {
			continue
		}
		under := strings.IndexByte(e.Name(), '_')
		if under <= 0 {
			return nil, fmt.Errorf("migration %q: expected NNN_name.sql", e.Name())
		}
		v, err := strconv.Atoi(e.Name()[:under])
		if err != nil {
			return nil, fmt.Errorf("migration %q: parse version: %w", e.Name(), err)
		}
		body, err := fs.ReadFile(fsys, path.Join(dir, e.Name()))
		if err != nil {
			return nil, fmt.Errorf("read migration %q: %w", e.Name(), err)
		}
		out = append(out, Migration{Version: v, Name: e.Name(), SQL: string(body)})
	}
	sort.Slice(out, func(i, j int) bool { return out[i].Version < out[j].Version })
	for i := 1; i < len(out); i++ {
		if out[i].Version == out[i-1].Version {
			return nil, fmt.Errorf("migration version %d duplicated (%s and %s)", out[i].Version, out[i-1].Name, out[i].Name)
		}
	}
	return out, nil
}

// Migrate applies every migration in migs whose version is not yet recorded in
// schema_migrations. Migration 001 is expected to establish the
// schema_migrations table itself.
func Migrate(ctx context.Context, conn *sql.DB, migs []Migration) error {
	if len(migs) == 0 {
		return nil
	}
	applied := map[int]bool{}
	if exists, err := tableExists(ctx, conn, "schema_migrations"); err != nil {
		return err
	} else if exists {
		rows, err := conn.QueryContext(ctx, `SELECT version FROM schema_migrations`)
		if err != nil {
			return fmt.Errorf("select schema_migrations: %w", err)
		}
		for rows.Next() {
			var v int
			if err := rows.Scan(&v); err != nil {
				rows.Close()
				return fmt.Errorf("scan schema_migrations: %w", err)
			}
			applied[v] = true
		}
		rows.Close()
		// Downgrade guard (PLAN §2.5): refuse to start on a DB carrying an applied
		// version this binary no longer embeds. The forward-only runner cannot
		// safely run against schema it does not know.
		embedded := map[int]bool{}
		for _, m := range migs {
			embedded[m.Version] = true
		}
		for v := range applied {
			if !embedded[v] {
				return fmt.Errorf("database has migration version %d that is not embedded in this binary; refusing to downgrade", v)
			}
		}
	}

	for _, m := range migs {
		if applied[m.Version] {
			continue
		}
		if err := applyOne(ctx, conn, m); err != nil {
			return err
		}
	}
	return nil
}

// AppliedVersion returns the highest version recorded in schema_migrations, or 0
// if the table is absent or empty. optctl install reads this against
// MaxEmbedded(migs) to decide whether the schema advances (→ backup first).
func AppliedVersion(ctx context.Context, conn *sql.DB) (int, error) {
	exists, err := tableExists(ctx, conn, "schema_migrations")
	if err != nil {
		return 0, err
	}
	if !exists {
		return 0, nil
	}
	var v sql.NullInt64
	if err := conn.QueryRowContext(ctx, `SELECT MAX(version) FROM schema_migrations`).Scan(&v); err != nil {
		return 0, fmt.Errorf("max schema_migrations: %w", err)
	}
	if !v.Valid {
		return 0, nil
	}
	return int(v.Int64), nil
}

// MaxEmbedded returns the highest version present in migs (0 for an empty set).
func MaxEmbedded(migs []Migration) int {
	max := 0
	for _, m := range migs {
		if m.Version > max {
			max = m.Version
		}
	}
	return max
}

func applyOne(ctx context.Context, conn *sql.DB, m Migration) error {
	tx, err := conn.BeginTx(ctx, nil)
	if err != nil {
		return fmt.Errorf("begin tx for migration %d: %w", m.Version, err)
	}
	defer tx.Rollback()
	if _, err := tx.ExecContext(ctx, m.SQL); err != nil {
		return fmt.Errorf("apply migration %s: %w", m.Name, err)
	}
	if _, err := tx.ExecContext(ctx,
		`INSERT INTO schema_migrations (version, applied_at) VALUES (?, ?)`,
		m.Version, time.Now().UTC().Format(time.RFC3339Nano),
	); err != nil {
		return fmt.Errorf("record migration %d: %w", m.Version, err)
	}
	if err := tx.Commit(); err != nil {
		return fmt.Errorf("commit migration %d: %w", m.Version, err)
	}
	return nil
}

func tableExists(ctx context.Context, conn *sql.DB, name string) (bool, error) {
	row := conn.QueryRowContext(ctx,
		`SELECT name FROM sqlite_master WHERE type='table' AND name=?`, name,
	)
	var got string
	switch err := row.Scan(&got); err {
	case nil:
		return true, nil
	case sql.ErrNoRows:
		return false, nil
	default:
		return false, err
	}
}
