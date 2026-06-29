package opsctl

import (
	"context"
	"fmt"
	"os"
	"path/filepath"
	"sort"
)

// Rollback repoints `current` at a prior release and restarts, following the ADR
// rollback sequence:
//
//  1. resolve the target — the immediately-prior release by default, or an
//     explicit older <version> that still exists under releases/.
//  2. restore the DB FIRST iff the release being rolled back FROM advanced the
//     schema (its pre-migration backup exists). The forward-only downgrade guard
//     refuses to boot a DB whose recorded version exceeds the older binary's
//     embedded max, so without restoring the matching snapshot the rolled-back
//     binary would crash on start (PLAN §2.5, §1.4).
//  3. atomic swap current → the target release.
//  4. restart + is-active.
//
// data/<app>.db is touched only by the explicit restore in step 2 (PLAN §2.7).
func (o *Opsctl) Rollback(ctx context.Context, app, target string) error {
	if app == "" {
		return fmt.Errorf("rollback: app is required")
	}
	if target != "" && !validVersion(target) {
		return fmt.Errorf("rollback: invalid target version %q: want %s", target, versionShape)
	}
	l := o.layout(app)

	from, err := o.currentVersion(l)
	if err != nil {
		return fmt.Errorf("rollback: read current: %w", err)
	}
	if from == "" {
		return fmt.Errorf("rollback: %s has no current release to roll back from", app)
	}

	// 1. Resolve the target release.
	if target == "" {
		target, err = o.priorRelease(l, from)
		if err != nil {
			return err
		}
	} else if _, statErr := os.Stat(l.ReleaseDir(target)); statErr != nil {
		return fmt.Errorf("rollback: target release %q does not exist under %s", target, l.ReleasesDir())
	}
	if target == from {
		return fmt.Errorf("rollback: target %q is already current; nothing to do", target)
	}

	// 2. Restore the DB FIRST iff rolling back FROM a schema-advancing release.
	//    The pre-migration backup keyed by the FROM-version exists exactly when
	//    that install advanced the schema.
	backup := l.PreMigrationBackup(from)
	if _, statErr := os.Stat(backup); statErr == nil {
		o.logf("rolling back from schema-advancing %s — restore DB from %s", from, backup)
		// Stop the unit before a file-level DB restore so no writer is mid-flight.
		// (Restart after the swap brings it back up on the restored DB.) We stop via
		// restart-after-swap; here we drive restore through the TARGET binary so a
		// producer re-mints its event-plane generation (Spec.Restore).
		targetBin := l.ReleaseBinary(target)
		if _, err := o.Runner.Run(ctx, targetBin, "restore",
			[]string{"--from", backup}, o.dbEnv(l)); err != nil {
			return fmt.Errorf("rollback: restore DB: %w", err)
		}
	}

	// 3. Atomic swap current → target.
	o.logf("atomic swap current -> releases/%s", target)
	if err := atomicSwap(l.CurrentLink(), filepath.Join("releases", target)); err != nil {
		return fmt.Errorf("rollback: %w", err)
	}

	// 4. Restart + is-active.
	o.logf("restart %s", app)
	if err := o.System.Restart(ctx, app); err != nil {
		return fmt.Errorf("rollback: restart: %w", err)
	}
	if err := o.System.IsActive(ctx, app); err != nil {
		return fmt.Errorf("rollback: %s did not come up on %s: %w", app, target, err)
	}

	o.logf("rolled back %s: %s -> %s", app, from, target)
	return nil
}

// currentVersion returns the version `current` points at (the basename of its
// target), or "" if current does not exist.
func (o *Opsctl) currentVersion(l Layout) (string, error) {
	dst, err := os.Readlink(l.CurrentLink())
	if err != nil {
		if os.IsNotExist(err) {
			return "", nil
		}
		return "", err
	}
	v := filepath.Base(dst)
	if !validVersion(v) {
		return "", fmt.Errorf("invalid current version %q: want %s", v, versionShape)
	}
	return v, nil
}

// priorRelease returns the release immediately preceding `from` in sorted order —
// the default rollback target. It lists releases/, sorts by semantic version,
// and picks the highest that is strictly less than `from`.
func (o *Opsctl) priorRelease(l Layout, from string) (string, error) {
	rels, err := o.listReleases(l)
	if err != nil {
		return "", err
	}
	idx := -1
	for i, v := range rels {
		if v == from {
			idx = i
			break
		}
	}
	if idx <= 0 {
		return "", fmt.Errorf("rollback: no prior release before %q (have %v)", from, rels)
	}
	return rels[idx-1], nil
}

// listReleases returns the version dir names under releases/, sorted ascending by
// semantic version (so the last is the newest, and priorRelease can index back).
func (o *Opsctl) listReleases(l Layout) ([]string, error) {
	entries, err := os.ReadDir(l.ReleasesDir())
	if err != nil {
		if os.IsNotExist(err) {
			return nil, nil
		}
		return nil, err
	}
	var out []string
	for _, e := range entries {
		if e.IsDir() {
			v := e.Name()
			if !validVersion(v) {
				return nil, fmt.Errorf("invalid release version %q under %s: want %s", v, l.ReleasesDir(), versionShape)
			}
			out = append(out, v)
		}
	}
	sort.SliceStable(out, func(i, j int) bool { return l.compareVersion(out[i], out[j]) < 0 })
	return out, nil
}
