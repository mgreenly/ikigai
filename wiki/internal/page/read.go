package page

import (
	"context"
	"database/sql"
	"fmt"
)

// The read-side registry queries (design §9). These back the public search verb,
// ask's inner tools (lookup / read_page), and the timeline verb. All are
// zero-LLM and read-only: the read surface writes nothing (design §9.1). A "hit"
// is the WHOLE page (subject id, type, kind, title, full body) — the search
// contract of §9.3 ("a hit is the whole page").

// WholePage is one whole-page read result (design §9.3 search contract): the
// registry's identity fields joined onto the page's prose. type/kind live on
// subjects, not pages, so a whole-page read joins them (design §9.3 comment).
type WholePage struct {
	// Subject is the subject id (= pages.subject = subjects.id) — the page-level
	// citation key (design §9.2).
	Subject string
	// Type is the subject's closed-set type.
	Type Type
	// Kind is the freeform subtype.
	Kind string
	// Title is the page title (frontmatter; denormalized for search + FTS).
	Title string
	// Body is the full page markdown (inline [inbox-id] citations intact).
	Body string
}

// Lookup resolves an exact name to its subject(s) via the alias registry (design
// §9.2 lookup tool): the corpus's structural advantage over generic RAG — exact
// identity resolution. It normalizes the name to the alias key and returns every
// subject any type knows it by (the same key space resolution matched on). The
// result is the whole page for each match, in stable subject-id order; an unknown
// name yields no rows (not an error).
func (s *Store) Lookup(ctx context.Context, name string) ([]WholePage, error) {
	key := Normalize(name)
	if key == "" {
		return nil, nil
	}
	rows, err := s.db.QueryContext(ctx, `
		SELECT s.id, s.type, s.kind, COALESCE(p.title, ''), COALESCE(p.body, '')
		FROM aliases a
		JOIN subjects s ON s.id = a.subject_id
		LEFT JOIN pages p ON p.subject = s.id
		WHERE a.norm = ?
		ORDER BY s.id`, key)
	if err != nil {
		return nil, fmt.Errorf("page: lookup %q: %w", name, err)
	}
	defer rows.Close()
	return scanWholePages(rows)
}

// ReadWholePage returns one subject's whole page (design §9.2 read_page tool /
// the search hit). ok=false (no error) when the subject has no page row yet (or no
// such subject) — read_page is about the prose, and a subject with no page has
// nothing to read.
func (s *Store) ReadWholePage(ctx context.Context, subject string) (WholePage, bool, error) {
	var wp WholePage
	err := s.db.QueryRowContext(ctx, `
		SELECT s.id, s.type, s.kind, p.title, p.body
		FROM subjects s
		JOIN pages p ON p.subject = s.id
		WHERE s.id = ?`, subject,
	).Scan(&wp.Subject, &wp.Type, &wp.Kind, &wp.Title, &wp.Body)
	if err == sql.ErrNoRows {
		return WholePage{}, false, nil
	}
	if err != nil {
		return WholePage{}, false, fmt.Errorf("page: read whole page %q: %w", subject, err)
	}
	return wp, true, nil
}

// SearchPages runs the lexical (FTS5/BM25) lane of the search verb (design §9.3):
// the query is matched against both page titles and bodies, results unioned (title
// hits first), de-duplicated by subject, ranked best-first, and returned as whole
// pages capped at limit. This is the build-sequencing FTS-only lane; the hybrid
// retriever (P11) slots behind the same call without changing this contract.
//
// The query is passed as an FTS5 phrase (ftsPhrase) so arbitrary user text never
// produces a syntax error or injection — the same safety the candidate lane uses.
func (s *Store) SearchPages(ctx context.Context, query string, limit int) ([]WholePage, error) {
	if limit <= 0 {
		return nil, nil
	}
	match := ftsPhrase(query)
	if match == "" {
		return nil, nil
	}
	seen := make(map[string]struct{})
	var out []WholePage

	run := func(col string) error {
		// pages_fts is external-content over pages; rowid joins back to pages then
		// subjects. col is a fixed literal ("title" | "body"), never user input.
		q := fmt.Sprintf(`
			SELECT s.id, s.type, s.kind, p.title, p.body
			FROM pages_fts
			JOIN pages p    ON p.rowid = pages_fts.rowid
			JOIN subjects s ON s.id = p.subject
			WHERE pages_fts.%s MATCH ?
			ORDER BY bm25(pages_fts)
			LIMIT ?`, col)
		rows, err := s.db.QueryContext(ctx, q, match, limit)
		if err != nil {
			return fmt.Errorf("page: search (%s): %w", col, err)
		}
		defer rows.Close()
		hits, err := scanWholePages(rows)
		if err != nil {
			return err
		}
		for _, h := range hits {
			if _, ok := seen[h.Subject]; ok {
				continue
			}
			seen[h.Subject] = struct{}{}
			out = append(out, h)
			if len(out) >= limit {
				break
			}
		}
		return nil
	}

	if err := run("title"); err != nil {
		return nil, err
	}
	if len(out) < limit {
		if err := run("body"); err != nil {
			return nil, err
		}
	}
	if len(out) > limit {
		out = out[:limit]
	}
	return out, nil
}

// TimelineEvent is one event subject in a timeline window (design §9.2 timeline):
// the event's identity plus its world-time. The body is omitted — timeline is a
// "list event subjects in a date window" view, not a page dump (design §9.2).
type TimelineEvent struct {
	Subject       string
	CanonicalName string
	Kind          string
	OccurredAt    string
	Title         string
}

// Timeline lists event subjects whose occurred_at falls in [from, to] (design
// §9.2 / §9.3): a zero-LLM registry query over type='event' subjects. The bounds
// are ISO-8601 prefixes compared lexicographically (an ISO-8601 prefix range is a
// lexicographic range), so "2024" .. "2024-06" works without date parsing. Either
// bound may be empty (open-ended on that side). Results are ordered by occurred_at
// ascending, then subject id for determinism.
func (s *Store) Timeline(ctx context.Context, from, to string, limit int) ([]TimelineEvent, error) {
	q := `
		SELECT s.id, s.canonical_name, s.kind, COALESCE(s.occurred_at, ''), COALESCE(p.title, '')
		FROM subjects s
		LEFT JOIN pages p ON p.subject = s.id
		WHERE s.type = 'event' AND s.occurred_at IS NOT NULL AND s.occurred_at <> ''`
	var args []any
	if from != "" {
		q += ` AND s.occurred_at >= ?`
		args = append(args, from)
	}
	if to != "" {
		// Inclusive of the prefix: "2024-06" should include "2024-06-30T..." — append
		// a high sentinel so any longer prefix under `to` still falls inside.
		q += ` AND s.occurred_at <= ?`
		args = append(args, to+"￿")
	}
	q += ` ORDER BY s.occurred_at, s.id`
	if limit > 0 {
		q += ` LIMIT ?`
		args = append(args, limit)
	}
	rows, err := s.db.QueryContext(ctx, q, args...)
	if err != nil {
		return nil, fmt.Errorf("page: timeline: %w", err)
	}
	defer rows.Close()
	var out []TimelineEvent
	for rows.Next() {
		var e TimelineEvent
		if err := rows.Scan(&e.Subject, &e.CanonicalName, &e.Kind, &e.OccurredAt, &e.Title); err != nil {
			return nil, fmt.Errorf("page: timeline scan: %w", err)
		}
		out = append(out, e)
	}
	return out, rows.Err()
}

// scanWholePages drains a *sql.Rows of (id, type, kind, title, body) into
// WholePage values.
func scanWholePages(rows *sql.Rows) ([]WholePage, error) {
	var out []WholePage
	for rows.Next() {
		var wp WholePage
		if err := rows.Scan(&wp.Subject, &wp.Type, &wp.Kind, &wp.Title, &wp.Body); err != nil {
			return nil, fmt.Errorf("page: scan whole page: %w", err)
		}
		out = append(out, wp)
	}
	return out, rows.Err()
}
