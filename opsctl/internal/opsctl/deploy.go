package opsctl

import (
	"context"
	"fmt"
	"io"
	"os"
	"path/filepath"
	"strings"
)

// Stage places a new version into releases/<version>/<app> WITHOUT making it live.
// It is the first half of the old monolithic install (ADR install steps 1–2):
//
//  1. preflight (static? amd64? version matches? manifest parses?) — refuse early.
//  2. SHA collision guard against any already-placed releases/<version>/<app>.
//  3. place the artifact into releases/<version>/<app>.
//
// The stable etc/manifest.env and the live `current` symlink are NEVER touched by
// stage — those change only at Deploy (the cutover), so a staged-but-not-deployed
// release is invisible to the running unit and the dashboard's manifest derivation.
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
	l := o.layout(app)

	// 1. Preflight — refuse a bad artifact before touching the release dir.
	o.logf("preflight %s %s", app, version)
	if err := o.preflight(ctx, artifact, app, version); err != nil {
		return err
	}

	relBin := l.ReleaseBinary(version)

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
			incoming, err := o.Runner.Run(ctx, artifact, "version", nil, nil)
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

	// 3. Place the artifact into releases/<version>/<app>.
	o.logf("place artifact -> %s", relBin)
	if err := os.MkdirAll(l.ReleaseDir(version), 0o755); err != nil {
		return fmt.Errorf("stage: mkdir release dir: %w", err)
	}
	if err := copyExecutable(artifact, relBin); err != nil {
		return fmt.Errorf("stage: place artifact: %w", err)
	}

	// On success: delete the /tmp artifact (decision 2). Leave it only on the
	// refusal/error paths above, which return before reaching here.
	if err := os.Remove(artifact); err != nil && !os.IsNotExist(err) {
		return fmt.Errorf("stage: remove staged artifact: %w", err)
	}

	o.logf("staged %s %s", app, version)
	return nil
}

// Deploy makes an already-staged release live, following the second half of the
// old monolithic install (ADR install steps 3–8):
//
//  3. regenerate etc/manifest.env via `<new binary> manifest` (the STABLE path the
//     dashboard derivation + bin/registry read).
//  4. back up data/<app>.db IF the schema will advance (applied < embedded), keyed
//     by <version> so the matching rollback can restore it.
//  5. migrate (forward-only).
//  6. chown the data tree back to the <app> service user.
//  7. ensure the stable bin/run link, then atomic swap current → releases/<version>.
//  8. restart the unit + is-active, then prune old releases.
//
// It refuses early if the release was never staged (releases/<version>/<app> is
// absent) so the manifest/schema/migrate execs never fail opaquely. data/<app>.db
// is NEVER overwritten — only read/migrated/snapshotted (PLAN §2.7). bin/run and
// etc/manifest.env stay valid throughout (PLAN §2.6).
func (o *Opsctl) Deploy(ctx context.Context, app, version string) error {
	if app == "" || version == "" {
		return fmt.Errorf("deploy: app and version are both required")
	}
	l := o.layout(app)
	relBin := l.ReleaseBinary(version)

	// Guard: the release must have been staged first. Refuse before any exec so a
	// missing stage fails loudly instead of through an opaque manifest/migrate error.
	if _, err := os.Stat(relBin); err != nil {
		return fmt.Errorf("deploy: release %s is not staged (run: opsctl stage %s %s --artifact …): %w", version, app, version, err)
	}

	// 3. Regenerate the stable manifest from the binary that will serve it. Written
	//    via temp+rename so the stable path never holds a partial file mid-write.
	//    The binary emits a PORTABLE manifest (no box paths baked in — appkit's
	//    config defaults the DB to a relative ./tmp/<app>.db suited to local dev).
	//    metaspot-launch sources this manifest and exports every KEY into the
	//    serving process's env (AGENTS.md "Service layer"), but it never sets the
	//    on-box <APP>_DB_PATH/<APP>_GENERATION_PATH — so the serving binary would
	//    otherwise fall back to the relative dev default and miss the real DB at
	//    data/<app>.db. opsctl is the on-box authority that owns the absolute /opt
	//    paths (it already injects them for its own migrate/schema/backup verbs via
	//    dbEnv), so it stamps them into the stable manifest here. Idempotent: the
	//    keys are only appended if the binary's own manifest did not already carry
	//    them. This is the FIRST live-facing change, bound to the cutover below.
	o.logf("regenerate %s", l.ManifestPath())
	manOut, err := o.Runner.Run(ctx, relBin, "manifest", nil, nil)
	if err != nil {
		return fmt.Errorf("deploy: manifest: %w", err)
	}
	manOut = stampDataPaths(manOut, l)
	if err := writeFileAtomic(l.ManifestPath(), []byte(manOut), 0o644); err != nil {
		return fmt.Errorf("deploy: write manifest: %w", err)
	}

	// 4. Back up the DB IF the schema advances. The new binary reports applied (the
	//    live DB) vs embedded (what it carries); embedded > applied ⇒ this deploy
	//    advances the schema, so snapshot first to make the matching rollback safe.
	advances, err := o.schemaAdvances(ctx, l, relBin)
	if err != nil {
		return err
	}
	if advances {
		if _, err := os.Stat(l.DBPath()); err == nil {
			backup := l.PreMigrationBackup(version)
			o.logf("schema advances — backup %s -> %s", l.DBPath(), backup)
			if err := os.MkdirAll(l.BackupsDir(), 0o755); err != nil {
				return fmt.Errorf("deploy: mkdir backups: %w", err)
			}
			if _, err := o.Runner.Run(ctx, relBin, "backup",
				[]string{"--out", backup}, o.dbEnv(l)); err != nil {
				return fmt.Errorf("deploy: pre-migration backup: %w", err)
			}
		} else {
			// No DB yet (first deploy of a brand-new app): nothing to back up, the
			// migrate below creates it. The schema-advance flag is still true, but the
			// rollback target for a first release does not exist, so no backup needed.
			o.logf("schema advances but no DB yet (first deploy) — no backup")
		}
	}

	// 5. Migrate (forward-only). Idempotent: applies only unapplied higher versions.
	//    Ensure data/ exists so the app can create data/<app>.db on first migrate
	//    (setup creates this tree on the box; deploy self-heals it). Creating the
	//    directory never touches an existing DB file (PLAN §2.7).
	if err := os.MkdirAll(l.DataDir(), 0o755); err != nil {
		return fmt.Errorf("deploy: mkdir data dir: %w", err)
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
	// fails and the unit crash-loops. Hand the whole data tree back to <app>:<app>
	// before the swap/restart. Unconditional + idempotent on every deploy: it also
	// reclaims root-owned -wal/-shm files migrate may create against an EXISTING DB.
	// The user/group is the bare app name to match setup exactly (EnsureSystemUser).
	o.logf("chown %s -> %s:%s", l.DataDir(), app, app)
	if err := o.System.ChownTree(ctx, app, app, l.DataDir()); err != nil {
		return fmt.Errorf("deploy: chown data dir: %w", err)
	}

	// Ensure the stable bin/run -> ../current/<app> link exists (self-heal). This
	// is set at setup; deploy never repoints it (the cutover is `current`).
	if err := ensureSymlink(l.RunLink(), l.RunTarget()); err != nil {
		return fmt.Errorf("deploy: ensure bin/run: %w", err)
	}

	// 6. Atomic swap: current -> releases/<version>. The symlink only ever points
	//    at a complete release (relative target so it survives a path move).
	o.logf("atomic swap current -> releases/%s", version)
	if err := atomicSwap(l.CurrentLink(), filepath.Join("releases", version)); err != nil {
		return fmt.Errorf("deploy: %w", err)
	}

	// 7. Restart + is-active. On failure the operator's recovery is `rollback`
	//    (the prior release dir + pre-migration backup are intact).
	o.logf("restart %s", app)
	if err := o.System.Restart(ctx, app); err != nil {
		return fmt.Errorf("deploy: restart: %w", err)
	}
	if err := o.System.IsActive(ctx, app); err != nil {
		return fmt.Errorf("deploy: %s did not come up (recover with: opsctl rollback %s): %w", app, app, err)
	}

	// 8. Prune old releases (never current's target / its predecessor).
	if err := o.Prune(ctx, app); err != nil {
		return fmt.Errorf("deploy: prune: %w", err)
	}

	o.logf("deployed %s %s", app, version)
	return nil
}

// stampDataPaths ensures the regenerated manifest carries the absolute on-box
// state paths the SERVING process needs (<APP>_DB_PATH, <APP>_GENERATION_PATH).
// metaspot-launch exports every manifest key into the app's env, so stamping
// them here is what points `<app> serve` at data/<app>.db instead of appkit's
// relative dev default. A key already present in the binary's own manifest output
// is left untouched (the binary wins); missing keys are appended. The result
// always ends with exactly one trailing newline.
func stampDataPaths(manifest string, l Layout) string {
	up := strings.ToUpper(l.App)
	want := []struct{ key, val string }{
		{up + "_DB_PATH", l.DBPath()},
		{up + "_GENERATION_PATH", l.GenerationPath()},
	}
	body := strings.TrimRight(manifest, "\n")
	for _, kv := range want {
		if manifestHasKey(body, kv.key) {
			continue
		}
		body += "\n" + kv.key + "=" + kv.val
	}
	return body + "\n"
}

// manifestHasKey reports whether the flat KEY=value manifest already assigns key
// (ignoring leading whitespace; comment lines never match a bare KEY= prefix).
func manifestHasKey(manifest, key string) bool {
	for _, line := range strings.Split(manifest, "\n") {
		if strings.HasPrefix(strings.TrimSpace(line), key+"=") {
			return true
		}
	}
	return false
}

// schemaAdvances asks the binary (via the `schema` verb) whether this deploy
// advances the DB schema: applied (live DB) < embedded (binary's max migration).
func (o *Opsctl) schemaAdvances(ctx context.Context, l Layout, binary string) (bool, error) {
	out, err := o.Runner.Run(ctx, binary, "schema", nil, o.dbEnv(l))
	if err != nil {
		return false, fmt.Errorf("deploy: schema check: %w", err)
	}
	applied, embedded, err := schemaVersions(out)
	if err != nil {
		return false, fmt.Errorf("deploy: %w", err)
	}
	return embedded > applied, nil
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
