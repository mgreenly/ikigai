package integrate

import (
	"context"
	"fmt"
	"strings"

	"wiki/internal/page"
)

// Outcome is the kind of resolution a single extracted subject reached — the
// P6b→P6b2 seam (design §4.3). It is deliberately a small closed set: P6b is the
// mechanical half (zero LLM), so each subject leaves it in exactly one of three
// states, and P6b2's match runs only on the Shortlist state.
type Outcome int

const (
	// OutcomeResolved — the alias lookup returned exactly one id: the subject is
	// that registry subject, no LLM needed (design §4.3 "one id"). SubjectID is set.
	OutcomeResolved Outcome = iota
	// OutcomeCreate — neither the alias lookup nor the FTS candidate step found
	// anything: this is a brand-new subject, mint it, no LLM (design §4.3 "zero
	// candidates → create it"). SubjectID is empty (P6b2/the commit mint the ULID).
	OutcomeCreate
	// OutcomeShortlist — either the alias lookup returned many ids, or it returned
	// zero but the FTS step found candidates: the match LLM call (P6b2) judges the
	// shortlist. Candidates holds the subjects to judge.
	OutcomeShortlist
)

// String renders an Outcome for tests and logs.
func (o Outcome) String() string {
	switch o {
	case OutcomeResolved:
		return "resolved"
	case OutcomeCreate:
		return "create"
	case OutcomeShortlist:
		return "shortlist"
	default:
		return fmt.Sprintf("Outcome(%d)", int(o))
	}
}

// Resolution is the per-subject mechanical resolution result — the P6b→P6b2 seam.
// P6b produces one Resolution per extracted subject; P6b2 consumes it: a Resolved
// subject is annotated directly, a Create subject is minted, and a Shortlist
// subject is handed to match. DupPairs carries the many-ids pairs P6b's resolve
// arm surfaces (design §4.3 "the pair is also dup-flagged"); P6b2 folds them into
// the manifest's DupPairs alongside match's side-channel pairs.
type Resolution struct {
	// Subject is the extracted subject this resolution is for (its index in the
	// input slice is its identity; the value is carried for P6b2's convenience).
	Subject Subject
	// Outcome is which of the three arms this subject took.
	Outcome Outcome
	// SubjectID is the resolved id when Outcome == OutcomeResolved (empty otherwise).
	SubjectID string
	// Candidates is the shortlist to judge when Outcome == OutcomeShortlist (nil
	// otherwise): for the many-ids arm it is the colliding registry subjects; for
	// the zero-ids arm it is the FTS candidate shortlist.
	Candidates []page.Candidate
	// DupPairs are canonical-order pairs this subject's resolution surfaced (the
	// many-ids arm dup-flags every pair among the colliding ids — design §4.3).
	DupPairs []DupPair
}

// registry is the minimal slice of *page.Store the resolver needs: the alias
// lookup and the candidate FTS shortlist. Declared as an interface so the resolver
// is unit-testable without a live DB and so P6b2 can reuse the same seam.
type registry interface {
	ResolveByKeys(ctx context.Context, typ page.Type, keys []string) ([]string, error)
	Candidates(ctx context.Context, typ page.Type, nameQuery, claimQuery string, limit int) ([]page.Candidate, error)
}

// Resolver runs the document pass's mechanical resolution half (design §4.3):
// per extracted subject it builds the key set, does the one alias lookup, and —
// on a miss — runs the two FTS candidate queries. It calls NO LLM and writes
// NOTHING; it produces the Resolution seam P6b2's match consumes. CandidateLimit
// is the config-injected FTS top-N (eval-harness knob, obligation 2), defaulting
// to the design's ~5 when unset.
type Resolver struct {
	reg            registry
	candidateLimit int
}

// NewResolver builds a Resolver over the registry and the config-injected
// candidate FTS limit (design §4.3 "top ~5"; a non-positive value falls back to
// the default 5 so a mis-set knob never disables candidate retrieval entirely).
func NewResolver(reg registry, candidateLimit int) *Resolver {
	if candidateLimit <= 0 {
		candidateLimit = DefaultCandidateLimit
	}
	return &Resolver{reg: reg, candidateLimit: candidateLimit}
}

// DefaultCandidateLimit is the design §4.3 "top ~5" candidate shortlist size used
// when the config knob is unset.
const DefaultCandidateLimit = 5

// Resolve runs the three-arm mechanical resolution for every extracted subject,
// in input order, returning one Resolution each. It is pure relative to the DB
// (read-only) and deterministic for a fixed registry state — the property the
// P6b deliverable gate and the eval harness's candidate-lane scoring both rely on.
func (r *Resolver) Resolve(ctx context.Context, subjects []Subject) ([]Resolution, error) {
	out := make([]Resolution, 0, len(subjects))
	for i := range subjects {
		res, err := r.resolveOne(ctx, subjects[i])
		if err != nil {
			return nil, fmt.Errorf("resolve: subject %d (%q): %w", i, subjects[i].Name, err)
		}
		out = append(out, res)
	}
	return out, nil
}

// resolveOne resolves a single extracted subject through the three arms.
func (r *Resolver) resolveOne(ctx context.Context, s Subject) (Resolution, error) {
	res := Resolution{Subject: s}

	keys := page.KeySet(s.Name, s.Aliases)
	ids, err := r.reg.ResolveByKeys(ctx, s.Type, keys)
	if err != nil {
		return res, fmt.Errorf("alias lookup: %w", err)
	}

	switch {
	case len(ids) == 1:
		// One id → resolved, no LLM (design §4.3).
		res.Outcome = OutcomeResolved
		res.SubjectID = ids[0]
		return res, nil

	case len(ids) > 1:
		// Many ids → the candidate set IS those subjects (straight to match, no
		// FTS); every pair among them is also dup-flagged (design §4.3). A create
		// answer remains legal — that is match's call (P6b2).
		res.Outcome = OutcomeShortlist
		res.Candidates = candidatesFromIDs(s.Type, ids)
		res.DupPairs = pairsAmong(ids)
		return res, nil

	default:
		// Zero ids → the two FTS candidate queries (design §4.3). The name/alias
		// query is the subject's surface forms; the claim query is its claim prose.
		cands, err := r.reg.Candidates(ctx, s.Type, nameQueryFor(s), claimQueryFor(s), r.candidateLimit)
		if err != nil {
			return res, fmt.Errorf("candidates: %w", err)
		}
		if len(cands) == 0 {
			// Zero candidates → create it, no LLM (design §4.3).
			res.Outcome = OutcomeCreate
			return res, nil
		}
		res.Outcome = OutcomeShortlist
		res.Candidates = cands
		return res, nil
	}
}

// candidatesFromIDs turns the many-ids collision set into Candidate shortlist
// entries (the many-ids arm hands the colliding subjects straight to match, no FTS
// — design §4.3). CanonicalName is left empty here: P6b2's match excerpt reads the
// page row for the name/body it judges, and these ids already address real pages.
func candidatesFromIDs(typ page.Type, ids []string) []page.Candidate {
	out := make([]page.Candidate, 0, len(ids))
	for _, id := range ids {
		out = append(out, page.Candidate{SubjectID: id, Type: typ})
	}
	return out
}

// nameQueryFor builds the name/alias FTS query text — the subject's surface forms
// joined, matched against registry page titles (design §4.3 query 1).
func nameQueryFor(s Subject) string {
	parts := make([]string, 0, 1+len(s.Aliases))
	if n := strings.TrimSpace(s.Name); n != "" {
		parts = append(parts, n)
	}
	for _, a := range s.Aliases {
		if t := strings.TrimSpace(a); t != "" {
			parts = append(parts, t)
		}
	}
	return strings.Join(parts, " ")
}

// claimQueryFor builds the claim-text FTS query — the subject's claim prose joined,
// matched against page bodies (design §4.3 query 2, the zero-token-overlap-synonym
// catcher).
func claimQueryFor(s Subject) string {
	parts := make([]string, 0, len(s.Claims))
	for _, c := range s.Claims {
		if t := strings.TrimSpace(c.Text); t != "" {
			parts = append(parts, t)
		}
	}
	return strings.Join(parts, " ")
}

// pairsAmong returns every unordered pair of the given ids in canonical order
// (smaller ULID first — Manifest canonicity / design §3). The ids come from
// ResolveByKeys already sorted ascending, so adjacent ordering is preserved, but
// canonicalize defensively rather than rely on the caller's sort.
func pairsAmong(ids []string) []DupPair {
	var out []DupPair
	for i := 0; i < len(ids); i++ {
		for j := i + 1; j < len(ids); j++ {
			a, b := ids[i], ids[j]
			if a > b {
				a, b = b, a
			}
			if a == b {
				continue
			}
			out = append(out, DupPair{SubjectA: a, SubjectB: b})
		}
	}
	return out
}
