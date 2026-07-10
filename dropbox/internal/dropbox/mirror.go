package dropbox

import (
	"crypto/sha256"
	"encoding/hex"
	"errors"
	"fmt"
	"io"
	"io/fs"
	"os"
	"path/filepath"
	"strings"
	"syscall"
)

// Mirror manages the private local mirror directory rooted at a configured
// mirror path (DROPBOX_MIRROR_PATH; box /opt/dropbox/data/mirror, dev
// ./tmp/mirror). It is a pure filesystem primitive layer (PLAN.md §8): no db, no
// HTTP, no Dropbox API. The sync engine (Phase 4) orders these primitives to
// satisfy the crash/replay invariant (§6) — the Mirror only provides the
// building blocks.
//
// The mirror stays PRIVATE (PLAN.md §4): directories are created 0750 and files
// 0640, so only the service user can read it; bytes reach consumers solely via
// the loopback /content endpoint (Phase 5).
//
// Path confinement (PLAN.md §4, load-bearing security): every relative path the
// Mirror touches is resolved and verified to stay under the mirror root before
// any filesystem operation. Absolute paths, paths escaping via "..", and paths
// that would resolve outside the root are refused with an error (never a panic).
type Mirror struct {
	// root is the absolute, symlink-resolved mirror root. All confined paths
	// resolve to a location at or beneath it.
	root string
}

const (
	// dirMode is the private directory mode for mirror subdirectories (0750):
	// owner rwx, group r-x, others none.
	dirMode fs.FileMode = 0o750
	// fileMode is the private file mode for mirror files (0640): owner rw,
	// group r, others none.
	fileMode fs.FileMode = 0o640
)

// ErrPathEscape is returned when a requested path resolves outside the mirror
// root (absolute path, "..\" escape, or symlink trickery). It is a security
// rejection, not a transient failure — the engine should never retry it.
var ErrPathEscape = errors.New("dropbox: path escapes mirror root")

// NewMirror creates a Mirror rooted at mirrorPath, creating the root directory
// (and any parents) 0750 if absent, then resolving it through symlinks so all
// later confinement checks compare against the real on-disk root. A relative
// mirrorPath (e.g. dev ./tmp/mirror) is made absolute against the process cwd.
func NewMirror(mirrorPath string) (*Mirror, error) {
	if mirrorPath == "" {
		return nil, fmt.Errorf("dropbox: mirror path is empty")
	}
	abs, err := filepath.Abs(mirrorPath)
	if err != nil {
		return nil, fmt.Errorf("resolve mirror path: %w", err)
	}
	if err := os.MkdirAll(abs, dirMode); err != nil {
		return nil, fmt.Errorf("create mirror root: %w", err)
	}
	// EvalSymlinks resolves the root once so confinement compares real paths;
	// the root now exists so this cannot fail on a missing component.
	real, err := filepath.EvalSymlinks(abs)
	if err != nil {
		return nil, fmt.Errorf("resolve mirror root symlinks: %w", err)
	}
	return &Mirror{root: real}, nil
}

// Root returns the absolute, symlink-resolved mirror root.
func (m *Mirror) Root() string { return m.root }

// resolve confines a Dropbox-style relative path (e.g. "/inbox/report.pdf") to a
// location under the mirror root and returns the absolute on-disk path. The
// leading slash that Dropbox paths carry is treated as relative-to-root, not as
// a filesystem-absolute path.
//
// Confinement is enforced by cleaning the joined path and verifying the result
// is the root itself or lies under root + os.PathSeparator. Because filepath.Join
// already applies filepath.Clean (collapsing ".." lexically), an escape attempt
// like "../../etc/passwd" cannot climb above the root via the lexical check.
// Symlink trickery is defended against by EvalSymlinks on the deepest existing
// ancestor and re-checking containment — a symlink pointing outside the root is
// refused even though the lexical path looked contained.
func (m *Mirror) resolve(rel string) (string, error) {
	// Strip the Dropbox leading slash(es) so the path is treated as relative to
	// the mirror root rather than the filesystem root.
	clean := strings.TrimLeft(filepath.ToSlash(rel), "/")
	if clean == "" {
		// The root itself is a valid target (e.g. Mkdir of the app-folder root).
		return m.root, nil
	}
	joined := filepath.Join(m.root, filepath.FromSlash(clean))
	if !withinRoot(m.root, joined) {
		return "", fmt.Errorf("%w: %q", ErrPathEscape, rel)
	}
	// Defend against symlink trickery: resolve the deepest existing ancestor of
	// the target through symlinks and require it to remain under the root. New
	// (not-yet-created) leaf components are fine — only existing components can
	// carry a symlink that redirects outside the root.
	resolved, err := evalExisting(joined)
	if err != nil {
		return "", err
	}
	if !withinRoot(m.root, resolved) {
		return "", fmt.Errorf("%w: %q", ErrPathEscape, rel)
	}
	return joined, nil
}

// withinRoot reports whether target is the root itself or lies strictly under it,
// using a path-separator boundary so "/a/rootx" is not considered under "/a/root".
func withinRoot(root, target string) bool {
	if target == root {
		return true
	}
	return strings.HasPrefix(target, root+string(os.PathSeparator))
}

// evalExisting resolves symlinks on the longest existing prefix of p (the leaf
// and any missing parents may not exist yet), re-attaching the not-yet-created
// trailing components. This catches a symlink in an existing ancestor that
// redirects outside the root, while still permitting writes that create new
// files/dirs under the root.
func evalExisting(p string) (string, error) {
	cur := p
	var tail []string
	for {
		resolved, err := filepath.EvalSymlinks(cur)
		if err == nil {
			return filepath.Join(append([]string{resolved}, tail...)...), nil
		}
		if !errors.Is(err, fs.ErrNotExist) {
			return "", fmt.Errorf("resolve symlinks: %w", err)
		}
		parent := filepath.Dir(cur)
		if parent == cur {
			// Reached the filesystem root without finding an existing prefix;
			// nothing to resolve, return p unchanged for the lexical check.
			return p, nil
		}
		tail = append([]string{filepath.Base(cur)}, tail...)
		cur = parent
	}
}

// WriteFrom atomically streams src to a confined relative path. It creates parent
// directories as needed (mkdir -p, 0750), writes to a temp file in the SAME
// directory as the destination (so the final os.Rename is atomic on one
// filesystem), syncs and closes it, then renames it over the final path. A
// reader (or /content) therefore never observes a partial file: it sees either
// the old bytes or the complete new bytes. Any error removes the temp file.
func (m *Mirror) WriteFrom(rel string, src io.Reader) (contentHash string, size int64, err error) {
	dst, err := m.resolve(rel)
	if err != nil {
		return "", 0, err
	}
	dir := filepath.Dir(dst)
	if err := os.MkdirAll(dir, dirMode); err != nil {
		return "", 0, fmt.Errorf("mkdir parents: %w", err)
	}
	tmp, err := os.CreateTemp(dir, ".tmp-"+filepath.Base(dst)+"-*")
	if err != nil {
		return "", 0, fmt.Errorf("create temp: %w", err)
	}
	tmpName := tmp.Name()
	cleanup := func() { _ = os.Remove(tmpName) }

	hasher := newStreamingContentHash()
	size, err = io.CopyBuffer(io.MultiWriter(tmp, hasher), src, make([]byte, 32*1024))
	if err != nil {
		_ = tmp.Close()
		cleanup()
		return "", 0, fmt.Errorf("write temp: %w", err)
	}
	if err := tmp.Sync(); err != nil {
		_ = tmp.Close()
		cleanup()
		return "", 0, fmt.Errorf("sync temp: %w", err)
	}
	if err := tmp.Close(); err != nil {
		cleanup()
		return "", 0, fmt.Errorf("close temp: %w", err)
	}
	// CreateTemp makes 0600; tighten to the private file mode before publishing.
	if err := os.Chmod(tmpName, fileMode); err != nil {
		cleanup()
		return "", 0, fmt.Errorf("chmod temp: %w", err)
	}
	if err := os.Rename(tmpName, dst); err != nil {
		cleanup()
		return "", 0, fmt.Errorf("rename into place: %w", err)
	}
	return hasher.Sum(), size, nil
}

// Open returns a seekable handle and metadata for the confined file at rel.
// Callers close the returned file.
func (m *Mirror) Open(rel string) (*os.File, os.FileInfo, error) {
	dst, err := m.resolve(rel)
	if err != nil {
		return nil, nil, err
	}
	f, err := os.Open(dst)
	if err != nil {
		return nil, nil, fmt.Errorf("open mirror file: %w", err)
	}
	info, err := f.Stat()
	if err != nil {
		_ = f.Close()
		return nil, nil, fmt.Errorf("stat mirror file: %w", err)
	}
	return f, info, nil
}

// streamingContentHash calculates Dropbox's block-SHA256 tree without holding
// a whole block (or file) in memory.
type streamingContentHash struct {
	overall io.Writer
	block   hashWriter
	inBlock int
}

type hashWriter interface {
	io.Writer
	Sum([]byte) []byte
	Reset()
}

func newStreamingContentHash() *streamingContentHash {
	return &streamingContentHash{overall: sha256.New(), block: sha256.New()}
}

func (h *streamingContentHash) Write(p []byte) (int, error) {
	original := len(p)
	for len(p) > 0 {
		n := contentHashBlockSize - h.inBlock
		if n > len(p) {
			n = len(p)
		}
		if _, err := h.block.Write(p[:n]); err != nil {
			return 0, err
		}
		h.inBlock += n
		p = p[n:]
		if h.inBlock == contentHashBlockSize {
			if _, err := h.overall.Write(h.block.Sum(nil)); err != nil {
				return 0, err
			}
			h.block.Reset()
			h.inBlock = 0
		}
	}
	return original, nil
}

func (h *streamingContentHash) Sum() string {
	if h.inBlock != 0 {
		_, _ = h.overall.Write(h.block.Sum(nil))
		h.block.Reset()
		h.inBlock = 0
	}
	if overall, ok := h.overall.(interface{ Sum([]byte) []byte }); ok {
		return hex.EncodeToString(overall.Sum(nil))
	}
	return ""
}

// Delete unlinks the file at a confined relative path. Deleting an
// already-absent path is NOT an error (idempotent): it reports existed == false
// and a nil error. This idempotency is load-bearing for the Phase 4 crash/replay
// invariant (PLAN.md §6: a delete delta replayed on an already-removed file must
// unlink without re-emitting an event). Returns existed == true when a file was
// actually removed.
func (m *Mirror) Delete(rel string) (existed bool, err error) {
	dst, err := m.resolve(rel)
	if err != nil {
		return false, err
	}
	if err := os.Remove(dst); err != nil {
		if errors.Is(err, fs.ErrNotExist) {
			return false, nil
		}
		return false, fmt.Errorf("delete: %w", err)
	}
	return true, nil
}

// RemoveTree removes a confined file or directory tree. It is idempotent and is
// used after the index transaction for a Dropbox folder delete.
func (m *Mirror) RemoveTree(rel string) error {
	dst, err := m.resolve(rel)
	if err != nil {
		return err
	}
	if err := os.RemoveAll(dst); err != nil {
		return fmt.Errorf("remove tree: %w", err)
	}
	return nil
}

// Mkdir creates a directory (and parents) at a confined relative path, 0750. Used
// when Dropbox reports folder entries (PLAN.md §2: folders are structural —
// mkdir on the mirror, no event). Idempotent: an existing directory is fine.
func (m *Mirror) Mkdir(rel string) error {
	dst, err := m.resolve(rel)
	if err != nil {
		return err
	}
	if err := os.MkdirAll(dst, dirMode); err != nil {
		return fmt.Errorf("mkdir: %w", err)
	}
	return nil
}

// Rename moves oldRel to newRel, both confined under the root, creating the
// destination's parent dirs as needed. It handles case-only renames
// (e.g. /Foo.txt -> /foo.txt) safely by going through a unique temp name in the
// destination directory first, so a case-only rename cannot collide with itself
// on either a case-sensitive or a case-insensitive filesystem (PLAN.md §2: a
// case-only rename is applied as an on-disk rename, never a second copy). The
// dev/box FS is case-sensitive ext4, but the temp-hop keeps this correct
// regardless.
func (m *Mirror) Rename(oldRel, newRel string) error {
	oldPath, err := m.resolve(oldRel)
	if err != nil {
		return err
	}
	newPath, err := m.resolve(newRel)
	if err != nil {
		return err
	}
	if oldPath == newPath {
		return nil
	}
	if err := os.MkdirAll(filepath.Dir(newPath), dirMode); err != nil {
		return fmt.Errorf("mkdir rename parents: %w", err)
	}
	// Two-step rename through a temp name in the destination directory so a
	// case-only change can't collide on a case-insensitive FS.
	tmp, err := os.CreateTemp(filepath.Dir(newPath), ".rename-*")
	if err != nil {
		return fmt.Errorf("create rename temp: %w", err)
	}
	tmpName := tmp.Name()
	_ = tmp.Close()
	if err := os.Remove(tmpName); err != nil {
		return fmt.Errorf("clear rename temp: %w", err)
	}
	if err := os.Rename(oldPath, tmpName); err != nil {
		return fmt.Errorf("rename to temp: %w", err)
	}
	if err := os.Rename(tmpName, newPath); err != nil {
		// Best-effort restore so we don't strand the source under the temp name.
		_ = os.Rename(tmpName, oldPath)
		return fmt.Errorf("rename temp to dest: %w", err)
	}
	return nil
}

// StatFS reports the mirror filesystem's free and total bytes (PLAN.md §3:
// disk_free_bytes / disk_total_bytes for dropbox_health). It uses the stdlib
// syscall.Statfs (Linux) rather than golang.org/x/sys/unix: syscall covers the
// statfs need on the only target OS (linux/amd64) without promoting x/sys from
// an indirect to a direct dependency, per the plan's "prefer stdlib syscall"
// guidance.
func (m *Mirror) StatFS() (freeBytes, totalBytes uint64, err error) {
	var st syscall.Statfs_t
	if err := syscall.Statfs(m.root, &st); err != nil {
		return 0, 0, fmt.Errorf("statfs: %w", err)
	}
	bsize := uint64(st.Bsize)
	// Total = all blocks; free = blocks available to an unprivileged user
	// (Bavail), which is the meaningful "space we can actually use" figure.
	totalBytes = st.Blocks * bsize
	freeBytes = st.Bavail * bsize
	return freeBytes, totalBytes, nil
}
