package store

import (
	"crypto/sha256"
	"encoding/hex"
	"os"
	"path/filepath"
	"strings"
	"testing"
	"time"
)

func newStore(t *testing.T) *Store {
	t.Helper()
	s, err := New(t.TempDir())
	if err != nil {
		t.Fatalf("New: %v", err)
	}
	return s
}

func TestRoot_DefaultsCollection(t *testing.T) {
	s := newStore(t)
	a, err := s.Root("alice@example.com", "")
	if err != nil {
		t.Fatalf("Root: %v", err)
	}
	b, err := s.Root("alice@example.com", DefaultCollection)
	if err != nil {
		t.Fatalf("Root default: %v", err)
	}
	if a != b {
		t.Fatalf("empty collection did not default to %q: %q vs %q", DefaultCollection, a, b)
	}
	if !strings.HasSuffix(a, filepath.Join("alice@example.com", "default")) {
		t.Fatalf("unexpected root layout: %q", a)
	}
}

func TestEnsureLayout_CreatesPinnedTree(t *testing.T) {
	s := newStore(t)
	root, err := s.EnsureLayout("bob", "default")
	if err != nil {
		t.Fatalf("EnsureLayout: %v", err)
	}
	for _, d := range []string{"raw", "sources", "concepts", "entities", "events", "synthesis", ".search"} {
		fi, err := os.Stat(filepath.Join(root, d))
		if err != nil {
			t.Fatalf("missing dir %q: %v", d, err)
		}
		if !fi.IsDir() {
			t.Fatalf("%q is not a directory", d)
		}
	}
	// index.md / log.md / the sqlite file must NOT be created by EnsureLayout.
	for _, f := range []string{"index.md", "log.md", filepath.Join(".search", "index.sqlite")} {
		if _, err := os.Stat(filepath.Join(root, f)); !os.IsNotExist(err) {
			t.Fatalf("EnsureLayout should not create %q (err=%v)", f, err)
		}
	}
}

func TestSearchIndexPath_ReservesSlotWithoutCreatingFile(t *testing.T) {
	s := newStore(t)
	p, err := s.SearchIndexPath("bob", "")
	if err != nil {
		t.Fatalf("SearchIndexPath: %v", err)
	}
	if !strings.HasSuffix(p, filepath.Join(".search", "index.sqlite")) {
		t.Fatalf("unexpected search index path: %q", p)
	}
	// The .search dir is reserved, but the sqlite file is owned by Task 3.2 and
	// must not exist yet.
	if _, err := os.Stat(filepath.Dir(p)); err != nil {
		t.Fatalf(".search dir not reserved: %v", err)
	}
	if _, err := os.Stat(p); !os.IsNotExist(err) {
		t.Fatalf("store must not create the sqlite file (err=%v)", err)
	}
}

func TestWriteRaw_FrontmatterStamped(t *testing.T) {
	fixed := time.Date(2026, 6, 4, 12, 0, 0, 0, time.UTC)
	s, err := New(t.TempDir(), WithClock(func() time.Time { return fixed }))
	if err != nil {
		t.Fatalf("New: %v", err)
	}

	content := []byte("# Otters\n\nOtters are mustelids.\n")
	meta := RawMeta{
		Title:  "Otter notes",
		Source: "chat:2026-06-04",
		Tags:   []string{"wildlife", "mammals"},
	}
	rd, err := s.WriteRaw("alice", "default", content, meta)
	if err != nil {
		t.Fatalf("WriteRaw: %v", err)
	}

	wantSum := sha256.Sum256(content)
	if rd.Sha256 != hex.EncodeToString(wantSum[:]) {
		t.Fatalf("sha256 mismatch: got %q", rd.Sha256)
	}
	if rd.AlreadyHad {
		t.Fatal("first write reported AlreadyHad=true")
	}
	if rd.RelPath != filepath.Join("raw", rd.Sha256+".md") {
		t.Fatalf("unexpected RelPath: %q", rd.RelPath)
	}

	stored, err := os.ReadFile(rd.Path)
	if err != nil {
		t.Fatalf("read stored raw doc: %v", err)
	}
	body := string(stored)

	for _, want := range []string{
		"---\n",
		"type: source\n",
		"sha256: \"" + rd.Sha256 + "\"",
		"ingested_at: \"" + fixed.Format(time.RFC3339) + "\"",
		`title: "Otter notes"`,
		`source: "chat:2026-06-04"`,
		`tags: ["wildlife", "mammals"]`,
		`collection: "default"`,
	} {
		if !strings.Contains(body, want) {
			t.Errorf("stored frontmatter missing %q\n--- doc ---\n%s", want, body)
		}
	}
	// The original content must follow the frontmatter unchanged.
	if !strings.Contains(body, string(content)) {
		t.Errorf("stored doc does not contain original content verbatim:\n%s", body)
	}
}

func TestWriteRaw_IdempotentOnIdenticalBytes(t *testing.T) {
	s := newStore(t)
	content := []byte("identical bytes\n")

	first, err := s.WriteRaw("alice", "default", content, RawMeta{Title: "v1"})
	if err != nil {
		t.Fatalf("first WriteRaw: %v", err)
	}
	if first.AlreadyHad {
		t.Fatal("first write reported AlreadyHad=true")
	}

	infoBefore, err := os.Stat(first.Path)
	if err != nil {
		t.Fatalf("stat after first write: %v", err)
	}
	bytesBefore, err := os.ReadFile(first.Path)
	if err != nil {
		t.Fatalf("read after first write: %v", err)
	}

	// Sleep beyond mtime granularity, then re-ingest identical bytes with
	// DIFFERENT metadata: the existing immutable doc must not be rewritten.
	time.Sleep(20 * time.Millisecond)
	second, err := s.WriteRaw("alice", "default", content, RawMeta{Title: "v2-different-meta"})
	if err != nil {
		t.Fatalf("second WriteRaw: %v", err)
	}

	if second.Sha256 != first.Sha256 {
		t.Fatalf("sha256 changed across identical writes: %q -> %q", first.Sha256, second.Sha256)
	}
	if second.Path != first.Path {
		t.Fatalf("path changed across identical writes: %q -> %q", first.Path, second.Path)
	}
	if !second.AlreadyHad {
		t.Fatal("second write of identical bytes did not report AlreadyHad=true")
	}

	infoAfter, err := os.Stat(second.Path)
	if err != nil {
		t.Fatalf("stat after second write: %v", err)
	}
	if !infoAfter.ModTime().Equal(infoBefore.ModTime()) {
		t.Fatalf("immutable raw doc was rewritten: mtime %v -> %v", infoBefore.ModTime(), infoAfter.ModTime())
	}
	bytesAfter, err := os.ReadFile(second.Path)
	if err != nil {
		t.Fatalf("read after second write: %v", err)
	}
	if string(bytesAfter) != string(bytesBefore) {
		t.Fatalf("immutable raw doc content changed across re-ingest")
	}
	// No duplicate file in raw/.
	entries, err := os.ReadDir(filepath.Dir(first.Path))
	if err != nil {
		t.Fatalf("read raw dir: %v", err)
	}
	n := 0
	for _, e := range entries {
		if strings.HasSuffix(e.Name(), ".md") {
			n++
		}
	}
	if n != 1 {
		t.Fatalf("expected exactly 1 raw doc, found %d", n)
	}
}

func TestConfinement_RejectsEscapes(t *testing.T) {
	s := newStore(t)

	// Bad owner / collection segments.
	badSegs := []struct{ owner, coll string }{
		{"..", "default"},
		{"../etc", "default"},
		{"a/b", "default"},
		{"alice", ".."},
		{"alice", "../../etc"},
		{"alice", "a/b"},
		{"", "default"},
		{"alice", "."},
		{".", "default"},
		{"/abs", "default"},
		{"alice", "/abs"},
		{" alice", "default"},
	}
	for _, bs := range badSegs {
		if _, err := s.Root(bs.owner, bs.coll); err == nil {
			t.Errorf("Root(%q,%q) should have been rejected", bs.owner, bs.coll)
		}
	}

	// Legit segments resolve inside the data root.
	root, err := s.Root("alice@example.com", "team-x")
	if err != nil {
		t.Fatalf("legit Root rejected: %v", err)
	}
	if !strings.HasPrefix(root, s.dataRoot) {
		t.Fatalf("root %q escaped data root %q", root, s.dataRoot)
	}

	// Bad page paths on read/write/list.
	badPaths := []string{
		"../secret.md",
		"../../etc/passwd",
		"concepts/../../escape.md",
		"/etc/passwd",
	}
	for _, p := range badPaths {
		if _, err := s.ReadPage("alice", "default", p); err == nil {
			t.Errorf("ReadPage(%q) should have been rejected", p)
		}
		if err := s.WritePage("alice", "default", p, []byte("x")); err == nil {
			t.Errorf("WritePage(%q) should have been rejected", p)
		}
		if _, err := s.ListPages("alice", "default", p); err == nil {
			t.Errorf("ListPages(%q) should have been rejected", p)
		}
	}
}

func TestConfinement_RejectsSymlinkEscape(t *testing.T) {
	s := newStore(t)
	root, err := s.EnsureLayout("alice", "default")
	if err != nil {
		t.Fatalf("EnsureLayout: %v", err)
	}
	// Plant a symlink inside the collection that points outside the data root.
	outside := t.TempDir()
	link := filepath.Join(root, "concepts", "escape")
	if err := os.Symlink(outside, link); err != nil {
		t.Skipf("symlink unsupported: %v", err)
	}
	// Writing through the symlink to a path that lands outside must be rejected.
	if err := s.WritePage("alice", "default", "concepts/escape/pwned.md", []byte("x")); err == nil {
		t.Fatal("write through escaping symlink was not rejected")
	}
}

func TestPageReadWriteList(t *testing.T) {
	s := newStore(t)

	// WritePage creates parent dirs (the agent's write tool does not).
	if err := s.WritePage("alice", "default", "concepts/otters.md", []byte("# Otters\n")); err != nil {
		t.Fatalf("WritePage: %v", err)
	}
	if err := s.WritePage("alice", "default", "concepts/beavers.md", []byte("# Beavers\n")); err != nil {
		t.Fatalf("WritePage: %v", err)
	}
	if err := s.WritePage("alice", "default", "index.md", []byte("# Index\n")); err != nil {
		t.Fatalf("WritePage index: %v", err)
	}

	got, err := s.ReadPage("alice", "default", "concepts/otters.md")
	if err != nil {
		t.Fatalf("ReadPage: %v", err)
	}
	if string(got) != "# Otters\n" {
		t.Fatalf("ReadPage content mismatch: %q", got)
	}

	pages, err := s.ListPages("alice", "default", "concepts")
	if err != nil {
		t.Fatalf("ListPages: %v", err)
	}
	if len(pages) != 2 {
		t.Fatalf("expected 2 concept pages, got %d: %+v", len(pages), pages)
	}
	if pages[0].RelPath != filepath.Join("concepts", "beavers.md") {
		t.Fatalf("unexpected sort/relpath: %+v", pages)
	}

	// Listing a never-created type dir yields an empty (not error) list.
	empty, err := s.ListPages("alice", "default", "events")
	if err != nil {
		t.Fatalf("ListPages empty dir: %v", err)
	}
	if len(empty) != 0 {
		t.Fatalf("expected empty list for unused type dir, got %d", len(empty))
	}

	// WalkPages returns curated pages + index.md, excluding raw/.
	if _, err := s.WriteRaw("alice", "default", []byte("raw bytes"), RawMeta{}); err != nil {
		t.Fatalf("WriteRaw: %v", err)
	}
	walked, err := s.WalkPages("alice", "default")
	if err != nil {
		t.Fatalf("WalkPages: %v", err)
	}
	var rels []string
	for _, w := range walked {
		rels = append(rels, w.RelPath)
		if strings.HasPrefix(w.RelPath, "raw"+string(filepath.Separator)) {
			t.Fatalf("WalkPages included a raw doc: %q", w.RelPath)
		}
	}
	joined := strings.Join(rels, ",")
	if !strings.Contains(joined, "index.md") ||
		!strings.Contains(joined, filepath.Join("concepts", "otters.md")) {
		t.Fatalf("WalkPages missing expected pages: %v", rels)
	}
}

func TestAppendLog(t *testing.T) {
	s := newStore(t)
	if err := s.AppendLog("alice", "default", "INGEST sha=abc"); err != nil {
		t.Fatalf("AppendLog 1: %v", err)
	}
	if err := s.AppendLog("alice", "default", "INGEST sha=def\n"); err != nil {
		t.Fatalf("AppendLog 2: %v", err)
	}
	b, err := s.ReadPage("alice", "default", "log.md")
	if err != nil {
		t.Fatalf("read log: %v", err)
	}
	if string(b) != "INGEST sha=abc\nINGEST sha=def\n" {
		t.Fatalf("unexpected log contents: %q", b)
	}
}

func TestReadRaw_RoundTrip(t *testing.T) {
	s := newStore(t)
	content := []byte("hello raw\n")
	rd, err := s.WriteRaw("alice", "default", content, RawMeta{Title: "t"})
	if err != nil {
		t.Fatalf("WriteRaw: %v", err)
	}
	got, err := s.ReadRaw("alice", "default", rd.Sha256)
	if err != nil {
		t.Fatalf("ReadRaw: %v", err)
	}
	if !strings.Contains(string(got), string(content)) {
		t.Fatalf("ReadRaw missing content: %q", got)
	}
	// A bogus sha is rejected by segment validation, not a 500.
	if _, err := s.ReadRaw("alice", "default", "../escape"); err == nil {
		t.Fatal("ReadRaw with traversal sha should be rejected")
	}
}
