package opsctl

import (
	"context"
	"fmt"
	"os"
	"text/tabwriter"
)

// Status reports, for one app or every installed app, the live release and its
// health: the version `bin/run` points at, the commit SHA that release's binary
// self-reports (via its `version` verb), and the systemd unit's raw state (via
// IsActiveState — the state string regardless of exit code, so a "failed" or
// "inactive" unit is shown rather than turned into an error).
//
// An empty app iterates the discovery helper (every OPSCTL_ROOT child with a
// `bin/run` symlink). The output is a simple aligned table: app · version · sha ·
// active. It is read-only — nothing on the box changes.
func (o *Opsctl) Status(ctx context.Context, app string) error {
	var apps []string
	if app != "" {
		apps = []string{app}
	} else {
		var err error
		apps, err = o.discoverApps()
		if err != nil {
			return fmt.Errorf("status: discover apps: %w", err)
		}
	}

	w := o.Out
	if w == nil {
		w = os.Stdout
	}
	tw := tabwriter.NewWriter(w, 0, 0, 2, ' ', 0)
	fmt.Fprintln(tw, "APP\tVERSION\tSHA\tACTIVE")
	for _, a := range apps {
		version, sha, active, err := o.appStatus(ctx, a)
		if err != nil {
			return fmt.Errorf("status: %s: %w", a, err)
		}
		fmt.Fprintf(tw, "%s\t%s\t%s\t%s\n", a, version, sha, active)
	}
	return tw.Flush()
}

// appStatus gathers one app's status row: the live version (bin/run's target
// basename, "-" if absent), the SHA the live binary self-reports
// ("-" if it has no current release or won't exec), and the raw systemd state.
func (o *Opsctl) appStatus(ctx context.Context, app string) (version, sha, active string, err error) {
	l := o.layout(app)

	version = "-"
	sha = "-"
	v, err := o.currentVersion(l)
	if err != nil {
		return "", "", "", err
	}
	if v != "" {
		version = v
		// Self-reported commit SHA from the live binary's `version` verb. Best
		// effort: a missing/corrupt binary just leaves sha as "-".
		if out, err := o.Runner.Run(ctx, l.RunLink(), "version", nil, nil); err == nil {
			if c := commitToken(out); c != "" {
				sha = c
			}
		}
	}

	active = "unknown"
	if state, err := o.System.IsActiveState(ctx, app); err == nil {
		active = state
	}
	return version, sha, active, nil
}
