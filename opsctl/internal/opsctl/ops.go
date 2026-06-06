package opsctl

import "context"

// followFlags are the args that already carry their own follow/range intent, so
// `tail` must NOT inject its default `-f`. Passing any of these means the
// operator has chosen the journalctl window explicitly: `-f` (follow), `-n`
// (last N lines), `--since` (a time range), or `--no-tail` (an opsctl-only knob
// meaning "no follow" — stripped before the journalctl call so it never leaks to
// journalctl, which does not know it).
var followFlags = map[string]bool{
	"-f": true, "-n": true, "--since": true, "--no-tail": true,
}

// Tail streams the app's systemd journal. With no extra args it follows live
// (`journalctl -u <app> -f`); the default `-f` is suppressed when the operator's
// args already carry a follow/range flag (-f/-n/--since/--no-tail). `--no-tail`
// is an opsctl-only token (turn off the default follow) and is dropped before
// the call so journalctl never sees it.
func (o *Opsctl) Tail(ctx context.Context, app string, args []string) error {
	jargs := []string{"-u", app}

	suppress := false
	for _, a := range args {
		if followFlags[a] {
			suppress = true
			break
		}
	}

	forwarded := make([]string, 0, len(args))
	for _, a := range args {
		if a == "--no-tail" {
			continue // opsctl-only knob; never forwarded to journalctl
		}
		forwarded = append(forwarded, a)
	}

	if !suppress {
		jargs = append(jargs, "-f")
	}
	jargs = append(jargs, forwarded...)
	return o.System.Journalctl(ctx, jargs...)
}

// Start/Stop/Restart/Enable/Disable are thin passthroughs to `systemctl <verb>
// <app> [extra…]`. extra args are forwarded verbatim (the dispatcher does NOT
// reorder them) so operator-supplied systemctl flags reach systemd unscrambled.
func (o *Opsctl) Start(ctx context.Context, app string, extra []string) error {
	return o.systemctl(ctx, "start", app, extra)
}

func (o *Opsctl) Stop(ctx context.Context, app string, extra []string) error {
	return o.systemctl(ctx, "stop", app, extra)
}

// Restartd is the operator-facing `restart` passthrough. (The deploy cutover's
// own restart is System.Restart — distinct from this passthrough verb.)
func (o *Opsctl) Restartd(ctx context.Context, app string, extra []string) error {
	return o.systemctl(ctx, "restart", app, extra)
}

func (o *Opsctl) Enable(ctx context.Context, app string, extra []string) error {
	return o.systemctl(ctx, "enable", app, extra)
}

func (o *Opsctl) Disable(ctx context.Context, app string, extra []string) error {
	return o.systemctl(ctx, "disable", app, extra)
}

func (o *Opsctl) systemctl(ctx context.Context, verb, app string, extra []string) error {
	args := append([]string{verb, app}, extra...)
	return o.System.Systemctl(ctx, args...)
}
