package opsctl

import (
	"archive/tar"
	"compress/gzip"
	"context"
	"fmt"
	"io"
	"os"
	"path/filepath"
	"strings"
)

// Stage places a new version's bundle tiers WITHOUT making it live.
// It is the first half of the old monolithic install (ADR install steps 1–2):
//
//  1. unpack the tar.gz bundle into a temp tree and preflight its libexec binary.
//  2. SHA collision guard against any already-placed libexec/<app>-<version>.
//  3. place libexec/<app>-<version>, etc/<version>, and share/<version>.
//
// The live symlinks are NEVER touched by stage — those change only at Deploy
// (the cutover), so a staged-but-not-deployed release is invisible to the
// running unit and the dashboard's manifest derivation.
//
// On success — whether the artifact was freshly placed OR the same-SHA build was
// already present (an idempotent no-op) — stage deletes the /tmp artifact (decision
// 2: the release is confirmed in place either way). On a refusal (different-SHA
// collision without --force, a corrupt existing release, a failed preflight) the
// /tmp artifact is LEFT so the operator can retry (e.g. with --force) without a
// re-scp.
func (o *Opsctl) Stage(ctx context.Context, app, version, artifact string, force bool) error {
	if app == "" || version == "" || artifact == "" {
		return fmt.Errorf("stage: app, version, and --artifact are all required")
	}
	if !validVersion(version) {
		return fmt.Errorf("stage: invalid version %q: want %s", version, versionShape)
	}
	l := o.layout(app)

	if err := os.MkdirAll(l.stageScratchParent(), 0o755); err != nil {
		return fmt.Errorf("stage: create temp parent: %w", err)
	}
	tmp, err := os.MkdirTemp(l.stageScratchParent(), "opsctl-stage-*")
	if err != nil {
		return fmt.Errorf("stage: create temp dir: %w", err)
	}
	defer os.RemoveAll(tmp)

	if err := unpackBundle(artifact, tmp); err != nil {
		return err
	}
	tmpBin := filepath.Join(tmp, "libexec", app+"-"+version)
	tmpEtc := filepath.Join(tmp, "etc", version)
	tmpShare := filepath.Join(tmp, "share", version)
	if err := requireBundlePaths(tmpBin, tmpEtc, tmpShare); err != nil {
		return err
	}

	// 1. Preflight — refuse a bad bundled binary before touching the release dir.
	o.logf("preflight %s %s", app, version)
	if err := o.preflight(ctx, tmpBin, app, version); err != nil {
		return err
	}

	relBin := l.LibexecBinary(version)

	// 2. Collision guard: if a binary is already placed for this version, compare
	//    commit SHAs. Same SHA → idempotent no-op (skip the copy). Different SHA, or
	//    the existing binary won't exec (corrupt) → refuse unless --force. On any
	//    refusal the /tmp artifact is kept (we return before the os.Remove below).
	if _, err := os.Stat(relBin); err == nil {
		existing, existErr := o.Runner.Run(ctx, relBin, "version", nil, nil)
		switch {
		case existErr != nil:
			if !force {
				return fmt.Errorf("stage: existing release %s will not exec (corrupt?); re-stage with --force: %w", relBin, existErr)
			}
			o.logf("existing release %s will not exec — --force overrides, replacing", relBin)
		default:
			incoming, err := o.Runner.Run(ctx, tmpBin, "version", nil, nil)
			if err != nil {
				return fmt.Errorf("stage: artifact %q version: %w", app, err)
			}
			existSHA := commitToken(existing)
			incomingSHA := commitToken(incoming)
			if existSHA == incomingSHA {
				// Same commit already staged — confirm in place, delete /tmp, done.
				o.logf("release %s already staged at the same commit %q — idempotent no-op", version, incomingSHA)
				if err := os.Remove(artifact); err != nil && !os.IsNotExist(err) {
					return fmt.Errorf("stage: remove staged artifact: %w", err)
				}
				return nil
			}
			if !force {
				return fmt.Errorf("stage: release %s already staged at commit %q, incoming is %q; re-stage with --force to replace",
					version, existSHA, incomingSHA)
			}
			o.logf("release %s already staged at commit %q — --force overrides, replacing with %q", version, existSHA, incomingSHA)
		}
	}

	// 3. Place the bundled tiers.
	o.logf("place bundle tiers -> %s", l.AppDir())
	if err := os.MkdirAll(l.LibexecDir(), 0o755); err != nil {
		return fmt.Errorf("stage: mkdir libexec dir: %w", err)
	}
	if err := os.MkdirAll(l.EtcDir(), 0o755); err != nil {
		return fmt.Errorf("stage: mkdir etc dir: %w", err)
	}
	if err := os.MkdirAll(l.shareDir(), 0o755); err != nil {
		return fmt.Errorf("stage: mkdir share dir: %w", err)
	}
	if err := replacePath(tmpBin, relBin); err != nil {
		return fmt.Errorf("stage: place libexec binary: %w", err)
	}
	if err := replacePath(tmpEtc, l.EtcVersionDir(version)); err != nil {
		return fmt.Errorf("stage: place etc version dir: %w", err)
	}
	if err := replacePath(tmpShare, l.ShareVersionDir(version)); err != nil {
		return fmt.Errorf("stage: place share version dir: %w", err)
	}

	// On success: delete the /tmp artifact (decision 2). Leave it only on the
	// refusal/error paths above, which return before reaching here.
	if err := os.Remove(artifact); err != nil && !os.IsNotExist(err) {
		return fmt.Errorf("stage: remove staged artifact: %w", err)
	}

	o.logf("staged %s %s", app, version)
	return nil
}

func unpackBundle(artifact, dst string) error {
	f, err := os.Open(artifact)
	if err != nil {
		return fmt.Errorf("stage: open bundle: %w", err)
	}
	defer f.Close()
	gz, err := gzip.NewReader(f)
	if err != nil {
		return fmt.Errorf("stage: artifact is not a valid tar.gz bundle: %w", err)
	}
	defer gz.Close()

	tr := tar.NewReader(gz)
	for {
		hdr, err := tr.Next()
		if err == io.EOF {
			return nil
		}
		if err != nil {
			return fmt.Errorf("stage: read bundle: %w", err)
		}
		name := filepath.Clean(filepath.FromSlash(hdr.Name))
		if name == "." || filepath.IsAbs(name) || strings.HasPrefix(name, ".."+string(filepath.Separator)) || name == ".." {
			return fmt.Errorf("stage: unsafe bundle path %q", hdr.Name)
		}
		target := filepath.Join(dst, name)
		switch hdr.Typeflag {
		case tar.TypeDir:
			if err := os.MkdirAll(target, fileMode(hdr.FileInfo().Mode(), 0o755)); err != nil {
				return fmt.Errorf("stage: create bundle dir %s: %w", hdr.Name, err)
			}
		case tar.TypeReg, tar.TypeRegA:
			if err := os.MkdirAll(filepath.Dir(target), 0o755); err != nil {
				return fmt.Errorf("stage: create parent for %s: %w", hdr.Name, err)
			}
			mode := fileMode(hdr.FileInfo().Mode(), 0o644)
			w, err := os.OpenFile(target, os.O_CREATE|os.O_EXCL|os.O_WRONLY, mode)
			if err != nil {
				return fmt.Errorf("stage: create bundle file %s: %w", hdr.Name, err)
			}
			_, copyErr := io.Copy(w, tr)
			closeErr := w.Close()
			if copyErr != nil {
				return fmt.Errorf("stage: extract bundle file %s: %w", hdr.Name, copyErr)
			}
			if closeErr != nil {
				return fmt.Errorf("stage: close bundle file %s: %w", hdr.Name, closeErr)
			}
			if err := os.Chmod(target, mode); err != nil {
				return fmt.Errorf("stage: chmod bundle file %s: %w", hdr.Name, err)
			}
		default:
			return fmt.Errorf("stage: unsupported bundle entry %s type %c", hdr.Name, hdr.Typeflag)
		}
	}
}

func requireBundlePaths(bin, etc, share string) error {
	if fi, err := os.Stat(bin); err != nil {
		return fmt.Errorf("stage: bundle missing libexec binary %s: %w", bin, err)
	} else if fi.IsDir() {
		return fmt.Errorf("stage: bundle libexec binary %s is a directory", bin)
	}
	for _, path := range []string{
		filepath.Join(etc, "nginx.conf"),
		filepath.Join(etc, "manifest.env"),
		share,
	} {
		if _, err := os.Stat(path); err != nil {
			return fmt.Errorf("stage: bundle missing %s: %w", path, err)
		}
	}
	return nil
}

func replacePath(src, dst string) error {
	if err := os.RemoveAll(dst); err != nil {
		return err
	}
	if err := os.MkdirAll(filepath.Dir(dst), 0o755); err != nil {
		return err
	}
	return os.Rename(src, dst)
}

func fileMode(mode, fallback os.FileMode) os.FileMode {
	perm := mode.Perm()
	if perm == 0 {
		return fallback
	}
	return perm
}

// Deploy makes an already-staged release live:
//
//  1. back up the current live release's state before any mutation.
//  2. migrate (forward-only).
//  3. chown the state tree back to the <app> service user.
//  4. atomically swap bin/run, etc/current, and share/current to the new version.
//  5. reload nginx, then restart the unit + is-active.
//  6. prune old releases.
//
// It refuses early if the release was never staged (libexec/<app>-<version> is
// absent) so the migrate exec never fails opaquely. state/<app>.db is NEVER
// overwritten — only read/migrated/snapshotted (PLAN §2.7). The live-facing
// symlinks stay valid throughout (PLAN §2.6).
func (o *Opsctl) Deploy(ctx context.Context, app, version string) error {
	if app == "" || version == "" {
		return fmt.Errorf("deploy: app and version are both required")
	}
	if !validVersion(version) {
		return fmt.Errorf("deploy: invalid version %q: want %s", version, versionShape)
	}
	l := o.layout(app)
	relBin := l.LibexecBinary(version)

	// Guard: the release must have been staged first. Refuse before any exec so a
	// missing stage fails loudly instead of through an opaque manifest/migrate error.
	if _, err := os.Stat(relBin); err != nil {
		return fmt.Errorf("deploy: release %s is not staged (run: opsctl stage %s %s --artifact …): %w", version, app, version, err)
	}

	current := ""
	if fi, err := os.Lstat(l.RunLink()); err == nil {
		if fi.Mode()&os.ModeSymlink != 0 {
			current, err = o.currentVersion(l)
			if err != nil {
				return fmt.Errorf("deploy: read current: %w", err)
			}
		}
	} else if !os.IsNotExist(err) {
		return fmt.Errorf("deploy: read current: %w", err)
	}
	if current != "" {
		o.logf("backup %s before deploy", app)
		if err := o.Backup(ctx, app); err != nil {
			return fmt.Errorf("deploy: backup: %w", err)
		}
	}

	// 2. Migrate (forward-only). Idempotent: applies only unapplied higher versions.
	//    Ensure state/ exists so the app can create state/<app>.db on first migrate
	//    (setup creates this tree on the box; deploy self-heals it). Creating the
	//    directory never touches an existing DB file (PLAN §2.7).
	if err := os.MkdirAll(l.StateDir(), 0o755); err != nil {
		return fmt.Errorf("deploy: mkdir state dir: %w", err)
	}
	o.logf("migrate %s", app)
	if _, err := o.Runner.Run(ctx, relBin, "migrate", nil, o.dbEnv(l)); err != nil {
		return fmt.Errorf("deploy: migrate: %w", err)
	}

	// opsctl runs privileged (sudo opsctl …), so the root-run migrate above creates
	// any FRESH DB (+ its -wal/-shm and the generation sidecar) owned root:root.
	// The unit runs as the dedicated `<app>` system user (opsctl setup's
	// EnsureSystemUser → `useradd --system <app>`, which also makes the matching
	// `<app>` group), so a root-owned DB leaves the service unable to take a write
	// lock — e.g. crm's event-plane outbox single-writer probe (`BEGIN IMMEDIATE`)
	// fails and the unit crash-loops. Hand the whole state tree back to <app>:<app>
	// before the swap/restart. Unconditional + idempotent on every deploy: it also
	// reclaims root-owned -wal/-shm files migrate may create against an EXISTING DB.
	// The user/group is the bare app name to match setup exactly (EnsureSystemUser).
	o.logf("chown %s -> %s:%s", l.StateDir(), app, app)
	if err := o.System.ChownTree(ctx, app, app, l.StateDir()); err != nil {
		return fmt.Errorf("deploy: chown state dir: %w", err)
	}

	// 4. Atomic swap: all live-facing symlinks repoint to the same staged version.
	//    Each symlink only ever points at a complete target: the temp symlink is
	//    fully formed before rename(2) moves it into place.
	o.logf("atomic swap bin/run -> libexec/%s-%s", app, version)
	if err := atomicSwap(l.RunLink(), l.runTarget(version)); err != nil {
		return fmt.Errorf("deploy: swap bin/run: %w", err)
	}
	o.logf("atomic swap etc/current -> %s", version)
	if err := atomicSwap(l.EtcCurrentLink(), version); err != nil {
		return fmt.Errorf("deploy: swap etc/current: %w", err)
	}
	o.logf("atomic swap share/current -> %s", version)
	if err := atomicSwap(l.ShareCurrentLink(), version); err != nil {
		return fmt.Errorf("deploy: swap share/current: %w", err)
	}

	// 5. Reload nginx after the config symlink has moved and before the service
	//    restart makes the new binary live.
	o.logf("reload nginx")
	if err := o.System.NginxReload(ctx); err != nil {
		return fmt.Errorf("deploy: nginx reload: %w", err)
	}

	// Restart + is-active. On failure the operator's recovery is `rollback`
	//    (the prior release dir + pre-migration backup are intact).
	o.logf("restart %s", app)
	if err := o.System.Restart(ctx, app); err != nil {
		return fmt.Errorf("deploy: restart: %w", err)
	}
	if err := o.System.IsActive(ctx, app); err != nil {
		return fmt.Errorf("deploy: %s did not come up (recover with: opsctl rollback %s): %w", app, app, err)
	}

	// 6. Prune old releases (never current's target / its predecessor).
	if err := o.Prune(ctx, app); err != nil {
		return fmt.Errorf("deploy: prune: %w", err)
	}

	o.logf("deployed %s %s", app, version)
	return nil
}

// copyExecutable copies src to dst (0755) via temp+rename so the destination is
// never a partial binary, then makes it executable. It refuses to clobber the DB
// path by construction (callers only ever pass a release-binary dst).
func copyExecutable(src, dst string) error {
	return copyFileMode(src, dst, 0o755)
}

func copyFileMode(src, dst string, mode os.FileMode) error {
	in, err := os.Open(src)
	if err != nil {
		return err
	}
	defer in.Close()
	if err := os.MkdirAll(filepath.Dir(dst), 0o755); err != nil {
		return err
	}
	tmp := dst + ".tmp"
	out, err := os.OpenFile(tmp, os.O_WRONLY|os.O_CREATE|os.O_TRUNC, mode)
	if err != nil {
		return err
	}
	if _, err := io.Copy(out, in); err != nil {
		out.Close()
		os.Remove(tmp)
		return err
	}
	if err := out.Close(); err != nil {
		os.Remove(tmp)
		return err
	}
	if err := os.Chmod(tmp, mode); err != nil {
		os.Remove(tmp)
		return err
	}
	if err := os.Rename(tmp, dst); err != nil {
		os.Remove(tmp)
		return err
	}
	return nil
}

// writeFileAtomic writes data to path via temp+rename, so a reader (the dashboard
// derivation, bin/registry) never observes a partial manifest.env mid-write.
func writeFileAtomic(path string, data []byte, mode os.FileMode) error {
	if err := os.MkdirAll(filepath.Dir(path), 0o755); err != nil {
		return err
	}
	tmp := path + ".tmp"
	if err := os.WriteFile(tmp, data, mode); err != nil {
		return err
	}
	if err := os.Rename(tmp, path); err != nil {
		os.Remove(tmp)
		return err
	}
	return nil
}
