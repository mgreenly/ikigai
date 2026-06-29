package opsctl

import (
	"bytes"
	"context"
	"fmt"
	"io"
	"os"
	"os/exec"
	"path/filepath"
	"sort"
	"strings"
	"testing"
)

type fakeStore struct {
	data    map[string][]byte
	deleted []string
	failPut bool
}

func newFakeStore() *fakeStore {
	return &fakeStore{data: map[string][]byte{}}
}

func (s *fakeStore) Put(ctx context.Context, key string, r io.Reader) error {
	if s.failPut {
		return fmt.Errorf("put failed")
	}
	b, err := io.ReadAll(r)
	if err != nil {
		return err
	}
	s.data[key] = b
	return nil
}

func (s *fakeStore) Get(ctx context.Context, key string, w io.Writer) error {
	b, ok := s.data[key]
	if !ok {
		return fmt.Errorf("missing object %s", key)
	}
	_, err := w.Write(b)
	return err
}

func (s *fakeStore) List(ctx context.Context, prefix string) ([]ObjInfo, error) {
	var out []ObjInfo
	for key, b := range s.data {
		if strings.HasPrefix(key, prefix) {
			out = append(out, ObjInfo{Key: key, Size: int64(len(b))})
		}
	}
	sort.Slice(out, func(i, j int) bool { return out[i].Key < out[j].Key })
	return out, nil
}

func (s *fakeStore) Delete(ctx context.Context, key string) error {
	delete(s.data, key)
	s.deleted = append(s.deleted, key)
	return nil
}

func testOps(root string, sys *stubSystem, store *fakeStore) *Opsctl {
	return &Opsctl{Root: root, System: sys, Store: store, Out: io.Discard, Err: io.Discard}
}

func writeStateFile(t *testing.T, l Layout, name, body string) {
	t.Helper()
	path := filepath.Join(l.StateDir(), name)
	if err := os.MkdirAll(filepath.Dir(path), 0o755); err != nil {
		t.Fatalf("mkdir state file: %v", err)
	}
	if err := os.WriteFile(path, []byte(body), 0o644); err != nil {
		t.Fatalf("write state file: %v", err)
	}
}

func tarList(t *testing.T, b []byte) []string {
	t.Helper()
	path := filepath.Join(t.TempDir(), "archive.tar")
	if err := os.WriteFile(path, b, 0o644); err != nil {
		t.Fatalf("write archive: %v", err)
	}
	cmd := exec.Command("tar", "-tf", path)
	out, err := cmd.CombinedOutput()
	if err != nil {
		t.Fatalf("tar -tf: %v: %s", err, out)
	}
	var entries []string
	for _, line := range strings.Split(strings.TrimSpace(string(out)), "\n") {
		if line != "" {
			entries = append(entries, line)
		}
	}
	return entries
}

func tarFile(t *testing.T, b []byte, name string) string {
	t.Helper()
	dir := t.TempDir()
	archive := filepath.Join(dir, "archive.tar")
	if err := os.WriteFile(archive, b, 0o644); err != nil {
		t.Fatalf("write archive: %v", err)
	}
	extract := filepath.Join(dir, "extract")
	if err := os.MkdirAll(extract, 0o755); err != nil {
		t.Fatalf("mkdir extract: %v", err)
	}
	cmd := exec.Command("tar", "-xf", archive, "-C", extract)
	if out, err := cmd.CombinedOutput(); err != nil {
		t.Fatalf("tar -xf: %v: %s", err, out)
	}
	got, err := os.ReadFile(filepath.Join(extract, name))
	if err != nil {
		t.Fatalf("read %s from archive: %v", name, err)
	}
	return string(got)
}

func snapshotObject(t *testing.T, store *fakeStore) (string, []byte) {
	t.Helper()
	var keys []string
	for key := range store.data {
		if strings.HasPrefix(key, snapshotPrefix("ledger")) {
			keys = append(keys, key)
		}
	}
	if len(keys) != 1 {
		t.Fatalf("snapshot object count = %d, want 1: %v", len(keys), keys)
	}
	return keys[0], store.data[keys[0]]
}

func makeArchive(t *testing.T, root, app string, files map[string]string) []byte {
	t.Helper()
	l := NewLayout(root, app)
	for name, body := range files {
		writeStateFile(t, l, name, body)
	}
	archive := filepath.Join(t.TempDir(), "state.tar")
	if err := createStateArchive(context.Background(), l, archive); err != nil {
		t.Fatalf("create archive: %v", err)
	}
	b, err := os.ReadFile(archive)
	if err != nil {
		t.Fatalf("read archive: %v", err)
	}
	return b
}

func TestBackupRestartsServiceAfterSnapshotFailure(t *testing.T) {
	// R-4GOT-W83B
	root := t.TempDir()
	sys := &stubSystem{}
	store := newFakeStore()
	err := testOps(root, sys, store).Backup(context.Background(), "ledger")
	if err == nil {
		t.Fatal("Backup without state dir succeeded, want tar failure")
	}
	got := strings.Join(sys.opSeq(), ",")
	for _, want := range []string{"systemctl:stop ledger", "systemctl:start ledger"} {
		if !strings.Contains(got, want) {
			t.Fatalf("system ops = %v, want %s", sys.opSeq(), want)
		}
	}
}

func TestBackupArchiveContainsOnlyStateDirectory(t *testing.T) {
	// R-4HWQ-9ZU0
	root := t.TempDir()
	l := NewLayout(root, "ledger")
	writeStateFile(t, l, "ledger.db", "db")
	writeStateFile(t, l, "nested/data.txt", "state data")
	if err := os.MkdirAll(l.CacheDir(), 0o755); err != nil {
		t.Fatal(err)
	}
	if err := os.WriteFile(filepath.Join(l.CacheDir(), "cache.txt"), []byte("cache"), 0o644); err != nil {
		t.Fatal(err)
	}
	if err := os.WriteFile(l.GenerationPath(), []byte("stale"), 0o644); err != nil {
		t.Fatal(err)
	}

	store := newFakeStore()
	if err := testOps(root, &stubSystem{}, store).Backup(context.Background(), "ledger"); err != nil {
		t.Fatalf("Backup: %v", err)
	}
	key, archive := snapshotObject(t, store)
	if latest := strings.TrimSpace(string(store.data[latestKey("ledger")])); latest != key {
		t.Fatalf("latest = %q, want %q", latest, key)
	}
	entries := strings.Join(tarList(t, archive), "\n")
	for _, want := range []string{"state/", "state/ledger.db", "state/nested/data.txt"} {
		if !strings.Contains(entries, want) {
			t.Fatalf("archive entries:\n%s\nmissing %s", entries, want)
		}
	}
	for _, forbidden := range []string{"cache", ".generation"} {
		if strings.Contains(entries, forbidden) {
			t.Fatalf("archive entries include %q:\n%s", forbidden, entries)
		}
	}
}

func TestBackupRetentionKeepsThirtySnapshotsWithoutPruningLatestOrPreRestore(t *testing.T) {
	// R-4J4M-NRKP
	root := t.TempDir()
	l := NewLayout(root, "ledger")
	writeStateFile(t, l, "ledger.db", "db")
	store := newFakeStore()
	for i := 0; i < 31; i++ {
		store.data[fmt.Sprintf("%s20000101T0000%02d.000000000Z.tar", snapshotPrefix("ledger"), i)] = []byte("old")
	}
	store.data[latestKey("ledger")] = []byte("do-not-prune")
	store.data["ledger/pre-restore/20000101T000000.000000000Z.tar"] = []byte("safety")

	if err := testOps(root, &stubSystem{}, store).Backup(context.Background(), "ledger"); err != nil {
		t.Fatalf("Backup: %v", err)
	}
	var snapshots int
	for key := range store.data {
		if strings.HasPrefix(key, snapshotPrefix("ledger")) {
			snapshots++
		}
	}
	if snapshots != backupKeep {
		t.Fatalf("snapshot count = %d, want %d", snapshots, backupKeep)
	}
	for _, key := range []string{latestKey("ledger"), "ledger/pre-restore/20000101T000000.000000000Z.tar"} {
		if _, ok := store.data[key]; !ok {
			t.Fatalf("%s was pruned", key)
		}
	}
}

func TestRestoreDefaultsThroughLatestButRequiresInteractiveConfirmation(t *testing.T) {
	// R-4KCJ-1JBE
	root := t.TempDir()
	l := NewLayout(root, "ledger")
	writeStateFile(t, l, "ledger.db", "old")
	store := newFakeStore()
	snapshot := snapshotPrefix("ledger") + "snapshot.tar"
	store.data[snapshot] = makeArchive(t, t.TempDir(), "ledger", map[string]string{"ledger.db": "new"})
	store.data[latestKey("ledger")] = []byte(snapshot + "\n")

	err := testOps(root, &stubSystem{}, store).Restore(context.Background(), "ledger", "", strings.NewReader(""))
	if err == nil {
		t.Fatal("Restore without typed confirmation succeeded")
	}
	got, readErr := os.ReadFile(l.DBPath())
	if readErr != nil {
		t.Fatalf("read live db: %v", readErr)
	}
	if string(got) != "old" {
		t.Fatalf("live db = %q, want old state unchanged", got)
	}
}

func TestRestoreWritesPreRestoreSnapshotBeforeReplacingState(t *testing.T) {
	// R-46XM-U25R
	root := t.TempDir()
	l := NewLayout(root, "ledger")
	writeStateFile(t, l, "ledger.db", "old")
	store := newFakeStore()
	snapshot := snapshotPrefix("ledger") + "snapshot.tar"
	store.data[snapshot] = makeArchive(t, t.TempDir(), "ledger", map[string]string{"ledger.db": "new"})
	store.data[latestKey("ledger")] = []byte(snapshot + "\n")

	if err := testOps(root, &stubSystem{}, store).Restore(context.Background(), "ledger", "", strings.NewReader("ledger\n")); err != nil {
		t.Fatalf("Restore: %v", err)
	}
	var preKeys []string
	for key := range store.data {
		if strings.HasPrefix(key, "ledger/pre-restore/") {
			preKeys = append(preKeys, key)
		}
	}
	if len(preKeys) != 1 {
		t.Fatalf("pre-restore snapshot count = %d, want 1: %v", len(preKeys), preKeys)
	}
	if got := tarFile(t, store.data[preKeys[0]], "state/ledger.db"); got != "old" {
		t.Fatalf("pre-restore db = %q, want old", got)
	}
}

func TestRestoreReplacesStateByteForByteAndClearsCacheGeneration(t *testing.T) {
	// R-49DF-LLN5
	root := t.TempDir()
	l := NewLayout(root, "ledger")
	writeStateFile(t, l, "ledger.db", "old")
	writeStateFile(t, l, "remove.txt", "gone")
	if err := os.MkdirAll(l.CacheDir(), 0o755); err != nil {
		t.Fatal(err)
	}
	if err := os.WriteFile(filepath.Join(l.CacheDir(), "cache.txt"), []byte("cache"), 0o644); err != nil {
		t.Fatal(err)
	}
	if err := os.WriteFile(l.GenerationPath(), []byte("stale"), 0o644); err != nil {
		t.Fatal(err)
	}
	if err := os.WriteFile(filepath.Join(l.AppDir(), "old.generation"), []byte("stale"), 0o644); err != nil {
		t.Fatal(err)
	}
	store := newFakeStore()
	snapshot := snapshotPrefix("ledger") + "snapshot.tar"
	store.data[snapshot] = makeArchive(t, t.TempDir(), "ledger", map[string]string{
		"ledger.db":        "new",
		"nested/value.txt": "kept",
	})

	if err := testOps(root, &stubSystem{}, store).Restore(context.Background(), "ledger", snapshot, strings.NewReader("ledger\n")); err != nil {
		t.Fatalf("Restore: %v", err)
	}
	checks := map[string]string{
		filepath.Join(l.StateDir(), "ledger.db"):        "new",
		filepath.Join(l.StateDir(), "nested/value.txt"): "kept",
	}
	for path, want := range checks {
		got, err := os.ReadFile(path)
		if err != nil {
			t.Fatalf("read %s: %v", path, err)
		}
		if !bytes.Equal(got, []byte(want)) {
			t.Fatalf("%s = %q, want %q", path, got, want)
		}
	}
	for _, path := range []string{
		filepath.Join(l.StateDir(), "remove.txt"),
		filepath.Join(l.CacheDir(), "cache.txt"),
		l.GenerationPath(),
		filepath.Join(l.AppDir(), "old.generation"),
	} {
		if _, err := os.Stat(path); !os.IsNotExist(err) {
			t.Fatalf("%s still exists after restore", path)
		}
	}
}
