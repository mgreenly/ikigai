package search

import (
	"context"
	"testing"

	"wiki/internal/store"
)

// storeAdapter wraps *store.Store to satisfy the local PageSource interface
// (store.PageEntry -> search.PageEntry). This is the exact thin adapter Phase 4
// / Task 5.1 will use to call ReindexCollection with the real store.
type storeAdapter struct{ s *store.Store }

func (a storeAdapter) WalkPages(owner, collection string) ([]PageEntry, error) {
	entries, err := a.s.WalkPages(owner, collection)
	if err != nil {
		return nil, err
	}
	out := make([]PageEntry, len(entries))
	for i, e := range entries {
		out[i] = PageEntry{RelPath: e.RelPath}
	}
	return out, nil
}

func (a storeAdapter) ReadPage(owner, collection, relPath string) ([]byte, error) {
	return a.s.ReadPage(owner, collection, relPath)
}

// newFixture builds a store-backed collection on disk and a BM25Index wired to
// the store's SearchIndexPath. It writes the given pages (relPath -> body) via
// the real store so Result.Path values are the store's RelPath values.
func newFixture(t *testing.T, owner, collection string, pages map[string]string) (*store.Store, *BM25Index) {
	t.Helper()
	s, err := store.New(t.TempDir())
	if err != nil {
		t.Fatalf("store.New: %v", err)
	}
	if _, err := s.EnsureLayout(owner, collection); err != nil {
		t.Fatalf("EnsureLayout: %v", err)
	}
	for rel, body := range pages {
		if err := s.WritePage(owner, collection, rel, []byte(body)); err != nil {
			t.Fatalf("WritePage %q: %v", rel, err)
		}
	}
	idx := NewBM25Index(s.SearchIndexPath)
	t.Cleanup(func() { idx.Close() })
	return s, idx
}

func TestSearch_RanksByBM25(t *testing.T) {
	owner, collection := "alice@example.com", "default"
	pages := map[string]string{
		// "otter" appears densely here -> should rank first (most negative bm25).
		"concepts/otters.md": "# Otters\notter otter otter otter otter river otter sea otter playful otter",
		// One passing mention -> ranks after the dense page.
		"concepts/wildlife.md": "# Wildlife notes\nWe saw one otter near the bank, plus deer and foxes.",
		// No "otter" term -> must be excluded.
		"concepts/beavers.md": "# Beavers\nBeavers build dams. Only beavers here, no other animals.",
		"index.md":            "# Index\nNavigation entry point for the collection.",
	}
	s, idx := newFixture(t, owner, collection, pages)

	if err := ReindexCollection(context.Background(), idx, storeAdapter{s}, owner, collection); err != nil {
		t.Fatalf("ReindexCollection: %v", err)
	}

	res, err := idx.Search(context.Background(), owner, collection, "otter", 10)
	if err != nil {
		t.Fatalf("Search: %v", err)
	}

	if len(res.Hits) != 2 {
		t.Fatalf("expected 2 hits (otters, wildlife), got %d: %+v", len(res.Hits), hitPaths(res))
	}
	// Ascending bm25: most relevant (densest term frequency) first.
	if res.Hits[0].Path != "concepts/otters.md" {
		t.Fatalf("expected concepts/otters.md ranked first, got %q (paths=%v)", res.Hits[0].Path, hitPaths(res))
	}
	if res.Hits[1].Path != "concepts/wildlife.md" {
		t.Fatalf("expected concepts/wildlife.md ranked second, got %q (paths=%v)", res.Hits[1].Path, hitPaths(res))
	}
	// Best-first means score ascends (more negative is better).
	if !(res.Hits[0].Score <= res.Hits[1].Score) {
		t.Fatalf("scores not ascending best-first: %v then %v", res.Hits[0].Score, res.Hits[1].Score)
	}
	// beavers.md (no term) must be absent.
	for _, h := range res.Hits {
		if h.Path == "concepts/beavers.md" {
			t.Fatalf("beavers.md (no matching term) should be excluded; got it in hits")
		}
	}
}

func TestSearch_ReturnsWholePages(t *testing.T) {
	owner, collection := "bob", "default"
	body := "# Otters\nThe full body of the otter page.\nMany lines.\nAll should round-trip."
	pages := map[string]string{
		"concepts/otters.md": body,
		"index.md":           "# Index\nentry",
	}
	s, idx := newFixture(t, owner, collection, pages)
	if err := ReindexCollection(context.Background(), idx, storeAdapter{s}, owner, collection); err != nil {
		t.Fatalf("ReindexCollection: %v", err)
	}

	res, err := idx.Search(context.Background(), owner, collection, "otter", 10)
	if err != nil {
		t.Fatalf("Search: %v", err)
	}
	if len(res.Hits) != 1 {
		t.Fatalf("expected 1 hit, got %d", len(res.Hits))
	}
	if res.Hits[0].Path != "concepts/otters.md" {
		t.Fatalf("Result.Path should match store RelPath, got %q", res.Hits[0].Path)
	}
	if res.Hits[0].Body != body {
		t.Fatalf("Result.Body should be the whole page; got %q want %q", res.Hits[0].Body, body)
	}
}

func TestSearch_IndexAlwaysPopulated(t *testing.T) {
	owner, collection := "carol", "default"
	pages := map[string]string{
		"concepts/otters.md": "# Otters\notter otter",
		"index.md":           "# Index\nNavigation home for carol's wiki.",
	}
	s, idx := newFixture(t, owner, collection, pages)
	if err := ReindexCollection(context.Background(), idx, storeAdapter{s}, owner, collection); err != nil {
		t.Fatalf("ReindexCollection: %v", err)
	}

	// Matching query: Index populated.
	res, err := idx.Search(context.Background(), owner, collection, "otter", 10)
	if err != nil {
		t.Fatalf("Search: %v", err)
	}
	if res.Index == nil {
		t.Fatalf("Results.Index should be populated with index.md")
	}
	if res.Index.Path != "index.md" {
		t.Fatalf("Results.Index.Path = %q, want index.md", res.Index.Path)
	}
	if res.Index.Body == "" {
		t.Fatalf("Results.Index.Body should be the whole index page")
	}

	// No-match query: Index still populated.
	res2, err := idx.Search(context.Background(), owner, collection, "zzznotapresentterm", 10)
	if err != nil {
		t.Fatalf("Search (no match): %v", err)
	}
	if len(res2.Hits) != 0 {
		t.Fatalf("expected zero hits for no-match query, got %d", len(res2.Hits))
	}
	if res2.Index == nil {
		t.Fatalf("Results.Index should still be populated on a no-match query")
	}
}

func TestSearch_IdempotentReindex(t *testing.T) {
	owner, collection := "dave", "default"
	pages := map[string]string{
		"concepts/otters.md": "# Otters\notter otter otter",
		"index.md":           "# Index\nentry",
	}
	s, idx := newFixture(t, owner, collection, pages)

	// Index the same collection three times; also IndexPage the same page again.
	for i := 0; i < 3; i++ {
		if err := ReindexCollection(context.Background(), idx, storeAdapter{s}, owner, collection); err != nil {
			t.Fatalf("ReindexCollection pass %d: %v", i, err)
		}
	}
	if err := idx.IndexPage(context.Background(), owner, collection, Page{
		Path: "concepts/otters.md", Title: "Otters", Body: "# Otters\notter otter otter",
	}); err != nil {
		t.Fatalf("IndexPage: %v", err)
	}

	res, err := idx.Search(context.Background(), owner, collection, "otter", 10)
	if err != nil {
		t.Fatalf("Search: %v", err)
	}
	if len(res.Hits) != 1 {
		t.Fatalf("idempotent reindex should not duplicate; got %d hits: %v", len(res.Hits), hitPaths(res))
	}
}

func TestSearch_OperatorCharactersDoNotError(t *testing.T) {
	owner, collection := "erin", "default"
	pages := map[string]string{
		"concepts/otters.md": "# Otters\notter river beaver",
		"index.md":           "# Index\nentry",
	}
	s, idx := newFixture(t, owner, collection, pages)
	if err := ReindexCollection(context.Background(), idx, storeAdapter{s}, owner, collection); err != nil {
		t.Fatalf("ReindexCollection: %v", err)
	}

	// Each of these contains raw FTS5 operator syntax that would error if passed
	// through unsanitized.
	queries := []string{
		`otter"`,
		`-otter`,
		`otter:river`,
		`otter OR beaver`,
		`otter AND beaver`,
		`NEAR(otter river)`,
		`otter*`,
		`"unterminated`,
		`()`,       // all-operator -> empty match -> zero hits, no error
		`   OR   `, // bare keyword + whitespace -> empty match
	}
	for _, q := range queries {
		res, err := idx.Search(context.Background(), owner, collection, q, 10)
		if err != nil {
			t.Fatalf("Search(%q) errored (sanitization failed): %v", q, err)
		}
		// Index must remain populated regardless.
		if res.Index == nil {
			t.Fatalf("Search(%q): Results.Index should stay populated", q)
		}
	}

	// A sanitizable query with operator chars should still find the page.
	res, err := idx.Search(context.Background(), owner, collection, `otter OR beaver`, 10)
	if err != nil {
		t.Fatalf("Search(otter OR beaver): %v", err)
	}
	if len(res.Hits) != 1 {
		t.Fatalf("expected 1 hit for sanitized 'otter OR beaver', got %d", len(res.Hits))
	}
}

func TestSearch_EmptyQueryZeroHits(t *testing.T) {
	owner, collection := "frank", "default"
	pages := map[string]string{
		"concepts/otters.md": "# Otters\notter otter",
		"index.md":           "# Index\nentry",
	}
	s, idx := newFixture(t, owner, collection, pages)
	if err := ReindexCollection(context.Background(), idx, storeAdapter{s}, owner, collection); err != nil {
		t.Fatalf("ReindexCollection: %v", err)
	}

	for _, q := range []string{"", "   ", "\t\n"} {
		res, err := idx.Search(context.Background(), owner, collection, q, 10)
		if err != nil {
			t.Fatalf("Search(%q): %v", q, err)
		}
		if len(res.Hits) != 0 {
			t.Fatalf("Search(%q): expected zero hits, got %d", q, len(res.Hits))
		}
		if res.Index == nil {
			t.Fatalf("Search(%q): Index should still be populated", q)
		}
	}
}

func TestSearch_OwnerIsolation(t *testing.T) {
	// Two owners with the same term must not see each other's pages (physical
	// per-collection file isolation).
	owner1, owner2, collection := "alice@example.com", "mallory@example.com", "default"
	s, err := store.New(t.TempDir())
	if err != nil {
		t.Fatalf("store.New: %v", err)
	}
	for _, o := range []string{owner1, owner2} {
		if _, err := s.EnsureLayout(o, collection); err != nil {
			t.Fatalf("EnsureLayout %s: %v", o, err)
		}
	}
	if err := s.WritePage(owner1, collection, "concepts/secret.md", []byte("# Secret\nalice secret otter plan")); err != nil {
		t.Fatalf("WritePage owner1: %v", err)
	}
	if err := s.WritePage(owner2, collection, "concepts/public.md", []byte("# Public\nmallory boring note")); err != nil {
		t.Fatalf("WritePage owner2: %v", err)
	}
	idx := NewBM25Index(s.SearchIndexPath)
	defer idx.Close()
	if err := ReindexCollection(context.Background(), idx, storeAdapter{s}, owner1, collection); err != nil {
		t.Fatalf("reindex owner1: %v", err)
	}
	if err := ReindexCollection(context.Background(), idx, storeAdapter{s}, owner2, collection); err != nil {
		t.Fatalf("reindex owner2: %v", err)
	}

	res, err := idx.Search(context.Background(), owner2, collection, "otter", 10)
	if err != nil {
		t.Fatalf("Search owner2: %v", err)
	}
	for _, h := range res.Hits {
		if h.Path == "concepts/secret.md" {
			t.Fatalf("owner isolation breach: owner2 saw owner1's secret.md")
		}
	}
}

func TestRemovePage(t *testing.T) {
	owner, collection := "grace", "default"
	pages := map[string]string{
		"concepts/otters.md": "# Otters\notter otter",
		"index.md":           "# Index\nentry",
	}
	s, idx := newFixture(t, owner, collection, pages)
	if err := ReindexCollection(context.Background(), idx, storeAdapter{s}, owner, collection); err != nil {
		t.Fatalf("ReindexCollection: %v", err)
	}

	// Removing an absent path is a no-op.
	if err := idx.RemovePage(context.Background(), owner, collection, "concepts/nope.md"); err != nil {
		t.Fatalf("RemovePage(absent) should be a no-op, got %v", err)
	}
	if err := idx.RemovePage(context.Background(), owner, collection, "concepts/otters.md"); err != nil {
		t.Fatalf("RemovePage: %v", err)
	}
	res, err := idx.Search(context.Background(), owner, collection, "otter", 10)
	if err != nil {
		t.Fatalf("Search: %v", err)
	}
	if len(res.Hits) != 0 {
		t.Fatalf("expected 0 hits after RemovePage, got %d", len(res.Hits))
	}
}

func TestDeriveTitle(t *testing.T) {
	cases := []struct {
		name, body, rel, want string
	}{
		{"frontmatter", "---\ntitle: My Page\n---\n# Heading\nbody", "concepts/x.md", "My Page"},
		{"h1", "# Real Heading\nbody", "concepts/x.md", "Real Heading"},
		{"h1 after frontmatter", "---\nsource: url\n---\n# Heading Two\nbody", "concepts/x.md", "Heading Two"},
		{"fallback path", "no heading here\njust text", "concepts/plain.md", "concepts/plain.md"},
	}
	for _, c := range cases {
		t.Run(c.name, func(t *testing.T) {
			if got := DeriveTitle(c.body, c.rel); got != c.want {
				t.Fatalf("DeriveTitle = %q, want %q", got, c.want)
			}
		})
	}
}

func hitPaths(r Results) []string {
	out := make([]string, len(r.Hits))
	for i, h := range r.Hits {
		out[i] = h.Path
	}
	return out
}
