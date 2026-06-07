package sites

import (
	"context"
	"errors"
	"path/filepath"
	"testing"
	"time"

	"sites/internal/db"
)

// newTestStore returns a Store wired to a fresh, migrated temp-file SQLite
// database with a deterministic, monotonically-increasing clock so created_at /
// updated_at ordering is stable. t.TempDir() cleans up the file.
func newTestStore(t *testing.T) *Store {
	t.Helper()
	path := filepath.Join(t.TempDir(), "sites_test.db")
	conn, err := db.Open(path)
	if err != nil {
		t.Fatalf("open test db: %v", err)
	}
	t.Cleanup(func() { conn.Close() })
	if err := db.Migrate(context.Background(), conn); err != nil {
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

	a, err := s.Create(ctx, "alpha")
	if err != nil {
		t.Fatalf("create alpha: %v", err)
	}
	if a.Name != "alpha" || a.Tier != "" || a.Published {
		t.Fatalf("create alpha: unexpected row %+v", a)
	}
	if a.PublishedAt != nil {
		t.Fatalf("create alpha: published_at should be nil, got %v", a.PublishedAt)
	}
	if a.CreatedAt.IsZero() || a.UpdatedAt.IsZero() {
		t.Fatalf("create alpha: timestamps unset %+v", a)
	}

	if _, err := s.Create(ctx, "bravo"); err != nil {
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
			_, err := s.Create(ctx, tc.slug)
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
			_, err := s.Create(ctx, name)
			if !errors.Is(err, ErrReservedName) {
				t.Fatalf("create %q: want ErrReservedName, got %v", name, err)
			}
		})
	}
}

func TestCreateDuplicate(t *testing.T) {
	s := newTestStore(t)
	ctx := context.Background()

	if _, err := s.Create(ctx, "dup"); err != nil {
		t.Fatalf("first create: %v", err)
	}
	_, err := s.Create(ctx, "dup")
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

func TestLayout(t *testing.T) {
	l := NewLayout("/srv/x")
	if got := l.WorkingDir("a"); got != "/srv/x/working/a" {
		t.Fatalf("WorkingDir: %q", got)
	}
	if got := l.ServedDir(PublicSeg, "a"); got != "/srv/x/served/public/a" {
		t.Fatalf("ServedDir public: %q", got)
	}
	if got := l.ServedDir(PrivateSeg, "a"); got != "/srv/x/served/private/a" {
		t.Fatalf("ServedDir private: %q", got)
	}
	// Empty root falls back to DefaultRoot.
	def := NewLayout("")
	if got := def.WorkingDir("a"); got != DefaultRoot+"/working/a" {
		t.Fatalf("default WorkingDir: %q", got)
	}
	// Zero-value Layout tolerates the missing root.
	var z Layout
	if got := z.ServedBase(); got != DefaultRoot+"/served" {
		t.Fatalf("zero ServedBase: %q", got)
	}
}
