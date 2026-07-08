package sites

import (
	"fmt"
	"os"
	"path/filepath"
)

// SITES_ROOT layout. Every path in the served tree is constructed here so that
// the app and the on-box opsctl tooling agree byte-for-byte on where a site's
// public or private directory lives.
//
// The root is *injected* (a Layout value), not read from the environment here:
// that keeps these helpers pure and testable without touching os.Getenv, and it
// lets the single env read happen once at process wiring (cmd/sites) where the
// rest of the config-from-env lives. DefaultRoot is the production default.
const (
	// DefaultRoot is the production SITES_ROOT when the env var is unset.
	DefaultRoot = "/opt/sites/state/www"

	// Path segments under the root. Exported so callers and opsctl reference the
	// same names rather than re-spelling string literals.
	PublicSeg  = "public"  // <root>/public/<name>  — public tier
	PrivateSeg = "private" // <root>/private/<name> — private tier
)

// Layout pins the SITES_ROOT all path helpers hang off. Construct with NewLayout;
// a zero Layout falls back to DefaultRoot via root().
type Layout struct {
	Root string
}

// NewLayout returns a Layout rooted at root, falling back to DefaultRoot when
// root is empty (e.g. SITES_ROOT unset). The caller in cmd/sites does the one
// os.Getenv("SITES_ROOT").
func NewLayout(root string) Layout {
	if root == "" {
		root = DefaultRoot
	}
	return Layout{Root: root}
}

// root returns the effective root, tolerating a zero-value Layout.
func (l Layout) root() string {
	if l.Root == "" {
		return DefaultRoot
	}
	return l.Root
}

// SiteDir is a site's on-disk directory for its visibility:
//
//	public  -> <root>/public/<slug>
//	private -> <root>/private/<slug>
func (l Layout) SiteDir(public bool, slug string) string {
	return filepath.Join(l.SiteBase(public), slug)
}

// SiteBase is <root>/<public|private> — the parent of a visibility's site dirs.
func (l Layout) SiteBase(public bool) string {
	if public {
		return filepath.Join(l.root(), PublicSeg)
	}
	return filepath.Join(l.root(), PrivateSeg)
}

// Move relocates a site's directory between private and public visibility
// parents. Missing source directories are tolerated so an empty site can be
// made public/private before it has files.
func (l Layout) Move(slug string, toPublic bool) error {
	src := l.SiteDir(!toPublic, slug)
	dst := l.SiteDir(toPublic, slug)

	if _, err := os.Stat(dst); err == nil {
		if _, srcErr := os.Stat(src); os.IsNotExist(srcErr) {
			return nil
		} else if srcErr != nil {
			return fmt.Errorf("move site %q: stat source: %w", slug, srcErr)
		}
		return fmt.Errorf("move site %q: destination already exists: %s", slug, dst)
	} else if !os.IsNotExist(err) {
		return fmt.Errorf("move site %q: stat destination: %w", slug, err)
	}

	if _, err := os.Stat(src); err != nil {
		if os.IsNotExist(err) {
			return nil
		}
		return fmt.Errorf("move site %q: stat source: %w", slug, err)
	}
	if err := os.MkdirAll(filepath.Dir(dst), 0o755); err != nil {
		return fmt.Errorf("move site %q: ensure destination parent: %w", slug, err)
	}
	if err := os.Rename(src, dst); err != nil {
		return fmt.Errorf("move site %q: rename: %w", slug, err)
	}
	return nil
}
