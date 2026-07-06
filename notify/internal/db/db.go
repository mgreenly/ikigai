// Package db holds notify's embedded migration set. The SQLite handle and the
// forward-only migration runner + downgrade guard are the uniform chassis half
// and live in appkit/db; this package keeps only what is app-side: the *.sql
// files (embedded for Spec.Migrations) and the byte-equality guard that
// 002_feed_offset.sql matches the library DDL (migrations_feed_offset_test.go).
package db

import (
	"embed"
)

//go:embed migrations/*.sql
var migrationsFS embed.FS

// FS exposes the embedded migration set so cmd/notify can hand it to
// appkit.Spec.Migrations (the binary is the source of truth for its own schema).
var FS = migrationsFS
