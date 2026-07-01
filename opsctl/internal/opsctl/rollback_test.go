package opsctl

import (
	"archive/tar"
	"bytes"
	"compress/gzip"
	"context"
	"fmt"
	"io"
	"os"
	"path/filepath"
	"strings"
	"testing"
	"time"
)

func TestRollback_DefaultMatchesExplicitNewestSnapshot(t *testing.T) {
	// R-88JG-D5AN
	rootA, rootB := t.TempDir(), t.TempDir()
	app := "ledger"
	storeA, storeB := recordingSnapshotStore(), recordingSnapshotStore()
	seedRollbackSnapshots(t, storeA.fakeStore, app, map[string]string{
		snapshotKey(app, "v1.9.0", mustSnapshotTime(t, "20240103T010000.000000000Z")):  "newest",
		snapshotKey(app, "v1.10.0", mustSnapshotTime(t, "20240102T010000.000000000Z")): "middle",
		snapshotKey(app, "v1.0.0", mustSnapshotTime(t, "20240101T010000.000000000Z")):  "oldest",
	})
	seedRollbackSnapshots(t, storeB.fakeStore, app, map[string]string{
		snapshotKey(app, "v1.9.0", mustSnapshotTime(t, "20240103T010000.000000000Z")):  "newest",
		snapshotKey(app, "v1.10.0", mustSnapshotTime(t, "20240102T010000.000000000Z")): "middle",
		snapshotKey(app, "v1.0.0", mustSnapshotTime(t, "20240101T010000.000000000Z")):  "oldest",
	})

	runRollbackWithStore(t, rootA, app, storeA, "")
	runRollbackWithStore(t, rootB, app, storeB, "-0")

	if len(storeA.gets) != 1 || len(storeB.gets) != 1 {
		t.Fatalf("Get calls default=%v explicit=%v, want one snapshot download each", storeA.gets, storeB.gets)
	}
	if storeA.gets[0] != storeB.gets[0] {
		t.Fatalf("default snapshot key = %q, explicit -0 key = %q", storeA.gets[0], storeB.gets[0])
	}
	if want := snapshotKey(app, "v1.9.0", mustSnapshotTime(t, "20240103T010000.000000000Z")); storeA.gets[0] != want {
		t.Fatalf("resolved key = %q, want newest by parsed timestamp %q", storeA.gets[0], want)
	}
}

func TestRollback_OffsetRestoresNthNewestSnapshotAndSwapsAllLinks(t *testing.T) {
	// R-89RC-QX1C
	root := t.TempDir()
	app := "ledger"
	l := NewLayout(root, app)
	store := recordingSnapshotStore()
	seedRollbackSnapshots(t, store.fakeStore, app, map[string]string{
		snapshotKey(app, "v1.9.0", mustSnapshotTime(t, "20240103T010000.000000000Z")):  "newest",
		snapshotKey(app, "v1.10.0", mustSnapshotTime(t, "20240102T010000.000000000Z")): "second",
		snapshotKey(app, "v1.0.0", mustSnapshotTime(t, "20240101T010000.000000000Z")):  "oldest",
	})
	setupRollbackReleases(t, root, app, []string{"v1.0.0", "v1.9.0", "v1.10.0", "v2.0.0"})

	o := rollbackOps(root, app, &stubSystem{}, store, fakeRunner{baseEnv: fakeEnv(app, "v1.10.0", 9, "")})
	if err := o.Rollback(context.Background(), app, "-1"); err != nil {
		t.Fatalf("rollback -1: %v", err)
	}

	if got := readFileString(t, filepath.Join(l.StateDir(), app+".db")); got != "second" {
		t.Fatalf("restored state = %q, want second snapshot bytes", got)
	}
	assertSymlinkText(t, l.RunLink(), l.runTarget("v1.10.0"))
	assertSymlinkText(t, l.EtcCurrentLink(), "v1.10.0")
	assertSymlinkText(t, l.ShareCurrentLink(), "v1.10.0")
	if want := snapshotKey(app, "v1.10.0", mustSnapshotTime(t, "20240102T010000.000000000Z")); store.gets[0] != want {
		t.Fatalf("downloaded key = %q, want second-newest %q", store.gets[0], want)
	}
}

func TestRollback_MissingSnapshotVersionBinaryFailsBeforeSwappingLinks(t *testing.T) {
	// R-8AZ9-4OS1
	root := t.TempDir()
	app := "ledger"
	l := NewLayout(root, app)
	store := recordingSnapshotStore()
	seedRollbackSnapshots(t, store.fakeStore, app, map[string]string{
		snapshotKey(app, "v9.9.9", mustSnapshotTime(t, "20240103T010000.000000000Z")): "missing",
	})
	setupRollbackReleases(t, root, app, []string{"v1.0.0", "v2.0.0"})
	sys := &stubSystem{}
	o := rollbackOps(root, app, sys, store, fakeRunner{baseEnv: fakeEnv(app, "v9.9.9", 9, "")})

	err := o.Rollback(context.Background(), app, "")
	if err == nil || !strings.Contains(err.Error(), "does not exist under") {
		t.Fatalf("rollback err = %v, want missing libexec binary failure", err)
	}
	assertSymlinkText(t, l.RunLink(), l.runTarget("v2.0.0"))
	assertSymlinkText(t, l.EtcCurrentLink(), "v2.0.0")
	assertSymlinkText(t, l.ShareCurrentLink(), "v2.0.0")
	if sys.restarts != 0 {
		t.Fatalf("restart count = %d, want no restart on missing binary", sys.restarts)
	}
}

func TestRollback_SchemaDowngradeGuardFailsBeforeRestartOrSymlinkSwap(t *testing.T) {
	// R-8C75-IGIQ
	root := t.TempDir()
	app := "ledger"
	l := NewLayout(root, app)
	store := recordingSnapshotStore()
	seedRollbackSnapshots(t, store.fakeStore, app, map[string]string{
		snapshotKey(app, "v1.0.0", mustSnapshotTime(t, "20240103T010000.000000000Z")): "5",
	})
	setupRollbackReleases(t, root, app, []string{"v1.0.0", "v2.0.0"})
	sys := &stubSystem{}
	o := rollbackOps(root, app, sys, store, fakeRunner{baseEnv: fakeEnv(app, "v1.0.0", 3, "")})

	err := o.Rollback(context.Background(), app, "")
	if err == nil || !strings.Contains(err.Error(), "exceeds target embedded") {
		t.Fatalf("rollback err = %v, want schema downgrade refusal", err)
	}
	assertSymlinkText(t, l.RunLink(), l.runTarget("v2.0.0"))
	assertSymlinkText(t, l.EtcCurrentLink(), "v2.0.0")
	assertSymlinkText(t, l.ShareCurrentLink(), "v2.0.0")
	if sys.restarts != 0 {
		t.Fatalf("restart count = %d, want no restart after schema refusal", sys.restarts)
	}
}

func TestRollback_RepointsAllLinksBackToOldReleaseAndKeepsOldTiers(t *testing.T) {
	// R-3UQN-0CQT
	root := t.TempDir()
	app := "ledger"
	l := NewLayout(root, app)
	store := recordingSnapshotStore()
	seedRollbackSnapshots(t, store.fakeStore, app, map[string]string{
		snapshotKey(app, "v1.0.0", mustSnapshotTime(t, "20240103T010000.000000000Z")): "1",
	})
	setupRollbackReleases(t, root, app, []string{"v1.0.0", "v1.1.0"})

	o := rollbackOps(root, app, &stubSystem{}, store, fakeRunner{baseEnv: fakeEnv(app, "v1.0.0", 9, "")})
	if err := o.Rollback(context.Background(), app, ""); err != nil {
		t.Fatalf("rollback: %v", err)
	}

	assertSymlinkText(t, l.RunLink(), l.runTarget("v1.0.0"))
	assertSymlinkText(t, l.EtcCurrentLink(), "v1.0.0")
	assertSymlinkText(t, l.ShareCurrentLink(), "v1.0.0")
	for _, path := range []string{
		l.LibexecBinary("v1.0.0"),
		filepath.Join(l.EtcVersionDir("v1.0.0"), "nginx.conf"),
		filepath.Join(l.EtcVersionDir("v1.0.0"), "manifest.env"),
		l.ShareVersionDir("v1.0.0"),
	} {
		if _, err := os.Stat(path); err != nil {
			t.Fatalf("old tier missing after rollback: %s: %v", path, err)
		}
	}
}

type recordingStore struct {
	*fakeStore
	gets []string
}

func recordingSnapshotStore() *recordingStore {
	return &recordingStore{fakeStore: newFakeStore()}
}

func (s *recordingStore) Get(ctx context.Context, key string, w io.Writer) error {
	s.gets = append(s.gets, key)
	return s.fakeStore.Get(ctx, key, w)
}

func runRollbackWithStore(t *testing.T, root, app string, store *recordingStore, target string) {
	t.Helper()
	setupRollbackReleases(t, root, app, []string{"v1.0.0", "v1.9.0", "v1.10.0", "v2.0.0"})
	o := rollbackOps(root, app, &stubSystem{}, store, fakeRunner{baseEnv: fakeEnv(app, "v1.9.0", 9, "")})
	if err := o.Rollback(context.Background(), app, target); err != nil {
		t.Fatalf("rollback %q: %v", target, err)
	}
}

func rollbackOps(root, app string, sys *stubSystem, store ObjectStore, runner AppRunner) *Opsctl {
	sys.app = app
	return &Opsctl{
		Root:   root,
		Keep:   3,
		System: sys,
		Runner: runner,
		Store:  store,
		Out:    io.Discard,
		Err:    io.Discard,
	}
}

func setupRollbackReleases(t *testing.T, root, app string, versions []string) {
	t.Helper()
	sys := &stubSystem{}
	for _, version := range versions {
		o := newOpsctl(t, root, app, sys, fakeEnv(app, version, 1, ""))
		if err := stageAndDeploy(t, o, app, version, stageArtifact(t, app+"-"+version)); err != nil {
			t.Fatalf("deploy %s: %v", version, err)
		}
	}
}

func seedRollbackSnapshots(t *testing.T, store *fakeStore, app string, stateByKey map[string]string) {
	t.Helper()
	for key, state := range stateByKey {
		store.data[key] = rollbackStateArchive(t, app, state)
	}
}

func rollbackStateArchive(t *testing.T, app, state string) []byte {
	t.Helper()
	var buf bytes.Buffer
	gz := gzip.NewWriter(&buf)
	tw := tar.NewWriter(gz)
	for _, name := range []string{"state/"} {
		if err := tw.WriteHeader(&tar.Header{Name: name, Typeflag: tar.TypeDir, Mode: 0o755}); err != nil {
			t.Fatalf("add archive dir %s: %v", name, err)
		}
	}
	body := []byte(state)
	if err := tw.WriteHeader(&tar.Header{Name: fmt.Sprintf("state/%s.db", app), Typeflag: tar.TypeReg, Mode: 0o644, Size: int64(len(body))}); err != nil {
		t.Fatalf("add archive db: %v", err)
	}
	if _, err := tw.Write(body); err != nil {
		t.Fatalf("write archive db: %v", err)
	}
	if err := tw.Close(); err != nil {
		t.Fatalf("close archive tar: %v", err)
	}
	if err := gz.Close(); err != nil {
		t.Fatalf("close archive gzip: %v", err)
	}
	return buf.Bytes()
}

func mustSnapshotTime(t *testing.T, stamp string) time.Time {
	t.Helper()
	at, err := time.Parse(snapshotTimeShape, stamp)
	if err != nil {
		t.Fatalf("parse snapshot time: %v", err)
	}
	return at
}

func readFileString(t *testing.T, path string) string {
	t.Helper()
	b, err := os.ReadFile(path)
	if err != nil {
		t.Fatalf("read %s: %v", path, err)
	}
	return string(b)
}
