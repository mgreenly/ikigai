package store

import (
	"fmt"
	"os"
	"path/filepath"
	"strings"
)

// validateSegment ensures a single path component (owner or collection) cannot
// be used to escape the data root. It must be a single, non-empty, non-dotted
// path segment: no separators, no "." / "..", no leading/trailing whitespace
// that could mask traversal. This is the first (cheap, string-level) line of
// the confinement boundary; confine() is the second (filesystem-level) line.
func validateSegment(kind, seg string) error {
	if seg == "" {
		return fmt.Errorf("store: empty %s", kind)
	}
	if seg != strings.TrimSpace(seg) {
		return fmt.Errorf("store: %s has surrounding whitespace: %q", kind, seg)
	}
	if seg == "." || seg == ".." {
		return fmt.Errorf("store: invalid %s: %q", kind, seg)
	}
	if filepath.IsAbs(seg) {
		return fmt.Errorf("store: %s must not be absolute: %q", kind, seg)
	}
	if strings.ContainsRune(seg, '/') || strings.ContainsRune(seg, os.PathSeparator) {
		return fmt.Errorf("store: %s must not contain a path separator: %q", kind, seg)
	}
	if strings.Contains(seg, "..") {
		return fmt.Errorf("store: %s must not contain %q: %q", "..", kind, seg)
	}
	// A NUL byte (or other control) can defeat path checks at the syscall edge.
	if strings.ContainsRune(seg, 0) {
		return fmt.Errorf("store: %s contains a NUL byte", kind)
	}
	return nil
}

// confine resolves relPath against root and returns the cleaned absolute path,
// guaranteed to stay inside root, or an error. It mirrors the suite's standard
// confinement (ralph/internal/sandbox, agentkit/tools): join, Clean, resolve
// symlinks on the longest existing ancestor (catching symlink escapes), then
// verify containment with filepath.Rel. relPath "" or "." resolves to root.
//
// An absolute relPath is rejected (it would name a location independent of
// root); callers always pass collection-relative paths.
func confine(root, relPath string) (string, error) {
	if filepath.IsAbs(relPath) {
		return "", fmt.Errorf("store: path must be relative to the collection root: %q", relPath)
	}
	if strings.ContainsRune(relPath, 0) {
		return "", fmt.Errorf("store: path contains a NUL byte")
	}

	abs := filepath.Join(root, relPath)
	abs = filepath.Clean(abs)

	realRoot, err := filepath.EvalSymlinks(root)
	if err != nil {
		realRoot = filepath.Clean(root)
	}

	resolved := resolveLongestExisting(abs)

	rel, err := filepath.Rel(realRoot, resolved)
	if err != nil || rel == ".." || strings.HasPrefix(rel, ".."+string(os.PathSeparator)) {
		return "", fmt.Errorf("store: path escapes the collection root: %q", relPath)
	}
	return abs, nil
}

// resolveLongestExisting walks up abs until an existing ancestor is found,
// EvalSymlinks-resolves that prefix, and re-appends the non-existing remainder
// — so a symlink anywhere in the chain that escapes the root is caught even
// when the leaf does not exist yet.
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
