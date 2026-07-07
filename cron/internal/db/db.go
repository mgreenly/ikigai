// Package db holds cron's embedded migration set and app-side guard tests. The
// SQLite handle and forward-only migration runner + downgrade guard live in
// appkit/db; this package owns only the *.sql files embedded for Spec.Migrations.
package db

import "embed"

//go:embed migrations/*.sql
var migrationsFS embed.FS

// FS exposes the embedded migration set so cmd/cron can hand it to
// appkit.Spec.Migrations (the binary is the source of truth for its own schema).
var FS = migrationsFS
