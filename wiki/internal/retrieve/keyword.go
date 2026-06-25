package retrieve

import (
	"context"
	"database/sql"
	"fmt"
	"strings"
)

// NewKeywordRetriever returns the SQLite FTS-backed full-text retriever.
func NewKeywordRetriever(db *sql.DB) Retriever {
	return &keywordRetriever{db: db}
}

// keywordRetriever runs the full-text lane.
type keywordRetriever struct {
	db *sql.DB
}

func (r *keywordRetriever) Search(ctx context.Context, query string, limits SearchLimits) (Result, error) {
	if r == nil || r.db == nil {
		return Result{}, fmt.Errorf("retrieve: nil keyword retriever database")
	}
	match := ftsPhrase(query)
	if match == "" {
		return Result{}, nil
	}
	limit := limits.Resolve().Limit
	rows, err := r.db.QueryContext(ctx, `
		SELECT p.subject_id,
		       s.type || '/' || s.norm_name AS path,
		       p.title,
		       snippet(pages_fts, -1, '', '', '...', 32) AS snippet,
		       bm25(pages_fts) AS rank
		FROM pages_fts
		JOIN pages p ON p.rowid = pages_fts.rowid
		JOIN subjects s ON s.id = p.subject_id
		WHERE pages_fts MATCH ?
		ORDER BY rank, p.subject_id
		LIMIT ?`, match, limit)
	if err != nil {
		return Result{}, err
	}
	defer rows.Close()

	var result Result
	for rows.Next() {
		var hit Hit
		if err := rows.Scan(&hit.PageID, &hit.Path, &hit.Title, &hit.Snippet, &hit.Score); err != nil {
			return Result{}, err
		}
		result.Hits = append(result.Hits, hit)
	}
	if err := rows.Err(); err != nil {
		return Result{}, err
	}
	return result, nil
}

func ftsPhrase(query string) string {
	terms := strings.Fields(query)
	out := make([]string, 0, len(terms))
	for _, term := range terms {
		term = strings.TrimSpace(term)
		if term == "" {
			continue
		}
		out = append(out, `"`+strings.ReplaceAll(term, `"`, `""`)+`"`)
	}
	return strings.Join(out, " OR ")
}
