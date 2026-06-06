// Package backup is the dashboard's divergent backup/restore, folded into the
// binary via appkit.Spec.Backup / Spec.Restore (PLAN §E6, §B1 map §3 risk 1).
//
// The dashboard is the apex app: its state is the SQLite DB PLUS the one apex TLS
// cert, so the path-routed default (a bare SQLite snapshot) is insufficient. This
// package snapshots both to s3://$IKIGENBA_BACKUP_BUCKET/<app>/ under the per-app
// prefix convention (one immutable DB + one immutable cert tar per run, a `latest`
// pointer for each, count-based retention), matching what the prior operator-side
// bin/backup did on the box — now running IN the binary, on the box, as the verb.
//
// Two callers, one verb. opsctl's install/rollback drive `<app> backup --out
// <path>` / `<app> restore --from <path>` for the pre-migration LOCAL DB snapshot
// the downgrade-guard rollback story needs (PLAN §1.4, §2.5). The operator drives
// the cert+S3 snapshot with no --out/--from. So these hooks dispatch on the flag:
//
//   - `--out <path>` present  → local VACUUM-INTO snapshot to <path> (opsctl).
//   - `--from <path>` present → local file restore from <path> (opsctl).
//   - neither                 → the cert+S3+DB snapshot/restore (operator).
//
// The local branch is reimplemented here (appkit's default backup/restore are
// unexported) so opsctl's contract is preserved byte-for-byte.
//
// SECRETS: this package reads only NON-secret config from the env
// (IKIGENBA_BACKUP_BUCKET, IKIGENBA_AWS_REGION, IKIGENBA_DOMAIN). It never reads,
// composes, or logs any secret (PLAN §2.8). The cert files it tars are public-key
// material handled as opaque bytes for archival; they are not logged.
package backup

import (
	"context"
	"errors"
	"flag"
	"fmt"
	"io"
	"os"
	"os/exec"
	"path/filepath"
	"strings"
	"time"

	"appkit"

	"dashboard/internal/db"
)

// keep is how many immutable DB/cert snapshots to retain per prefix (count-based).
const keep = 30

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
	return s3Backup(ctx, req)
}

// Restore is the Spec.Restore hook. See the package doc for the --from dispatch.
func Restore(ctx context.Context, req appkit.RestoreReq) error {
	if from, ok := flagValue(req.Args, "from"); ok {
		return localRestore(req.DBPath, from, req.Stdout)
	}
	if err := rejectUnknownFlags(req.App+" restore", req.Args, req.Stderr); err != nil {
		return err
	}
	return s3Restore(ctx, req)
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

// --- the operator's cert+S3+DB snapshot/restore ---

// s3Backup snapshots the DB and the apex TLS cert to the per-app S3 prefix. It
// runs on the box (as root, via `sudo opsctl backup` in the new model), reads the
// non-secret bucket/region/domain from the env, and shells out to `aws`/`tar` the
// same way the prior bin/backup did — but in-binary, as the `backup` verb.
//
// DB consistency: the binary does NOT stop the unit (it runs as the app and can't
// systemctl); the orchestrating opsctl verb stops/starts around this call. The
// VACUUM-INTO snapshot is itself consistent against a live WAL DB, so a copy is
// safe even without a stop.
func s3Backup(ctx context.Context, req appkit.BackupReq) error {
	env, err := resolveEnv()
	if err != nil {
		return err
	}
	stamp := time.Now().UTC().Format("20060102T150405Z")

	work, err := os.MkdirTemp("", "dashboard-backup-*")
	if err != nil {
		return fmt.Errorf("backup: temp dir: %w", err)
	}
	defer os.RemoveAll(work)

	// ---- DB snapshot (VACUUM INTO → single consistent file → gzip → upload) ----
	if _, err := os.Stat(req.DBPath); err != nil {
		return fmt.Errorf("backup: db %s does not exist (has dashboard started?): %w", req.DBPath, err)
	}
	snap := filepath.Join(work, "dashboard.db")
	if err := localSnapshot(ctx, req.DBPath, snap, io.Discard); err != nil {
		return err
	}
	dbTar := filepath.Join(work, "db.tar.gz")
	if err := run(ctx, req.Stderr, "tar", "-czf", dbTar, "-C", work, "dashboard.db"); err != nil {
		return fmt.Errorf("backup: tar db: %w", err)
	}
	dbKey := fmt.Sprintf("%s/dashboard.db.%s.tar.gz", req.App, stamp)
	if err := s3cp(ctx, env, req.Stderr, dbTar, "s3://"+env.bucket+"/"+dbKey); err != nil {
		return err
	}
	if err := s3PutString(ctx, env, req.Stderr, work, dbKey+"\n", "s3://"+env.bucket+"/"+req.App+"/latest"); err != nil {
		return err
	}
	fmt.Fprintf(req.Stdout, "backed up dashboard DB to s3://%s/%s\n", env.bucket, dbKey)
	if err := prune(ctx, env, req.Stderr, req.App+"/dashboard.db."); err != nil {
		return err
	}

	// ---- Cert snapshot (no app stop; the cert is not app-mutated) ----
	certLive := filepath.Join("/etc/letsencrypt/live", env.domain)
	if _, err := os.Stat(certLive); err == nil {
		certTar := filepath.Join(work, "cert.tar.gz")
		// Paths relative to / so a restore's `tar -x -C /` lands them back in place;
		// live/ symlinks stored as-is, archive/ holds the real key material.
		if err := run(ctx, req.Stderr, "tar", "-czf", certTar, "-C", "/",
			filepath.Join("etc/letsencrypt/archive", env.domain),
			filepath.Join("etc/letsencrypt/renewal", env.domain+".conf"),
			filepath.Join("etc/letsencrypt/live", env.domain),
		); err != nil {
			return fmt.Errorf("backup: tar cert: %w", err)
		}
		certKey := fmt.Sprintf("%s/cert/cert.%s.tar.gz", req.App, stamp)
		if err := s3cp(ctx, env, req.Stderr, certTar, "s3://"+env.bucket+"/"+certKey); err != nil {
			return err
		}
		if err := s3PutString(ctx, env, req.Stderr, work, certKey+"\n", "s3://"+env.bucket+"/"+req.App+"/cert/latest"); err != nil {
			return err
		}
		fmt.Fprintf(req.Stdout, "backed up dashboard cert to s3://%s/%s\n", env.bucket, certKey)
		if err := prune(ctx, env, req.Stderr, req.App+"/cert/cert."); err != nil {
			return err
		}
	} else {
		fmt.Fprintf(req.Stdout, "no cert at %s — skipping cert snapshot\n", certLive)
	}

	fmt.Fprintln(req.Stdout, "backup complete.")
	return nil
}

// s3Restore replaces the live DB from a snapshot in the bucket. It resolves the
// newest snapshot via <app>/latest when no key arg is given (a bare key arg is
// used as-is). A pre-restore snapshot of the current DB is pushed first. The
// orchestrating opsctl verb stops/starts the unit around this call.
func s3Restore(ctx context.Context, req appkit.RestoreReq) error {
	env, err := resolveEnv()
	if err != nil {
		return err
	}

	key := strings.TrimSpace(strings.Join(req.Args, " "))
	if key == "" {
		out, err := s3cat(ctx, env, "s3://"+env.bucket+"/"+req.App+"/latest")
		if err != nil {
			return fmt.Errorf("restore: read %s/latest pointer: %w", req.App, err)
		}
		key = strings.TrimSpace(out)
		if key == "" {
			return fmt.Errorf("restore: no %s/latest pointer; pass an explicit snapshot key", req.App)
		}
	}

	work, err := os.MkdirTemp("", "dashboard-restore-*")
	if err != nil {
		return fmt.Errorf("restore: temp dir: %w", err)
	}
	defer os.RemoveAll(work)

	// Pre-restore snapshot of the current DB so a wrong-snapshot mistake is
	// recoverable (kept under a separate prefix, count-bounded).
	if _, err := os.Stat(req.DBPath); err == nil {
		pre := filepath.Join(work, "pre.db")
		if err := localSnapshot(ctx, req.DBPath, pre, io.Discard); err == nil {
			preTar := filepath.Join(work, "pre.tar.gz")
			if err := run(ctx, req.Stderr, "tar", "-czf", preTar, "-C", work, "pre.db"); err == nil {
				pKey := fmt.Sprintf("%s/pre-restore/dashboard.db.%s.tar.gz", req.App, time.Now().UTC().Format("20060102T150405Z"))
				_ = s3cp(ctx, env, req.Stderr, preTar, "s3://"+env.bucket+"/"+pKey)
			}
		}
	}

	dl := filepath.Join(work, "restore.tar.gz")
	if err := s3cp(ctx, env, req.Stderr, "s3://"+env.bucket+"/"+key, dl); err != nil {
		return err
	}
	extracted := filepath.Join(work, "extract")
	if err := os.MkdirAll(extracted, 0o755); err != nil {
		return err
	}
	if err := run(ctx, req.Stderr, "tar", "-xzf", dl, "-C", extracted); err != nil {
		return fmt.Errorf("restore: untar %s: %w", key, err)
	}
	if err := copyFile(filepath.Join(extracted, "dashboard.db"), req.DBPath); err != nil {
		return fmt.Errorf("restore: place db: %w", err)
	}
	for _, suffix := range []string{"-wal", "-shm"} {
		_ = os.Remove(req.DBPath + suffix)
	}
	fmt.Fprintf(req.Stdout, "restored dashboard from s3://%s/%s\n", env.bucket, key)
	return nil
}

// --- env + shell-out helpers ---

// boxEnv is the non-secret box config the S3 backup/restore needs.
type boxEnv struct {
	bucket string
	region string
	domain string
}

// resolveEnv reads IKIGENBA_BACKUP_BUCKET / IKIGENBA_AWS_REGION / IKIGENBA_DOMAIN.
// The bucket and domain are required for the S3 branch; the region defaults to
// us-east-2 (the platform default). None of these are secrets.
func resolveEnv() (boxEnv, error) {
	bucket := os.Getenv("IKIGENBA_BACKUP_BUCKET")
	if bucket == "" {
		return boxEnv{}, fmt.Errorf("backup: IKIGENBA_BACKUP_BUCKET is required for the cert+S3 backup")
	}
	domain := os.Getenv("IKIGENBA_DOMAIN")
	if domain == "" {
		return boxEnv{}, fmt.Errorf("backup: IKIGENBA_DOMAIN is required for the cert+S3 backup")
	}
	region := os.Getenv("IKIGENBA_AWS_REGION")
	if region == "" {
		region = "us-east-2"
	}
	return boxEnv{bucket: bucket, region: region, domain: domain}, nil
}

func (e boxEnv) awsEnv() []string {
	return append(os.Environ(), "AWS_DEFAULT_REGION="+e.region)
}

func s3cp(ctx context.Context, env boxEnv, stderr io.Writer, src, dst string) error {
	cmd := exec.CommandContext(ctx, "aws", "s3", "cp", "--no-progress", src, dst)
	cmd.Env = env.awsEnv()
	cmd.Stderr = stderr
	if err := cmd.Run(); err != nil {
		return fmt.Errorf("backup: aws s3 cp %s -> %s: %w", src, dst, err)
	}
	return nil
}

// s3PutString writes content to a temp file under work and uploads it to dst.
func s3PutString(ctx context.Context, env boxEnv, stderr io.Writer, work, content, dst string) error {
	f, err := os.CreateTemp(work, "ptr-*")
	if err != nil {
		return err
	}
	if _, err := f.WriteString(content); err != nil {
		f.Close()
		return err
	}
	f.Close()
	return s3cp(ctx, env, stderr, f.Name(), dst)
}

func s3cat(ctx context.Context, env boxEnv, src string) (string, error) {
	cmd := exec.CommandContext(ctx, "aws", "s3", "cp", src, "-")
	cmd.Env = env.awsEnv()
	out, err := cmd.Output()
	if err != nil {
		return "", err
	}
	return string(out), nil
}

// prune keeps the newest `keep` objects under prefix, deleting the rest. It lists
// keys sorted ascending and removes the oldest (count - keep).
func prune(ctx context.Context, env boxEnv, stderr io.Writer, prefix string) error {
	cmd := exec.CommandContext(ctx, "aws", "s3api", "list-objects-v2",
		"--bucket", env.bucket, "--prefix", prefix,
		"--query", "sort_by(Contents,&Key)[].Key", "--output", "text")
	cmd.Env = env.awsEnv()
	out, err := cmd.Output()
	if err != nil {
		// A list failure on prune is non-fatal: the snapshot already uploaded.
		fmt.Fprintf(stderr, "prune: list %s failed (non-fatal): %v\n", prefix, err)
		return nil
	}
	var keys []string
	for _, k := range strings.Fields(string(out)) {
		if k != "" && k != "None" {
			keys = append(keys, k)
		}
	}
	if len(keys) <= keep {
		return nil
	}
	for _, k := range keys[:len(keys)-keep] {
		_ = run(ctx, stderr, "aws", "s3", "rm", "s3://"+env.bucket+"/"+k)
	}
	return nil
}

func run(ctx context.Context, stderr io.Writer, name string, args ...string) error {
	cmd := exec.CommandContext(ctx, name, args...)
	cmd.Stderr = stderr
	return cmd.Run()
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
