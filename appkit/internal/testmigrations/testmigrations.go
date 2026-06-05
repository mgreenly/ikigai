// Package testmigrations exposes a small embedded migration set (001 chassis +
// 002 widgets) shared by appkit's unit tests, so a test can exercise the real
// embed.FS-backed migration runner and dispatcher without a service present.
package testmigrations

import "embed"

//go:embed migrations/*.sql
var FS embed.FS
