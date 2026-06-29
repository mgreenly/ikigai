// Package sandbox owns the per-run sandbox folders under the durable state tree.
//
// Each run gets its own workspace <sandboxesDir>/<run_id>/sandbox/ which is the
// agent's durable workspace for that run. This package has two jobs:
//
//  1. Lifecycle + confinement root for the engine: Create/Remove a
//     run's sandbox folder and expose its absolute path (Root) as the
//     single source of truth for "the confinement root for run X". The
//     runner hands that string to the engine's Dispatch as sandboxRoot.
//
//  2. Read surface for the MCP foreground: List and Read over a run's
//     sandbox folder, rejecting any path that escapes it. The sandbox is
//     read-only from the foreground — the agent writes via the engine toolset.
//
// The Manager is rooted at the sandboxes dir; every id it takes is a run_id, and
// resolves to <sandboxesDir>/<run_id>/sandbox.
package sandbox

import (
	"bufio"
	"fmt"
	"os"
	"path/filepath"
	"strings"
)

// Manager owns the <sandboxesDir>/<run_id>/sandbox folders under a base
// directory.
type Manager struct {
	base string
}

// Entry describes one directory member returned by List.
type Entry struct {
	Name  string `json:"name"`
	IsDir bool   `json:"is_dir"`
	Size  int64  `json:"size"`
}

// New returns a Manager rooted at baseDir (the durable sandboxes dir, e.g.
// "<state>/sandboxes"), creating baseDir if needed.
func New(baseDir string) (*Manager, error) {
	if baseDir == "" {
		return nil, fmt.Errorf("sandbox: base dir is empty")
	}
	abs, err := filepath.Abs(baseDir)
	if err != nil {
		return nil, fmt.Errorf("sandbox: resolve base dir: %w", err)
	}
	if err := os.MkdirAll(abs, 0o755); err != nil {
		return nil, fmt.Errorf("sandbox: create base dir: %w", err)
	}
	return &Manager{base: abs}, nil
}

// Create makes the empty sandbox folder for run id (idempotent — fine if
// it already exists, per the crash-recovery "forward-only on disk" rule).
func (m *Manager) Create(id string) error {
	if err := validateID(id); err != nil {
		return err
	}
	if err := os.MkdirAll(m.Root(id), 0o755); err != nil {
		return fmt.Errorf("sandbox: create run sandbox folder: %w", err)
	}
	return nil
}

// Remove deletes run id's sandbox folder and everything under it.
func (m *Manager) Remove(id string) error {
	if err := validateID(id); err != nil {
		return err
	}
	if err := os.RemoveAll(m.Root(id)); err != nil {
		return fmt.Errorf("sandbox: remove run sandbox folder: %w", err)
	}
	return nil
}

// Root returns the absolute confinement root for run id — the value the
// engine's Dispatch consumes as sandboxRoot. It resolves to
// <sandboxesDir>/<run_id>/sandbox and need not exist yet.
//
// If id is invalid (empty, contains a path separator or "..") Root returns
// the empty string; callers should Create (which validates) first.
func (m *Manager) Root(id string) string {
	if validateID(id) != nil {
		return ""
	}
	return filepath.Join(m.base, id, "sandbox")
}

// List returns the entries directly under relPath within run id's sandbox
// folder. relPath "" or "." means the sandbox root. Escapes are rejected.
func (m *Manager) List(id, relPath string) ([]Entry, error) {
	root, err := m.promptRoot(id)
	if err != nil {
		return nil, err
	}
	target, err := confine(root, relPath)
	if err != nil {
		return nil, err
	}
	infos, err := os.ReadDir(target)
	if err != nil {
		if os.IsNotExist(err) {
			return nil, fmt.Errorf("sandbox: not found: %q", relPath)
		}
		return nil, fmt.Errorf("sandbox: list %q: %w", relPath, err)
	}
	entries := make([]Entry, 0, len(infos))
	for _, de := range infos {
		fi, err := de.Info()
		if err != nil {
			return nil, fmt.Errorf("sandbox: stat %q: %w", de.Name(), err)
		}
		entries = append(entries, Entry{
			Name:  de.Name(),
			IsDir: fi.IsDir(),
			Size:  fi.Size(),
		})
	}
	return entries, nil
}

// Read returns up to limit lines of the file at relPath within run
// id's sandbox folder, starting at 1-based line offset (offset<=0 and limit<=0
// mean "from start" / "no limit"). Escapes and not-a-file are rejected.
func (m *Manager) Read(id, relPath string, offset, limit int) (string, error) {
	root, err := m.promptRoot(id)
	if err != nil {
		return "", err
	}
	target, err := confine(root, relPath)
	if err != nil {
		return "", err
	}
	fi, err := os.Stat(target)
	if err != nil {
		if os.IsNotExist(err) {
			return "", fmt.Errorf("sandbox: not found: %q", relPath)
		}
		return "", fmt.Errorf("sandbox: stat %q: %w", relPath, err)
	}
	if fi.IsDir() {
		return "", fmt.Errorf("sandbox: not a file: %q", relPath)
	}

	f, err := os.Open(target)
	if err != nil {
		return "", fmt.Errorf("sandbox: open %q: %w", relPath, err)
	}
	defer f.Close()

	if offset <= 0 {
		offset = 1
	}

	var b strings.Builder
	sc := bufio.NewScanner(f)
	sc.Buffer(make([]byte, 0, 64*1024), 16*1024*1024)
	lineNo := 0
	written := 0
	for sc.Scan() {
		lineNo++
		if lineNo < offset {
			continue
		}
		if limit > 0 && written >= limit {
			break
		}
		b.WriteString(sc.Text())
		b.WriteByte('\n')
		written++
	}
	if err := sc.Err(); err != nil {
		return "", fmt.Errorf("sandbox: read %q: %w", relPath, err)
	}
	return b.String(), nil
}

// sandboxRoot validates id and resolves the (must-exist) run sandbox root,
// resolving symlinks on the root itself.
func (m *Manager) promptRoot(id string) (string, error) {
	if err := validateID(id); err != nil {
		return "", err
	}
	root := m.Root(id)
	if _, err := os.Stat(root); err != nil {
		if os.IsNotExist(err) {
			return "", fmt.Errorf("sandbox: run %q has no sandbox folder", id)
		}
		return "", fmt.Errorf("sandbox: stat run %q: %w", id, err)
	}
	return root, nil
}

// validateID ensures id is a single non-empty path segment so a malicious
// prompt id cannot escape the base directory.
func validateID(id string) error {
	if id == "" {
		return fmt.Errorf("sandbox: empty prompt id")
	}
	if id == "." || id == ".." {
		return fmt.Errorf("sandbox: invalid prompt id: %q", id)
	}
	if strings.ContainsRune(id, '/') || strings.ContainsRune(id, os.PathSeparator) {
		return fmt.Errorf("sandbox: invalid prompt id (contains separator): %q", id)
	}
	if strings.Contains(id, "..") {
		return fmt.Errorf("sandbox: invalid prompt id (contains ..): %q", id)
	}
	return nil
}

// confine resolves relPath against the prompt root and returns the
// cleaned absolute path guaranteed within root, or an error. It mirrors
// the engine's write-path confinement: join, Clean, resolve symlinks on
// the longest existing ancestor (catching symlink escapes), then verify
// containment with filepath.Rel.
func confine(root, relPath string) (string, error) {
	abs := relPath
	if !filepath.IsAbs(abs) {
		abs = filepath.Join(root, relPath)
	}
	abs = filepath.Clean(abs)

	realRoot, err := filepath.EvalSymlinks(root)
	if err != nil {
		realRoot = filepath.Clean(root)
	}

	resolved := resolveLongestExisting(abs)

	rel, err := filepath.Rel(realRoot, resolved)
	if err != nil || rel == ".." || strings.HasPrefix(rel, ".."+string(os.PathSeparator)) {
		return "", fmt.Errorf("sandbox: path escapes prompt folder: %q", relPath)
	}
	return abs, nil
}

// resolveLongestExisting walks up abs until an existing ancestor is found,
// EvalSymlinks-resolves that prefix, and re-appends the non-existing
// remainder — so a symlink anywhere in the chain that escapes the root is
// caught even when the leaf does not exist yet.
func resolveLongestExisting(abs string) string {
	existing := abs
	var remainder string
	for {
		if _, err := os.Lstat(existing); err == nil {
			break
		}
		parent := filepath.Dir(existing)
		if parent == existing {
			return abs
		}
		remainder = filepath.Join(filepath.Base(existing), remainder)
		existing = parent
	}
	resolvedPrefix, err := filepath.EvalSymlinks(existing)
	if err != nil {
		resolvedPrefix = existing
	}
	if remainder == "" {
		return resolvedPrefix
	}
	return filepath.Join(resolvedPrefix, remainder)
}
