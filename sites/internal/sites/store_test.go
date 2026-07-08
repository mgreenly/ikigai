package sites

import (
	"context"
	"errors"
	"path/filepath"
	"testing"
	"time"

	sqlkit "appkit/db"

	"sites/internal/db"
)

// newTestStore returns a Store wired to a fresh, migrated temp-file SQLite
// database with a deterministic, monotonically-increasing clock so created_at /
// updated_at ordering is stable. t.TempDir() cleans up the file.
func newTestStore(t *testing.T) *Store {
	t.Helper()
	path := filepath.Join(t.TempDir(), "sites_test.db")
	conn, err := sqlkit.Open(path)
	if err != nil {
		t.Fatalf("open test db: %v", err)
	}
	t.Cleanup(func() { conn.Close() })
	migs, err := sqlkit.LoadMigrations(db.FS, "migrations")
	if err != nil {
		t.Fatalf("load migrations: %v", err)
	}
	if err := sqlkit.Migrate(context.Background(), conn, migs); err != nil {
		t.Fatalf("migrate test db: %v", err)
	}
	clk := &time.Time{}
	*clk = time.Date(2026, 6, 3, 12, 0, 0, 0, time.UTC)
	s := NewStore(conn)
	s.Now = func() time.Time {
		*clk = clk.Add(time.Millisecond)
		return *clk
	}
	return s
}

func TestCRUDRoundtrip(t *testing.T) {
	s := newTestStore(t)
	ctx := context.Background()

	a, err := s.Create(ctx, "alpha", "")
	if err != nil {
		t.Fatalf("create alpha: %v", err)
	}
	if a.Name != "alpha" || a.Public {
		t.Fatalf("create alpha: unexpected row %+v", a)
	}
	if a.CreatedAt.IsZero() || a.UpdatedAt.IsZero() {
		t.Fatalf("create alpha: timestamps unset %+v", a)
	}

	if _, err := s.Create(ctx, "bravo", ""); err != nil {
		t.Fatalf("create bravo: %v", err)
	}

	// List returns both, sorted by name.
	list, err := s.List(ctx)
	if err != nil {
		t.Fatalf("list: %v", err)
	}
	if len(list) != 2 {
		t.Fatalf("list: want 2, got %d", len(list))
	}
	if list[0].Name != "alpha" || list[1].Name != "bravo" {
		t.Fatalf("list: not sorted: %q, %q", list[0].Name, list[1].Name)
	}

	// Get returns one.
	got, err := s.Get(ctx, "alpha")
	if err != nil {
		t.Fatalf("get alpha: %v", err)
	}
	if got.Name != "alpha" {
		t.Fatalf("get alpha: got %q", got.Name)
	}

	// Delete removes it; subsequent get is ErrNotFound.
	if err := s.Delete(ctx, "alpha"); err != nil {
		t.Fatalf("delete alpha: %v", err)
	}
	if _, err := s.Get(ctx, "alpha"); !errors.Is(err, ErrNotFound) {
		t.Fatalf("get after delete: want ErrNotFound, got %v", err)
	}

	// And only bravo remains.
	list, err = s.List(ctx)
	if err != nil {
		t.Fatalf("list after delete: %v", err)
	}
	if len(list) != 1 || list[0].Name != "bravo" {
		t.Fatalf("list after delete: want [bravo], got %+v", list)
	}
}

func TestSlugReject(t *testing.T) {
	s := newTestStore(t)
	ctx := context.Background()

	bad := []struct {
		name, slug string
	}{
		{"uppercase", "Foo"},
		{"underscore", "a_b"},
		{"leading-hyphen", "-lead"},
		{"space", "has space"},
		{"too-long", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"}, // 64 chars
		{"empty", ""},
	}
	for _, tc := range bad {
		t.Run(tc.name, func(t *testing.T) {
			_, err := s.Create(ctx, tc.slug, "")
			if !errors.Is(err, ErrInvalidSlug) {
				t.Fatalf("create %q: want ErrInvalidSlug, got %v", tc.slug, err)
			}
		})
	}
}

func TestReservedNameReject(t *testing.T) {
	s := newTestStore(t)
	ctx := context.Background()

	for _, name := range []string{"mcp", ".well-known"} {
		t.Run(name, func(t *testing.T) {
			_, err := s.Create(ctx, name, "")
			if !errors.Is(err, ErrReservedName) {
				t.Fatalf("create %q: want ErrReservedName, got %v", name, err)
			}
		})
	}
}

func TestCreateDuplicate(t *testing.T) {
	s := newTestStore(t)
	ctx := context.Background()

	if _, err := s.Create(ctx, "dup", ""); err != nil {
		t.Fatalf("first create: %v", err)
	}
	_, err := s.Create(ctx, "dup", "")
	if !errors.Is(err, ErrExists) {
		t.Fatalf("duplicate create: want ErrExists, got %v", err)
	}
}

func TestGetDeleteNotFound(t *testing.T) {
	s := newTestStore(t)
	ctx := context.Background()

	if _, err := s.Get(ctx, "ghost"); !errors.Is(err, ErrNotFound) {
		t.Fatalf("get missing: want ErrNotFound, got %v", err)
	}
	if err := s.Delete(ctx, "ghost"); !errors.Is(err, ErrNotFound) {
		t.Fatalf("delete missing: want ErrNotFound, got %v", err)
	}
}

func TestCreatePersistsCreatedByAndDefaultsPrivate(t *testing.T) {
	s := newTestStore(t)
	ctx := context.Background()

	created, err := s.Create(ctx, "creator-site", "user-123")
	if err != nil {
		t.Fatalf("create: %v", err)
	}
	// R-QSLO-SAIQ
	if created.Public {
		t.Fatalf("Create Public = true, want false")
	}
	// R-QRDS-EIS1
	if created.CreatedBy != "user-123" {
		t.Fatalf("Create CreatedBy = %q, want %q", created.CreatedBy, "user-123")
	}

	got, err := s.Get(ctx, "creator-site")
	if err != nil {
		t.Fatalf("get: %v", err)
	}
	if got.CreatedBy != "user-123" {
		t.Fatalf("Get CreatedBy = %q, want %q", got.CreatedBy, "user-123")
	}
	if got.Public {
		t.Fatalf("Get Public = true, want false")
	}

	list, err := s.List(ctx)
	if err != nil {
		t.Fatalf("list: %v", err)
	}
	if len(list) != 1 {
		t.Fatalf("list len = %d, want 1", len(list))
	}
	if list[0].CreatedBy != "user-123" {
		t.Fatalf("List CreatedBy = %q, want %q", list[0].CreatedBy, "user-123")
	}
	if list[0].Public {
		t.Fatalf("List Public = true, want false")
	}

	for _, column := range []string{"public", "created_by"} {
		var found int
		if err := s.db.QueryRowContext(ctx, `SELECT count(*) FROM pragma_table_info('sites') WHERE name = ?`, column).Scan(&found); err != nil {
			t.Fatalf("query schema column %q: %v", column, err)
		}
		if found != 1 {
			t.Fatalf("schema column %q count = %d, want 1", column, found)
		}
	}
}

func TestSitesSchemaDropsPublishLifecycleColumns(t *testing.T) {
	s := newTestStore(t)
	ctx := context.Background()

	rows, err := s.db.QueryContext(ctx, `SELECT name FROM pragma_table_info('sites')`)
	if err != nil {
		t.Fatalf("query schema columns: %v", err)
	}
	defer rows.Close()

	columns := map[string]bool{}
	for rows.Next() {
		var name string
		if err := rows.Scan(&name); err != nil {
			t.Fatalf("scan schema column: %v", err)
		}
		columns[name] = true
	}
	if err := rows.Err(); err != nil {
		t.Fatalf("schema rows: %v", err)
	}

	// R-QQ5W-0R1C
	for _, name := range []string{"public", "created_by"} {
		if !columns[name] {
			t.Fatalf("schema missing %q column; columns=%v", name, columns)
		}
	}
	for _, name := range []string{"tier", "published", "published_at"} {
		if columns[name] {
			t.Fatalf("schema still has %q column; columns=%v", name, columns)
		}
	}
}

func TestSetVisibilityPersistsAndAdvancesUpdatedAt(t *testing.T) {
	s := newTestStore(t)
	ctx := context.Background()

	site, err := s.Create(ctx, "visibility", "")
	if err != nil {
		t.Fatalf("create: %v", err)
	}
	if site.Public {
		t.Fatalf("initial Public = true, want false")
	}

	// R-QTTL-629F
	if err := s.SetVisibility(ctx, "visibility", true); err != nil {
		t.Fatalf("set public: %v", err)
	}
	public, err := s.Get(ctx, "visibility")
	if err != nil {
		t.Fatalf("get public: %v", err)
	}
	if !public.Public {
		t.Fatalf("after SetVisibility true Public = false, want true")
	}
	if !public.UpdatedAt.After(site.UpdatedAt) {
		t.Fatalf("after SetVisibility true UpdatedAt = %v, want after %v", public.UpdatedAt, site.UpdatedAt)
	}

	if err := s.SetVisibility(ctx, "visibility", false); err != nil {
		t.Fatalf("set private: %v", err)
	}
	private, err := s.Get(ctx, "visibility")
	if err != nil {
		t.Fatalf("get private: %v", err)
	}
	if private.Public {
		t.Fatalf("after SetVisibility false Public = true, want false")
	}
	if !private.UpdatedAt.After(public.UpdatedAt) {
		t.Fatalf("after SetVisibility false UpdatedAt = %v, want after %v", private.UpdatedAt, public.UpdatedAt)
	}

	if err := s.SetVisibility(ctx, "missing", true); !errors.Is(err, ErrNotFound) {
		t.Fatalf("missing SetVisibility: want ErrNotFound, got %v", err)
	}
}
