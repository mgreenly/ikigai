// Package db holds gmail's embedded migration set for appkit.Spec.Migrations
// plus guard tests around loading and outbox DDL drift.
package db

import (
	"embed"
)

//go:embed migrations/*.sql
var migrationsFS embed.FS

// FS exposes the embedded migration set so cmd/gmail can hand it to
// appkit.Spec.Migrations (the binary is the source of truth for its own schema).
var FS = migrationsFS
