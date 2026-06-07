// Package sites is the slug/registry domain: CRUD over the `sites` table plus
// the path helpers (layout.go) that pin where each site lives under SITES_ROOT.
// Each row is one hosted static website keyed by its slug `name`. This package
// does no filesystem mutation beyond what plain CRUD needs — the symlink/publish
// machinery (the served-tier swap) is a later phase and lives elsewhere.
package sites

import (
	"context"
	"database/sql"
	"errors"
	"fmt"
	"regexp"
	"strings"
	"time"
)

// timeFormat is the canonical storage timestamp rendering (RFC3339-ish, UTC,
// nanosecond precision) — matches the suite's peer services so stored values
// round-trip identically across the box.
const timeFormat = "2006-01-02T15:04:05.000000000Z07:00"

// slugRe is the slug grammar pinned by migration 002_sites.sql: a leading
// lowercase-alnum char then up to 62 more lowercase-alnum-or-hyphen chars
// (1..63 total). Compiled once at package load.
var slugRe = regexp.MustCompile(`^[a-z0-9][a-z0-9-]{0,62}$`)

// reservedNames are slugs the registry refuses defensively: they collide with
// reserved routes/dirs in the served tree. `.well-known` wouldn't match slugRe
// anyway (the dot), but we guard it explicitly so the intent is enforced and not
// merely incidental.
var reservedNames = map[string]bool{
	"mcp":         true,
	".well-known": true,
}

// Error sentinels. Callers match with errors.Is.
var (
	// ErrInvalidSlug — name fails the slug grammar (bad chars, too long, empty).
	ErrInvalidSlug = errors.New("sites: invalid slug")
	// ErrReservedName — name is syntactically valid but reserved.
	ErrReservedName = errors.New("sites: reserved name")
	// ErrExists — a row with that name already exists.
	ErrExists = errors.New("sites: already exists")
	// ErrNotFound — no row with that name.
	ErrNotFound = errors.New("sites: not found")
)

// Site mirrors one `sites` row. PublishedAt is nil until first published.
type Site struct {
	Name        string
	Tier        string
	Published   bool
	PublishedAt *time.Time
	CreatedAt   time.Time
	UpdatedAt   time.Time
}

// Store is the registry's data-access boundary over the `sites` table. Now is
// injected so tests can pin time; it defaults to time.Now. Layout pins the
// SITES_ROOT the publish phase (publish.go) symlinks under — a zero Layout falls
// back to DefaultRoot, so the plain CRUD path (Phase 2) needs no layout wiring.
type Store struct {
	db     *sql.DB
	Layout Layout
	Now    func() time.Time
}

// NewStore wraps an open *sql.DB with a zero (DefaultRoot) Layout. Now defaults
// to time.Now (UTC is applied at write time).
func NewStore(db *sql.DB) *Store {
	return &Store{db: db, Now: time.Now}
}

// NewStoreWithLayout wraps an open *sql.DB and pins the Layout used by the
// publish/unpublish symlink machinery. Use this at process wiring (cmd/sites)
// once the SITES_ROOT-derived Layout is available.
func NewStoreWithLayout(db *sql.DB, layout Layout) *Store {
	return &Store{db: db, Layout: layout, Now: time.Now}
}

// validateName runs the slug grammar then the reserved-name guard. It returns
// ErrInvalidSlug or ErrReservedName (wrapped with the offending value).
func validateName(name string) error {
	if reservedNames[name] {
		return fmt.Errorf("%w: %q", ErrReservedName, name)
	}
	if !slugRe.MatchString(name) {
		return fmt.Errorf("%w: %q", ErrInvalidSlug, name)
	}
	// Belt-and-suspenders: a reserved name with mixed/odd casing can't reach here
	// because slugRe already rejects it, but normalize-and-recheck makes the guard
	// independent of regex details.
	if reservedNames[strings.ToLower(name)] {
		return fmt.Errorf("%w: %q", ErrReservedName, name)
	}
	return nil
}

// fmtTime renders t as the canonical UTC storage string.
func fmtTime(t time.Time) string { return t.UTC().Format(timeFormat) }

// parseTime parses a stored timestamp; a malformed value yields the zero time
// (storage is always written by fmtTime, so this is defensive).
func parseTime(s string) time.Time {
	t, _ := time.Parse(timeFormat, s)
	return t
}

// Create validates the slug + reserved guard, then inserts a fresh row with
// tier='' and published=0. created_at/updated_at are set to now (UTC). Returns
// ErrExists if the name is already taken.
func (s *Store) Create(ctx context.Context, name string) (Site, error) {
	if err := validateName(name); err != nil {
		return Site{}, err
	}
	now := s.Now().UTC()
	ts := fmtTime(now)
	_, err := s.db.ExecContext(ctx,
		`INSERT INTO sites (name, tier, published, published_at, created_at, updated_at)
		 VALUES (?, '', 0, NULL, ?, ?)`,
		name, ts, ts)
	if err != nil {
		if strings.Contains(err.Error(), "UNIQUE constraint failed") ||
			strings.Contains(err.Error(), "PRIMARY KEY") {
			return Site{}, fmt.Errorf("%w: %q", ErrExists, name)
		}
		return Site{}, fmt.Errorf("create site %q: %w", name, err)
	}
	return Site{
		Name:      name,
		Tier:      "",
		Published: false,
		CreatedAt: parseTime(ts),
		UpdatedAt: parseTime(ts),
	}, nil
}

// Get fetches one site by name. Returns ErrNotFound when absent.
func (s *Store) Get(ctx context.Context, name string) (Site, error) {
	row := s.db.QueryRowContext(ctx,
		`SELECT name, tier, published, published_at, created_at, updated_at
		 FROM sites WHERE name = ?`, name)
	site, err := scanSite(row)
	if errors.Is(err, sql.ErrNoRows) {
		return Site{}, fmt.Errorf("%w: %q", ErrNotFound, name)
	}
	if err != nil {
		return Site{}, fmt.Errorf("get site %q: %w", name, err)
	}
	return site, nil
}

// List returns every site ordered by name (deterministic).
func (s *Store) List(ctx context.Context) ([]Site, error) {
	rows, err := s.db.QueryContext(ctx,
		`SELECT name, tier, published, published_at, created_at, updated_at
		 FROM sites ORDER BY name`)
	if err != nil {
		return nil, fmt.Errorf("list sites: %w", err)
	}
	defer rows.Close()
	out := []Site{}
	for rows.Next() {
		site, err := scanSite(rows)
		if err != nil {
			return nil, fmt.Errorf("list sites: %w", err)
		}
		out = append(out, site)
	}
	if err := rows.Err(); err != nil {
		return nil, fmt.Errorf("list sites: %w", err)
	}
	return out, nil
}

// Delete removes the row only. Filesystem/symlink teardown is a later phase.
// Returns ErrNotFound when no such row.
func (s *Store) Delete(ctx context.Context, name string) error {
	res, err := s.db.ExecContext(ctx, `DELETE FROM sites WHERE name = ?`, name)
	if err != nil {
		return fmt.Errorf("delete site %q: %w", name, err)
	}
	n, err := res.RowsAffected()
	if err != nil {
		return fmt.Errorf("delete site %q: %w", name, err)
	}
	if n == 0 {
		return fmt.Errorf("%w: %q", ErrNotFound, name)
	}
	return nil
}

// rowScanner is satisfied by both *sql.Row and *sql.Rows.
type rowScanner interface {
	Scan(dest ...any) error
}

// scanSite maps a sites row into a Site, translating the nullable published_at
// and the integer published flag.
func scanSite(sc rowScanner) (Site, error) {
	var (
		name, tier, createdAt, updatedAt string
		published                        int64
		publishedAt                      sql.NullString
	)
	if err := sc.Scan(&name, &tier, &published, &publishedAt, &createdAt, &updatedAt); err != nil {
		return Site{}, err
	}
	site := Site{
		Name:      name,
		Tier:      tier,
		Published: published != 0,
		CreatedAt: parseTime(createdAt),
		UpdatedAt: parseTime(updatedAt),
	}
	if publishedAt.Valid {
		t := parseTime(publishedAt.String)
		site.PublishedAt = &t
	}
	return site, nil
}
