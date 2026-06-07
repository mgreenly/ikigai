package sites

import (
	"context"
	"errors"
	"os"
	"path/filepath"
	"testing"
	"time"

	"sites/internal/db"
)

// newTestPublisher returns a Store wired to a fresh migrated temp DB and a
// Layout rooted at a temp dir, with a deterministic monotonic clock.
func newTestPublisher(t *testing.T) (*Store, Layout) {
	t.Helper()
	root := t.TempDir()
	path := filepath.Join(t.TempDir(), "sites_test.db")
	conn, err := db.Open(path)
	if err != nil {
		t.Fatalf("open test db: %v", err)
	}
	t.Cleanup(func() { conn.Close() })
	if err := db.Migrate(context.Background(), conn); err != nil {
		t.Fatalf("migrate test db: %v", err)
	}
	layout := NewLayout(root)
	s := NewStoreWithLayout(conn, layout)
	clk := time.Date(2026, 6, 3, 12, 0, 0, 0, time.UTC)
	s.Now = func() time.Time {
		clk = clk.Add(time.Millisecond)
		return clk
	}
	return s, layout
}

// makeWorking pre-creates working/<name> with a marker file so resolution tests
// land somewhere real.
func makeWorking(t *testing.T, l Layout, name string) string {
	t.Helper()
	wd := l.WorkingDir(name)
	if err := os.MkdirAll(wd, 0755); err != nil {
		t.Fatalf("mkdir working: %v", err)
	}
	marker := filepath.Join(wd, "index.html")
	if err := os.WriteFile(marker, []byte("hello"), 0644); err != nil {
		t.Fatalf("write marker: %v", err)
	}
	return marker
}

func TestPublishCreatesResolvingSymlink(t *testing.T) {
	s, l := newTestPublisher(t)
	ctx := context.Background()
	makeWorking(t, l, "demo")
	if _, err := s.Create(ctx, "demo"); err != nil {
		t.Fatalf("create: %v", err)
	}

	if err := s.Publish(ctx, "demo", PublicSeg); err != nil {
		t.Fatalf("publish: %v", err)
	}

	link := l.ServedDir(PublicSeg, "demo")
	fi, err := os.Lstat(link)
	if err != nil {
		t.Fatalf("lstat link: %v", err)
	}
	if fi.Mode()&os.ModeSymlink == 0 {
		t.Fatalf("served path is not a symlink: %v", fi.Mode())
	}
	tgt, err := os.Readlink(link)
	if err != nil {
		t.Fatalf("readlink: %v", err)
	}
	if want := "../../working/demo"; tgt != want {
		t.Fatalf("symlink target = %q, want %q", tgt, want)
	}

	// Resolving through the link reaches the working marker file.
	served := filepath.Join(link, "index.html")
	body, err := os.ReadFile(served)
	if err != nil {
		t.Fatalf("read through served link: %v", err)
	}
	if string(body) != "hello" {
		t.Fatalf("served content = %q, want %q", body, "hello")
	}
	resolved, err := filepath.EvalSymlinks(link)
	if err != nil {
		t.Fatalf("evalsymlinks: %v", err)
	}
	wantResolved, _ := filepath.EvalSymlinks(l.WorkingDir("demo"))
	if resolved != wantResolved {
		t.Fatalf("resolved = %q, want %q", resolved, wantResolved)
	}

	site, err := s.Get(ctx, "demo")
	if err != nil {
		t.Fatalf("get: %v", err)
	}
	if site.Tier != PublicSeg || !site.Published || site.PublishedAt == nil {
		t.Fatalf("row not published correctly: %+v", site)
	}
}

func TestRepublishSwitchesTierLeavingOnlyOne(t *testing.T) {
	s, l := newTestPublisher(t)
	ctx := context.Background()
	makeWorking(t, l, "demo")
	if _, err := s.Create(ctx, "demo"); err != nil {
		t.Fatalf("create: %v", err)
	}

	if err := s.Publish(ctx, "demo", PublicSeg); err != nil {
		t.Fatalf("publish public: %v", err)
	}
	if err := s.Publish(ctx, "demo", PrivateSeg); err != nil {
		t.Fatalf("publish private: %v", err)
	}

	// Public link must be gone.
	if _, err := os.Lstat(l.ServedDir(PublicSeg, "demo")); !os.IsNotExist(err) {
		t.Fatalf("public link still exists: err=%v", err)
	}
	// Private link must be the symlink.
	fi, err := os.Lstat(l.ServedDir(PrivateSeg, "demo"))
	if err != nil {
		t.Fatalf("lstat private: %v", err)
	}
	if fi.Mode()&os.ModeSymlink == 0 {
		t.Fatalf("private path is not a symlink")
	}

	site, err := s.Get(ctx, "demo")
	if err != nil {
		t.Fatalf("get: %v", err)
	}
	if site.Tier != PrivateSeg {
		t.Fatalf("tier = %q, want %q", site.Tier, PrivateSeg)
	}
}

func TestRepublishSameTierIdempotent(t *testing.T) {
	s, l := newTestPublisher(t)
	ctx := context.Background()
	makeWorking(t, l, "demo")
	if _, err := s.Create(ctx, "demo"); err != nil {
		t.Fatalf("create: %v", err)
	}
	if err := s.Publish(ctx, "demo", PublicSeg); err != nil {
		t.Fatalf("publish 1: %v", err)
	}
	if err := s.Publish(ctx, "demo", PublicSeg); err != nil {
		t.Fatalf("publish 2 (idempotent): %v", err)
	}
	tgt, err := os.Readlink(l.ServedDir(PublicSeg, "demo"))
	if err != nil {
		t.Fatalf("readlink: %v", err)
	}
	if tgt != "../../working/demo" {
		t.Fatalf("target = %q", tgt)
	}
}

func TestUnpublishRemovesLink(t *testing.T) {
	s, l := newTestPublisher(t)
	ctx := context.Background()
	makeWorking(t, l, "demo")
	if _, err := s.Create(ctx, "demo"); err != nil {
		t.Fatalf("create: %v", err)
	}
	if err := s.Publish(ctx, "demo", PublicSeg); err != nil {
		t.Fatalf("publish: %v", err)
	}
	if err := s.Unpublish(ctx, "demo"); err != nil {
		t.Fatalf("unpublish: %v", err)
	}

	if _, err := os.Lstat(l.ServedDir(PublicSeg, "demo")); !os.IsNotExist(err) {
		t.Fatalf("public link still exists: %v", err)
	}
	if _, err := os.Lstat(l.ServedDir(PrivateSeg, "demo")); !os.IsNotExist(err) {
		t.Fatalf("private link exists: %v", err)
	}

	site, err := s.Get(ctx, "demo")
	if err != nil {
		t.Fatalf("get: %v", err)
	}
	if site.Tier != "" || site.Published || site.PublishedAt != nil {
		t.Fatalf("row not unpublished: %+v", site)
	}
}

func TestUnpublishAlreadyUnpublishedIsNil(t *testing.T) {
	s, l := newTestPublisher(t)
	ctx := context.Background()
	makeWorking(t, l, "demo")
	if _, err := s.Create(ctx, "demo"); err != nil {
		t.Fatalf("create: %v", err)
	}
	// Never published — unpublish must be a no-op without error.
	if err := s.Unpublish(ctx, "demo"); err != nil {
		t.Fatalf("unpublish on unpublished site: %v", err)
	}
	// Calling twice after a publish is also fine.
	if err := s.Publish(ctx, "demo", PublicSeg); err != nil {
		t.Fatalf("publish: %v", err)
	}
	if err := s.Unpublish(ctx, "demo"); err != nil {
		t.Fatalf("unpublish 1: %v", err)
	}
	if err := s.Unpublish(ctx, "demo"); err != nil {
		t.Fatalf("unpublish 2: %v", err)
	}
}

func TestDeleteOrderSafety(t *testing.T) {
	s, l := newTestPublisher(t)
	ctx := context.Background()
	makeWorking(t, l, "demo")
	if _, err := s.Create(ctx, "demo"); err != nil {
		t.Fatalf("create: %v", err)
	}
	if err := s.Publish(ctx, "demo", PublicSeg); err != nil {
		t.Fatalf("publish: %v", err)
	}
	// Phase-4 delete ordering: unpublish first, then teardown working.
	if err := s.Unpublish(ctx, "demo"); err != nil {
		t.Fatalf("unpublish: %v", err)
	}
	if err := os.RemoveAll(l.WorkingDir("demo")); err != nil {
		t.Fatalf("removeall working: %v", err)
	}
	// No dangling served link.
	if _, err := os.Lstat(l.ServedDir(PublicSeg, "demo")); !os.IsNotExist(err) {
		t.Fatalf("served link dangles after teardown: %v", err)
	}
}

func TestPublishInvalidTier(t *testing.T) {
	s, l := newTestPublisher(t)
	ctx := context.Background()
	makeWorking(t, l, "demo")
	if _, err := s.Create(ctx, "demo"); err != nil {
		t.Fatalf("create: %v", err)
	}
	for _, tier := range []string{"", "foo"} {
		if err := s.Publish(ctx, "demo", tier); !errors.Is(err, ErrInvalidTier) {
			t.Fatalf("Publish(tier=%q) err = %v, want ErrInvalidTier", tier, err)
		}
	}
}

func TestPublishNonexistentSite(t *testing.T) {
	s, _ := newTestPublisher(t)
	ctx := context.Background()
	if err := s.Publish(ctx, "ghost", PublicSeg); !errors.Is(err, ErrNotFound) {
		t.Fatalf("Publish(ghost) err = %v, want ErrNotFound", err)
	}
}
