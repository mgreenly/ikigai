package retrieve

import (
	"context"
	"fmt"
	"sort"
	"strings"
	"sync"
)

// VectorCache holds page vectors in memory for query-time scans.
type VectorCache = vectorCache

// VectorEntry is one cached page vector.
type VectorEntry = vectorEntry

// NewVectorCache returns an empty in-memory vector cache.
func NewVectorCache() *VectorCache {
	return &vectorCache{}
}

// NewVectorRetriever returns a retriever that embeds the query and scans cache.
func NewVectorRetriever(embed func(context.Context, string) ([]float32, error), cache *VectorCache) Retriever {
	return &vectorRetriever{embed: embed, cache: cache}
}

type vectorCache struct {
	mu      sync.RWMutex
	entries []vectorEntry
}

type vectorEntry struct {
	SubjectID string
	Title     string
	Vec       []float32
}

func (c *vectorCache) Replace(all []vectorEntry) {
	if c == nil {
		return
	}
	c.mu.Lock()
	defer c.mu.Unlock()

	c.entries = cloneVectorEntries(all)
}

func (c *vectorCache) Upsert(e vectorEntry) {
	if c == nil {
		return
	}
	c.mu.Lock()
	defer c.mu.Unlock()

	e = cloneVectorEntry(e)
	for i := range c.entries {
		if c.entries[i].SubjectID == e.SubjectID {
			c.entries[i] = e
			return
		}
	}
	c.entries = append(c.entries, e)
}

func (c *vectorCache) Remove(subjectID string) {
	if c == nil {
		return
	}
	subjectID = strings.TrimSpace(subjectID)
	if subjectID == "" {
		return
	}
	c.mu.Lock()
	defer c.mu.Unlock()

	for i := range c.entries {
		if c.entries[i].SubjectID != subjectID {
			continue
		}
		copy(c.entries[i:], c.entries[i+1:])
		c.entries[len(c.entries)-1] = vectorEntry{}
		c.entries = c.entries[:len(c.entries)-1]
		return
	}
}

func (c *vectorCache) nearest(q []float32, k int) []Hit {
	if c == nil || len(q) == 0 || k <= 0 {
		return nil
	}
	c.mu.RLock()
	defer c.mu.RUnlock()

	scored := make([]Hit, 0, len(c.entries))
	for _, e := range c.entries {
		if len(e.Vec) != len(q) || len(e.Vec) == 0 {
			continue
		}
		scored = append(scored, Hit{
			PageID: e.SubjectID,
			Title:  e.Title,
			Score:  dot(q, e.Vec),
		})
	}
	sort.Slice(scored, func(i, j int) bool {
		if scored[i].Score != scored[j].Score {
			return scored[i].Score > scored[j].Score
		}
		if scored[i].Title != scored[j].Title {
			return scored[i].Title < scored[j].Title
		}
		return scored[i].PageID < scored[j].PageID
	})
	if len(scored) > k {
		scored = scored[:k]
	}
	return append([]Hit(nil), scored...)
}

type vectorRetriever struct {
	embed func(ctx context.Context, text string) ([]float32, error)
	cache *vectorCache
}

func (r *vectorRetriever) Search(ctx context.Context, query string, limits SearchLimits) (Result, error) {
	if r == nil || r.embed == nil {
		return Result{}, fmt.Errorf("retrieve: nil vector retriever embedder")
	}
	if r.cache == nil {
		return Result{}, fmt.Errorf("retrieve: nil vector retriever cache")
	}
	query = strings.TrimSpace(query)
	if query == "" {
		return Result{}, nil
	}
	vec, err := r.embed(ctx, query)
	if err != nil {
		return Result{}, err
	}
	return Result{Hits: r.cache.nearest(vec, limits.Resolve().Limit)}, nil
}

func cloneVectorEntries(entries []vectorEntry) []vectorEntry {
	if len(entries) == 0 {
		return nil
	}
	out := make([]vectorEntry, len(entries))
	for i, e := range entries {
		out[i] = cloneVectorEntry(e)
	}
	return out
}

func cloneVectorEntry(e vectorEntry) vectorEntry {
	e.Vec = append([]float32(nil), e.Vec...)
	return e
}

func dot(a, b []float32) float64 {
	var score float64
	for i := range a {
		score += float64(a[i]) * float64(b[i])
	}
	return score
}
