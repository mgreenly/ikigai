package integrate

import (
	"context"
	"reflect"
	"testing"

	"wiki/internal/page"
)

// fakeRegistry is a deterministic, in-memory registry stub for the resolver's
// three arms — no DB, no LLM. ids maps a (type, normalized-key) to a subject id;
// cands maps a type to the FTS candidate shortlist returned for any non-empty
// query (P6b's resolver only branches on len(candidates), so a per-type fixed
// shortlist exercises every arm).
type fakeRegistry struct {
	ids   map[string]string // key: type + "\x00" + norm
	cands map[page.Type][]page.Candidate
	// lastNameQuery / lastClaimQuery capture the last Candidates call args so a
	// test can assert the two FTS queries are built from the subject's forms.
	lastNameQuery  string
	lastClaimQuery string
	lastLimit      int
}

func (f *fakeRegistry) ResolveByKeys(_ context.Context, typ page.Type, keys []string) ([]string, error) {
	seen := map[string]struct{}{}
	var out []string
	for _, k := range keys {
		if id, ok := f.ids[typ+"\x00"+k]; ok {
			if _, dup := seen[id]; !dup {
				seen[id] = struct{}{}
				out = append(out, id)
			}
		}
	}
	// Mirror the store's deterministic ascending order.
	for i := 0; i < len(out); i++ {
		for j := i + 1; j < len(out); j++ {
			if out[j] < out[i] {
				out[i], out[j] = out[j], out[i]
			}
		}
	}
	return out, nil
}

func (f *fakeRegistry) Candidates(_ context.Context, typ page.Type, nameQuery, claimQuery string, limit int) ([]page.Candidate, error) {
	f.lastNameQuery = nameQuery
	f.lastClaimQuery = claimQuery
	f.lastLimit = limit
	return f.cands[typ], nil
}

func subj(name, typ string, aliases []string, claims ...string) Subject {
	cl := make([]Claim, 0, len(claims))
	for _, c := range claims {
		cl = append(cl, Claim{Text: c})
	}
	return Subject{Type: typ, Name: name, Aliases: aliases, Claims: cl}
}

func TestResolveOneIDIsResolved(t *testing.T) {
	reg := &fakeRegistry{ids: map[string]string{
		TypeEntity + "\x00" + page.Normalize("Acme"): "subj-A",
	}}
	r := NewResolver(reg, 5)
	got, err := r.Resolve(context.Background(), []Subject{subj("Acme", TypeEntity, nil, "makes anvils")})
	if err != nil {
		t.Fatal(err)
	}
	if got[0].Outcome != OutcomeResolved {
		t.Fatalf("outcome = %v, want resolved", got[0].Outcome)
	}
	if got[0].SubjectID != "subj-A" {
		t.Errorf("SubjectID = %q, want subj-A", got[0].SubjectID)
	}
}

func TestResolveManyIDsIsShortlistAndDupFlagged(t *testing.T) {
	reg := &fakeRegistry{ids: map[string]string{
		TypeEntity + "\x00" + page.Normalize("Acme"): "subj-A",
		TypeEntity + "\x00" + page.Normalize("Beta"): "subj-B",
	}}
	r := NewResolver(reg, 5)
	// One subject whose key set bridges two registry subjects.
	got, err := r.Resolve(context.Background(), []Subject{subj("Acme", TypeEntity, []string{"Beta"}, "a claim")})
	if err != nil {
		t.Fatal(err)
	}
	res := got[0]
	if res.Outcome != OutcomeShortlist {
		t.Fatalf("outcome = %v, want shortlist", res.Outcome)
	}
	wantCands := []page.Candidate{
		{SubjectID: "subj-A", Type: TypeEntity},
		{SubjectID: "subj-B", Type: TypeEntity},
	}
	if !reflect.DeepEqual(res.Candidates, wantCands) {
		t.Errorf("candidates = %+v, want %+v", res.Candidates, wantCands)
	}
	wantPairs := []DupPair{{SubjectA: "subj-A", SubjectB: "subj-B"}}
	if !reflect.DeepEqual(res.DupPairs, wantPairs) {
		t.Errorf("dup pairs = %+v, want %+v", res.DupPairs, wantPairs)
	}
}

func TestResolveManyIDsThreeWayPairsCanonical(t *testing.T) {
	// Three colliding ids → three canonical-order pairs.
	got := pairsAmong([]string{"subj-C", "subj-A", "subj-B"})
	want := []DupPair{
		{SubjectA: "subj-A", SubjectB: "subj-C"},
		{SubjectA: "subj-B", SubjectB: "subj-C"},
		{SubjectA: "subj-A", SubjectB: "subj-B"},
	}
	if !reflect.DeepEqual(got, want) {
		t.Errorf("pairsAmong = %+v, want %+v", got, want)
	}
}

func TestResolveZeroIDsZeroCandidatesIsCreate(t *testing.T) {
	reg := &fakeRegistry{} // no ids, no candidates
	r := NewResolver(reg, 5)
	got, err := r.Resolve(context.Background(), []Subject{subj("Newbie", TypeEntity, nil, "a fresh claim")})
	if err != nil {
		t.Fatal(err)
	}
	if got[0].Outcome != OutcomeCreate {
		t.Fatalf("outcome = %v, want create", got[0].Outcome)
	}
	if got[0].SubjectID != "" {
		t.Errorf("SubjectID = %q, want empty (minted later)", got[0].SubjectID)
	}
}

func TestResolveZeroIDsWithCandidatesIsShortlist(t *testing.T) {
	reg := &fakeRegistry{cands: map[page.Type][]page.Candidate{
		TypeEntity: {{SubjectID: "subj-X", Type: TypeEntity, CanonicalName: "Existing"}},
	}}
	r := NewResolver(reg, 5)
	got, err := r.Resolve(context.Background(), []Subject{subj("Ambiguous", TypeEntity, []string{"AltName"}, "claim one", "claim two")})
	if err != nil {
		t.Fatal(err)
	}
	res := got[0]
	if res.Outcome != OutcomeShortlist {
		t.Fatalf("outcome = %v, want shortlist", res.Outcome)
	}
	if len(res.Candidates) != 1 || res.Candidates[0].SubjectID != "subj-X" {
		t.Errorf("candidates = %+v, want [subj-X]", res.Candidates)
	}
	if len(res.DupPairs) != 0 {
		t.Errorf("zero-ids arm must not dup-flag: %+v", res.DupPairs)
	}
	// The two FTS queries are built from the subject's forms.
	if reg.lastNameQuery != "Ambiguous AltName" {
		t.Errorf("name query = %q, want %q", reg.lastNameQuery, "Ambiguous AltName")
	}
	if reg.lastClaimQuery != "claim one claim two" {
		t.Errorf("claim query = %q, want %q", reg.lastClaimQuery, "claim one claim two")
	}
}

func TestResolverDefaultsLimit(t *testing.T) {
	reg := &fakeRegistry{}
	r := NewResolver(reg, 0) // non-positive → DefaultCandidateLimit
	_, err := r.Resolve(context.Background(), []Subject{subj("X", TypeEntity, nil, "c")})
	if err != nil {
		t.Fatal(err)
	}
	if reg.lastLimit != DefaultCandidateLimit {
		t.Errorf("candidate limit = %d, want %d", reg.lastLimit, DefaultCandidateLimit)
	}
}

func TestResolvePreservesOrderAndCount(t *testing.T) {
	reg := &fakeRegistry{ids: map[string]string{
		TypeEntity + "\x00" + page.Normalize("Known"): "subj-K",
	}}
	r := NewResolver(reg, 5)
	in := []Subject{
		subj("Known", TypeEntity, nil, "c1"),
		subj("Unknown", TypeEntity, nil, "c2"),
	}
	got, err := r.Resolve(context.Background(), in)
	if err != nil {
		t.Fatal(err)
	}
	if len(got) != 2 {
		t.Fatalf("got %d resolutions, want 2", len(got))
	}
	if got[0].Outcome != OutcomeResolved || got[1].Outcome != OutcomeCreate {
		t.Errorf("outcomes = [%v %v], want [resolved create]", got[0].Outcome, got[1].Outcome)
	}
	if got[0].Subject.Name != "Known" || got[1].Subject.Name != "Unknown" {
		t.Errorf("subject order not preserved: %q %q", got[0].Subject.Name, got[1].Subject.Name)
	}
}
