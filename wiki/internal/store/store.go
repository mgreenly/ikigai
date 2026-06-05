// Package store is the wiki's on-disk content model: a filesystem store, scoped
// per (owner, collection), confined under a configurable data root. It owns the
// pinned page-tree layout, the immutable raw-document writer (sha256-keyed,
// frontmatter-stamped, idempotent re-ingest), and confinement-checked page
// read/list helpers that the Phase-4 ingest core and the Task-3.2 search
// indexer both consume. Owner and collection are always parameters, never
// globals — the service is owner-scoped and collection-keyed from day one
// (PLAN Decision 4), defaulting collection to "default".
//
// Security boundary: every path the store returns or touches is verified to
// live inside the resolved <data-root>/<owner>/<collection> root. Owner and
// collection are validated as single, traversal-free path segments; arbitrary
// page paths are confined with the suite's standard symlink-aware check.
package store

import (
	"crypto/sha256"
	"encoding/hex"
	"fmt"
	"os"
	"path/filepath"
	"sort"
	"strings"
	"time"
)

// DefaultCollection is the collection key used when a caller does not specify
// one. The model carries the collection key from day one so splitting into many
// wikis later is additive (PLAN Decision 4); no verb exposes it yet.
const DefaultCollection = "default"

// Page-tree directory names. These are the curated page types (source / concept
// / entity / event / synthesis) plus the immutable raw store and the search
// slot. They are created lazily as writers need them.
const (
	dirRaw       = "raw"
	dirSources   = "sources"
	dirConcepts  = "concepts"
	dirEntities  = "entities"
	dirEvents    = "events"
	dirSynthesis = "synthesis"
	dirSearch    = ".search"

	// fileIndex and fileLog are the two navigation files at the collection root.
	fileIndex = "index.md"
	fileLog   = "log.md"

	// searchIndexFile is the per-collection SQLite search index path, owned by
	// Task 3.2. The store only reserves/returns its location; it never creates
	// the file.
	searchIndexFile = "index.sqlite"
)

// pageDirs is the set of curated page-type directories the store pre-creates so
// the ingest agent's write tool (which does NOT create parent dirs) can write
// into them. raw/ and .search/ are managed separately by their own writers.
var pageDirs = []string{dirSources, dirConcepts, dirEntities, dirEvents, dirSynthesis}

// dirPerm / filePerm are the modes used for created directories and files.
const (
	dirPerm  os.FileMode = 0o755
	filePerm os.FileMode = 0o644
)

// Store is the filesystem content store rooted at a single data root. It is
// stateless beyond that root plus a clock; all per-request scoping is passed as
// (owner, collection) arguments. A single Store instance serves every owner.
type Store struct {
	dataRoot string
	now      func() time.Time
}

// Option configures a Store.
type Option func(*Store)

// WithClock overrides the timestamp source (for deterministic tests).
func WithClock(now func() time.Time) Option {
	return func(s *Store) { s.now = now }
}

// New returns a Store rooted at dataRoot, creating dataRoot if needed. dataRoot
// is the suite's per-service data directory (e.g. /opt/wiki/data); each owner's
// collections live beneath it.
func New(dataRoot string, opts ...Option) (*Store, error) {
	if dataRoot == "" {
		return nil, fmt.Errorf("store: data root is empty")
	}
	abs, err := filepath.Abs(dataRoot)
	if err != nil {
		return nil, fmt.Errorf("store: resolve data root: %w", err)
	}
	if err := os.MkdirAll(abs, dirPerm); err != nil {
		return nil, fmt.Errorf("store: create data root: %w", err)
	}
	s := &Store{
		dataRoot: abs,
		now:      func() time.Time { return time.Now().UTC() },
	}
	for _, o := range opts {
		o(s)
	}
	return s, nil
}

// normalizeCollection applies the default and is a no-op otherwise.
func normalizeCollection(collection string) string {
	if collection == "" {
		return DefaultCollection
	}
	return collection
}

// Root resolves the confined absolute root for (owner, collection) under the
// data root, defaulting an empty collection to "default". It validates both
// segments and verifies the joined path stays inside the data root. The
// directory is NOT created — use EnsureLayout for that. Root is the single
// source of truth for "where (owner, collection) lives on disk".
func (s *Store) Root(owner, collection string) (string, error) {
	collection = normalizeCollection(collection)
	if err := validateSegment("owner", owner); err != nil {
		return "", err
	}
	if err := validateSegment("collection", collection); err != nil {
		return "", err
	}
	// owner and collection are each validated single segments, so this is the
	// only place they are joined; confine() is the belt-and-suspenders check
	// against any residual escape (e.g. via a symlinked data root).
	root := filepath.Join(s.dataRoot, owner, collection)
	if _, err := confine(s.dataRoot, filepath.Join(owner, collection)); err != nil {
		return "", err
	}
	return root, nil
}

// EnsureLayout creates the pinned on-disk layout for (owner, collection):
//
//	<root>/{raw/, sources/, concepts/, entities/, events/, synthesis/, .search/}
//
// index.md and log.md are NOT created (they are page/append files written on
// first use); .search/index.sqlite is NOT created (it is owned by the Task-3.2
// search backend — only the .search/ directory slot is reserved here).
// EnsureLayout is idempotent.
func (s *Store) EnsureLayout(owner, collection string) (string, error) {
	root, err := s.Root(owner, collection)
	if err != nil {
		return "", err
	}
	for _, d := range append([]string{dirRaw, dirSearch}, pageDirs...) {
		if err := os.MkdirAll(filepath.Join(root, d), dirPerm); err != nil {
			return "", fmt.Errorf("store: create %s dir: %w", d, err)
		}
	}
	return root, nil
}

// SearchIndexPath returns the absolute path of the per-collection search index
// SQLite file (<root>/.search/index.sqlite), reserving its location for the
// Task-3.2 search backend. The store ensures the .search/ directory exists but
// NEVER creates or opens the SQLite file itself.
func (s *Store) SearchIndexPath(owner, collection string) (string, error) {
	root, err := s.Root(owner, collection)
	if err != nil {
		return "", err
	}
	if err := os.MkdirAll(filepath.Join(root, dirSearch), dirPerm); err != nil {
		return "", fmt.Errorf("store: create .search dir: %w", err)
	}
	return filepath.Join(root, dirSearch, searchIndexFile), nil
}

// RawDoc is the result of an ingest into the immutable raw store.
type RawDoc struct {
	Sha256     string // content hash (hex), the raw-store key
	Path       string // absolute path of the stored raw doc (raw/<sha256>.md)
	RelPath    string // collection-relative path (raw/<sha256>.md)
	IngestedAt string // RFC3339 timestamp stamped into the frontmatter
	AlreadyHad bool   // true if a raw doc with this sha256 already existed (no-op)
}

// RawMeta is the caller-supplied provenance stamped onto an ingested raw doc.
// All fields are optional; the store always adds sha256 + ingested_at +
// collection. Mirrors the wiki_ingest_text verb arguments (PLAN Phase 4).
type RawMeta struct {
	Title  string
	Source string
	Tags   []string
}

// WriteRaw persists content into the immutable raw store for (owner, collection)
// and returns a RawDoc describing the stored document.
//
// The raw doc is keyed by the sha256 of the *content bytes* (not of the stamped
// file): identical bytes always map to the same raw/<sha256>.md path. The stored
// file is the caller's provenance + the store's sha256/ingested_at/collection
// rendered as `---`-fenced frontmatter, followed by the original content.
//
// Immutability invariant: if a raw doc with that sha256 already exists, WriteRaw
// does NOT rewrite or mutate it — re-ingest of identical bytes is a safe no-op.
// In that case RawDoc.AlreadyHad is true and the existing file's ingested_at is
// returned (read back from the stored frontmatter), so the original ingest time
// is preserved. The write of a new doc is atomic (temp file + rename) so a crash
// mid-write never leaves a partial raw doc.
func (s *Store) WriteRaw(owner, collection string, content []byte, meta RawMeta) (RawDoc, error) {
	collection = normalizeCollection(collection)
	root, err := s.Root(owner, collection)
	if err != nil {
		return RawDoc{}, err
	}

	sum := sha256.Sum256(content)
	hexSum := hex.EncodeToString(sum[:])
	relPath := filepath.Join(dirRaw, hexSum+".md")
	absPath, err := confine(root, relPath)
	if err != nil {
		return RawDoc{}, err
	}

	rawDir := filepath.Join(root, dirRaw)
	if err := os.MkdirAll(rawDir, dirPerm); err != nil {
		return RawDoc{}, fmt.Errorf("store: create raw dir: %w", err)
	}

	// Immutability: an existing raw doc is never rewritten. Read back its
	// ingested_at so the returned RawDoc reflects the original ingest time.
	if existing, err := os.ReadFile(absPath); err == nil {
		return RawDoc{
			Sha256:     hexSum,
			Path:       absPath,
			RelPath:    relPath,
			IngestedAt: ingestedAtFromFrontmatter(existing),
			AlreadyHad: true,
		}, nil
	} else if !os.IsNotExist(err) {
		return RawDoc{}, fmt.Errorf("store: stat raw doc: %w", err)
	}

	ingestedAt := s.now().Format(time.RFC3339)
	fm := Frontmatter{
		Sha256:     hexSum,
		IngestedAt: ingestedAt,
		Title:      meta.Title,
		Source:     meta.Source,
		Tags:       meta.Tags,
		Collection: collection,
	}
	var doc []byte
	doc = append(doc, fm.render()...)
	doc = append(doc, content...)

	if err := atomicWrite(absPath, doc); err != nil {
		return RawDoc{}, err
	}

	return RawDoc{
		Sha256:     hexSum,
		Path:       absPath,
		RelPath:    relPath,
		IngestedAt: ingestedAt,
		AlreadyHad: false,
	}, nil
}

// ReadRaw returns the full stored bytes (frontmatter + content) of the raw doc
// keyed by sha256 for (owner, collection), or an error if absent.
func (s *Store) ReadRaw(owner, collection, sha256hex string) ([]byte, error) {
	if err := validateSegment("sha256", sha256hex); err != nil {
		return nil, err
	}
	return s.ReadPage(owner, collection, filepath.Join(dirRaw, sha256hex+".md"))
}

// ReadPage returns the full bytes of the page at relPath within (owner,
// collection), confinement-checked. relPath is collection-relative
// (e.g. "concepts/otters.md", "index.md").
func (s *Store) ReadPage(owner, collection, relPath string) ([]byte, error) {
	root, err := s.Root(owner, collection)
	if err != nil {
		return nil, err
	}
	abs, err := confine(root, relPath)
	if err != nil {
		return nil, err
	}
	b, err := os.ReadFile(abs)
	if err != nil {
		if os.IsNotExist(err) {
			return nil, fmt.Errorf("store: page not found: %q", relPath)
		}
		return nil, fmt.Errorf("store: read page %q: %w", relPath, err)
	}
	return b, nil
}

// WritePage writes content to the page at relPath within (owner, collection),
// creating parent directories as needed (the ingest agent's own write tool does
// not, so the store must). The write is atomic (temp file + rename). relPath is
// confinement-checked. This is the store-side writer the ingest core uses; the
// agent toolset writes through its own confined root.
func (s *Store) WritePage(owner, collection, relPath string, content []byte) error {
	root, err := s.Root(owner, collection)
	if err != nil {
		return err
	}
	abs, err := confine(root, relPath)
	if err != nil {
		return err
	}
	if err := os.MkdirAll(filepath.Dir(abs), dirPerm); err != nil {
		return fmt.Errorf("store: create parent dir for %q: %w", relPath, err)
	}
	return atomicWrite(abs, content)
}

// AppendLog appends a line to the collection's log.md (append-don't-destroy),
// creating it if absent. A trailing newline is added if the line lacks one.
func (s *Store) AppendLog(owner, collection, line string) error {
	root, err := s.Root(owner, collection)
	if err != nil {
		return err
	}
	abs, err := confine(root, fileLog)
	if err != nil {
		return err
	}
	if err := os.MkdirAll(filepath.Dir(abs), dirPerm); err != nil {
		return fmt.Errorf("store: create collection root: %w", err)
	}
	f, err := os.OpenFile(abs, os.O_APPEND|os.O_CREATE|os.O_WRONLY, filePerm)
	if err != nil {
		return fmt.Errorf("store: open log: %w", err)
	}
	defer f.Close()
	if !strings.HasSuffix(line, "\n") {
		line += "\n"
	}
	if _, err := f.WriteString(line); err != nil {
		return fmt.Errorf("store: append log: %w", err)
	}
	return nil
}

// PageEntry describes one page discovered by ListPages.
type PageEntry struct {
	RelPath string // collection-relative path (e.g. "concepts/otters.md")
	Name    string // base filename
	Size    int64  // byte size
}

// ListPages lists the markdown pages directly under the given collection-
// relative directory (e.g. "concepts", "sources"). dir "" or "." lists the
// collection root. Only regular .md files are returned; subdirectories and
// non-markdown files are skipped. The directory is confinement-checked; a
// missing directory yields an empty list (not an error), so callers can list a
// type dir before any page of that type exists.
func (s *Store) ListPages(owner, collection, dir string) ([]PageEntry, error) {
	root, err := s.Root(owner, collection)
	if err != nil {
		return nil, err
	}
	target, err := confine(root, dir)
	if err != nil {
		return nil, err
	}
	infos, err := os.ReadDir(target)
	if err != nil {
		if os.IsNotExist(err) {
			return []PageEntry{}, nil
		}
		return nil, fmt.Errorf("store: list %q: %w", dir, err)
	}
	out := make([]PageEntry, 0, len(infos))
	for _, de := range infos {
		if de.IsDir() || !strings.HasSuffix(de.Name(), ".md") {
			continue
		}
		fi, err := de.Info()
		if err != nil {
			return nil, fmt.Errorf("store: stat %q: %w", de.Name(), err)
		}
		rel := de.Name()
		if dir != "" && dir != "." {
			rel = filepath.Join(filepath.Clean(dir), de.Name())
		}
		out = append(out, PageEntry{RelPath: rel, Name: de.Name(), Size: fi.Size()})
	}
	sort.Slice(out, func(i, j int) bool { return out[i].RelPath < out[j].RelPath })
	return out, nil
}

// WalkPages returns every curated markdown page in the collection's page tree
// (sources/, concepts/, entities/, events/, synthesis/) plus index.md, as
// collection-relative paths. raw/ and .search/ are excluded (raw docs are never
// indexed; .search is the index itself). This is the page set the Task-3.2
// search indexer walks. Order is stable (sorted by RelPath).
func (s *Store) WalkPages(owner, collection string) ([]PageEntry, error) {
	root, err := s.Root(owner, collection)
	if err != nil {
		return nil, err
	}
	var out []PageEntry
	for _, d := range pageDirs {
		entries, err := s.ListPages(owner, collection, d)
		if err != nil {
			return nil, err
		}
		out = append(out, entries...)
	}
	// index.md at the collection root, if present.
	if abs, err := confine(root, fileIndex); err == nil {
		if fi, statErr := os.Stat(abs); statErr == nil && !fi.IsDir() {
			out = append(out, PageEntry{RelPath: fileIndex, Name: fileIndex, Size: fi.Size()})
		}
	}
	sort.Slice(out, func(i, j int) bool { return out[i].RelPath < out[j].RelPath })
	return out, nil
}

// atomicWrite writes data to path via a temp file in the same directory that is
// renamed over the target, so a crash mid-write never leaves a partial file.
func atomicWrite(path string, data []byte) error {
	dir := filepath.Dir(path)
	tmp, err := os.CreateTemp(dir, ".wiki-tmp-*")
	if err != nil {
		return fmt.Errorf("store: create temp file: %w", err)
	}
	tmpPath := tmp.Name()
	cleanup := func() {
		tmp.Close()
		os.Remove(tmpPath)
	}
	if _, err := tmp.Write(data); err != nil {
		cleanup()
		return fmt.Errorf("store: write temp file: %w", err)
	}
	if err := tmp.Chmod(filePerm); err != nil {
		cleanup()
		return fmt.Errorf("store: chmod temp file: %w", err)
	}
	if err := tmp.Close(); err != nil {
		os.Remove(tmpPath)
		return fmt.Errorf("store: close temp file: %w", err)
	}
	if err := os.Rename(tmpPath, path); err != nil {
		os.Remove(tmpPath)
		return fmt.Errorf("store: rename temp file: %w", err)
	}
	return nil
}

// ingestedAtFromFrontmatter extracts the ingested_at value from a stored raw
// doc's frontmatter so an already-present raw doc reports its original ingest
// time. Returns "" if the field is absent/unparseable (best-effort).
func ingestedAtFromFrontmatter(doc []byte) string {
	const key = "ingested_at:"
	lines := strings.Split(string(doc), "\n")
	if len(lines) == 0 || strings.TrimSpace(lines[0]) != "---" {
		return ""
	}
	for _, ln := range lines[1:] {
		if strings.TrimSpace(ln) == "---" {
			break
		}
		if strings.HasPrefix(ln, key) {
			v := strings.TrimSpace(strings.TrimPrefix(ln, key))
			v = strings.Trim(v, `"`)
			return v
		}
	}
	return ""
}
