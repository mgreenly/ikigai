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
	Port     int    // loopback PORT — substituted for __PORT__ in the fragment
	Fragment string // the service's nginx location fragment SOURCE (with __PORT__
	// placeholders), exactly the committed etc/nginx.conf body. Empty ⇒ a service
	// with no public route (worker/batch) — no fragment is dropped.

	// DeferNginx stages the fragment file but skips the `nginx -t` + reload. On a
	// greenfield box nginx is not yet serviceable (the apex 443 cert does not
	// exist until the apex/dashboard deploy issues it), so `nginx -t` would
	// hard-fail. With DeferNginx the fragment is written and validated/reloaded
	// later, when the cert lands and nginx comes up. Mirrors init-box --skip-cert.
	DeferNginx bool
}

// Setup performs first-time, idempotent per-app provisioning, the per-APP half of
// the old bin/setup split out from the box-global init-box (PLAN §D1):
//
//  1. create the dedicated --system app user (seam; idempotent),
//  2. create the /opt/<app> tree (releases/ bin/ etc/ data/ backups/),
//  3. write + enable-NOT-start the systemd unit
//     (ExecStart=/usr/local/bin/ikigenba-launch <app>),
//  4. drop the service's nginx fragment into conf.d/locations/<app>.conf (with
//     __PORT__ substituted), then nginx -t + reload.
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
	app := opts.App
	l := o.layout(app)

	// init-box must have run first: the locations include dir is box-global and
	// owned by init-box, mirroring the old service bin/setup precondition check.
	if _, err := os.Stat(l.LocationsDir()); err != nil {
		return fmt.Errorf("setup: %s missing — run `opsctl init-box` first: %w", l.LocationsDir(), err)
	}

	// 1. App user (seam — never executed in tests).
	o.logf("ensure app user %s (home %s)", app, l.AppDir())
	if err := o.System.EnsureSystemUser(ctx, app, l.AppDir()); err != nil {
		return fmt.Errorf("setup: ensure app user: %w", err)
	}

	// 2. The /opt/<app> tree (PLAN §1.4). data/ is 0750, the rest 0755 — matching
	//    the old bin/setup install -d modes. The DB itself is never created here.
	o.logf("create /opt/%s tree", app)
	if err := mkdirAll755(
		l.AppDir(), l.ReleasesDir(), l.BinDir(), l.EtcDir(), l.BackupsDir(),
	); err != nil {
		return fmt.Errorf("setup: create app tree: %w", err)
	}
	if err := os.MkdirAll(l.DataDir(), 0o750); err != nil {
		return fmt.Errorf("setup: create data dir: %w", err)
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

	// 4. nginx location fragment — substitute __PORT__, write, validate, reload.
	if opts.Fragment != "" {
		frag := renderFragment(opts.Fragment, opts.Port)
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
		o.logf("no nginx fragment (worker/batch service)")
	}

	o.logf("setup complete for %s — next: opsctl stage %s <version> --artifact …, then opsctl deploy %s <version>", app, app, app)
	return nil
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

// renderFragment substitutes __PORT__ with the loopback port, exactly as the old
// bin/setup did: FRAGMENT="$(sed "s/__PORT__/${PORT}/g" etc/nginx.conf)" then
// `printf '%s\n'`. Command substitution strips the source's trailing newline and
// printf re-adds exactly one, so the on-box bytes are the substituted body with a
// single trailing newline — reproduced here with TrimRight + "\n".
func renderFragment(src string, port int) string {
	body := strings.ReplaceAll(src, "__PORT__", fmt.Sprintf("%d", port))
	return strings.TrimRight(body, "\n") + "\n"
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
