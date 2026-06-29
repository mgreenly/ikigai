package sandbox

import (
	"os"
	"path/filepath"
	"runtime"
	"strings"
	"testing"
)

func newManager(t *testing.T) *Manager {
	t.Helper()
	m, err := New(filepath.Join(t.TempDir(), "sandboxes"))
	if err != nil {
		t.Fatalf("New: %v", err)
	}
	return m
}

func TestRootUsesDurableSandboxesBase(t *testing.T) {
	// R-4LKF-FB23
	base := filepath.Join(t.TempDir(), "state", "sandboxes")
	m, err := New(base)
	if err != nil {
		t.Fatalf("New: %v", err)
	}
	if got, want := m.Root("run1"), filepath.Join(base, "run1", "sandbox"); got != want {
		t.Fatalf("Root = %q, want %q", got, want)
	}
	if err := m.Create("run1"); err != nil {
		t.Fatalf("Create: %v", err)
	}
	if fi, err := os.Stat(filepath.Join(base, "run1", "sandbox")); err != nil || !fi.IsDir() {
		t.Fatalf("durable sandbox not created under state/sandboxes: fi=%v err=%v", fi, err)
	}
}

func TestCreateRootRemove(t *testing.T) {
	m := newManager(t)

	if !filepath.IsAbs(m.Root("s1")) {
		t.Fatalf("Root not absolute: %q", m.Root("s1"))
	}

	if err := m.Create("s1"); err != nil {
		t.Fatalf("Create: %v", err)
	}
	// Idempotent.
	if err := m.Create("s1"); err != nil {
		t.Fatalf("Create (second): %v", err)
	}
	if fi, err := os.Stat(m.Root("s1")); err != nil || !fi.IsDir() {
		t.Fatalf("session folder not a dir: err=%v", err)
	}

	if err := m.Remove("s1"); err != nil {
		t.Fatalf("Remove: %v", err)
	}
	if _, err := os.Stat(m.Root("s1")); !os.IsNotExist(err) {
		t.Fatalf("folder should be gone, stat err=%v", err)
	}
	// List after remove errors.
	if _, err := m.List("s1", ""); err == nil {
		t.Fatalf("List after Remove should error")
	}
	// Re-create works.
	if err := m.Create("s1"); err != nil {
		t.Fatalf("Create after Remove: %v", err)
	}
}

func TestInvalidID(t *testing.T) {
	m := newManager(t)
	for _, id := range []string{"", ".", "..", "a/b", "../evil", "x/../y"} {
		if err := m.Create(id); err == nil {
			t.Errorf("Create(%q) should error", id)
		}
		if _, err := m.List(id, ""); err == nil {
			t.Errorf("List(%q) should error", id)
		}
		if _, err := m.Read(id, "f", 0, 0); err == nil {
			t.Errorf("Read(%q) should error", id)
		}
	}
}

func TestListAndRead(t *testing.T) {
	m := newManager(t)
	if err := m.Create("s1"); err != nil {
		t.Fatalf("Create: %v", err)
	}
	root := m.Root("s1")

	// Simulate the agent writing on disk.
	if err := os.WriteFile(filepath.Join(root, "file.txt"), []byte("a\nb\nc\nd\n"), 0o644); err != nil {
		t.Fatal(err)
	}
	if err := os.MkdirAll(filepath.Join(root, "subdir"), 0o755); err != nil {
		t.Fatal(err)
	}
	if err := os.WriteFile(filepath.Join(root, "subdir", "inner.txt"), []byte("hello"), 0o644); err != nil {
		t.Fatal(err)
	}

	entries, err := m.List("s1", "")
	if err != nil {
		t.Fatalf("List(\"\"): %v", err)
	}
	got := map[string]Entry{}
	for _, e := range entries {
		got[e.Name] = e
	}
	if len(got) != 2 {
		t.Fatalf("expected 2 entries, got %d: %+v", len(got), entries)
	}
	if e, ok := got["file.txt"]; !ok || e.IsDir || e.Size != 8 {
		t.Errorf("file.txt entry wrong: %+v ok=%v", e, ok)
	}
	if e, ok := got["subdir"]; !ok || !e.IsDir {
		t.Errorf("subdir entry wrong: %+v ok=%v", e, ok)
	}

	// Descend.
	sub, err := m.List("s1", "subdir")
	if err != nil {
		t.Fatalf("List(subdir): %v", err)
	}
	if len(sub) != 1 || sub[0].Name != "inner.txt" {
		t.Fatalf("List(subdir) wrong: %+v", sub)
	}

	// "." means root.
	if dot, err := m.List("s1", "."); err != nil || len(dot) != 2 {
		t.Fatalf("List(\".\") wrong: %+v err=%v", dot, err)
	}

	// Read full file.
	content, err := m.Read("s1", "file.txt", 0, 0)
	if err != nil {
		t.Fatalf("Read: %v", err)
	}
	if content != "a\nb\nc\nd\n" {
		t.Errorf("Read full got %q", content)
	}

	// Read with offset/limit: lines 2..3.
	slice, err := m.Read("s1", "file.txt", 2, 2)
	if err != nil {
		t.Fatalf("Read offset/limit: %v", err)
	}
	if slice != "b\nc\n" {
		t.Errorf("Read(2,2) got %q", slice)
	}

	// offset past end → empty.
	if s, err := m.Read("s1", "file.txt", 99, 0); err != nil || s != "" {
		t.Errorf("Read past end got %q err=%v", s, err)
	}
}

func TestReadErrors(t *testing.T) {
	m := newManager(t)
	if err := m.Create("s1"); err != nil {
		t.Fatalf("Create: %v", err)
	}
	if err := os.MkdirAll(filepath.Join(m.Root("s1"), "dir"), 0o755); err != nil {
		t.Fatal(err)
	}

	// Read of a directory → not-a-file.
	if _, err := m.Read("s1", "dir", 0, 0); err == nil || !strings.Contains(err.Error(), "not a file") {
		t.Errorf("Read(dir) expected not-a-file, got %v", err)
	}
	// Read of non-existent → not-found.
	if _, err := m.Read("s1", "nope.txt", 0, 0); err == nil || !strings.Contains(err.Error(), "not found") {
		t.Errorf("Read(nope) expected not-found, got %v", err)
	}
	// List of non-existent → not-found.
	if _, err := m.List("s1", "nope"); err == nil || !strings.Contains(err.Error(), "not found") {
		t.Errorf("List(nope) expected not-found, got %v", err)
	}
}

func TestEscapeRejection(t *testing.T) {
	m := newManager(t)
	if err := m.Create("s1"); err != nil {
		t.Fatalf("Create: %v", err)
	}
	root := m.Root("s1")

	// An outside dir with a secret file, both reachable absolutely.
	outside := t.TempDir()
	secret := filepath.Join(outside, "secret.txt")
	if err := os.WriteFile(secret, []byte("top secret"), 0o644); err != nil {
		t.Fatal(err)
	}

	// ../ traversal.
	if _, err := m.List("s1", "../outside"); err == nil {
		t.Errorf("List(../outside) should be rejected")
	}
	if _, err := m.Read("s1", "../../../etc/passwd", 0, 0); err == nil {
		t.Errorf("Read(../../etc/passwd) should be rejected")
	}

	// Absolute path outside the root.
	if _, err := m.Read("s1", secret, 0, 0); err == nil {
		t.Errorf("Read(abs outside) should be rejected")
	}
	if _, err := m.List("s1", outside); err == nil {
		t.Errorf("List(abs outside) should be rejected")
	}

	// Symlink escape: a symlink inside the session pointing to outside.
	if runtime.GOOS != "windows" {
		link := filepath.Join(root, "escape")
		if err := os.Symlink(outside, link); err != nil {
			t.Fatalf("symlink: %v", err)
		}
		if _, err := m.List("s1", "escape"); err == nil {
			t.Errorf("List through escaping symlink should be rejected")
		}
		if _, err := m.Read("s1", "escape/secret.txt", 0, 0); err == nil {
			t.Errorf("Read through escaping symlink should be rejected")
		}
	}
}
