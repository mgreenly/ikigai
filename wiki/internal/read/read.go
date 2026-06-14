package read

import (
	"context"

	"wiki/internal/page"
)

// Retriever is the read side's pluggable retrieval primitive (design §9.3). P10
// backs it with the lexical FTS5 lane (page.Store); P11's hybrid retriever
// (BM25 + vector, RRF-fused) slots in behind this SAME interface without
// changing the search verb or ask's search tool. Search is registry-first: an
// exact alias match pins that subject's page at rank 1, the retriever fills the
// remainder (design §9.3 search contract).
type Retriever interface {
	// Search returns up to limit whole-page hits for the query, best-first.
	Search(ctx context.Context, query string, limit int) ([]page.WholePage, error)
}

// storeRetriever is the lexical FTS5 retriever (the P10 lane). It is a thin
// adapter over page.Store.SearchPages.
type storeRetriever struct{ store *page.Store }

// NewStoreRetriever builds the lexical retriever over the page store.
func NewStoreRetriever(store *page.Store) Retriever { return storeRetriever{store: store} }

func (r storeRetriever) Search(ctx context.Context, query string, limit int) ([]page.WholePage, error) {
	return r.store.SearchPages(ctx, query, limit)
}

// SearchLimits is the search verb's limit contract (design §9.3): a default and a
// hard cap. Resolve clamps a caller-supplied limit into [1, Cap], defaulting a
// non-positive value to Default.
type SearchLimits struct {
	Default int
	Cap     int
}

// Resolve clamps a caller limit into the contract: <=0 → Default; >Cap → Cap.
func (l SearchLimits) Resolve(limit int) int {
	if limit <= 0 {
		return l.Default
	}
	if l.Cap > 0 && limit > l.Cap {
		return l.Cap
	}
	return limit
}

// Service is the read side's verb surface: search and timeline (zero-LLM) plus
// the inputs ask's agent shares. It is read-only by construction.
type Service struct {
	store     *page.Store
	retriever Retriever
	limits    SearchLimits
}

// NewService builds the read service over the page store + retriever + the limit
// contract. The retriever may be the lexical store retriever (P10) or the hybrid
// one (P11) — same interface.
func NewService(store *page.Store, retriever Retriever, limits SearchLimits) *Service {
	return &Service{store: store, retriever: retriever, limits: limits}
}

// Search is the public search verb (design §9.3): registry-first resolution (an
// exact alias match pins that subject's page at rank 1), the retriever fills the
// remainder; a hit is the WHOLE page; rank order only, no scores; nothing
// prepended. The limit is clamped to the contract.
func (s *Service) Search(ctx context.Context, query string, limit int) ([]page.WholePage, error) {
	limit = s.limits.Resolve(limit)

	// Registry-first: an exact alias match pins those subjects' pages at the front.
	pinned, err := s.store.Lookup(ctx, query)
	if err != nil {
		return nil, err
	}
	out := make([]page.WholePage, 0, limit)
	seen := make(map[string]struct{})
	for _, p := range pinned {
		if len(out) >= limit {
			break
		}
		// Only pin pages that actually have a body (a name with no page yet is not a
		// search hit).
		if p.Body == "" {
			continue
		}
		if _, ok := seen[p.Subject]; ok {
			continue
		}
		seen[p.Subject] = struct{}{}
		out = append(out, p)
	}

	// The retriever fills the remainder, skipping anything already pinned.
	if len(out) < limit {
		hits, err := s.retriever.Search(ctx, query, limit)
		if err != nil {
			return nil, err
		}
		for _, h := range hits {
			if len(out) >= limit {
				break
			}
			if _, ok := seen[h.Subject]; ok {
				continue
			}
			seen[h.Subject] = struct{}{}
			out = append(out, h)
		}
	}
	return out, nil
}

// MCPReader adapts the read Service + Asker to the mcp.Reader interface (search /
// ask / timeline returning plain serializable values), keeping the mcp package
// decoupled from the read package's concrete types. The composition root builds
// it once and hands it to mcp.NewHandler.
type MCPReader struct {
	svc   *Service
	asker *Asker
}

// NewMCPReader builds the MCP read adapter over the read service + ask agent.
func NewMCPReader(svc *Service, asker *Asker) *MCPReader {
	return &MCPReader{svc: svc, asker: asker}
}

// Search dispatches the public search verb (mcp.Reader).
func (m *MCPReader) Search(ctx context.Context, query string, limit int) (any, error) {
	hits, err := m.svc.Search(ctx, query, limit)
	if err != nil {
		return nil, err
	}
	return renderWholePages(hits), nil
}

// Ask dispatches the hosted-ask agent (mcp.Reader).
func (m *MCPReader) Ask(ctx context.Context, owner, question string) (any, error) {
	return m.asker.Ask(ctx, owner, question)
}

// Timeline dispatches the public timeline verb (mcp.Reader).
func (m *MCPReader) Timeline(ctx context.Context, from, to string, limit int) (any, error) {
	evs, err := m.svc.Timeline(ctx, from, to, limit)
	if err != nil {
		return nil, err
	}
	return evs, nil
}

// renderWholePages renders search hits as caller-facing whole-page summaries
// (subject id + title + type/kind + body + version-free) — the §9.3 contract:
// a hit is the whole page, no scores.
func renderWholePages(pages []page.WholePage) []map[string]any {
	out := make([]map[string]any, 0, len(pages))
	for _, p := range pages {
		out = append(out, map[string]any{
			"subject": p.Subject,
			"title":   p.Title,
			"type":    p.Type,
			"kind":    p.Kind,
			"body":    p.Body,
		})
	}
	return out
}

// Timeline is the public timeline verb (design §9.2/§9.3): list event subjects
// whose occurred_at falls in [from, to] (ISO-8601 prefixes, lexicographic range).
// Zero-LLM registry query; it lists event subjects in a date window, it does not
// "answer questions about a period." limit<=0 means no cap.
func (s *Service) Timeline(ctx context.Context, from, to string, limit int) ([]page.TimelineEvent, error) {
	return s.store.Timeline(ctx, from, to, limit)
}

// resolveName is ask's lookup tool primitive (design §9.2): exact alias → whole
// page(s). Distinct from Search — no retriever fill, just the registry.
func (s *Service) resolveName(ctx context.Context, name string) ([]page.WholePage, error) {
	return s.store.Lookup(ctx, name)
}

// readPage is ask's read_page tool primitive (design §9.2): one subject's whole
// page. A missing page is reported, not an error.
func (s *Service) readPage(ctx context.Context, subject string) (page.WholePage, bool, error) {
	return s.store.ReadWholePage(ctx, subject)
}
