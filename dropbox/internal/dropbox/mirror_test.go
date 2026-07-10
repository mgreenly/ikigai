package dropbox

import (
	"bytes"
	"errors"
	"io"
	"os"
	"path/filepath"
	"testing"
)

type gatedReader struct {
	r       *bytes.Reader
	started chan<- struct{}
	release <-chan struct{}
	once    bool
}

func (r *gatedReader) Read(p []byte) (int, error) {
	if !r.once {
		r.once = true
		close(r.started)
		<-r.release
	}
	return r.r.Read(p)
}

func writeMirror(t testing.TB, m *Mirror, path string, data []byte) {
	t.Helper()
	if _, _, err := m.WriteFrom(path, bytes.NewReader(data)); err != nil {
		t.Fatalf("WriteFrom: %v", err)
	}
}

// newTestMirror roots a Mirror at a fresh temp dir.
func newTestMirror(t *testing.T) *Mirror {
	t.Helper()
	m, err := NewMirror(filepath.Join(t.TempDir(), "mirror"))
	if err != nil {
		t.Fatalf("NewMirror: %v", err)
	}
	return m
}

func TestWriteAtomicAndModes(t *testing.T) {
	m := newTestMirror(t)
	want := []byte("hello dropbox")

	writeMirror(t, m, "/inbox/sub/report.pdf", want)

	dst := filepath.Join(m.Root(), "inbox", "sub", "report.pdf")
	got, err := os.ReadFile(dst)
	if err != nil {
		t.Fatalf("read back: %v", err)
	}
	if string(got) != string(want) {
		t.Fatalf("bytes mismatch: got %q want %q", got, want)
	}

	// File mode must be private 0640.
	fi, err := os.Stat(dst)
	if err != nil {
		t.Fatalf("stat file: %v", err)
	}
	if perm := fi.Mode().Perm(); perm != 0o640 {
		t.Fatalf("file mode = %o, want 0640", perm)
	}

	// Auto-created parent dir must be private 0750.
	di, err := os.Stat(filepath.Join(m.Root(), "inbox", "sub"))
	if err != nil {
		t.Fatalf("stat dir: %v", err)
	}
	if perm := di.Mode().Perm(); perm != 0o750 {
		t.Fatalf("dir mode = %o, want 0750", perm)
	}

	// No leftover temp files in the destination dir.
	entries, err := os.ReadDir(filepath.Dir(dst))
	if err != nil {
		t.Fatalf("readdir: %v", err)
	}
	for _, e := range entries {
		if len(e.Name()) > 0 && e.Name()[0] == '.' {
			t.Fatalf("leftover temp file: %s", e.Name())
		}
	}
}

func TestWriteOverwriteIsAtomic(t *testing.T) {
	m := newTestMirror(t)
	writeMirror(t, m, "/a.txt", []byte("v1"))
	writeMirror(t, m, "/a.txt", []byte("v2-longer"))
	got, err := os.ReadFile(filepath.Join(m.Root(), "a.txt"))
	if err != nil {
		t.Fatalf("read: %v", err)
	}
	if string(got) != "v2-longer" {
		t.Fatalf("got %q want v2-longer", got)
	}
}

func TestWriteFromStreamsMultiBlockAndPublishesAtomically(t *testing.T) {
	// R-JV0A-6XDB
	m := newTestMirror(t)
	writeMirror(t, m, "/large.bin", []byte("old bytes"))
	data := make([]byte, 8<<20+257)
	for i := range data {
		data[i] = byte(i % 251)
	}
	started := make(chan struct{})
	release := make(chan struct{})
	result := make(chan error, 1)
	go func() {
		hash, size, err := m.WriteFrom("/large.bin", &gatedReader{r: bytes.NewReader(data), started: started, release: release})
		if err == nil && (hash != ContentHash(data) || size != int64(len(data))) {
			err = errors.New("streamed hash or size mismatch")
		}
		result <- err
	}()
	<-started
	f, _, err := m.Open("/large.bin")
	if err != nil {
		t.Fatalf("Open during write: %v", err)
	}
	old, err := io.ReadAll(f)
	f.Close()
	if err != nil || string(old) != "old bytes" {
		t.Fatalf("Open during write returned partial/new data %q, err=%v", old, err)
	}
	close(release)
	if err := <-result; err != nil {
		t.Fatalf("WriteFrom: %v", err)
	}
	f, _, err = m.Open("/large.bin")
	if err != nil {
		t.Fatalf("Open after write: %v", err)
	}
	got, err := io.ReadAll(f)
	f.Close()
	if err != nil || !bytes.Equal(got, data) {
		t.Fatalf("final bytes mismatch: err=%v", err)
	}
}

func TestConfinementRejected(t *testing.T) {
	m := newTestMirror(t)
	// Any path whose cleaned form climbs above the root via ".." is rejected.
	escapes := []string{
		"../foo",
		"../../etc/passwd",
		"/../etc/passwd",
		"inbox/../../../etc/passwd",
		"a/b/../../../../outside",
	}
	for _, p := range escapes {
		_, _, err := m.WriteFrom(p, bytes.NewReader([]byte("nope")))
		if err == nil {
			t.Fatalf("Write(%q) succeeded, expected rejection", p)
		}
		if !errors.Is(err, ErrPathEscape) {
			t.Fatalf("Write(%q) error = %v, want ErrPathEscape", p, err)
		}
	}

	// A filesystem-absolute "/etc/passwd" is treated as a Dropbox root-relative
	// path: it is confined to <root>/etc/passwd and MUST NOT touch the real
	// /etc/passwd. Confinement (not rejection) is the security property here.
	before, _ := os.ReadFile("/etc/passwd")
	writeMirror(t, m, "/etc/passwd", []byte("ATTACK"))
	after, _ := os.ReadFile("/etc/passwd")
	if string(before) != string(after) {
		t.Fatalf("the real /etc/passwd was modified — confinement breached")
	}
	got, err := os.ReadFile(filepath.Join(m.Root(), "etc", "passwd"))
	if err != nil || string(got) != "ATTACK" {
		t.Fatalf("confined write did not land under root: got %q err %v", got, err)
	}

	// Nothing must have been written outside the root for the ".." escapes: the
	// parent of the root should contain only the root dir.
	parent := filepath.Dir(m.Root())
	entries, err := os.ReadDir(parent)
	if err != nil {
		t.Fatalf("readdir parent: %v", err)
	}
	for _, e := range entries {
		if e.Name() != filepath.Base(m.Root()) {
			t.Fatalf("unexpected entry written outside root: %s", e.Name())
		}
	}
}

func TestConfinementRejectsSymlinkAncestor(t *testing.T) {
	m := newTestMirror(t)
	// Create an outside dir and a symlink inside the mirror pointing to it.
	outside := t.TempDir()
	link := filepath.Join(m.Root(), "evil")
	if err := os.Symlink(outside, link); err != nil {
		t.Fatalf("symlink: %v", err)
	}
	// Writing "through" the symlink would escape the root; must be rejected.
	_, _, err := m.WriteFrom("/evil/passwd", bytes.NewReader([]byte("nope")))
	if err == nil || !errors.Is(err, ErrPathEscape) {
		t.Fatalf("write through symlink: err = %v, want ErrPathEscape", err)
	}
	if _, statErr := os.Stat(filepath.Join(outside, "passwd")); statErr == nil {
		t.Fatalf("file leaked outside the mirror via symlink")
	}
}

func TestDeleteIdempotent(t *testing.T) {
	m := newTestMirror(t)
	writeMirror(t, m, "/gone.txt", []byte("x"))

	existed, err := m.Delete("/gone.txt")
	if err != nil {
		t.Fatalf("delete: %v", err)
	}
	if !existed {
		t.Fatalf("first delete: existed = false, want true")
	}
	if _, statErr := os.Stat(filepath.Join(m.Root(), "gone.txt")); statErr == nil {
		t.Fatalf("file still present after delete")
	}

	// Deleting an already-absent path is success, existed == false.
	existed, err = m.Delete("/gone.txt")
	if err != nil {
		t.Fatalf("second delete returned error: %v", err)
	}
	if existed {
		t.Fatalf("second delete: existed = true, want false")
	}

	// A never-existed path is also success.
	existed, err = m.Delete("/never/was/here.txt")
	if err != nil {
		t.Fatalf("delete absent path returned error: %v", err)
	}
	if existed {
		t.Fatalf("absent delete: existed = true, want false")
	}
}

func TestCaseOnlyRename(t *testing.T) {
	m := newTestMirror(t)
	want := []byte("case-fold bytes")
	writeMirror(t, m, "/Foo.txt", want)

	if err := m.Rename("/Foo.txt", "/foo.txt"); err != nil {
		t.Fatalf("rename: %v", err)
	}

	// On case-sensitive ext4, the old name must be gone.
	if _, err := os.Lstat(filepath.Join(m.Root(), "Foo.txt")); err == nil {
		t.Fatalf("old name /Foo.txt still present after case-only rename")
	}
	got, err := os.ReadFile(filepath.Join(m.Root(), "foo.txt"))
	if err != nil {
		t.Fatalf("read new name: %v", err)
	}
	if string(got) != string(want) {
		t.Fatalf("bytes mismatch after rename: got %q want %q", got, want)
	}
}

func TestRenameConfined(t *testing.T) {
	m := newTestMirror(t)
	writeMirror(t, m, "/ok.txt", []byte("x"))
	if err := m.Rename("/ok.txt", "../escape.txt"); !errors.Is(err, ErrPathEscape) {
		t.Fatalf("rename to escape: err = %v, want ErrPathEscape", err)
	}
}

func TestMkdir(t *testing.T) {
	m := newTestMirror(t)
	if err := m.Mkdir("/a/b/c"); err != nil {
		t.Fatalf("mkdir: %v", err)
	}
	fi, err := os.Stat(filepath.Join(m.Root(), "a", "b", "c"))
	if err != nil {
		t.Fatalf("stat: %v", err)
	}
	if !fi.IsDir() {
		t.Fatalf("not a directory")
	}
	if perm := fi.Mode().Perm(); perm != 0o750 {
		t.Fatalf("dir mode = %o, want 0750", perm)
	}
	// Idempotent.
	if err := m.Mkdir("/a/b/c"); err != nil {
		t.Fatalf("mkdir again: %v", err)
	}
}

func TestStatFS(t *testing.T) {
	m := newTestMirror(t)
	free, total, err := m.StatFS()
	if err != nil {
		t.Fatalf("statfs: %v", err)
	}
	if total == 0 {
		t.Fatalf("total bytes = 0, want > 0")
	}
	if free == 0 {
		t.Fatalf("free bytes = 0, want > 0")
	}
	if free > total {
		t.Fatalf("free (%d) > total (%d)", free, total)
	}
}
