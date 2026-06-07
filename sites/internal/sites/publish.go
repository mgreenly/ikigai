package sites

import (
	"context"
	"errors"
	"fmt"
	"os"
)

// ErrInvalidTier — tier is neither PublicSeg nor PrivateSeg (includes '').
var ErrInvalidTier = errors.New("sites: invalid tier")

// symlinkTarget builds the relative symlink target a served link points at:
// served/<tier>/<name> -> ../../working/<name>. The target is ALWAYS this
// literal relative string (never an absolute path) — nginx serves through the
// served tree and the relative form keeps the link valid regardless of where
// SITES_ROOT is mounted.
func symlinkTarget(name string) string {
	return "../../" + WorkingSeg + "/" + name
}

// Publish makes a site reachable through the served front-door tree by creating
// a relative symlink served/<tier>/<name> -> ../../working/<name> and flipping
// the row to published. Re-publishing to a different tier first drops the old
// tier's link so a site is never reachable under both tiers at once.
// Re-publishing to the same tier is idempotent.
func (s *Store) Publish(ctx context.Context, name, tier string) error {
	if tier != PublicSeg && tier != PrivateSeg {
		return fmt.Errorf("%w: %q", ErrInvalidTier, tier)
	}
	site, err := s.Get(ctx, name)
	if err != nil {
		return err
	}

	// Drop the old tier's link first if switching tiers — never reachable under
	// both tiers simultaneously.
	if site.Tier != "" && site.Tier != tier {
		if err := os.Remove(s.Layout.ServedDir(site.Tier, name)); err != nil && !os.IsNotExist(err) {
			return fmt.Errorf("publish %q: drop old tier link: %w", name, err)
		}
	}

	if err := os.MkdirAll(s.Layout.ServedTierBase(tier), 0755); err != nil {
		return fmt.Errorf("publish %q: ensure tier base: %w", name, err)
	}

	dst := s.Layout.ServedDir(tier, name)
	// Remove any existing link/file at the destination so re-publish is idempotent.
	if err := os.Remove(dst); err != nil && !os.IsNotExist(err) {
		return fmt.Errorf("publish %q: clear served link: %w", name, err)
	}
	if err := os.Symlink(symlinkTarget(name), dst); err != nil {
		return fmt.Errorf("publish %q: symlink: %w", name, err)
	}

	now := s.Now().UTC()
	ts := fmtTime(now)
	if _, err := s.db.ExecContext(ctx,
		`UPDATE sites SET tier = ?, published = 1, published_at = ?, updated_at = ? WHERE name = ?`,
		tier, ts, ts, name); err != nil {
		return fmt.Errorf("publish %q: update row: %w", name, err)
	}
	return nil
}

// Unpublish removes a site's served symlink (the current tier's, if any) and
// flips the row back to unpublished (tier='', published=0, published_at=NULL).
// It is safe to call on an already-unpublished site: a missing link is tolerated
// and the row update is a no-op flip. This lets Phase-4 delete always unpublish
// before RemoveAll(working) to guarantee the unpublish-before-teardown ordering.
func (s *Store) Unpublish(ctx context.Context, name string) error {
	site, err := s.Get(ctx, name)
	if err != nil {
		return err
	}

	if site.Tier != "" {
		if err := os.Remove(s.Layout.ServedDir(site.Tier, name)); err != nil && !os.IsNotExist(err) {
			return fmt.Errorf("unpublish %q: remove served link: %w", name, err)
		}
	}

	now := s.Now().UTC()
	ts := fmtTime(now)
	if _, err := s.db.ExecContext(ctx,
		`UPDATE sites SET tier = '', published = 0, published_at = NULL, updated_at = ? WHERE name = ?`,
		ts, name); err != nil {
		return fmt.Errorf("unpublish %q: update row: %w", name, err)
	}
	return nil
}
