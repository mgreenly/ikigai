package optctl

import (
	"fmt"
	"io"
	"os"
	"path/filepath"
	"strings"
)

// DefaultKeep is how many recent releases prune retains by default (PLAN §C2:
// "prune keeps the right N", a small N). current's target and the immediate
// predecessor rollback would aim at are always kept regardless of N.
const DefaultKeep = 3

// Optctl binds the install/rollback/prune verbs to a configurable root and the
// two box-only seams. Construct with New; tests inject stub seams and a temp
// root, the box uses RealSystem / RealRunner and OPTCTL_ROOT (default /opt).
type Optctl struct {
	Root   string    // OPTCTL_ROOT, "" ⇒ /opt
	Keep   int       // releases prune retains (0 ⇒ DefaultKeep)
	System System    // systemd restart / is-active seam
	Runner AppRunner // app-binary verb seam
	// Out / Err are the human-readable progress streams (the verbs' data goes to
	// the box, not here). Default os.Stdout / os.Stderr.
	Out io.Writer
	Err io.Writer
}

// New builds an Optctl with the real box seams and the given root. cmd/optctl
// uses this; tests construct the struct literally with stubs.
func New(root string) *Optctl {
	return &Optctl{
		Root:   root,
		Keep:   DefaultKeep,
		System: RealSystem{},
		Runner: RealRunner{},
		Out:    os.Stdout,
		Err:    os.Stderr,
	}
}

func (o *Optctl) keep() int {
	if o.Keep <= 0 {
		return DefaultKeep
	}
	return o.Keep
}

func (o *Optctl) layout(app string) Layout { return NewLayout(o.Root, app) }

func (o *Optctl) logf(format string, args ...any) {
	w := o.Out
	if w == nil {
		w = os.Stdout
	}
	fmt.Fprintf(w, ">> "+format+"\n", args...)
}

// dbEnv returns the env overrides that point the app binary's verbs at the
// stable on-box data paths (the binary reads <APP>_DB_PATH / <APP>_GENERATION_PATH
// from the env per appkit/config). Used for schema|migrate|backup|restore.
func (o *Optctl) dbEnv(l Layout) []string {
	up := strings.ToUpper(l.App)
	return []string{
		up + "_DB_PATH=" + l.DBPath(),
		up + "_GENERATION_PATH=" + l.GenerationPath(),
	}
}

// atomicSwap repoints the `current` symlink at target using the create-temp +
// rename technique — the POSIX-atomic equivalent of `ln -sfn`. The symlink only
// ever resolves to a complete release: the temp link is fully formed before the
// rename, and rename(2) on the same directory is atomic, so a concurrent
// metaspot-launch either sees the old or the new target, never a half-written
// one (PLAN §1.4, §2.6).
func atomicSwap(linkPath, target string) error {
	dir := filepath.Dir(linkPath)
	tmp, err := os.MkdirTemp(dir, ".current-swap-*")
	if err != nil {
		return fmt.Errorf("swap: temp dir: %w", err)
	}
	// Use a stable name inside the unique temp dir for the staged link, then move
	// the link itself into place. os.MkdirTemp gives us a collision-free slot.
	staged := filepath.Join(tmp, "current")
	if err := os.Symlink(target, staged); err != nil {
		os.RemoveAll(tmp)
		return fmt.Errorf("swap: symlink: %w", err)
	}
	if err := os.Rename(staged, linkPath); err != nil {
		os.RemoveAll(tmp)
		return fmt.Errorf("swap: rename into place: %w", err)
	}
	os.RemoveAll(tmp)
	return nil
}

// ensureSymlink makes linkPath a symlink to target, creating it if absent and
// leaving it untouched if it already points there. Used for the stable bin/run
// link (set once at setup, but install self-heals it so a fresh release dir is
// never left without it). Unlike atomicSwap it is not a hot-path cutover.
func ensureSymlink(linkPath, target string) error {
	if cur, err := os.Readlink(linkPath); err == nil {
		if cur == target {
			return nil
		}
		// Repoint via the atomic technique to stay valid mid-change.
		return atomicSwap(linkPath, target)
	}
	if err := os.MkdirAll(filepath.Dir(linkPath), 0o755); err != nil {
		return err
	}
	return os.Symlink(target, linkPath)
}
