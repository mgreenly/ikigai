package opsctl

import (
	"fmt"
	"io"
	"os"
	"path/filepath"
	"sort"
	"strings"
)

// DefaultKeep is how many recent releases prune retains by default (PLAN §C2:
// "prune keeps the right N", a small N). bin/run's target and the immediate
// predecessor rollback would aim at are always kept regardless of N.
const DefaultKeep = 3

// Opsctl binds the install/rollback/prune verbs to a configurable root and the
// two box-only seams. Construct with New; tests inject stub seams and a temp
// root, the box uses RealSystem / RealRunner and OPSCTL_ROOT (default /opt).
type Opsctl struct {
	Root    string    // OPSCTL_ROOT, "" ⇒ /opt — the /opt/<app> tree
	SysRoot string    // OPSCTL_SYSROOT, "" ⇒ / — the /etc + /var system-config tree
	Keep    int       // releases prune retains (0 ⇒ DefaultKeep)
	System  System    // systemd / provisioning seam
	Runner  AppRunner // app-binary verb seam
	Store   ObjectStore
	// Out / Err are the human-readable progress streams (the verbs' data goes to
	// the box, not here). Default os.Stdout / os.Stderr.
	Out io.Writer
	Err io.Writer
}

// New builds an Opsctl with the real box seams and the given root. cmd/opsctl
// uses this; tests construct the struct literally with stubs.
func New(root string) *Opsctl {
	return &Opsctl{
		Root:    root,
		SysRoot: os.Getenv("OPSCTL_SYSROOT"),
		Keep:    DefaultKeep,
		System:  RealSystem{},
		Runner:  RealRunner{},
		Out:     os.Stdout,
		Err:     os.Stderr,
	}
}

func (o *Opsctl) keep() int {
	if o.Keep <= 0 {
		return DefaultKeep
	}
	return o.Keep
}

func (o *Opsctl) layout(app string) Layout { return NewLayoutSys(o.Root, o.SysRoot, app) }

// discoverApps lists the installed apps under OPSCTL_ROOT: the immediate
// subdirectories of Root that carry a `bin/run` symlink (a deployed release).
// Bare /opt children without one (a setup-but-never-deployed tree, or unrelated
// dirs) are skipped, so `status` and friends report only live apps. The result is
// sorted ascending by name.
func (o *Opsctl) discoverApps() ([]string, error) {
	root := o.Root
	if root == "" {
		root = "/opt"
	}
	entries, err := os.ReadDir(root)
	if err != nil {
		if os.IsNotExist(err) {
			return nil, nil
		}
		return nil, err
	}
	var apps []string
	for _, e := range entries {
		if !e.IsDir() {
			continue
		}
		l := o.layout(e.Name())
		if fi, lerr := os.Lstat(l.RunLink()); lerr == nil && fi.Mode()&os.ModeSymlink != 0 {
			apps = append(apps, e.Name())
		}
	}
	sort.Strings(apps)
	return apps, nil
}

func (o *Opsctl) logf(format string, args ...any) {
	w := o.Out
	if w == nil {
		w = os.Stdout
	}
	fmt.Fprintf(w, ">> "+format+"\n", args...)
}

// dbEnv returns the env overrides that point the app binary's verbs at the
// stable on-box data paths (the binary reads <APP>_DB_PATH / <APP>_GENERATION_PATH
// from the env per appkit/config). Used for schema|migrate|backup|restore.
func (o *Opsctl) dbEnv(l Layout) []string {
	up := strings.ToUpper(l.App)
	return []string{
		up + "_DB_PATH=" + l.DBPath(),
		up + "_GENERATION_PATH=" + l.GenerationPath(),
	}
}

// atomicSwap repoints a symlink at target using the create-temp +
// rename technique — the POSIX-atomic equivalent of `ln -sfn`. The symlink only
// ever resolves to a complete release: the temp link is fully formed before the
// rename, and rename(2) on the same directory is atomic, so a concurrent
// ikigenba-launch either sees the old or the new target, never a half-written
// one (PLAN §1.4, §2.6).
func atomicSwap(linkPath, target string) error {
	dir := filepath.Dir(linkPath)
	if err := os.MkdirAll(dir, 0o755); err != nil {
		return fmt.Errorf("swap: mkdir parent: %w", err)
	}
	tmp, err := os.MkdirTemp(dir, ".run-swap-*")
	if err != nil {
		return fmt.Errorf("swap: temp dir: %w", err)
	}
	// Use a stable name inside the unique temp dir for the staged link, then move
	// the link itself into place. os.MkdirTemp gives us a collision-free slot.
	staged := filepath.Join(tmp, "run")
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
// leaving it untouched if it already points there. Unlike atomicSwap it is not a
// hot-path cutover.
func ensureSymlink(linkPath, target string) error {
	if cur, err := os.Readlink(linkPath); err == nil {
		if cur == target {
			return nil
		}
		// Already a symlink, but pointing elsewhere — repoint via the atomic
		// technique to stay valid mid-change.
		return atomicSwap(linkPath, target)
	}
	if err := os.MkdirAll(filepath.Dir(linkPath), 0o755); err != nil {
		return err
	}
	// linkPath either does not exist or exists as a NON-symlink (Readlink returns
	// EINVAL on a regular file / dir). On a brand-new tree it is absent; on a
	// conversion from an old layout it is a legacy `bin/run` wrapper script (a
	// regular file). A plain os.Symlink fails with EEXIST in that case, so route
	// the replacement through atomicSwap, whose rename(2) atomically replaces an
	// existing regular file — leaving the stable path valid throughout.
	if _, err := os.Lstat(linkPath); err == nil {
		return atomicSwap(linkPath, target)
	}
	return os.Symlink(target, linkPath)
}
