// Package crontab is cron's domain layer: the persistent crontab of named
// 5-field schedules and the CRUD store over it. `name` is the identity (and the
// suffix of the emitted cron.<name> event); the DB CHECK on the crontab table is
// the validation boundary for the charset (decisions §2). The MCP surface and
// the tick worker that consume this store land in P5; this phase provides the
// store and the matcher only.
package crontab

import (
	"context"
	"database/sql"
	"errors"
	"fmt"
	"strings"
	"time"
)

// timeFormat is the canonical stored timestamp shape (UTC RFC3339). last_slot is
// stored minute-truncated; created_at/updated_at are full-precision.
const timeFormat = time.RFC3339

// Sentinel errors. Callers (the P5 MCP layer) match these with errors.Is to
// render the right wire envelope; the store never formats user-facing messages.
var (
	// ErrNotFound is returned by Get/Update/Delete when no row has the name.
	ErrNotFound = errors.New("crontab entry not found")
	// ErrExists is returned by Create when a row with the name already exists.
	ErrExists = errors.New("crontab entry already exists")
	// ErrInvalid is returned when a write violates the table's CHECK constraints
	// (the name charset / non-empty expr boundary). The parser is the authority
	// for expr validity at the MCP layer; this guards a malformed direct write.
	ErrInvalid = errors.New("crontab entry violates a constraint")
)

// Entry is one crontab row. last_slot is nil until the tick worker first emits
// for the schedule (P5).
type Entry struct {
	Name      string
	Expr      string
	CreatedAt time.Time
	UpdatedAt time.Time
	LastSlot  *time.Time
}

// Store is a thin CRUD layer over the crontab table. It holds the shared
// single-writer *sql.DB handle (appkit's). Each method runs one statement; the
// atomic emit+last_slot transaction is the tick worker's concern (P5).
type Store struct {
	db *sql.DB
}

// NewStore builds a Store over the chassis DB handle.
func NewStore(db *sql.DB) *Store {
	return &Store{db: db}
}

// Create inserts a new schedule. now is the create/update timestamp (UTC). A
// name collision is ErrExists; a CHECK violation is ErrInvalid.
func (s *Store) Create(ctx context.Context, name, expr string, now time.Time) (*Entry, error) {
	ts := fmtTime(now)
	_, err := s.db.ExecContext(ctx, `
		INSERT INTO crontab (name, expr, created_at, updated_at, last_slot)
		VALUES (?, ?, ?, ?, NULL)`, name, expr, ts, ts)
	if err != nil {
		return nil, mapWriteErr(err, name)
	}
	return s.Get(ctx, name)
}

// Get returns the schedule by name, or ErrNotFound.
func (s *Store) Get(ctx context.Context, name string) (*Entry, error) {
	row := s.db.QueryRowContext(ctx, `
		SELECT name, expr, created_at, updated_at, last_slot
		FROM crontab WHERE name = ?`, name)
	return scanEntry(row)
}

// List returns every schedule, ordered by name.
func (s *Store) List(ctx context.Context) ([]Entry, error) {
	rows, err := s.db.QueryContext(ctx, `
		SELECT name, expr, created_at, updated_at, last_slot
		FROM crontab ORDER BY name ASC`)
	if err != nil {
		return nil, fmt.Errorf("list crontab: %w", err)
	}
	defer rows.Close()
	out := []Entry{}
	for rows.Next() {
		e, err := scanEntry(rows)
		if err != nil {
			return nil, err
		}
		out = append(out, *e)
	}
	return out, rows.Err()
}

// Update changes a schedule's expr (bumping updated_at). last_slot is deliberately
// untouched — an expr change does not clear the double-emit guard (decisions §2).
// An unknown name is ErrNotFound; a CHECK violation is ErrInvalid.
func (s *Store) Update(ctx context.Context, name, expr string, now time.Time) (*Entry, error) {
	res, err := s.db.ExecContext(ctx, `
		UPDATE crontab SET expr = ?, updated_at = ? WHERE name = ?`,
		expr, fmtTime(now), name)
	if err != nil {
		return nil, mapWriteErr(err, name)
	}
	n, err := res.RowsAffected()
	if err != nil {
		return nil, fmt.Errorf("update crontab %q: %w", name, err)
	}
	if n == 0 {
		return nil, fmt.Errorf("%w: %s", ErrNotFound, name)
	}
	return s.Get(ctx, name)
}

// Delete removes a schedule by name. An unknown name is ErrNotFound.
func (s *Store) Delete(ctx context.Context, name string) error {
	res, err := s.db.ExecContext(ctx, `DELETE FROM crontab WHERE name = ?`, name)
	if err != nil {
		return fmt.Errorf("delete crontab %q: %w", name, err)
	}
	n, err := res.RowsAffected()
	if err != nil {
		return fmt.Errorf("delete crontab %q: %w", name, err)
	}
	if n == 0 {
		return fmt.Errorf("%w: %s", ErrNotFound, name)
	}
	return nil
}

// ── helpers ──────────────────────────────────────────────────────────────────

// rowScanner is satisfied by both *sql.Row and *sql.Rows.
type rowScanner interface {
	Scan(dest ...any) error
}

func scanEntry(sc rowScanner) (*Entry, error) {
	var e Entry
	var created, updated string
	var lastSlot sql.NullString
	switch err := sc.Scan(&e.Name, &e.Expr, &created, &updated, &lastSlot); err {
	case nil:
	case sql.ErrNoRows:
		return nil, ErrNotFound
	default:
		return nil, fmt.Errorf("scan crontab row: %w", err)
	}
	e.CreatedAt = parseTime(created)
	e.UpdatedAt = parseTime(updated)
	if lastSlot.Valid {
		t := parseTime(lastSlot.String)
		e.LastSlot = &t
	}
	return &e, nil
}

func fmtTime(t time.Time) string { return t.UTC().Format(timeFormat) }

// parseTime parses a stored timestamp; a malformed value yields the zero time
// rather than an error (storage is always written by fmtTime, so this is
// defensive).
func parseTime(s string) time.Time {
	t, _ := time.Parse(timeFormat, s)
	return t.UTC()
}

// mapWriteErr translates a SQLite constraint violation into the right sentinel: a
// PRIMARY KEY collision is ErrExists, a CHECK failure is ErrInvalid; other errors
// are wrapped with context.
func mapWriteErr(err error, name string) error {
	if err == nil {
		return nil
	}
	msg := err.Error()
	switch {
	case strings.Contains(msg, "UNIQUE constraint failed"),
		strings.Contains(msg, "PRIMARY KEY"):
		return fmt.Errorf("%w: %s", ErrExists, name)
	case strings.Contains(msg, "CHECK constraint failed"):
		return fmt.Errorf("%w: %s", ErrInvalid, name)
	default:
		return fmt.Errorf("write crontab %q: %w", name, err)
	}
}
