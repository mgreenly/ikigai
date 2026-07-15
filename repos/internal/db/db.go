// Package db owns the repos service's embedded SQLite migrations.
package db

import (
	"embed"
	"fmt"
	"strings"

	appdb "appkit/db"
	"eventplane/outbox"
)

// migrationsFS is the service's complete, forward-only migration set.
//
//go:embed migrations/*.sql
var migrationsFS embed.FS

// FS is the complete migration filesystem handed to the appkit chassis.
var FS = migrationsFS

// Migrations loads the ordered embedded migration set and guards the copied
// eventplane outbox DDL against drift.
func Migrations() ([]appdb.Migration, error) {
	migrations, err := appdb.LoadMigrations(migrationsFS, "migrations")
	if err != nil {
		return nil, err
	}
	if len(migrations) == 0 {
		return nil, fmt.Errorf("repos db: no embedded migrations")
	}
	newest := migrations[len(migrations)-1]
	if !strings.Contains(newest.SQL, outbox.SchemaSQL) {
		return nil, fmt.Errorf("repos db: newest migration %s does not contain eventplane outbox schema verbatim", newest.Name)
	}
	return migrations, nil
}
