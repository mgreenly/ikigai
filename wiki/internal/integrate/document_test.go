package integrate

import (
	"context"
	"testing"

	"wiki/internal/config"
	"wiki/internal/page"
)

// TestDocumentReResolveLandsOnWinner proves the duplicate-mint conflict arm (P7b2):
// ReResolve re-runs RESOLVE → MATCH → MERGE for the ONE colliding subject (never
// extract), and — because the winner is now in the registry — the subject resolves
// ONTO the winner. The conflicting manifest entry's identity is replaced in place and
// merge re-folds, so the next commit targets the winner's page.
func TestDocumentReResolveLandsOnWinner(t *testing.T) {
	// The registry now resolves "Acme" → the winner (the winner committed its alias
	// between this run's first resolve and the conflict).
	reg := &fakeRegistry{ids: map[string]string{
		TypeEntity + "\x00" + page.Normalize("Acme"): "winner",
	}}
	res := NewResolver(reg, 5)

	matchMock := &mockCaller{resp: `{"verdict":"no_match","candidate_pairs":[]}`}
	matcher := NewMatcher(matchMock, &fakeExcerptReader{}, config.CallSite{Name: "match", Prompt: "p", Model: "claude-sonnet-4-6", Effort: "low"}, 600)
	asm := NewAssembler(matcher, func() string { return "fresh-id" })

	reader := &fakePageReader{
		versions: map[string]int{"winner": 3},
		pages:    map[string]struct{ title, body string }{"winner": {"Acme", "winner body [01OLD]"}},
	}
	mergeMock := &mockCaller{resp: `{"pages":[{"subject":"winner","title":"Acme","body":"Acme builds rockets. [01HXDOC] [01OLD]","superseded":[]}]}`}
	merger := NewMerger(mergeMock, reader, mergeSite())

	d := NewDocument(nil, nil, res, asm, merger)

	// The manifest as it stood when the loser minted "loser" (the colliding id).
	m := &Manifest{Subjects: []Subject{{
		Type: TypeEntity, Kind: "org", Name: "Acme", Aliases: []string{"acme"},
		SubjectID: "loser", TargetPage: "loser", BaseVersion: 0,
		PageTitle: "Acme", PageBody: "stale body [01HXDOC]",
		Claims: []Claim{{Text: "Acme builds rockets.", Cites: []string{"01HXDOC"}}},
	}}}

	if err := d.ReResolve(context.Background(), m, "loser"); err != nil {
		t.Fatalf("re-resolve: %v", err)
	}

	if len(m.Subjects) != 1 {
		t.Fatalf("subjects = %d, want 1", len(m.Subjects))
	}
	got := m.Subjects[0]
	if got.SubjectID != "winner" {
		t.Fatalf("re-resolved subject id = %q, want winner", got.SubjectID)
	}
	if got.TargetPage != "winner" {
		t.Fatalf("re-resolved target page = %q, want winner", got.TargetPage)
	}
	// Merge re-folded against the winner's fresh page: the base version is the
	// winner's current version and the page body is the re-merged prose.
	if got.BaseVersion != 3 {
		t.Fatalf("base version = %d, want 3 (the winner's current version)", got.BaseVersion)
	}
	if got.PageBody == "stale body [01HXDOC]" {
		t.Fatalf("page body not re-merged after re-resolve")
	}
}

// TestDocumentReResolveUnknownSubject proves ReResolve errors cleanly when the named
// subject is not in the manifest (a wiring bug, never a silent no-op).
func TestDocumentReResolveUnknownSubject(t *testing.T) {
	d := NewDocument(nil, nil, NewResolver(&fakeRegistry{}, 5),
		NewAssembler(nil, func() string { return "x" }), nil)
	m := &Manifest{Subjects: []Subject{{SubjectID: "a"}}}
	if err := d.ReResolve(context.Background(), m, "missing"); err == nil {
		t.Fatal("re-resolve of an absent subject must error")
	}
}
