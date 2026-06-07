package sites

import "path/filepath"

// SITES_ROOT layout. Every path in the served tree is constructed here so that
// the publish phase and the on-box opsctl tooling agree byte-for-byte on where a
// site's working copy and its served (symlinked) tree live.
//
// The root is *injected* (a Layout value), not read from the environment here:
// that keeps these helpers pure and testable without touching os.Getenv, and it
// lets the single env read happen once at process wiring (cmd/sites) where the
// rest of the config-from-env lives. DefaultRoot is the production default.
const (
	// DefaultRoot is the production SITES_ROOT when the env var is unset.
	DefaultRoot = "/opt/sites/www"

	// Path segments under the root. Exported so the publish phase and opsctl
	// reference the same names rather than re-spelling string literals.
	WorkingSeg = "working" // <root>/working/<name>   — editable source tree
	ServedSeg  = "served"  // <root>/served/...        — what the front door serves
	PublicSeg  = "public"  // <root>/served/public/... — public tier
	PrivateSeg = "private" // <root>/served/private/.. — private tier
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

// WorkingDir is a site's editable working tree: <root>/working/<name>.
func (l Layout) WorkingDir(name string) string {
	return filepath.Join(l.root(), WorkingSeg, name)
}

// ServedDir is a site's served (front-door) tree for a tier:
// <root>/served/<tier>/<name>. tier is one of PublicSeg / PrivateSeg.
func (l Layout) ServedDir(tier, name string) string {
	return filepath.Join(l.root(), ServedSeg, tier, name)
}

// WorkingBase is <root>/working — the parent of every working tree.
func (l Layout) WorkingBase() string {
	return filepath.Join(l.root(), WorkingSeg)
}

// ServedBase is <root>/served — the parent of the tier dirs.
func (l Layout) ServedBase() string {
	return filepath.Join(l.root(), ServedSeg)
}

// ServedTierBase is <root>/served/<tier> — the parent of a tier's site dirs.
func (l Layout) ServedTierBase(tier string) string {
	return filepath.Join(l.root(), ServedSeg, tier)
}
