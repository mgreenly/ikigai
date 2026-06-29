// Package backup preserves dashboard's local appkit backup/restore contract.
//
// opsctl's install/rollback drive `<app> backup --out <path>` / `<app> restore
// --from <path>` for the pre-migration local DB snapshot the downgrade-guard
// rollback story needs. The former no-flag dashboard S3 branch is retired; box
// backups now live in opsctl behind the ObjectStore seam.
package backup

import (
	"context"
	"errors"
	"flag"
	"fmt"
	"io"
	"os"
	"path/filepath"
	"strings"

	"appkit"

	"dashboard/internal/db"
)

// Backup is the Spec.Backup hook. See the package doc for the --out dispatch.
func Backup(ctx context.Context, req appkit.BackupReq) error {
	if out, ok := flagValue(req.Args, "out"); ok {
		return localSnapshot(ctx, req.DBPath, out, req.Stdout)
	}
	// Reject any other flags so a typo'd operator invocation fails loudly rather
	// than silently doing the S3 backup.
	if err := rejectUnknownFlags(req.App+" backup", req.Args, req.Stderr); err != nil {
		return err
	}
	return fmt.Errorf("backup: dashboard in-binary S3 backup is retired; use opsctl backup")
}

// Restore is the Spec.Restore hook. See the package doc for the --from dispatch.
func Restore(ctx context.Context, req appkit.RestoreReq) error {
	if from, ok := flagValue(req.Args, "from"); ok {
		return localRestore(req.DBPath, from, req.Stdout)
	}
	if err := rejectUnknownFlags(req.App+" restore", req.Args, req.Stderr); err != nil {
		return err
	}
	return fmt.Errorf("restore: dashboard in-binary S3 restore is retired; use opsctl restore")
}

// --- opsctl's local DB snapshot/restore (the --out/--from contract) ---

// localSnapshot writes a transactionally consistent copy of dbPath to out using
// VACUUM INTO (the same mechanism as appkit's default backup), so opsctl's
// pre-migration backup keyed by version is identical in shape to every other
// service's. A missing source DB is an error (opsctl only calls this when the DB
// exists).
func localSnapshot(ctx context.Context, dbPath, out string, stdout io.Writer) error {
	if _, err := os.Stat(dbPath); err != nil {
		return fmt.Errorf("backup: source db %s: %w", dbPath, err)
	}
	if err := os.MkdirAll(filepath.Dir(out), 0o755); err != nil {
		return fmt.Errorf("backup: mkdir %s: %w", filepath.Dir(out), err)
	}
	// VACUUM INTO refuses to overwrite an existing file, so clear a stale snapshot.
	if err := os.Remove(out); err != nil && !errors.Is(err, os.ErrNotExist) {
		return fmt.Errorf("backup: clear %s: %w", out, err)
	}
	conn, err := db.Open(dbPath)
	if err != nil {
		return err
	}
	defer conn.Close()
	if _, err := conn.ExecContext(ctx, `VACUUM INTO ?`, out); err != nil {
		return fmt.Errorf("backup: vacuum into %s: %w", out, err)
	}
	fmt.Fprintf(stdout, "backed up dashboard to %s\n", out)
	return nil
}

// localRestore replaces dbPath with the snapshot at from (atomic rename), then
// drops any WAL/SHM sidecar so the restored snapshot is authoritative on next
// open. The caller (opsctl rollback) stops the unit first.
func localRestore(dbPath, from string, stdout io.Writer) error {
	if _, err := os.Stat(from); err != nil {
		return fmt.Errorf("restore: snapshot %s: %w", from, err)
	}
	if err := copyFile(from, dbPath); err != nil {
		return fmt.Errorf("restore: %w", err)
	}
	for _, suffix := range []string{"-wal", "-shm"} {
		if err := os.Remove(dbPath + suffix); err != nil && !errors.Is(err, os.ErrNotExist) {
			return fmt.Errorf("restore: clear %s: %w", dbPath+suffix, err)
		}
	}
	fmt.Fprintf(stdout, "restored dashboard from %s\n", from)
	return nil
}

// flagValue scans args for `--name value` or `--name=value` (also single-dash) and
// returns the value and whether it was present.
func flagValue(args []string, name string) (string, bool) {
	for i, a := range args {
		if a == "--"+name || a == "-"+name {
			if i+1 < len(args) {
				return args[i+1], true
			}
			return "", true
		}
		for _, pfx := range []string{"--" + name + "=", "-" + name + "="} {
			if strings.HasPrefix(a, pfx) {
				return strings.TrimPrefix(a, pfx), true
			}
		}
	}
	return "", false
}

// rejectUnknownFlags fails if args carry any flag in the S3 (no --out/--from)
// branch, so a typo doesn't silently fall through to a destructive S3 op. A
// non-flag positional (the restore snapshot key) is allowed.
func rejectUnknownFlags(name string, args []string, stderr io.Writer) error {
	fs := flag.NewFlagSet(name, flag.ContinueOnError)
	fs.SetOutput(stderr)
	if err := fs.Parse(args); err != nil {
		return err
	}
	return nil
}

func copyFile(src, dst string) error {
	in, err := os.Open(src)
	if err != nil {
		return err
	}
	defer in.Close()
	if err := os.MkdirAll(filepath.Dir(dst), 0o755); err != nil {
		return err
	}
	tmp := dst + ".restore.tmp"
	out, err := os.Create(tmp)
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
	if err := os.Rename(tmp, dst); err != nil {
		os.Remove(tmp)
		return err
	}
	return nil
}
