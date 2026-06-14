package lint

import (
	"context"
	"sort"
	"testing"

	"wiki/internal/integrate"
	"wiki/internal/page"
)

// fakeSweepStore records the flags lint-sweep makes and serves canned candidate
// hits per subject, so the job is exercised with NO DB and NO LLM (the sweep has
// no LLM at all — this proves the mechanical walk + flag logic).
type fakeSweepStore struct {
	subjects []page.SweepSubject
	// cands maps a subject id → the candidate hits its two queries return.
	cands map[string][]page.Candidate
	// flagged records each canonical-ordered flag pair, de-duplicated like the
	// real FlagDupAuto (the pair UNIQUE).
	flagged map[string]struct{}
	limit   int // the limit the job passed to Candidates (asserted = config knob)
}

func newFakeSweepStore() *fakeSweepStore {
	return &fakeSweepStore{cands: map[string][]page.Candidate{}, flagged: map[string]struct{}{}}
}

func (f *fakeSweepStore) EnumerateSweepSubjects(context.Context) ([]page.SweepSubject, error) {
	return f.subjects, nil
}

func (f *fakeSweepStore) FlagDupAuto(_ context.Context, a, b string) error {
	if a == b || a == "" || b == "" {
		return nil
	}
	if a > b {
		a, b = b, a
	}
	f.flagged[a+","+b] = struct{}{}
	return nil
}

// sweepStoreByID drives Candidates off the enumerating subject's id, which is what
// the job's per-subject call needs. It wraps fakeSweepStore so Candidates returns
// the canned hits for the subject currently being swept.
type sweepStoreByID struct {
	*fakeSweepStore
	idx int // next subject index to serve candidates for
}

func (s *sweepStoreByID) Candidates(_ context.Context, _ page.Type, _, _ string, limit int) ([]page.Candidate, error) {
	s.limit = limit
	id := s.subjects[s.idx].SubjectID
	s.idx++
	return s.cands[id], nil
}

func TestSweepFlagsCandidatePairs(t *testing.T) {
	base := newFakeSweepStore()
	base.subjects = []page.SweepSubject{
		{SubjectID: "01A", Type: page.TypeEntity, CanonicalName: "Acme", Body: "maker"},
		{SubjectID: "01B", Type: page.TypeEntity, CanonicalName: "ACME Corp", Body: "maker"},
		{SubjectID: "01C", Type: page.TypeEntity, CanonicalName: "Globex", Body: "other"},
	}
	// 01A's queries return 01B (a real dup) and 01A itself (self-hit, dropped).
	base.cands["01A"] = []page.Candidate{
		{SubjectID: "01A", Type: page.TypeEntity},
		{SubjectID: "01B", Type: page.TypeEntity},
	}
	// 01B's queries return 01A again (the pair is symmetric; idempotent flag).
	base.cands["01B"] = []page.Candidate{{SubjectID: "01A", Type: page.TypeEntity}}
	// 01C matches nobody.
	base.cands["01C"] = nil

	store := &sweepStoreByID{fakeSweepStore: base}
	j := NewSweepJob(store, 7)
	if _, err := j.Integrate(context.Background(), integrate.Unit{}); err != nil {
		t.Fatalf("integrate: %v", err)
	}

	// Exactly one flag: the (01A,01B) pair, canonical-ordered and de-duped despite
	// being surfaced from both directions; the self-hit was dropped.
	var got []string
	for k := range base.flagged {
		got = append(got, k)
	}
	sort.Strings(got)
	if len(got) != 1 || got[0] != "01A,01B" {
		t.Fatalf("want exactly [01A,01B], got %v", got)
	}
	// The config-injected per-lane flag threshold reached Candidates verbatim.
	if store.limit != 7 {
		t.Fatalf("flag threshold not injected to Candidates: got %d", store.limit)
	}
}

func TestSweepEmptyManifestAndJobName(t *testing.T) {
	store := &sweepStoreByID{fakeSweepStore: newFakeSweepStore()}
	j := NewSweepJob(store, 0) // non-positive → default
	if j.Job() != SweepJobName {
		t.Fatalf("job name: %q", j.Job())
	}
	m, err := j.Integrate(context.Background(), integrate.Unit{})
	if err != nil {
		t.Fatalf("integrate: %v", err)
	}
	if m == nil {
		t.Fatal("manifest must be non-nil (empty)")
	}
	if len(m.Subjects) != 0 || len(m.DupPairs) != 0 {
		t.Fatalf("lint-sweep must return an EMPTY manifest, got %+v", m)
	}
}

func TestSweepLimitDefault(t *testing.T) {
	store := &sweepStoreByID{fakeSweepStore: newFakeSweepStore()}
	store.subjects = []page.SweepSubject{{SubjectID: "01A", Type: page.TypeEntity}}
	j := NewSweepJob(store, 0)
	if _, err := j.Integrate(context.Background(), integrate.Unit{}); err != nil {
		t.Fatalf("integrate: %v", err)
	}
	if store.limit != DefaultSweepLimit {
		t.Fatalf("non-positive knob must fall back to default %d, got %d", DefaultSweepLimit, store.limit)
	}
}
