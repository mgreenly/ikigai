// Package retrieve defines the search seam used by question answering.
package retrieve

import "context"

const (
	// DefaultLimit is the number of hits used when a caller leaves Limit unset.
	DefaultLimit = 8
	// LimitCap is the largest hit count a retriever should return for one search.
	LimitCap = 20
)

// Hit is one ranked wiki search result.
type Hit struct {
	PageID  string
	Path    string
	Title   string
	Snippet string
	Score   float64
}

// Result is the full response from one search request.
type Result struct {
	Hits     []Hit
	TopDense float64
	Pinned   bool
}

// Retriever searches wiki content without exposing the backing index.
type Retriever interface {
	Search(ctx context.Context, query string, limits SearchLimits) (Result, error)
}

// SearchLimits carries caller-controlled bounds for one search request.
type SearchLimits struct {
	Limit int
}

// Resolve returns limits clamped to the retriever contract.
func (l SearchLimits) Resolve() SearchLimits {
	switch {
	case l.Limit == 0:
		l.Limit = DefaultLimit
	case l.Limit < 0:
		l.Limit = 1
	case l.Limit > LimitCap:
		l.Limit = LimitCap
	}
	return l
}
