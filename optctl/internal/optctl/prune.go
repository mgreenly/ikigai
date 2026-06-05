package optctl

import (
	"context"
	"fmt"
	"os"
)

// Prune bounds the on-box release history: keep the N most recent releases
// (Optctl.Keep, default DefaultKeep) plus, unconditionally, current's target and
// the immediate predecessor rollback would target — then delete older
// releases/<version>/ dirs. It NEVER deletes current or its predecessor, even if
// N would otherwise drop them (PLAN §C2, ADR prune). Runs at the tail of install
// and is invocable standalone.
//
// The matching pre-migration backup of a pruned release is removed too, so the
// backups dir does not grow unbounded — but a backup for a still-kept release (in
// particular current's predecessor, the live rollback target) is preserved.
func (o *Optctl) Prune(ctx context.Context, app string) error {
	if app == "" {
		return fmt.Errorf("prune: app is required")
	}
	l := o.layout(app)

	rels, err := o.listReleases(l)
	if err != nil {
		return fmt.Errorf("prune: list releases: %w", err)
	}
	if len(rels) == 0 {
		return nil
	}

	current, err := o.currentVersion(l)
	if err != nil {
		return fmt.Errorf("prune: read current: %w", err)
	}

	keep := o.keepSet(l, rels, current)

	for _, v := range rels {
		if keep[v] {
			continue
		}
		o.logf("prune release %s", v)
		if err := os.RemoveAll(l.ReleaseDir(v)); err != nil {
			return fmt.Errorf("prune: remove release %s: %w", v, err)
		}
		// Drop the matching pre-migration backup for the pruned release.
		if err := os.Remove(l.PreMigrationBackup(v)); err != nil && !os.IsNotExist(err) {
			return fmt.Errorf("prune: remove backup for %s: %w", v, err)
		}
	}
	return nil
}

// keepSet computes the set of release versions prune must retain: the newest N
// (Optctl.keep), plus current's target and the immediate predecessor (the live
// rollback target) regardless of N. rels is sorted ascending by version.
func (o *Optctl) keepSet(l Layout, rels []string, current string) map[string]bool {
	keep := map[string]bool{}
	n := o.keep()

	// Newest N (the tail of the ascending-sorted slice).
	start := len(rels) - n
	if start < 0 {
		start = 0
	}
	for _, v := range rels[start:] {
		keep[v] = true
	}

	// current's target — never deleted.
	if current != "" {
		keep[current] = true
		// The immediate predecessor of current (default rollback target) — never
		// deleted even if N would drop it.
		if prior, err := o.priorRelease(l, current); err == nil && prior != "" {
			keep[prior] = true
		}
	}
	return keep
}
