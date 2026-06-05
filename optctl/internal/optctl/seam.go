package optctl

import (
	"context"
	"fmt"
	"os/exec"
	"strings"
)

// System is the seam over the box-only platform: systemd unit control, the
// is-active health gate, and the provisioning ops that init-box / setup invoke
// (package install, user creation, unit enable, nginx validate/reload, certbot).
// The real box wiring (C3/D1/D2) plugs in via RealSystem; tests inject a stub so
// the whole package runs against a temp root with no systemd / no root.
// PLAN §C2/§D1: "Systemd/sudo calls behind a seam the tests stub."
//
// The config artifacts these verbs emit (the systemd unit, the nginx apex block
// + fragments, the renew timer) are WRITTEN to SysRoot-rooted paths by optctl
// itself so tests can byte-assert them; only the IMPERATIVE box ops below go
// through this seam, where tests record (but never execute) them.
type System interface {
	// Restart restarts the app's systemd unit (the box runs `systemctl restart
	// <app>`). It is the cutover point after the atomic symlink swap.
	Restart(ctx context.Context, app string) error
	// IsActive returns nil iff the unit is active (the box runs `systemctl
	// is-active <app>`). A non-nil error means the new release failed to come up
	// and the operator's recovery is `optctl rollback`.
	IsActive(ctx context.Context, app string) error

	// InstallPackages installs the named OS packages (the box runs `dnf install
	// -y <pkgs...>`). init-box installs nginx+certbot; setup installs the app's
	// own runtime deps. Idempotent (dnf is a no-op when already present).
	InstallPackages(ctx context.Context, pkgs ...string) error
	// EnsureSystemUser creates the dedicated `--system` app user if absent (the
	// box runs `useradd --system --home-dir /opt/<app> --shell /usr/sbin/nologin
	// <app>`). Idempotent: a no-op when the user already exists.
	EnsureSystemUser(ctx context.Context, app, homeDir string) error
	// ChownTree recursively chowns path to owner:group (the box runs `chown -R
	// <owner>:<group> <path>`). install uses it to hand the data dir back to the
	// `<app>` service user after the root-run migrate, which would otherwise leave
	// a freshly-created DB (+ -wal/-shm + generation file) owned root:root and
	// crash-loop the unit (the service user cannot take a write lock). Idempotent.
	ChownTree(ctx context.Context, owner, group, path string) error
	// DaemonReload reloads systemd's unit cache after a unit file is written (the
	// box runs `systemctl daemon-reload`).
	DaemonReload(ctx context.Context) error
	// EnableUnit enables a unit so it starts on boot WITHOUT starting it now (the
	// box runs `systemctl enable <unit>`). setup enables the app unit
	// enabled-not-started; init-box enables the renew timer with now=true.
	EnableUnit(ctx context.Context, unit string, now bool) error
	// NginxTest validates the nginx config (the box runs `nginx -t`). Called
	// after writing the apex block / a location fragment, before reload.
	NginxTest(ctx context.Context) error
	// NginxReload reloads nginx (the box runs `systemctl reload nginx`), applying
	// a freshly-written apex block or location fragment.
	NginxReload(ctx context.Context) error
	// ObtainCert obtains (or confirms) the apex TLS cert via certbot HTTP-01
	// webroot (the box runs `certbot certonly --webroot …`). init-box only; a
	// path-routed service never issues a cert. Idempotent: certbot reuses a live
	// cert and never re-issues unnecessarily.
	ObtainCert(ctx context.Context, domain, email, webroot string) error
}

// AppRunner is the seam over invoking the app binary's fixed verbs (version |
// manifest | migrate | schema | backup | restore). Tests provide a fake binary
// (a tiny stub) so no real service is needed; the box uses RealRunner, which
// execs the binary with <APP>_DB_PATH / <APP>_GENERATION_PATH pointed at the
// stable data paths.
//
// Run returns the verb's combined-relevant streams split: stdout (the verb's
// data — a version string, a manifest, the `applied=… embedded=…` line) and an
// error carrying stderr on a non-zero exit.
type AppRunner interface {
	// Run executes `<binary> <verb> <args...>` with the given extra environment
	// (KEY=VALUE) layered over the process env, returning the verb's stdout.
	Run(ctx context.Context, binary, verb string, args []string, env []string) (stdout string, err error)
}

// RealSystem drives systemd via systemctl. On the box optctl runs privileged
// (sudo optctl …) so systemctl needs no further escalation here.
type RealSystem struct {
	// Systemctl overrides the binary name/path (default "systemctl"); handy if a
	// box wants an absolute path.
	Systemctl string
}

func (s RealSystem) systemctl() string {
	if s.Systemctl != "" {
		return s.Systemctl
	}
	return "systemctl"
}

func (s RealSystem) Restart(ctx context.Context, app string) error {
	cmd := exec.CommandContext(ctx, s.systemctl(), "restart", app)
	if out, err := cmd.CombinedOutput(); err != nil {
		return fmt.Errorf("systemctl restart %s: %w: %s", app, err, strings.TrimSpace(string(out)))
	}
	return nil
}

func (s RealSystem) IsActive(ctx context.Context, app string) error {
	cmd := exec.CommandContext(ctx, s.systemctl(), "is-active", app)
	out, err := cmd.CombinedOutput()
	if err != nil {
		return fmt.Errorf("systemctl is-active %s: %w: %s", app, err, strings.TrimSpace(string(out)))
	}
	if strings.TrimSpace(string(out)) != "active" {
		return fmt.Errorf("unit %s is not active: %s", app, strings.TrimSpace(string(out)))
	}
	return nil
}

// run is the shared helper for the provisioning ops: exec a command, fold stderr
// into the error on failure.
func run(ctx context.Context, name string, args ...string) error {
	cmd := exec.CommandContext(ctx, name, args...)
	if out, err := cmd.CombinedOutput(); err != nil {
		return fmt.Errorf("%s %s: %w: %s", name, strings.Join(args, " "), err, strings.TrimSpace(string(out)))
	}
	return nil
}

func (s RealSystem) InstallPackages(ctx context.Context, pkgs ...string) error {
	if len(pkgs) == 0 {
		return nil
	}
	return run(ctx, "dnf", append([]string{"install", "-y"}, pkgs...)...)
}

func (s RealSystem) EnsureSystemUser(ctx context.Context, app, homeDir string) error {
	// id <app> succeeds iff the user exists; useradd only when absent (idempotent,
	// matching `id "$APP" &>/dev/null || useradd …` in the old bin/setup).
	if err := exec.CommandContext(ctx, "id", app).Run(); err == nil {
		return nil
	}
	return run(ctx, "useradd", "--system", "--home-dir", homeDir, "--shell", "/usr/sbin/nologin", app)
}

func (s RealSystem) ChownTree(ctx context.Context, owner, group, path string) error {
	return run(ctx, "chown", "-R", owner+":"+group, path)
}

func (s RealSystem) DaemonReload(ctx context.Context) error {
	return run(ctx, s.systemctl(), "daemon-reload")
}

func (s RealSystem) EnableUnit(ctx context.Context, unit string, now bool) error {
	args := []string{"enable"}
	if now {
		args = append(args, "--now")
	}
	args = append(args, unit)
	return run(ctx, s.systemctl(), args...)
}

func (s RealSystem) NginxTest(ctx context.Context) error {
	return run(ctx, "nginx", "-t")
}

func (s RealSystem) NginxReload(ctx context.Context) error {
	return run(ctx, s.systemctl(), "reload", "nginx")
}

func (s RealSystem) ObtainCert(ctx context.Context, domain, email, webroot string) error {
	return run(ctx, "certbot", "certonly", "--webroot", "-w", webroot,
		"-d", domain, "--non-interactive", "--agree-tos", "-m", email,
		"--deploy-hook", "systemctl reload nginx")
}

// RealRunner execs the app binary directly. The binary reads its config from the
// environment (PLAN §1.1), so the env slice carries <APP>_DB_PATH etc.
type RealRunner struct{}

func (RealRunner) Run(ctx context.Context, binary, verb string, args []string, env []string) (string, error) {
	full := append([]string{verb}, args...)
	cmd := exec.CommandContext(ctx, binary, full...)
	if len(env) > 0 {
		// Inherit the process env and layer the caller's overrides on top.
		base := append([]string{}, currentEnv()...)
		cmd.Env = append(base, env...)
	}
	var stdout, stderr strings.Builder
	cmd.Stdout = &stdout
	cmd.Stderr = &stderr
	if err := cmd.Run(); err != nil {
		return stdout.String(), fmt.Errorf("%s %s: %w: %s", binary, verb, err, strings.TrimSpace(stderr.String()))
	}
	return stdout.String(), nil
}
