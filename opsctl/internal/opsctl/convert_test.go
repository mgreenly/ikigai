package opsctl

import (
	"context"
	"os"
	"path/filepath"
	"testing"
)

func TestConvertOldLayoutMovesLiveServiceToStateCacheLibexecAndIsIdempotent(t *testing.T) {
	// R-4MSB-T2SS
	root := t.TempDir()
	app := "ledger"
	version := "v1.2.3"
	l := NewLayout(root, app)
	oldData := filepath.Join(l.AppDir(), "data")
	oldRelease := filepath.Join(l.AppDir(), "releases", version)

	writeFile(t, filepath.Join(oldData, app+".db"), []byte("db bytes"), 0o640)
	writeFile(t, filepath.Join(oldData, app+".db-wal"), []byte("wal bytes"), 0o640)
	writeFile(t, filepath.Join(oldData, app+".db-shm"), []byte("shm bytes"), 0o640)
	writeFile(t, filepath.Join(oldData, app+".db.generation"), []byte("42"), 0o640)
	writeFile(t, filepath.Join(oldRelease, app), []byte("#!/bin/sh\necho ledger\n"), 0o755)
	if err := os.Symlink(filepath.Join("releases", version), filepath.Join(l.AppDir(), "current")); err != nil {
		t.Fatalf("symlink current: %v", err)
	}

	o := &Opsctl{Root: root}
	if err := o.ConvertOldLayout(context.Background(), app); err != nil {
		t.Fatalf("ConvertOldLayout first run: %v", err)
	}
	assertFileBytes(t, l.DBPath(), []byte("db bytes"))
	assertFileBytes(t, filepath.Join(l.StateDir(), app+".db-wal"), []byte("wal bytes"))
	assertFileBytes(t, filepath.Join(l.StateDir(), app+".db-shm"), []byte("shm bytes"))
	assertFileBytes(t, l.GenerationPath(), []byte("42"))
	assertFileBytes(t, l.LibexecBinary(version), []byte("#!/bin/sh\necho ledger\n"))
	assertExecutable(t, l.LibexecBinary(version))
	resolved, err := filepath.EvalSymlinks(l.RunLink())
	if err != nil {
		t.Fatalf("eval bin/run: %v", err)
	}
	if resolved != l.LibexecBinary(version) {
		t.Fatalf("bin/run resolves to %q, want %q", resolved, l.LibexecBinary(version))
	}

	if err := o.ConvertOldLayout(context.Background(), app); err != nil {
		t.Fatalf("ConvertOldLayout second run: %v", err)
	}
	assertFileBytes(t, l.DBPath(), []byte("db bytes"))
	assertFileBytes(t, filepath.Join(l.StateDir(), app+".db-wal"), []byte("wal bytes"))
	assertFileBytes(t, filepath.Join(l.StateDir(), app+".db-shm"), []byte("shm bytes"))
	assertFileBytes(t, l.GenerationPath(), []byte("42"))
	assertFileBytes(t, l.LibexecBinary(version), []byte("#!/bin/sh\necho ledger\n"))
}

func TestConvertOldLayoutCompletesHalfConvertedTreeWithoutDroppingData(t *testing.T) {
	// R-4MSB-T2SS
	root := t.TempDir()
	app := "crm"
	version := "v2.0.0"
	l := NewLayout(root, app)
	oldData := filepath.Join(l.AppDir(), "data")
	oldRelease := filepath.Join(l.AppDir(), "releases", version)

	writeFile(t, l.DBPath(), []byte("state already moved"), 0o640)
	writeFile(t, filepath.Join(oldData, app+".db-wal"), []byte("remaining wal"), 0o640)
	writeFile(t, filepath.Join(oldData, app+".db.generation"), []byte("remaining generation"), 0o640)
	writeFile(t, l.LibexecBinary(version), []byte("binary already moved"), 0o755)
	writeFile(t, filepath.Join(oldRelease, app), []byte("binary already moved"), 0o755)
	if err := os.Symlink(filepath.Join("releases", version), filepath.Join(l.AppDir(), "current")); err != nil {
		t.Fatalf("symlink current: %v", err)
	}

	if err := (&Opsctl{Root: root}).ConvertOldLayout(context.Background(), app); err != nil {
		t.Fatalf("ConvertOldLayout half-converted tree: %v", err)
	}
	assertFileBytes(t, l.DBPath(), []byte("state already moved"))
	assertFileBytes(t, filepath.Join(l.StateDir(), app+".db-wal"), []byte("remaining wal"))
	assertFileBytes(t, l.GenerationPath(), []byte("remaining generation"))
	assertFileBytes(t, l.LibexecBinary(version), []byte("binary already moved"))
	resolved, err := filepath.EvalSymlinks(l.RunLink())
	if err != nil {
		t.Fatalf("eval bin/run: %v", err)
	}
	if resolved != l.LibexecBinary(version) {
		t.Fatalf("bin/run resolves to %q, want %q", resolved, l.LibexecBinary(version))
	}
}

func writeFile(t *testing.T, path string, contents []byte, mode os.FileMode) {
	t.Helper()
	if err := os.MkdirAll(filepath.Dir(path), 0o755); err != nil {
		t.Fatalf("mkdir %s: %v", filepath.Dir(path), err)
	}
	if err := os.WriteFile(path, contents, mode); err != nil {
		t.Fatalf("write %s: %v", path, err)
	}
}

func assertFileBytes(t *testing.T, path string, want []byte) {
	t.Helper()
	got, err := os.ReadFile(path)
	if err != nil {
		t.Fatalf("read %s: %v", path, err)
	}
	if string(got) != string(want) {
		t.Fatalf("%s = %q, want %q", path, got, want)
	}
}

func assertExecutable(t *testing.T, path string) {
	t.Helper()
	fi, err := os.Stat(path)
	if err != nil {
		t.Fatalf("stat %s: %v", path, err)
	}
	if fi.Mode().Perm()&0o111 == 0 {
		t.Fatalf("%s mode %v is not executable", path, fi.Mode().Perm())
	}
}
