package mcp

import (
	"context"
	"testing"

	"wiki/internal/search"
	"wiki/internal/store"
)

// stubSearcher is a test double for the Searcher seam: it records the last call
// and returns canned Results, proving the wiki_search verb wiring (arg parsing,
// owner threading, default-collection, result shaping) without a real FTS5
// backend.
type stubSearcher struct {
	gotOwner      string
	gotCollection string
	gotQuery      string
	gotLimit      int
	result        search.Results
	err           error
}

func (s *stubSearcher) Search(_ context.Context, owner, collection, query string, limit int) (search.Results, error) {
	s.gotOwner = owner
	s.gotCollection = collection
	s.gotQuery = query
	s.gotLimit = limit
	return s.result, s.err
}

// TestSearch_Verb_Dispatch proves the verb threads owner from the identity, the
// default ("") collection, and shapes the index-first + ranked-pages result with
// the score negated so higher == more relevant.
func TestSearch_Verb_Dispatch(t *testing.T) {
	stub := &stubSearcher{result: search.Results{
		Index: &search.Result{Path: "index.md", Title: "Index", Body: "# Index\nnav"},
		Hits: []search.Result{
			{Path: "concepts/otters.md", Title: "Otters", Body: "# Otters\nall about otters", Score: -3.2},
			{Path: "concepts/wildlife.md", Title: "Wildlife", Body: "# Wildlife\none otter", Score: -1.1},
		},
	}}
	h := NewHandler(nil, stub, nil)

	p, isErr := callTool(t, h, "wiki_search", `{"query":"otter","limit":5}`)
	if isErr {
		t.Fatalf("wiki_search returned isError: %v", p)
	}

	// Owner threaded from the injected identity; default ("") collection; limit passed.
	if stub.gotOwner != "me@example.com" {
		t.Fatalf("owner threaded = %q, want me@example.com", stub.gotOwner)
	}
	if stub.gotCollection != store.DefaultCollection {
		t.Fatalf("collection = %q, want %q (default, PLAN Decision 4)", stub.gotCollection, store.DefaultCollection)
	}
	if stub.gotQuery != "otter" {
		t.Fatalf("query = %q", stub.gotQuery)
	}
	if stub.gotLimit != 5 {
		t.Fatalf("limit = %d, want 5", stub.gotLimit)
	}

	// Index page surfaced first (navigation entry point).
	idx, ok := p["index"].(map[string]any)
	if !ok {
		t.Fatalf("result missing index page: %v", p)
	}
	if idx["path"] != "index.md" || idx["body"] != "# Index\nnav" {
		t.Fatalf("index page = %v", idx)
	}
	if _, hasScore := idx["score"]; hasScore {
		t.Fatalf("index page should carry no score, got %v", idx["score"])
	}

	// Ranked whole pages, best-first, with negated (higher = better) score.
	results, ok := p["results"].([]any)
	if !ok || len(results) != 2 {
		t.Fatalf("expected 2 result pages, got %v", p["results"])
	}
	if p["count"].(float64) != 2 {
		t.Fatalf("count = %v, want 2", p["count"])
	}
	first := results[0].(map[string]any)
	if first["path"] != "concepts/otters.md" {
		t.Fatalf("first result path = %v, want concepts/otters.md", first["path"])
	}
	if first["body"] != "# Otters\nall about otters" {
		t.Fatalf("first result body not the whole page: %v", first["body"])
	}
	// raw bm25 -3.2 -> presented relevance +3.2 (higher = better).
	if first["score"].(float64) != 3.2 {
		t.Fatalf("first result score = %v, want 3.2 (negated bm25)", first["score"])
	}
	// best-first: first relevance >= second relevance.
	second := results[1].(map[string]any)
	if first["score"].(float64) < second["score"].(float64) {
		t.Fatalf("results not best-first: %v then %v", first["score"], second["score"])
	}
}

// TestSearch_Verb_EmptyNoMatch proves zero hits is not an error and the index
// page is still surfaced (the navigation entry point is always present).
func TestSearch_Verb_EmptyNoMatch(t *testing.T) {
	stub := &stubSearcher{result: search.Results{
		Index: &search.Result{Path: "index.md", Title: "Index", Body: "# Index\nnav"},
		Hits:  nil,
	}}
	h := NewHandler(nil, stub, nil)

	p, isErr := callTool(t, h, "wiki_search", `{"query":"zzznotpresent"}`)
	if isErr {
		t.Fatalf("wiki_search (no match) returned isError: %v", p)
	}
	if stub.gotLimit != defaultSearchLimit {
		t.Fatalf("default limit = %d, want %d", stub.gotLimit, defaultSearchLimit)
	}
	if p["count"].(float64) != 0 {
		t.Fatalf("count = %v, want 0", p["count"])
	}
	results, _ := p["results"].([]any)
	if len(results) != 0 {
		t.Fatalf("expected zero result pages on no-match, got %v", results)
	}
	if _, ok := p["index"].(map[string]any); !ok {
		t.Fatalf("index page must still be surfaced on a no-match query: %v", p)
	}
}

func TestSearch_Verb_RequiresQuery(t *testing.T) {
	h := NewHandler(nil, &stubSearcher{}, nil)
	_, isErr := callTool(t, h, "wiki_search", `{"limit":5}`)
	if !isErr {
		t.Fatal("wiki_search with empty query should be a tool-error")
	}
}

func TestSearch_Verb_NilBackendUnavailable(t *testing.T) {
	h := NewHandler(nil, nil, nil)
	_, isErr := callTool(t, h, "wiki_search", `{"query":"x"}`)
	if !isErr {
		t.Fatal("wiki_search with nil backend should be a tool-error")
	}
}

// storeAdapter wraps *store.Store to satisfy search.PageSource (store.PageEntry
// -> search.PageEntry) for the real-index integration test below.
type storeAdapter struct{ s *store.Store }

func (a storeAdapter) WalkPages(owner, collection string) ([]search.PageEntry, error) {
	entries, err := a.s.WalkPages(owner, collection)
	if err != nil {
		return nil, err
	}
	out := make([]search.PageEntry, len(entries))
	for i, e := range entries {
		out[i] = search.PageEntry{RelPath: e.RelPath}
	}
	return out, nil
}

func (a storeAdapter) ReadPage(owner, collection, relPath string) ([]byte, error) {
	return a.s.ReadPage(owner, collection, relPath)
}

// TestSearch_Verb_OverRealIndex drives wiki_search end to end over the real
// BM25 backend (no stub): it files pages into the store exactly as the ingest
// integration pass does, reindexes via internal/search, then asserts the verb
// surfaces the filed page as a WHOLE page (body round-trips) and the index page.
// This is the "search-after-ingest returns the integrated page(s)" acceptance.
func TestSearch_Verb_OverRealIndex(t *testing.T) {
	owner := "me@example.com" // must match the X-Owner-Email the rpc helper injects.
	collection := store.DefaultCollection

	s, err := store.New(t.TempDir())
	if err != nil {
		t.Fatalf("store.New: %v", err)
	}
	if _, err := s.EnsureLayout(owner, collection); err != nil {
		t.Fatalf("EnsureLayout: %v", err)
	}
	otterBody := "# Otters\notter otter otter river otter sea otter — the integrated source page."
	if err := s.WritePage(owner, collection, "concepts/otters.md", []byte(otterBody)); err != nil {
		t.Fatalf("WritePage otters: %v", err)
	}
	if err := s.WritePage(owner, collection, "index.md", []byte("# Index\nNavigation entry point.")); err != nil {
		t.Fatalf("WritePage index: %v", err)
	}

	idx := search.NewBM25Index(s.SearchIndexPath)
	defer idx.Close()
	if err := search.ReindexCollection(context.Background(), idx, storeAdapter{s}, owner, collection); err != nil {
		t.Fatalf("ReindexCollection: %v", err)
	}

	h := NewHandler(nil, idx, nil)
	p, isErr := callTool(t, h, "wiki_search", `{"query":"otter"}`)
	if isErr {
		t.Fatalf("wiki_search returned isError: %v", p)
	}

	// Index page surfaced.
	if _, ok := p["index"].(map[string]any); !ok {
		t.Fatalf("index page should be surfaced: %v", p)
	}
	// The filed page appears as a whole page (full body round-trips).
	results, _ := p["results"].([]any)
	if len(results) != 1 {
		t.Fatalf("expected the filed page in results, got %d: %v", len(results), results)
	}
	got := results[0].(map[string]any)
	if got["path"] != "concepts/otters.md" {
		t.Fatalf("result path = %v, want concepts/otters.md", got["path"])
	}
	if got["body"] != otterBody {
		t.Fatalf("result body should be the whole page; got %q", got["body"])
	}
	if got["score"].(float64) <= 0 {
		t.Fatalf("presented relevance should be positive (negated bm25), got %v", got["score"])
	}
}
