package page

import (
	"context"
	"database/sql"
	"fmt"
	"strings"
)

// Store is the registry's read surface over the subjects/aliases/pages tables
// (design §4.1, §4.3). P6b uses it for the two zero-LLM resolution reads: the
// alias lookup (ResolveByKeys) and the FTS candidate shortlist (Candidates).
// Writes (minting subjects, inserting aliases, page upserts) land in the
// end-of-run transaction (P7a) — Store is read-only here by design: resolution
// produces a plan, the commit applies it.
type Store struct {
	db *sql.DB
}

// NewStore wraps a migrated *sql.DB. The caller owns the handle's lifecycle.
func NewStore(db *sql.DB) *Store {
	return &Store{db: db}
}

// ResolveByKeys runs the single alias lookup of design §4.3:
//
//	SELECT DISTINCT subject_id FROM aliases WHERE type=? AND norm IN (keys)
//
// It is the mechanical heart of resolution. The returned id set drives the three
// arms: exactly one id → resolved; many ids → the candidate set is those subjects
// (handed to match, dup-flagged); zero ids → fall through to Candidates. The
// result order is stable (ascending subject_id) so the three arms and their tests
// are deterministic. An empty keys slice returns no ids without touching the DB.
func (s *Store) ResolveByKeys(ctx context.Context, typ Type, keys []string) ([]string, error) {
	if len(keys) == 0 {
		return nil, nil
	}
	placeholders := make([]string, len(keys))
	args := make([]any, 0, len(keys)+1)
	args = append(args, typ)
	for i, k := range keys {
		placeholders[i] = "?"
		args = append(args, k)
	}
	q := fmt.Sprintf(
		`SELECT DISTINCT subject_id FROM aliases WHERE type=? AND norm IN (%s) ORDER BY subject_id`,
		strings.Join(placeholders, ","),
	)
	rows, err := s.db.QueryContext(ctx, q, args...)
	if err != nil {
		return nil, fmt.Errorf("page: resolve by keys: %w", err)
	}
	defer rows.Close()

	var ids []string
	for rows.Next() {
		var id string
		if err := rows.Scan(&id); err != nil {
			return nil, fmt.Errorf("page: resolve by keys scan: %w", err)
		}
		ids = append(ids, id)
	}
	return ids, rows.Err()
}

// Candidate is one shortlist entry the match step (P6b2) will judge: the subject's
// id plus the registry fields the match excerpt is built from (canonical name +
// type). The page body excerpt itself is read by P6b2 (it owns WIKI_MATCH_EXCERPT_CHARS);
// P6b's job is only to produce the deterministic shortlist of subject ids.
type Candidate struct {
	// SubjectID is the candidate subject's id (→ subjects.id / pages.subject).
	SubjectID string
	// Type is the subject's type (always == the queried type — candidates never
	// cross the type boundary, design §4.3).
	Type Type
	// CanonicalName is the subject's chosen display name.
	CanonicalName string
}

// Candidates runs the two FTS5 queries of design §4.3's zero-ids arm, both scoped
// to the same type and capped at limit, and unions them into a deterministic
// shortlist:
//
//  1. name/alias tokens vs registry page titles (the canonical name lives in
//     pages_fts.title) — the lexical lane;
//  2. claim text vs page bodies — catches zero-token-overlap synonyms (AWS vs
//     "Amazon Cloud") the title query misses.
//
// This is the build-sequencing FTS-only candidate lane (the embedding lane that
// joins it lands in P11, design §9.3). Results are ordered by BM25 rank within
// each query, then unioned with the title-lane hits first and de-duplicated by
// subject, so the same input always yields the same shortlist. Either query text
// may be empty (it is then skipped); a subject with no page row cannot be a
// candidate (it has no FTS content yet), which is correct — a just-minted subject
// is never its own duplicate candidate.
func (s *Store) Candidates(ctx context.Context, typ Type, nameQuery, claimQuery string, limit int) ([]Candidate, error) {
	if limit <= 0 {
		return nil, nil
	}
	seen := make(map[string]struct{})
	var out []Candidate

	run := func(matchCol, query string) error {
		q := strings.TrimSpace(query)
		if q == "" {
			return nil
		}
		hits, err := s.ftsQuery(ctx, typ, matchCol, q, limit)
		if err != nil {
			return err
		}
		for _, c := range hits {
			if _, ok := seen[c.SubjectID]; ok {
				continue
			}
			seen[c.SubjectID] = struct{}{}
			out = append(out, c)
		}
		return nil
	}

	// Title lane first (the registry-name lexical match), then the body lane.
	if err := run("title", nameQuery); err != nil {
		return nil, err
	}
	if err := run("body", claimQuery); err != nil {
		return nil, err
	}
	return out, nil
}

// ftsQuery runs one FTS5 MATCH against a single column of pages_fts, joins to
// subjects to filter by type and pull the canonical name, and returns up to limit
// hits ordered by BM25 rank (best first). The match term is passed as a phrase so
// arbitrary user/claim text (which may contain FTS5 operator characters) never
// produces a syntax error or an injection — a near-certain failure mode if raw
// text were interpolated into the MATCH grammar.
func (s *Store) ftsQuery(ctx context.Context, typ Type, matchCol, term string, limit int) ([]Candidate, error) {
	match := ftsPhrase(term)
	if match == "" {
		return nil, nil
	}
	// pages_fts is external-content over pages (content_rowid='rowid'); its rowid
	// equals pages.rowid, so we join pages_fts.rowid → pages.rowid → subjects.id.
	// A column-scoped MATCH (pages_fts.title / pages_fts.body) restricts the search
	// to one column; bm25(pages_fts) ranks best-first. FTS5 ranking/auxiliary
	// functions require the table name (not an alias), so pages_fts is referenced
	// unaliased. matchCol is a fixed internal literal ("title" | "body"), never user
	// input, so the format interpolation is injection-safe.
	q := fmt.Sprintf(`
		SELECT s.id, s.type, s.canonical_name
		FROM pages_fts
		JOIN pages p    ON p.rowid = pages_fts.rowid
		JOIN subjects s ON s.id = p.subject
		WHERE pages_fts.%s MATCH ? AND s.type = ?
		ORDER BY bm25(pages_fts)
		LIMIT ?`, matchCol)

	rows, err := s.db.QueryContext(ctx, q, match, typ, limit)
	if err != nil {
		return nil, fmt.Errorf("page: candidate fts (%s): %w", matchCol, err)
	}
	defer rows.Close()

	var out []Candidate
	for rows.Next() {
		var c Candidate
		if err := rows.Scan(&c.SubjectID, &c.Type, &c.CanonicalName); err != nil {
			return nil, fmt.Errorf("page: candidate fts scan: %w", err)
		}
		out = append(out, c)
	}
	return out, rows.Err()
}

// ftsPhrase wraps the tokens of a free-text term into a single FTS5 phrase query,
// escaping embedded double-quotes by doubling them (FTS5 string-literal rule). A
// phrase query treats the whole term as ordinary tokens — no operator, prefix, or
// column-filter syntax is honored — so arbitrary text is safe and deterministic.
// Returns "" if the term has no FTS-indexable tokens.
func ftsPhrase(term string) string {
	t := strings.TrimSpace(term)
	if t == "" {
		return ""
	}
	// Double any embedded quotes, then wrap in quotes to form a phrase literal.
	escaped := strings.ReplaceAll(t, `"`, `""`)
	return `"` + escaped + `"`
}
