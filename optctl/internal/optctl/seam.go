package optctl

import (
	"context"
	"fmt"
	"os/exec"
	"strings"
)

// System is the seam over the box-only platform: systemd unit control and the
// is-active health gate. The real box wiring (C3/D1/D2) plugs in via RealSystem;
// tests inject a stub so the whole package runs against a temp root with no
// systemd. PLAN §C2: "Systemd/sudo calls behind a seam the tests stub."
type System interface {
	// Restart restarts the app's systemd unit (the box runs `systemctl restart
	// <app>`). It is the cutover point after the atomic symlink swap.
	Restart(ctx context.Context, app string) error
	// IsActive returns nil iff the unit is active (the box runs `systemctl
	// is-active <app>`). A non-nil error means the new release failed to come up
	// and the operator's recovery is `optctl rollback`.
	IsActive(ctx context.Context, app string) error
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
