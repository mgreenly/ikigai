package optctl

import (
	"context"
	"fmt"
	"io"
	"os"
	"path/filepath"
	"strings"
)

// Install ships a new version live, following the ADR install sequence exactly:
//
//  1. preflight (static? amd64? version matches? manifest parses?) — refuse early,
//     live release untouched.
//  2. place the artifact into releases/<version>/<app>.
//  3. regenerate etc/manifest.env via `<new binary> manifest` (the STABLE path the
//     dashboard derivation + bin/registry read).
//  4. back up data/<app>.db IF the schema will advance (applied < embedded), keyed
//     by <version> so the matching rollback can restore it.
//  5. migrate (forward-only).
//  6. atomic swap current → releases/<version> (ln -sfn equivalent).
//  7. restart the unit + is-active.
//  8. prune old releases.
//
// data/<app>.db is NEVER overwritten by placement — only read/migrated/snapshotted
// (PLAN §2.7). bin/run and etc/manifest.env stay valid throughout (PLAN §2.6).
func (o *Optctl) Install(ctx context.Context, app, version, artifact string) error {
	if app == "" || version == "" || artifact == "" {
		return fmt.Errorf("install: app, version, and --artifact are all required")
	}
	l := o.layout(app)

	// 1. Preflight — before touching anything live.
	o.logf("preflight %s %s", app, version)
	if err := o.preflight(ctx, artifact, app, version); err != nil {
		return err
	}

	// 2. Place the artifact into releases/<version>/<app> (idempotent re-install).
	relBin := l.ReleaseBinary(version)
	o.logf("place artifact -> %s", relBin)
	if err := os.MkdirAll(l.ReleaseDir(version), 0o755); err != nil {
		return fmt.Errorf("install: mkdir release dir: %w", err)
	}
	if err := copyExecutable(artifact, relBin); err != nil {
		return fmt.Errorf("install: place artifact: %w", err)
	}

	// 3. Regenerate the stable manifest from the binary that will serve it. Written
	//    via temp+rename so the stable path never holds a partial file mid-write.
	//    The binary emits a PORTABLE manifest (no box paths baked in — appkit's
	//    config defaults the DB to a relative ./tmp/<app>.db suited to local dev).
	//    metaspot-launch sources this manifest and exports every KEY into the
	//    serving process's env (AGENTS.md "Service layer"), but it never sets the
	//    on-box <APP>_DB_PATH/<APP>_GENERATION_PATH — so the serving binary would
	//    otherwise fall back to the relative dev default and miss the real DB at
	//    data/<app>.db. optctl is the on-box authority that owns the absolute /opt
	//    paths (it already injects them for its own migrate/schema/backup verbs via
	//    dbEnv), so it stamps them into the stable manifest here. Idempotent: the
	//    keys are only appended if the binary's own manifest did not already carry
	//    them.
	o.logf("regenerate %s", l.ManifestPath())
	manOut, err := o.Runner.Run(ctx, relBin, "manifest", nil, nil)
	if err != nil {
		return fmt.Errorf("install: manifest: %w", err)
	}
	manOut = stampDataPaths(manOut, l)
	if err := writeFileAtomic(l.ManifestPath(), []byte(manOut), 0o644); err != nil {
		return fmt.Errorf("install: write manifest: %w", err)
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
				return fmt.Errorf("install: mkdir backups: %w", err)
			}
			if _, err := o.Runner.Run(ctx, relBin, "backup",
				[]string{"--out", backup}, o.dbEnv(l)); err != nil {
				return fmt.Errorf("install: pre-migration backup: %w", err)
			}
		} else {
			// No DB yet (first install of a brand-new app): nothing to back up, the
			// migrate below creates it. The schema-advance flag is still true, but the
			// rollback target for a first release does not exist, so no backup needed.
			o.logf("schema advances but no DB yet (first install) — no backup")
		}
	}

	// 5. Migrate (forward-only). Idempotent: applies only unapplied higher versions.
	//    Ensure data/ exists so the app can create data/<app>.db on first migrate
	//    (setup creates this tree on the box; install self-heals it). Creating the
	//    directory never touches an existing DB file (PLAN §2.7).
	if err := os.MkdirAll(l.DataDir(), 0o755); err != nil {
		return fmt.Errorf("install: mkdir data dir: %w", err)
	}
	o.logf("migrate %s", app)
	if _, err := o.Runner.Run(ctx, relBin, "migrate", nil, o.dbEnv(l)); err != nil {
		return fmt.Errorf("install: migrate: %w", err)
	}

	// optctl runs privileged (sudo optctl …), so the root-run migrate above creates
	// any FRESH DB (+ its -wal/-shm and the generation sidecar) owned root:root.
	// The unit runs as the dedicated `<app>` system user (optctl setup's
	// EnsureSystemUser → `useradd --system <app>`, which also makes the matching
	// `<app>` group), so a root-owned DB leaves the service unable to take a write
	// lock — e.g. crm's event-plane outbox single-writer probe (`BEGIN IMMEDIATE`)
	// fails and the unit crash-loops. Hand the whole data tree back to <app>:<app>
	// before the swap/restart. Unconditional + idempotent on every install: it also
	// reclaims root-owned -wal/-shm files migrate may create against an EXISTING DB.
	// The user/group is the bare app name to match setup exactly (EnsureSystemUser).
	o.logf("chown %s -> %s:%s", l.DataDir(), app, app)
	if err := o.System.ChownTree(ctx, app, app, l.DataDir()); err != nil {
		return fmt.Errorf("install: chown data dir: %w", err)
	}

	// Ensure the stable bin/run -> ../current/<app> link exists (self-heal). This
	// is set at setup; install never repoints it (the cutover is `current`).
	if err := ensureSymlink(l.RunLink(), l.RunTarget()); err != nil {
		return fmt.Errorf("install: ensure bin/run: %w", err)
	}

	// 6. Atomic swap: current -> releases/<version>. The symlink only ever points
	//    at a complete release (relative target so it survives a path move).
	o.logf("atomic swap current -> releases/%s", version)
	if err := atomicSwap(l.CurrentLink(), filepath.Join("releases", version)); err != nil {
		return fmt.Errorf("install: %w", err)
	}

	// 7. Restart + is-active. On failure the operator's recovery is `rollback`
	//    (the prior release dir + pre-migration backup are intact).
	o.logf("restart %s", app)
	if err := o.System.Restart(ctx, app); err != nil {
		return fmt.Errorf("install: restart: %w", err)
	}
	if err := o.System.IsActive(ctx, app); err != nil {
		return fmt.Errorf("install: %s did not come up (recover with: optctl rollback %s): %w", app, app, err)
	}

	// 8. Prune old releases (never current's target / its predecessor).
	if err := o.Prune(ctx, app); err != nil {
		return fmt.Errorf("install: prune: %w", err)
	}

	o.logf("installed %s %s", app, version)
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
func (o *Optctl) schemaAdvances(ctx context.Context, l Layout, binary string) (bool, error) {
	out, err := o.Runner.Run(ctx, binary, "schema", nil, o.dbEnv(l))
	if err != nil {
		return false, fmt.Errorf("install: schema check: %w", err)
	}
	applied, embedded, err := schemaVersions(out)
	if err != nil {
		return false, fmt.Errorf("install: %w", err)
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
