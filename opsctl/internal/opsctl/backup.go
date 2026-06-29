package opsctl

import (
	"bufio"
	"context"
	"encoding/json"
	"errors"
	"fmt"
	"io"
	"os"
	"os/exec"
	"path/filepath"
	"sort"
	"strings"
	"time"
)

const backupKeep = 30

func (o *Opsctl) Backup(ctx context.Context, app string) (err error) {
	if err := o.backupState(ctx, app); err != nil {
		return err
	}
	if app == apexApp {
		if err := o.backupCert(ctx, app); err != nil {
			return err
		}
	}
	return nil
}

func (o *Opsctl) BackupAll(ctx context.Context) error {
	apps, err := o.discoverApps()
	if err != nil {
		return fmt.Errorf("backup --all: discover apps: %w", err)
	}
	var errs []error
	for _, app := range apps {
		if err := o.backupState(ctx, app); err != nil {
			errs = append(errs, fmt.Errorf("%s: %w", app, err))
		}
	}
	if err := o.backupCert(ctx, apexApp); err != nil {
		errs = append(errs, fmt.Errorf("%s cert: %w", apexApp, err))
	}
	if len(errs) > 0 {
		return fmt.Errorf("backup --all: %d failure(s): %w", len(errs), errors.Join(errs...))
	}
	return nil
}

func (o *Opsctl) backupState(ctx context.Context, app string) (err error) {
	l := o.layout(app)
	store := o.objectStore()
	if err := o.Stop(ctx, app, nil); err != nil {
		return err
	}
	defer func() {
		if startErr := o.Start(ctx, app, nil); startErr != nil && err == nil {
			err = startErr
		}
	}()

	work, err := os.MkdirTemp("", "opsctl-backup-*")
	if err != nil {
		return fmt.Errorf("backup: temp dir: %w", err)
	}
	defer os.RemoveAll(work)

	archive := filepath.Join(work, "state.tar")
	if err := createStateArchive(ctx, l, archive); err != nil {
		return err
	}
	f, err := os.Open(archive)
	if err != nil {
		return fmt.Errorf("backup: open archive: %w", err)
	}
	defer f.Close()

	key := snapshotKey(app, time.Now().UTC())
	if err := store.Put(ctx, key, f); err != nil {
		return err
	}
	if err := store.Put(ctx, latestKey(app), strings.NewReader(key+"\n")); err != nil {
		return err
	}
	if err := pruneBackups(ctx, store, app); err != nil {
		return err
	}
	o.logf("backed up %s to %s", app, key)
	return nil
}

func (o *Opsctl) backupCert(ctx context.Context, app string) error {
	l := o.layout(app)
	store := o.objectStore()
	work, err := os.MkdirTemp("", "opsctl-backup-cert-*")
	if err != nil {
		return fmt.Errorf("backup: temp dir: %w", err)
	}
	defer os.RemoveAll(work)

	archive := filepath.Join(work, "cert.tar")
	if err := createCertArchive(ctx, l, archive); err != nil {
		return err
	}
	certFile, err := os.Open(archive)
	if err != nil {
		return fmt.Errorf("backup: open cert archive: %w", err)
	}
	defer certFile.Close()
	certKey := certSnapshotKey(app, time.Now().UTC())
	if err := store.Put(ctx, certKey, certFile); err != nil {
		return err
	}
	if err := store.Put(ctx, certLatestKey(app), strings.NewReader(certKey+"\n")); err != nil {
		return err
	}
	o.logf("backed up %s cert to %s", app, certKey)
	return nil
}

func (o *Opsctl) Restore(ctx context.Context, app, key string, confirm io.Reader) (err error) {
	l := o.layout(app)
	store := o.objectStore()
	if key == "" {
		var latest strings.Builder
		if err := store.Get(ctx, latestKey(app), &latest); err != nil {
			return fmt.Errorf("restore: read latest: %w", err)
		}
		key = strings.TrimSpace(latest.String())
		if key == "" {
			return fmt.Errorf("restore: latest is empty")
		}
	}
	if err := confirmRestore(app, key, confirm); err != nil {
		return err
	}
	if err := o.Stop(ctx, app, nil); err != nil {
		return err
	}
	defer func() {
		if startErr := o.Start(ctx, app, nil); startErr != nil && err == nil {
			err = startErr
		}
	}()

	work, err := os.MkdirTemp("", "opsctl-restore-*")
	if err != nil {
		return fmt.Errorf("restore: temp dir: %w", err)
	}
	defer os.RemoveAll(work)

	if err := putPreRestore(ctx, store, l, app, work); err != nil {
		return err
	}
	archive := filepath.Join(work, "restore.tar")
	out, err := os.Create(archive)
	if err != nil {
		return fmt.Errorf("restore: create archive: %w", err)
	}
	if err := store.Get(ctx, key, out); err != nil {
		out.Close()
		return err
	}
	if err := out.Close(); err != nil {
		return fmt.Errorf("restore: close archive: %w", err)
	}
	if err := replaceStateFromArchive(ctx, l, archive); err != nil {
		return err
	}
	if app == "dashboard" {
		if err := restoreLatestCert(ctx, store, l, work); err != nil {
			return err
		}
	}
	o.logf("restored %s from %s", app, key)
	return nil
}

func (o *Opsctl) objectStore() ObjectStore {
	if o.Store != nil {
		return o.Store
	}
	return AWSObjectStore{
		Bucket: os.Getenv("IKIGENBA_BACKUP_BUCKET"),
		Region: firstNonEmpty(os.Getenv("IKIGENBA_AWS_REGION"), os.Getenv("AWS_DEFAULT_REGION")),
		Stderr: o.Err,
	}
}

func confirmRestore(app, key string, in io.Reader) error {
	if in == nil {
		return fmt.Errorf("restore: confirmation required; type %s to restore %s", app, key)
	}
	line, err := bufio.NewReader(in).ReadString('\n')
	if err != nil && len(line) == 0 {
		return fmt.Errorf("restore: confirmation required; type %s to restore %s", app, key)
	}
	if strings.TrimSpace(line) != app {
		return fmt.Errorf("restore: confirmation mismatch; type %s to restore %s", app, key)
	}
	return nil
}

func createStateArchive(ctx context.Context, l Layout, dst string) error {
	if _, err := os.Stat(l.StateDir()); err != nil {
		return fmt.Errorf("backup: state dir %s: %w", l.StateDir(), err)
	}
	if err := execTar(ctx, "tar", "-cf", dst, "-C", l.AppDir(), "state"); err != nil {
		return fmt.Errorf("backup: tar state: %w", err)
	}
	return nil
}

func createCertArchive(ctx context.Context, l Layout, dst string) error {
	root := letsEncryptRoot(l)
	for _, name := range []string{"archive", "renewal", "live"} {
		path := filepath.Join(root, name)
		if _, err := os.Stat(path); err != nil {
			return fmt.Errorf("backup: cert %s: %w", path, err)
		}
	}
	if err := execTar(ctx, "tar", "-cf", dst, "-C", l.SysRoot,
		"etc/letsencrypt/archive", "etc/letsencrypt/renewal", "etc/letsencrypt/live"); err != nil {
		return fmt.Errorf("backup: tar cert: %w", err)
	}
	return nil
}

func putPreRestore(ctx context.Context, store ObjectStore, l Layout, app, work string) error {
	if _, err := os.Stat(l.StateDir()); err != nil {
		return fmt.Errorf("restore: pre-restore state dir %s: %w", l.StateDir(), err)
	}
	archive := filepath.Join(work, "pre-restore.tar")
	if err := createStateArchive(ctx, l, archive); err != nil {
		return err
	}
	f, err := os.Open(archive)
	if err != nil {
		return fmt.Errorf("restore: open pre-restore archive: %w", err)
	}
	defer f.Close()
	return store.Put(ctx, preRestoreKey(app, time.Now().UTC()), f)
}

func restoreLatestCert(ctx context.Context, store ObjectStore, l Layout, work string) error {
	var latest strings.Builder
	if err := store.Get(ctx, certLatestKey(l.App), &latest); err != nil {
		return fmt.Errorf("restore: read cert latest: %w", err)
	}
	key := strings.TrimSpace(latest.String())
	if key == "" {
		return fmt.Errorf("restore: cert latest is empty")
	}
	archive := filepath.Join(work, "cert-restore.tar")
	out, err := os.Create(archive)
	if err != nil {
		return fmt.Errorf("restore: create cert archive: %w", err)
	}
	if err := store.Get(ctx, key, out); err != nil {
		out.Close()
		return err
	}
	if err := out.Close(); err != nil {
		return fmt.Errorf("restore: close cert archive: %w", err)
	}
	if err := replaceCertFromArchive(ctx, l, archive); err != nil {
		return err
	}
	return nil
}

func replaceStateFromArchive(ctx context.Context, l Layout, archive string) error {
	if err := os.RemoveAll(l.StateDir()); err != nil {
		return fmt.Errorf("restore: clear state: %w", err)
	}
	if err := os.RemoveAll(l.CacheDir()); err != nil {
		return fmt.Errorf("restore: clear cache: %w", err)
	}
	if err := removeGenerationFiles(l.AppDir()); err != nil {
		return err
	}
	if err := os.MkdirAll(l.AppDir(), 0o755); err != nil {
		return fmt.Errorf("restore: mkdir app dir: %w", err)
	}
	if err := execTar(ctx, "tar", "-xf", archive, "-C", l.AppDir()); err != nil {
		return fmt.Errorf("restore: untar state: %w", err)
	}
	if _, err := os.Stat(l.StateDir()); err != nil {
		return fmt.Errorf("restore: archive did not contain state/: %w", err)
	}
	return nil
}

func replaceCertFromArchive(ctx context.Context, l Layout, archive string) error {
	root := letsEncryptRoot(l)
	for _, name := range []string{"archive", "renewal", "live"} {
		if err := os.RemoveAll(filepath.Join(root, name)); err != nil {
			return fmt.Errorf("restore: clear cert %s: %w", name, err)
		}
	}
	if err := os.MkdirAll(root, 0o755); err != nil {
		return fmt.Errorf("restore: mkdir cert root: %w", err)
	}
	if err := execTar(ctx, "tar", "-xf", archive, "-C", l.SysRoot); err != nil {
		return fmt.Errorf("restore: untar cert: %w", err)
	}
	for _, name := range []string{"archive", "renewal", "live"} {
		if _, err := os.Stat(filepath.Join(root, name)); err != nil {
			return fmt.Errorf("restore: cert archive did not contain %s/: %w", name, err)
		}
	}
	return nil
}

func removeGenerationFiles(root string) error {
	return filepath.WalkDir(root, func(path string, d os.DirEntry, err error) error {
		if err != nil {
			return err
		}
		if d.IsDir() {
			if path == filepath.Join(root, "state") {
				return filepath.SkipDir
			}
			return nil
		}
		if strings.HasSuffix(d.Name(), ".generation") {
			if err := os.Remove(path); err != nil {
				return fmt.Errorf("restore: remove generation %s: %w", path, err)
			}
		}
		return nil
	})
}

func pruneBackups(ctx context.Context, store ObjectStore, app string) error {
	infos, err := store.List(ctx, snapshotPrefix(app))
	if err != nil {
		return err
	}
	keys := make([]string, 0, len(infos))
	for _, info := range infos {
		if strings.HasPrefix(info.Key, snapshotPrefix(app)) {
			keys = append(keys, info.Key)
		}
	}
	sort.Strings(keys)
	if len(keys) <= backupKeep {
		return nil
	}
	for _, key := range keys[:len(keys)-backupKeep] {
		if err := store.Delete(ctx, key); err != nil {
			return err
		}
	}
	return nil
}

func snapshotPrefix(app string) string { return app + "/snapshots/" }
func latestKey(app string) string      { return app + "/latest" }
func certPrefix(app string) string     { return app + "/cert/" }
func certLatestKey(app string) string  { return certPrefix(app) + "latest" }

func letsEncryptRoot(l Layout) string {
	return filepath.Join(l.SysRoot, "etc", "letsencrypt")
}

func snapshotKey(app string, t time.Time) string {
	return snapshotPrefix(app) + t.Format("20060102T150405.000000000Z") + ".tar"
}

func certSnapshotKey(app string, t time.Time) string {
	return certPrefix(app) + t.Format("20060102T150405.000000000Z") + ".tar"
}

func preRestoreKey(app string, t time.Time) string {
	return app + "/pre-restore/" + t.Format("20060102T150405.000000000Z") + ".tar"
}

func execTar(ctx context.Context, name string, args ...string) error {
	cmd := exec.CommandContext(ctx, name, args...)
	if out, err := cmd.CombinedOutput(); err != nil {
		return fmt.Errorf("%s %s: %w: %s", name, strings.Join(args, " "), err, strings.TrimSpace(string(out)))
	}
	return nil
}

type AWSObjectStore struct {
	Bucket string
	Region string
	Stderr io.Writer
}

func (s AWSObjectStore) Put(ctx context.Context, key string, r io.Reader) error {
	if s.Bucket == "" {
		return fmt.Errorf("object store: IKIGENBA_BACKUP_BUCKET is required")
	}
	cmd := exec.CommandContext(ctx, "aws", "s3", "cp", "--no-progress", "-", "s3://"+s.Bucket+"/"+key)
	cmd.Env = s.awsEnv()
	cmd.Stdin = r
	cmd.Stderr = s.Stderr
	if err := cmd.Run(); err != nil {
		return fmt.Errorf("object store put %s: %w", key, err)
	}
	return nil
}

func (s AWSObjectStore) Get(ctx context.Context, key string, w io.Writer) error {
	if s.Bucket == "" {
		return fmt.Errorf("object store: IKIGENBA_BACKUP_BUCKET is required")
	}
	cmd := exec.CommandContext(ctx, "aws", "s3", "cp", "--no-progress", "s3://"+s.Bucket+"/"+key, "-")
	cmd.Env = s.awsEnv()
	cmd.Stdout = w
	cmd.Stderr = s.Stderr
	if err := cmd.Run(); err != nil {
		return fmt.Errorf("object store get %s: %w", key, err)
	}
	return nil
}

func (s AWSObjectStore) List(ctx context.Context, prefix string) ([]ObjInfo, error) {
	if s.Bucket == "" {
		return nil, fmt.Errorf("object store: IKIGENBA_BACKUP_BUCKET is required")
	}
	cmd := exec.CommandContext(ctx, "aws", "s3api", "list-objects-v2", "--bucket", s.Bucket, "--prefix", prefix, "--output", "json")
	cmd.Env = s.awsEnv()
	cmd.Stderr = s.Stderr
	out, err := cmd.Output()
	if err != nil {
		return nil, fmt.Errorf("object store list %s: %w", prefix, err)
	}
	var parsed struct {
		Contents []struct {
			Key  string
			Size int64
		}
	}
	if err := json.Unmarshal(out, &parsed); err != nil {
		return nil, fmt.Errorf("object store list %s: parse aws json: %w", prefix, err)
	}
	infos := make([]ObjInfo, 0, len(parsed.Contents))
	for _, obj := range parsed.Contents {
		infos = append(infos, ObjInfo{Key: obj.Key, Size: obj.Size})
	}
	return infos, nil
}

func (s AWSObjectStore) Delete(ctx context.Context, key string) error {
	if s.Bucket == "" {
		return fmt.Errorf("object store: IKIGENBA_BACKUP_BUCKET is required")
	}
	cmd := exec.CommandContext(ctx, "aws", "s3", "rm", "s3://"+s.Bucket+"/"+key)
	cmd.Env = s.awsEnv()
	cmd.Stderr = s.Stderr
	if err := cmd.Run(); err != nil {
		return fmt.Errorf("object store delete %s: %w", key, err)
	}
	return nil
}

func (s AWSObjectStore) awsEnv() []string {
	env := os.Environ()
	if s.Region != "" {
		env = append(env, "AWS_DEFAULT_REGION="+s.Region)
	}
	return env
}

func firstNonEmpty(vals ...string) string {
	for _, v := range vals {
		if v != "" {
			return v
		}
	}
	return ""
}
