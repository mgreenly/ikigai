package search

import (
	"context"
	"database/sql"
	"fmt"
	"strings"
	"sync"

	_ "modernc.org/sqlite" // pure-Go SQLite driver; FTS5 + bm25() baked in, works with CGO_ENABLED=0
)

// indexFileName is the per-collection index page's relative path. It is always
// surfaced as Results.Index, independent of whether it matched the query.
const indexFileName = "index.md"

// defaultLimit caps Search hits when the caller passes limit <= 0.
const defaultLimit = 20

// IndexPathFunc resolves the per-collection SQLite file location for
// (owner, collection). It matches *store.Store.SearchIndexPath, so the wiring in
// Phase 4 / Task 5.1 passes store.SearchIndexPath directly. Keeping it as a
// function value avoids importing internal/store here (the search store has no
// other reason to depend on it).
type IndexPathFunc func(owner, collection string) (string, error)

// BM25Index is the Phase-1 FTS5/bm25() implementation of Index over
// modernc.org/sqlite (pure-Go, CGO_ENABLED=0). It keeps one *sql.DB per
// (owner, collection), opened lazily and cached; each handle points at a
// distinct per-collection SQLite file (physical owner isolation), opened WAL
// with SetMaxOpenConns(1) to match the suite's single-writer discipline.
type BM25Index struct {
	indexPath IndexPathFunc

	mu     sync.Mutex
	dbs    map[string]*sql.DB // key: owner + "\x00" + collection
	closed bool
}

// NewBM25Index returns a BM25Index that resolves per-collection SQLite files via
// indexPath (typically store.SearchIndexPath). Handles are opened lazily on
// first use and cached; call Close to release them all.
func NewBM25Index(indexPath IndexPathFunc) *BM25Index {
	return &BM25Index{
		indexPath: indexPath,
		dbs:       make(map[string]*sql.DB),
	}
}

// Ensure BM25Index satisfies the Index interface.
var _ Index = (*BM25Index)(nil)

func handleKey(owner, collection string) string {
	return owner + "\x00" + collection
}

// db returns the cached handle for (owner, collection), opening and initializing
// the per-collection SQLite file on first use.
func (b *BM25Index) db(ctx context.Context, owner, collection string) (*sql.DB, error) {
	b.mu.Lock()
	defer b.mu.Unlock()
	if b.closed {
		return nil, fmt.Errorf("search: index is closed")
	}
	key := handleKey(owner, collection)
	if conn, ok := b.dbs[key]; ok {
		return conn, nil
	}
	path, err := b.indexPath(owner, collection)
	if err != nil {
		return nil, fmt.Errorf("search: resolve index path: %w", err)
	}
	conn, err := openIndexDB(ctx, path)
	if err != nil {
		return nil, err
	}
	b.dbs[key] = conn
	return conn, nil
}

// openIndexDB opens the per-collection SQLite search index with the suite's
// pragmas (WAL, foreign_keys, busy_timeout) and single-writer connection, then
// creates the schema if needed. Mirrors wiki/internal/db/db.go's open path.
func openIndexDB(ctx context.Context, path string) (*sql.DB, error) {
	dsn := fmt.Sprintf(
		"file:%s?_pragma=journal_mode(WAL)&_pragma=foreign_keys(ON)&_pragma=busy_timeout(5000)",
		path,
	)
	conn, err := sql.Open("sqlite", dsn)
	if err != nil {
		return nil, fmt.Errorf("search: open sqlite %s: %w", path, err)
	}
	conn.SetMaxOpenConns(1) // single serialized writer, avoids FTS5 write contention
	if err := conn.PingContext(ctx); err != nil {
		conn.Close()
		return nil, fmt.Errorf("search: ping sqlite %s: %w", path, err)
	}
	if err := initSchema(ctx, conn); err != nil {
		conn.Close()
		return nil, err
	}
	return conn, nil
}

// schemaSQL creates the durable content table and its trigger-fed FTS5 index.
//
// The content table (pages) holds the whole-page rows; pages_fts is the BM25
// index, kept in sync by AFTER INSERT/UPDATE/DELETE triggers (qmd's pattern).
// path is stored UNINDEXED in the FTS table so it round-trips without affecting
// ranking; the body is the only ranked text column besides title. The tokenizer
// 'porter unicode61' (porter stemming so "otters"/"otter" match, unicode61 for
// case/diacritic folding) is FROZEN at create time — an FTS5 table's tokenizer
// cannot be altered later without a full rebuild, so it is pinned here.
const schemaSQL = `
CREATE TABLE IF NOT EXISTS pages (
  id         INTEGER PRIMARY KEY AUTOINCREMENT,
  owner      TEXT NOT NULL,
  collection TEXT NOT NULL,
  path       TEXT NOT NULL,
  title      TEXT NOT NULL,
  body       TEXT NOT NULL,
  UNIQUE(owner, collection, path)
);

CREATE VIRTUAL TABLE IF NOT EXISTS pages_fts USING fts5(
  title,
  body,
  path UNINDEXED,
  tokenize = 'porter unicode61'
);

CREATE TRIGGER IF NOT EXISTS pages_ai AFTER INSERT ON pages BEGIN
  INSERT INTO pages_fts(rowid, title, body, path)
  VALUES (new.id, new.title, new.body, new.path);
END;

CREATE TRIGGER IF NOT EXISTS pages_ad AFTER DELETE ON pages BEGIN
  DELETE FROM pages_fts WHERE rowid = old.id;
END;

CREATE TRIGGER IF NOT EXISTS pages_au AFTER UPDATE ON pages BEGIN
  DELETE FROM pages_fts WHERE rowid = old.id;
  INSERT INTO pages_fts(rowid, title, body, path)
  VALUES (new.id, new.title, new.body, new.path);
END;
`

func initSchema(ctx context.Context, conn *sql.DB) error {
	if _, err := conn.ExecContext(ctx, schemaSQL); err != nil {
		return fmt.Errorf("search: init schema: %w", err)
	}
	return nil
}

// IndexPage upserts one whole page, keyed by (owner, collection, path). It
// delete+inserts so a re-index of the same path replaces the prior row (the
// triggers keep pages_fts in sync), making re-ingest idempotent at the index
// layer.
func (b *BM25Index) IndexPage(ctx context.Context, owner, collection string, page Page) error {
	conn, err := b.db(ctx, owner, collection)
	if err != nil {
		return err
	}
	tx, err := conn.BeginTx(ctx, nil)
	if err != nil {
		return fmt.Errorf("search: begin tx: %w", err)
	}
	defer tx.Rollback()
	if err := upsertPage(ctx, tx, owner, collection, page); err != nil {
		return err
	}
	if err := tx.Commit(); err != nil {
		return fmt.Errorf("search: commit: %w", err)
	}
	return nil
}

// IndexPages bulk-upserts pages for a collection in a single transaction. Each
// page is upserted by (owner, collection, path); this is the whole-collection
// reindex hook the ingest core calls after an integration pass.
func (b *BM25Index) IndexPages(ctx context.Context, owner, collection string, pages []Page) error {
	conn, err := b.db(ctx, owner, collection)
	if err != nil {
		return err
	}
	tx, err := conn.BeginTx(ctx, nil)
	if err != nil {
		return fmt.Errorf("search: begin tx: %w", err)
	}
	defer tx.Rollback()
	for _, page := range pages {
		if err := upsertPage(ctx, tx, owner, collection, page); err != nil {
			return err
		}
	}
	if err := tx.Commit(); err != nil {
		return fmt.Errorf("search: commit: %w", err)
	}
	return nil
}

// upsertPage deletes any existing row for (owner, collection, path) and inserts
// the new content. The delete+insert (rather than INSERT OR REPLACE) keeps the
// AFTER DELETE / AFTER INSERT triggers' FTS bookkeeping simple and correct.
func upsertPage(ctx context.Context, tx *sql.Tx, owner, collection string, page Page) error {
	if _, err := tx.ExecContext(ctx,
		`DELETE FROM pages WHERE owner = ? AND collection = ? AND path = ?`,
		owner, collection, page.Path,
	); err != nil {
		return fmt.Errorf("search: delete existing page %q: %w", page.Path, err)
	}
	if _, err := tx.ExecContext(ctx,
		`INSERT INTO pages (owner, collection, path, title, body) VALUES (?, ?, ?, ?, ?)`,
		owner, collection, page.Path, page.Title, page.Body,
	); err != nil {
		return fmt.Errorf("search: insert page %q: %w", page.Path, err)
	}
	return nil
}

// RemovePage drops a page by its relative path. Removing an absent path is a
// no-op (not an error).
func (b *BM25Index) RemovePage(ctx context.Context, owner, collection, path string) error {
	conn, err := b.db(ctx, owner, collection)
	if err != nil {
		return err
	}
	if _, err := conn.ExecContext(ctx,
		`DELETE FROM pages WHERE owner = ? AND collection = ? AND path = ?`,
		owner, collection, path,
	); err != nil {
		return fmt.Errorf("search: remove page %q: %w", path, err)
	}
	return nil
}

// Search runs a BM25 query and returns ranked whole pages plus the collection's
// index page. Results are ordered best-first by ascending bm25() (more relevant
// == more negative; NO DESC). Result.Score carries the raw bm25 value unchanged.
// An empty or no-match query yields zero Hits and no error, but Results.Index is
// still populated when the collection has an index.md.
func (b *BM25Index) Search(ctx context.Context, owner, collection, query string, limit int) (Results, error) {
	conn, err := b.db(ctx, owner, collection)
	if err != nil {
		return Results{}, err
	}
	if limit <= 0 {
		limit = defaultLimit
	}

	var out Results

	// The index page is always the navigation entry point, independent of the
	// query match — fetch it directly by path.
	if idx, err := fetchPage(ctx, conn, owner, collection, indexFileName); err != nil {
		return Results{}, err
	} else if idx != nil {
		out.Index = idx
	}

	match := sanitizeFTSQuery(query)
	if match == "" {
		// Empty / all-operator query: no hits, but Index stays populated.
		return out, nil
	}

	rows, err := conn.QueryContext(ctx, `
		SELECT p.path, p.title, p.body, bm25(pages_fts) AS score
		  FROM pages_fts
		  JOIN pages p ON p.id = pages_fts.rowid
		 WHERE pages_fts MATCH ?
		   AND p.owner = ? AND p.collection = ?
		 ORDER BY score
		 LIMIT ?`,
		match, owner, collection, limit,
	)
	if err != nil {
		return Results{}, fmt.Errorf("search: query: %w", err)
	}
	defer rows.Close()

	for rows.Next() {
		var r Result
		if err := rows.Scan(&r.Path, &r.Title, &r.Body, &r.Score); err != nil {
			return Results{}, fmt.Errorf("search: scan row: %w", err)
		}
		out.Hits = append(out.Hits, r)
	}
	if err := rows.Err(); err != nil {
		return Results{}, fmt.Errorf("search: iterate rows: %w", err)
	}
	return out, nil
}

// fetchPage returns the page at path as a Result (Score 0), or nil if absent.
func fetchPage(ctx context.Context, conn *sql.DB, owner, collection, path string) (*Result, error) {
	row := conn.QueryRowContext(ctx,
		`SELECT path, title, body FROM pages WHERE owner = ? AND collection = ? AND path = ?`,
		owner, collection, path,
	)
	var r Result
	switch err := row.Scan(&r.Path, &r.Title, &r.Body); err {
	case nil:
		return &r, nil
	case sql.ErrNoRows:
		return nil, nil
	default:
		return nil, fmt.Errorf("search: fetch page %q: %w", path, err)
	}
}

// Close closes all cached handles and marks the index closed.
func (b *BM25Index) Close() error {
	b.mu.Lock()
	defer b.mu.Unlock()
	if b.closed {
		return nil
	}
	b.closed = true
	var firstErr error
	for key, conn := range b.dbs {
		if err := conn.Close(); err != nil && firstErr == nil {
			firstErr = fmt.Errorf("search: close handle: %w", err)
		}
		delete(b.dbs, key)
	}
	return firstErr
}

// sanitizeFTSQuery turns an arbitrary user query into a safe FTS5 MATCH string.
//
// A raw query is parsed by the FTS5 query grammar, where bare punctuation and
// keywords (- " * : NEAR OR AND, parentheses, etc.) are operators that can error
// or behave surprisingly. To keep the agent-facing surface forgiving we strip
// the FTS5 operator characters, drop any tokens that are bare FTS5 keywords, and
// wrap each surviving whitespace-separated term in double quotes so it is matched
// as a literal phrase/term. The wrapped terms are space-joined, which FTS5 treats
// as an implicit AND of phrases. An empty result (query was only operators /
// whitespace) signals the caller to skip the MATCH entirely.
func sanitizeFTSQuery(query string) string {
	// Replace FTS5 special characters with spaces. This neutralizes phrase
	// quotes, prefix/column operators, NEAR/parenthesis grouping, etc.
	replacer := strings.NewReplacer(
		`"`, " ",
		`*`, " ",
		`:`, " ",
		`-`, " ",
		`(`, " ",
		`)`, " ",
		`^`, " ",
		`+`, " ",
		`,`, " ",
		`.`, " ",
	)
	cleaned := replacer.Replace(query)

	fields := strings.Fields(cleaned)
	terms := make([]string, 0, len(fields))
	for _, f := range fields {
		// Drop bare FTS5 boolean/proximity keywords so a stray "OR"/"AND"/"NEAR"
		// in natural-language input isn't parsed as an operator.
		switch strings.ToUpper(f) {
		case "AND", "OR", "NOT", "NEAR":
			continue
		}
		terms = append(terms, `"`+f+`"`)
	}
	return strings.Join(terms, " ")
}
