package opsctl

import (
	"context"
	"fmt"
	"os"
	"path/filepath"
	"sort"
	"strconv"
	"strings"
	"time"
)

const snapshotTimeShape = "20060102T150405.000000000Z"

// Rollback restores an S3 snapshot selected by recency, then repoints the three
// stable D02 symlinks at the version embedded in that snapshot key.
func (o *Opsctl) Rollback(ctx context.Context, app, target string) error {
	if app == "" {
		return fmt.Errorf("rollback: app is required")
	}
	offset, err := rollbackOffset(target)
	if err != nil {
		return err
	}
	l := o.layout(app)

	from, err := o.currentVersion(l)
	if err != nil {
		return fmt.Errorf("rollback: read current: %w", err)
	}
	if from == "" {
		return fmt.Errorf("rollback: %s has no current release to roll back from", app)
	}

	snap, err := o.rollbackSnapshot(ctx, app, offset)
	if err != nil {
		return err
	}
	targetBin := l.LibexecBinary(snap.version)
	if _, err := os.Stat(targetBin); err != nil {
		return fmt.Errorf("rollback: target release %q does not exist under %s: %w", snap.version, l.LibexecDir(), err)
	}

	archive, err := o.downloadRollbackSnapshot(ctx, snap.key)
	if err != nil {
		return err
	}
	defer os.Remove(archive)

	o.logf("restore %s state from %s", app, snap.key)
	if err := replaceStateFromArchive(ctx, l, archive); err != nil {
		return fmt.Errorf("rollback: restore state: %w", err)
	}
	if err := os.MkdirAll(l.CacheDir(), 0o755); err != nil {
		return fmt.Errorf("rollback: mkdir cache: %w", err)
	}
	if err := o.System.ChownTree(ctx, app, app, l.CacheDir()); err != nil {
		return fmt.Errorf("rollback: chown cache: %w", err)
	}

	if err := o.checkRollbackSchema(ctx, l, targetBin); err != nil {
		return err
	}

	o.logf("atomic swap %s -> %s", app, snap.version)
	if err := atomicSwap(l.RunLink(), l.runTarget(snap.version)); err != nil {
		return fmt.Errorf("rollback: %w", err)
	}
	if err := atomicSwap(l.EtcCurrentLink(), snap.version); err != nil {
		return fmt.Errorf("rollback: %w", err)
	}
	if err := atomicSwap(l.ShareCurrentLink(), snap.version); err != nil {
		return fmt.Errorf("rollback: %w", err)
	}

	o.logf("restart %s", app)
	if err := o.System.Restart(ctx, app); err != nil {
		return fmt.Errorf("rollback: restart: %w", err)
	}
	if err := o.System.IsActive(ctx, app); err != nil {
		return fmt.Errorf("rollback: %s did not come up on %s: %w", app, snap.version, err)
	}

	o.logf("rolled back %s: %s -> %s", app, from, snap.version)
	return nil
}

type rollbackSnapshot struct {
	key     string
	version string
	at      time.Time
}

func rollbackOffset(target string) (int, error) {
	if target == "" {
		return 0, nil
	}
	if validVersion(target) {
		return 0, fmt.Errorf("rollback: explicit target version %q is no longer supported; use -N snapshot recency", target)
	}
	if !strings.HasPrefix(target, "-") {
		return 0, fmt.Errorf("rollback: invalid target version %q: want %s", target, versionShape)
	}
	n, err := strconv.Atoi(strings.TrimPrefix(target, "-"))
	if err != nil || n < 0 {
		return 0, fmt.Errorf("rollback: invalid snapshot offset %q: want -N", target)
	}
	return n, nil
}

func (o *Opsctl) rollbackSnapshot(ctx context.Context, app string, offset int) (rollbackSnapshot, error) {
	infos, err := o.objectStore().List(ctx, snapshotPrefix(app))
	if err != nil {
		return rollbackSnapshot{}, fmt.Errorf("rollback: list snapshots: %w", err)
	}
	snaps := make([]rollbackSnapshot, 0, len(infos))
	for _, info := range infos {
		snap, err := parseRollbackSnapshotKey(app, info.Key)
		if err != nil {
			return rollbackSnapshot{}, err
		}
		snaps = append(snaps, snap)
	}
	sort.SliceStable(snaps, func(i, j int) bool { return snaps[i].at.After(snaps[j].at) })
	if offset >= len(snaps) {
		return rollbackSnapshot{}, fmt.Errorf("rollback: snapshot -%d not found for %s (have %d)", offset, app, len(snaps))
	}
	return snaps[offset], nil
}

func parseRollbackSnapshotKey(app, key string) (rollbackSnapshot, error) {
	prefix := snapshotPrefix(app)
	if !strings.HasPrefix(key, prefix) {
		return rollbackSnapshot{}, fmt.Errorf("rollback: snapshot key %q does not match prefix %q", key, prefix)
	}
	body := strings.TrimPrefix(key, prefix)
	body, ok := strings.CutSuffix(body, ".tar.gz")
	if !ok {
		return rollbackSnapshot{}, fmt.Errorf("rollback: invalid snapshot key %q: missing .tar.gz", key)
	}
	if len(body) <= len(snapshotTimeShape)+1 || body[len(body)-len(snapshotTimeShape)-1] != '.' {
		return rollbackSnapshot{}, fmt.Errorf("rollback: invalid snapshot key %q: missing timestamp", key)
	}
	version := body[:len(body)-len(snapshotTimeShape)-1]
	stamp := body[len(body)-len(snapshotTimeShape):]
	if !validVersion(version) {
		return rollbackSnapshot{}, fmt.Errorf("rollback: invalid snapshot version %q in %s: want %s", version, key, versionShape)
	}
	at, err := time.Parse(snapshotTimeShape, stamp)
	if err != nil {
		return rollbackSnapshot{}, fmt.Errorf("rollback: invalid snapshot timestamp %q in %s: %w", stamp, key, err)
	}
	return rollbackSnapshot{key: key, version: version, at: at}, nil
}

func (o *Opsctl) downloadRollbackSnapshot(ctx context.Context, key string) (string, error) {
	f, err := os.CreateTemp("", "opsctl-rollback-*.tar.gz")
	if err != nil {
		return "", fmt.Errorf("rollback: create snapshot archive: %w", err)
	}
	archive := f.Name()
	defer f.Close()
	if err := o.objectStore().Get(ctx, key, f); err != nil {
		os.Remove(archive)
		return "", fmt.Errorf("rollback: download snapshot %s: %w", key, err)
	}
	return archive, nil
}

func (o *Opsctl) checkRollbackSchema(ctx context.Context, l Layout, targetBin string) error {
	out, err := o.Runner.Run(ctx, targetBin, "schema", nil, o.dbEnv(l))
	if err != nil {
		return fmt.Errorf("rollback: schema check: %w", err)
	}
	applied, embedded, err := parseSchemaReport(out)
	if err != nil {
		return fmt.Errorf("rollback: schema check: %w", err)
	}
	if applied > embedded {
		return fmt.Errorf("rollback: restored schema applied=%d exceeds target embedded=%d", applied, embedded)
	}
	return nil
}

func parseSchemaReport(out string) (int, int, error) {
	fields := strings.Fields(out)
	values := map[string]int{}
	for _, field := range fields {
		name, raw, ok := strings.Cut(field, "=")
		if !ok {
			continue
		}
		if name != "applied" && name != "embedded" {
			continue
		}
		n, err := strconv.Atoi(raw)
		if err != nil {
			return 0, 0, fmt.Errorf("parse %s=%q: %w", name, raw, err)
		}
		values[name] = n
	}
	applied, haveApplied := values["applied"]
	embedded, haveEmbedded := values["embedded"]
	if !haveApplied || !haveEmbedded {
		return 0, 0, fmt.Errorf("missing applied/embedded in %q", strings.TrimSpace(out))
	}
	return applied, embedded, nil
}

// currentVersion returns the version `bin/run` points at (parsed from the
// <app>-<version> basename), or "" if bin/run does not exist.
func (o *Opsctl) currentVersion(l Layout) (string, error) {
	dst, err := os.Readlink(l.RunLink())
	if err != nil {
		if os.IsNotExist(err) {
			return "", nil
		}
		return "", err
	}
	base := filepath.Base(dst)
	prefix := l.App + "-"
	if !strings.HasPrefix(base, prefix) {
		return "", fmt.Errorf("invalid run target %q: want %s-vMAJOR.MINOR.PATCH", base, l.App)
	}
	v := strings.TrimPrefix(base, prefix)
	if !validVersion(v) {
		return "", fmt.Errorf("invalid current version %q: want %s", v, versionShape)
	}
	return v, nil
}

// listReleases returns versions found under libexec/, sorted ascending by
// semantic version.
func (o *Opsctl) listReleases(l Layout) ([]string, error) {
	entries, err := os.ReadDir(l.LibexecDir())
	if err != nil {
		if os.IsNotExist(err) {
			return nil, nil
		}
		return nil, err
	}
	var out []string
	prefix := l.App + "-"
	for _, e := range entries {
		if e.IsDir() || !strings.HasPrefix(e.Name(), prefix) {
			continue
		}
		v := strings.TrimPrefix(e.Name(), prefix)
		if !validVersion(v) {
			return nil, fmt.Errorf("invalid release version %q under %s: want %s", v, l.LibexecDir(), versionShape)
		}
		out = append(out, v)
	}
	sort.SliceStable(out, func(i, j int) bool { return l.compareVersion(out[i], out[j]) < 0 })
	return out, nil
}

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
		return "", fmt.Errorf("no prior release before %q (have %v)", from, rels)
	}
	return rels[idx-1], nil
}
