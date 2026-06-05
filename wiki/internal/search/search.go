// Package search is the wiki's ranked page-retrieval backend. The Phase-1
// implementation is BM25 over modernc.org/sqlite FTS5 (pure-Go, CGO_ENABLED=0).
// It is an interface so a later vector/hybrid backend is an additive swap.
//
// The implementation indexes and returns WHOLE markdown pages (not fragments),
// scoped per (owner, collection). The collection's index.md page is always
// surfaced as the navigation entry point, independent of whether it matched the
// query. See wiki/notes/search-backend.md for the design and the Task-0.2 spike
// that confirmed FTS5 + bm25() work under CGO_ENABLED=0.
package search

import "context"

// Page is a whole markdown page handed to the indexer. Path is the page's
// relative path within the owner+collection tree (e.g. "concepts/otters.md")
// and is the stable identity used for upsert/delete.
type Page struct {
	Path  string // relative path within the collection tree; unique per (owner, collection)
	Title string // page title (frontmatter title, or first H1, or derived from Path)
	Body  string // the full markdown body (whole page, not a fragment)
}

// Result is one ranked whole-page hit.
type Result struct {
	Path  string  // relative path within the collection tree
	Title string  // page title
	Body  string  // the full markdown page body
	Score float64 // raw SQLite bm25() score: LOWER (more negative) == MORE relevant
}

// Results is a ranked search response. Hits are ordered best-first
// (ascending bm25 score). Index is the collection's index.md page, always
// included so the caller can navigate index-first; it is nil only if the
// collection has no index page yet.
type Results struct {
	Hits  []Result // whole pages, best-first
	Index *Result  // the collection's index.md (navigation entry point), or nil
}

// Index is the search backend: it maintains a BM25-ranked index of whole
// markdown pages per (owner, collection) and answers ranked page queries.
//
// Implementations are owner-scoped: an owner+collection's pages never appear in
// another owner's results. The Phase-1 implementation is FTS5/bm25() on
// modernc.org/sqlite; vector/hybrid is a future implementation of this same
// interface.
type Index interface {
	// IndexPage upserts a single whole page into the (owner, collection) index,
	// keyed by page.Path. Re-indexing the same path replaces it (re-ingest safe).
	IndexPage(ctx context.Context, owner, collection string, page Page) error

	// IndexPages bulk-upserts pages for a collection (e.g. a full reindex after
	// an ingest integration pass). Implementations should do this in one
	// transaction.
	IndexPages(ctx context.Context, owner, collection string, pages []Page) error

	// RemovePage drops a page from the index by its relative path. Idempotent:
	// removing an absent path is not an error.
	RemovePage(ctx context.Context, owner, collection, path string) error

	// Search runs a BM25 query over the (owner, collection) index and returns
	// ranked whole pages plus the collection's index page. limit caps Hits
	// (limit <= 0 selects an implementation default).
	Search(ctx context.Context, owner, collection, query string, limit int) (Results, error)

	// Close releases the backend's resources (e.g. the SQLite handle).
	Close() error
}
