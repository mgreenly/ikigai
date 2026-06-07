// Package db holds sites's embedded migration set and a thin migrate helper for
// the domain tests. The SQLite handle and the forward-only migration runner +
// downgrade guard are the uniform chassis half and live in appkit/db — this
// package keeps only what is app-side: the *.sql files, embedded for
// Spec.Migrations (the binary is the source of truth for its own schema).
package db

import (
	"context"
	"database/sql"
	"embed"

	"appkit/db"

	_ "modernc.org/sqlite"
)

//go:embed migrations/*.sql
var migrationsFS embed.FS

// FS exposes the embedded migration set so cmd/sites can hand it to
// appkit.Spec.Migrations (the binary is the source of truth for its own schema).
var FS = migrationsFS

// Open opens sites's SQLite database with the chassis pragmas (delegates to
// appkit/db so there is one Open implementation across every service).
func Open(dbPath string) (*sql.DB, error) {
	return db.Open(dbPath)
}

// Migrate applies sites's embedded migrations against conn using appkit's
// forward-only runner + downgrade guard. The domain tests call this to stand up
// a schema; serve/migrate on the box go through appkit.Spec.Migrations directly.
func Migrate(ctx context.Context, conn *sql.DB) error {
	migs, err := db.LoadMigrations(migrationsFS, "migrations")
	if err != nil {
		return err
	}
	return db.Migrate(ctx, conn, migs)
}
