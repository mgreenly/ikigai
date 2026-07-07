// Package db holds scripts' embedded migration set. Opening SQLite handles and
// running migrations are appkit-owned concerns wired through Spec.Migrations.
package db

import "embed"

//go:embed migrations/*.sql
var migrationsFS embed.FS

// FS exposes the embedded migration set so cmd/scripts can hand it to
// appkit.Spec.Migrations (the binary is the source of truth for its own schema).
var FS = migrationsFS
