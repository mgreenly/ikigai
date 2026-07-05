package opsctl

import (
	"context"
	"fmt"
	"os"
	"path/filepath"
	"strings"
)

// SetupOptions parameterises `opsctl setup <app>` — per-app provisioning (PLAN
// §D1, ADR "init-box vs setup"). It carries the non-secret routing/identity the
// old per-service bin/setup sourced from etc/manifest.env so opsctl can emit a
// byte-identical unit + nginx fragment.
type SetupOptions struct {
	App      string // service name (== systemd unit / fragment basename / app user)
	Fragment string // the service's nginx location fragment SOURCE — exactly the
	// committed etc/nginx.conf body (its loopback port is a literal, fixed by the
	// registry). Empty ⇒ a service with no public route (worker/batch) — no
	// fragment is dropped.
	IsDefault bool // true for the apex/DEFAULT app whose nginx block is init-box/deploy-owned.

	// WWWDirs are extra directories (absolute paths) to create at mode 0750 and
	// hand to the app user plus web group via `chown -R <app>:web` on the www
	// ROOT. They back the sites service's SEPARATE state/www tree (working/,
	// public/, private/): the stock per-app state/ is 0750 <app>:<app> so nginx
	// cannot traverse it, so sites serves from this web-group tree instead.
	// Apps that need no static tree (every app but sites) pass none — and
	// then setup creates no www dir at all, leaving their behavior unchanged. The
	// command layer derives this list per-app (see wwwDirsFor); it is not an
	// operator flag, so `opsctl setup sites …` provisions the tree automatically.
	WWWDirs []string

	// DeferNginx stages the fragment file but skips the `nginx -t` + reload. On a
	// greenfield box nginx is not yet serviceable (the apex 443 cert does not
	// exist until the apex/dashboard deploy issues it), so `nginx -t` would
	// hard-fail. With DeferNginx the fragment is written and validated/reloaded
	// later, when the cert lands and nginx comes up. Mirrors init-box --skip-cert.
	DeferNginx bool

	// Packages are OS packages the service declares it needs on the box (e.g.
	// scripts needs python3.11 to execute runs). They are installed early in
	// setup via the System.InstallPackages seam — idempotently, since the
	// package manager skips already-present packages — so a service can request
	// its own runtime deps independently of any sibling service. Empty ⇒ no
	// package install step (the common case). Mirrors init-box's nginx+certbot
	// install (initbox.go:58).
	Packages []string
}

// Setup performs first-time, idempotent per-app provisioning, the per-APP half of
// the old bin/setup split out from the box-global init-box (PLAN §D1):
//
//  1. create the dedicated --system app user (seam; idempotent),
//  2. create the /opt/<app> tree (libexec/ bin/ etc/ share/ state/ cache/),
//  3. write + enable-NOT-start the systemd unit
//     (ExecStart=/usr/local/bin/ikigenba-launch <app>),
//  4. drop the service's nginx fragment into conf.d/locations/<app>.conf
//     verbatim (its loopback port is already a literal), then nginx -t + reload.
//
// It assumes init-box already ran (conf.d/locations/ exists, the apex block +
// cert are in place). The config artifacts (the unit, the fragment) are WRITTEN
// to SysRoot-rooted paths so tests byte-assert them; the privileged ops (user
// creation, enable, nginx test/reload) go through the System seam.
//
// The unit file and nginx fragment it emits byte-match what today's bin/setup
// produces — the diff from the old behavior is only the init-box/setup split.
func (o *Opsctl) Setup(ctx context.Context, opts SetupOptions) error {
	if opts.App == "" {
		return fmt.Errorf("setup: app is required")
	}
	if opts.IsDefault && opts.Fragment != "" {
		return fmt.Errorf("setup: --default cannot be combined with --fragment")
	}
	app := opts.App
	l := o.layout(app)

	// init-box must have run first: the locations include dir is box-global and
	// owned by init-box, mirroring the old service bin/setup precondition check.
	if !opts.IsDefault {
		if _, err := os.Stat(l.LocationsDir()); err != nil {
			return fmt.Errorf("setup: %s missing — run `opsctl init-box` first: %w", l.LocationsDir(), err)
		}
	}

	// 0. OS packages the service declares it needs (e.g. scripts → python3.11).
	//    Installed before anything else so the runtime deps are present by the
	//    time the unit is enabled. Idempotent: the package manager no-ops on
	//    already-present packages, so multiple services may declare the same one.
	if len(opts.Packages) > 0 {
		o.logf("install packages: %s", strings.Join(opts.Packages, " "))
		if err := o.System.InstallPackages(ctx, opts.Packages...); err != nil {
			return fmt.Errorf("setup: install packages: %w", err)
		}
	}

	// 1. App user (seam — never executed in tests).
	o.logf("ensure app user %s (home %s)", app, l.AppDir())
	if err := o.System.EnsureSystemUser(ctx, app, l.AppDir()); err != nil {
		return fmt.Errorf("setup: ensure app user: %w", err)
	}

	// 2. The /opt/<app> tree.
	o.logf("create /opt/%s tree", app)
	if opts.IsDefault {
		// The DEFAULT app owns the apex route through init-box/deploy, not a
		// per-app location fragment. It gets the normal service tree and unit but
		// no worker state/www tree and no nginx web group.
		if err := mkdirAll755(
			l.AppDir(), l.BinDir(), l.EtcDir(), l.LibexecDir(), l.CacheDir(), l.BackupsDir(),
		); err != nil {
			return fmt.Errorf("setup: create app tree: %w", err)
		}
		if err := mkdirAllMode(0o750, l.StateDir()); err != nil {
			return fmt.Errorf("setup: create state dir: %w", err)
		}
	} else if opts.Fragment == "" {
		// Worker/no-route services use the current state/cache/libexec layout. The
		// app-owned state dir is traverse-only for non-owners, and the DB exists
		// with service-private group-readable bits. Workers have no served tree.
		if err := mkdirAll755(
			l.AppDir(), l.BinDir(), l.EtcDir(), l.LibexecDir(), l.CacheDir(), l.shareDir(),
		); err != nil {
			return fmt.Errorf("setup: create app tree: %w", err)
		}
		if err := mkdirAllMode(0o711, l.StateDir()); err != nil {
			return fmt.Errorf("setup: create state dir: %w", err)
		}
		if err := ensureFileMode(l.DBPath(), 0o640); err != nil {
			return fmt.Errorf("setup: create db: %w", err)
		}
		if err := o.System.ChownTree(ctx, app, app, l.StateDir()); err != nil {
			return fmt.Errorf("setup: chown state dir: %w", err)
		}
		if err := o.System.ChownTree(ctx, app, app, l.DBPath()); err != nil {
			return fmt.Errorf("setup: chown db: %w", err)
		}
	} else {
		// Path-routed services still consume the existing fragment-driven setup
		// contract guarded by the provisioning tests.
		if err := mkdirAll755(
			l.AppDir(), l.BinDir(), l.EtcDir(), l.LibexecDir(), l.CacheDir(), l.BackupsDir(),
		); err != nil {
			return fmt.Errorf("setup: create app tree: %w", err)
		}
		if err := os.MkdirAll(l.StateDir(), 0o750); err != nil {
			return fmt.Errorf("setup: create state dir: %w", err)
		}

		// 2b. The OPTIONAL served www/ tree (sites only). state/ is 0750 so
		//     nginx cannot traverse it; sites serves from this web-group subtree
		//     instead. Create each requested dir at 0750, then hand the www ROOT
		//     to <app>:web and setgid the tier dirs so future entries inherit web.
		//     Apps that request none skip this entirely — behavior unchanged.
		if len(opts.WWWDirs) > 0 {
			o.logf("create served www tree for %s", app)
			if err := mkdirAllMode(0o750, opts.WWWDirs...); err != nil {
				return fmt.Errorf("setup: create www tree: %w", err)
			}
			if err := o.ensureWWWPerms(ctx, app, l); err != nil {
				return fmt.Errorf("setup: ensure www perms: %w", err)
			}
		}
	}

	// 3. systemd unit — written to the SysRoot path, then enabled-not-started.
	o.logf("write systemd unit %s", l.UnitPath())
	if err := writeFileAtomic(l.UnitPath(), []byte(unitFile(app)), 0o644); err != nil {
		return fmt.Errorf("setup: write unit: %w", err)
	}
	if err := o.System.DaemonReload(ctx); err != nil {
		return fmt.Errorf("setup: daemon-reload: %w", err)
	}
	if err := o.System.EnableUnit(ctx, app+".service", false); err != nil {
		return fmt.Errorf("setup: enable unit: %w", err)
	}

	// 4. nginx location fragment. Legacy fragment-driven setup still writes the
	// rendered fragment file. The D01 no-fragment setup installs a stable system
	// symlink to the active release's nginx.conf; the target is intentionally
	// dangling until the first deploy creates etc/current.
	if opts.IsDefault {
		o.logf("default app: apex block is owned by init-box/deploy; no nginx conf.d artifact written")
	} else if opts.Fragment != "" {
		frag := renderFragment(opts.Fragment)
		o.logf("write nginx fragment %s", l.FragmentPath())
		if err := writeFileAtomic(l.FragmentPath(), []byte(frag), 0o644); err != nil {
			return fmt.Errorf("setup: write fragment: %w", err)
		}
		if opts.DeferNginx {
			o.logf("defer-nginx: fragment staged; not validating/reloading nginx (cert issued later)")
		} else {
			if err := o.System.NginxTest(ctx); err != nil {
				return fmt.Errorf("setup: nginx -t: %w", err)
			}
			if err := o.System.NginxReload(ctx); err != nil {
				return fmt.Errorf("setup: nginx reload: %w", err)
			}
		}
	} else {
		o.logf("link nginx fragment %s -> %s", l.FragmentPath(), l.ActiveNginxConf())
		if err := os.Remove(l.FragmentPath()); err != nil && !os.IsNotExist(err) {
			return fmt.Errorf("setup: replace fragment link: %w", err)
		}
		if err := os.Symlink(l.ActiveNginxConf(), l.FragmentPath()); err != nil {
			return fmt.Errorf("setup: link fragment: %w", err)
		}
	}

	o.logf("setup complete for %s — next: opsctl stage %s <version> --artifact …, then opsctl deploy %s <version>", app, app, app)
	return nil
}

// WWWDirsFor returns the absolute www-tree dirs setup must create for app under
// root, or nil for apps that need no static tree. Only `sites` opts in: it serves
// from a SEPARATE world-readable www/ tree (the stock state/ is 0750 <app>:<app>,
// untraversable by nginx's www-data). The dirs are ordered parent-first so a
// single mkdir pass suffices: www/ → working/ → public/ → private/.
// The command layer calls this to populate SetupOptions.WWWDirs, so the tree is
// derived per-app — `opsctl setup sites …` provisions it with no operator flag.
func WWWDirsFor(root, app string) []string {
	if app != "sites" {
		return nil
	}
	l := NewLayout(root, app)
	return []string{
		l.WWWRoot(),
		l.WWWWorkingDir(),
		l.WWWPublicDir(),
		l.WWWPrivateDir(),
	}
}

// mkdirAll755 creates each dir with 0755 (the app-tree default).
func mkdirAll755(dirs ...string) error {
	for _, d := range dirs {
		if err := os.MkdirAll(d, 0o755); err != nil {
			return err
		}
	}
	return nil
}

func mkdirAllMode(mode os.FileMode, dirs ...string) error {
	for _, d := range dirs {
		if err := os.MkdirAll(d, mode); err != nil {
			return err
		}
		if err := os.Chmod(d, mode); err != nil {
			return err
		}
	}
	return nil
}

func ensureFileMode(path string, mode os.FileMode) error {
	f, err := os.OpenFile(path, os.O_RDWR|os.O_CREATE, mode)
	if err != nil {
		return err
	}
	if err := f.Close(); err != nil {
		return err
	}
	return os.Chmod(path, mode)
}

// renderFragment normalizes a fragment source to a single trailing newline. The
// fragment's loopback port is already a literal (fixed by the registry), so no
// substitution happens; this only reproduces the old bin/setup byte behavior
// (command substitution stripped the trailing newline, `printf '%s\n'` re-added
// exactly one) with TrimRight + "\n".
func renderFragment(src string) string {
	return strings.TrimRight(src, "\n") + "\n"
}

func stateWWWFragment(l Layout) string {
	return fmt.Sprintf(`location /srv/%[1]s/public/ {
    alias %[2]s/;
}

location /srv/%[1]s/private/ {
    auth_request /_session-authn;
    alias %[3]s/;
}
`, l.App, l.WWWPublicDir(), l.WWWPrivateDir())
}

// LoadFragmentFile reads a fragment source file (the committed etc/nginx.conf) for
// the CLI path; on the box the operator stages it next to the binary. An empty
// path yields an empty fragment (a worker/batch service with no public route).
func LoadFragmentFile(path string) (string, error) {
	if path == "" {
		return "", nil
	}
	b, err := os.ReadFile(filepath.Clean(path))
	if err != nil {
		return "", fmt.Errorf("setup: read fragment %s: %w", path, err)
	}
	return string(b), nil
}
