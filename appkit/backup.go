package appkit

import (
	"context"
	"errors"
	"flag"
	"fmt"
	"io"
	"os"
	"path/filepath"

	"appkit/db"
)

// defaultBackup is appkit's minimal consistent SQLite snapshot of the app's DB
// (PLAN §B1 map §3 risk 1: backup/restore have no extractable body — written
// fresh here, overridable via Spec.Backup). It writes a single self-contained
// snapshot file using `VACUUM INTO`, which produces a transactionally consistent
// copy of a live WAL database without stopping writers.
//
// Usage: `<app> backup [--out <path>]` (default <DBPath>.backup). A service that
// needs more (the dashboard's cert+S3 snapshot, retention, a `latest` pointer)
// supplies Spec.Backup instead.
func defaultBackup(ctx context.Context, req BackupReq) error {
	fs := flag.NewFlagSet(req.App+" backup", flag.ContinueOnError)
	fs.SetOutput(req.Stderr)
	out := fs.String("out", req.DBPath+".backup", "snapshot output path")
	if err := fs.Parse(req.Args); err != nil {
		return helpOrErr(err)
	}
	if _, err := os.Stat(req.DBPath); err != nil {
		return fmt.Errorf("backup: source db %s: %w", req.DBPath, err)
	}
	if err := os.MkdirAll(filepath.Dir(*out), 0o755); err != nil {
		return fmt.Errorf("backup: mkdir %s: %w", filepath.Dir(*out), err)
	}
	// VACUUM INTO refuses to overwrite an existing file, so clear a stale snapshot.
	if err := os.Remove(*out); err != nil && !errors.Is(err, os.ErrNotExist) {
		return fmt.Errorf("backup: clear %s: %w", *out, err)
	}

	conn, err := db.Open(req.DBPath)
	if err != nil {
		return err
	}
	defer conn.Close()
	if _, err := conn.ExecContext(ctx, `VACUUM INTO ?`, *out); err != nil {
		return fmt.Errorf("backup: vacuum into %s: %w", *out, err)
	}
	fmt.Fprintf(req.Stdout, "backed up %s to %s\n", req.App, *out)
	return nil
}

// defaultRestore replaces the app's DB file with a snapshot produced by
// defaultBackup (PLAN §B1 map §3 risk 1; overridable via Spec.Restore). It is a
// file-level copy, so the caller must ensure the service is stopped first
// (optctl rollback/install own that orchestration). The generation sidecar is
// left untouched here — a producer that must re-mint its epoch on restore
// supplies Spec.Restore (PLAN §B1 map §5).
//
// Usage: `<app> restore --from <snapshot>`.
func defaultRestore(ctx context.Context, req RestoreReq) error {
	fs := flag.NewFlagSet(req.App+" restore", flag.ContinueOnError)
	fs.SetOutput(req.Stderr)
	from := fs.String("from", req.DBPath+".backup", "snapshot to restore from")
	if err := fs.Parse(req.Args); err != nil {
		return helpOrErr(err)
	}
	if _, err := os.Stat(*from); err != nil {
		return fmt.Errorf("restore: snapshot %s: %w", *from, err)
	}
	if err := copyFile(*from, req.DBPath); err != nil {
		return fmt.Errorf("restore: %w", err)
	}
	// Drop any WAL/SHM sidecars so the restored snapshot is authoritative on next
	// open (a stale -wal would otherwise overlay the restored file).
	for _, suffix := range []string{"-wal", "-shm"} {
		if err := os.Remove(req.DBPath + suffix); err != nil && !errors.Is(err, os.ErrNotExist) {
			return fmt.Errorf("restore: clear %s: %w", req.DBPath+suffix, err)
		}
	}
	fmt.Fprintf(req.Stdout, "restored %s from %s\n", req.App, *from)
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
	// Atomic swap into place.
	if err := os.Rename(tmp, dst); err != nil {
		os.Remove(tmp)
		return err
	}
	return nil
}
