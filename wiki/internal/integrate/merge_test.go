package integrate

import (
	"context"
	"testing"

	"wiki/internal/config"
)

// fakePageReader is the unit gate's stub page read surface for merge: canned
// version + content per subject so merge is exercised without a DB.
type fakePageReader struct {
	versions map[string]int
	pages    map[string]struct{ title, body string }
}

func (f *fakePageReader) ReadVersion(_ context.Context, subject string) (int, error) {
	return f.versions[subject], nil
}

func (f *fakePageReader) ReadPage(_ context.Context, subject string) (string, string, bool, error) {
	if p, ok := f.pages[subject]; ok {
		return p.title, p.body, true, nil
	}
	return "", "", false, nil
}

func mergeSite() config.CallSite {
	return config.CallSite{Name: "merge", Prompt: "placeholder", Model: "claude-sonnet-4-6", Effort: "high"}
}

func manifestForMerge() *Manifest {
	return &Manifest{
		Subjects: []Subject{{
			Type: TypeEntity, Kind: "org", Name: "Acme", Aliases: []string{"acme"},
			SubjectID: "subj-1", TargetPage: "subj-1",
			Claims: []Claim{{Text: "Acme builds rockets.", Cites: []string{"01HXDOC"}}},
		}},
	}
}

// TestMergeRecordsBaseVersionAndPageContent proves merge records the base version
// slot (P7b's guard) and folds the model's page output into the manifest's
// per-subject PageTitle/PageBody + Superseded.
func TestMergeRecordsBaseVersionAndPageContent(t *testing.T) {
	reader := &fakePageReader{
		versions: map[string]int{"subj-1": 7},
		pages:    map[string]struct{ title, body string }{"subj-1": {"Acme", "old body [01OLD]"}},
	}
	resp := `{"pages":[{"subject":"subj-1","title":"Acme","body":"Acme builds rockets. [01HXDOC]","superseded":["01OLD"]}]}`
	mock := &mockCaller{resp: resp}
	m := NewMerger(mock, reader, mergeSite())

	manifest := manifestForMerge()
	if _, err := m.Merge(context.Background(), manifest); err != nil {
		t.Fatalf("merge: %v", err)
	}

	s := manifest.Subjects[0]
	if s.BaseVersion != 7 {
		t.Fatalf("BaseVersion = %d, want 7 (the version merge read)", s.BaseVersion)
	}
	if s.PageBody != "Acme builds rockets. [01HXDOC]" {
		t.Fatalf("PageBody = %q", s.PageBody)
	}
	if s.PageTitle != "Acme" {
		t.Fatalf("PageTitle = %q, want Acme", s.PageTitle)
	}
	if len(s.Superseded) != 1 || s.Superseded[0] != "01OLD" {
		t.Fatalf("Superseded = %v, want [01OLD]", s.Superseded)
	}
	// Merge was given the manifest, never an original document.
	if mock.gotSite.Name != "merge" {
		t.Fatalf("merge ran on the wrong site: %q", mock.gotSite.Name)
	}
}

// TestMergeAppendsStaleNotes proves a stale note merge surfaces (a contradicted
// neighbor) lands on the manifest's StaleNotes carrier for the end-of-run commit.
func TestMergeAppendsStaleNotes(t *testing.T) {
	reader := &fakePageReader{versions: map[string]int{"subj-1": 0}}
	resp := `{"pages":[{"subject":"subj-1","title":"Acme","body":"Acme builds rockets. [01HXDOC]"}],` +
		`"stale_notes":[{"subject":"neighbor-9","note":"contradicts the new rocket claim","cites":["01HXDOC"]}]}`
	mock := &mockCaller{resp: resp}
	m := NewMerger(mock, reader, mergeSite())

	manifest := manifestForMerge()
	if _, err := m.Merge(context.Background(), manifest); err != nil {
		t.Fatalf("merge: %v", err)
	}
	if len(manifest.StaleNotes) != 1 {
		t.Fatalf("stale notes = %d, want 1", len(manifest.StaleNotes))
	}
	sn := manifest.StaleNotes[0]
	if sn.Subject != "neighbor-9" || sn.Note == "" || len(sn.Cites) != 1 {
		t.Fatalf("stale note = %+v", sn)
	}
}

// TestApplyMergeRejectsPageOutsideWriteSet proves the write-set conformance check:
// merge may not write a page it was not handed (design §4.4 "the manifest's pages,
// exactly").
func TestApplyMergeRejectsPageOutsideWriteSet(t *testing.T) {
	manifest := manifestForMerge()
	resp := `{"pages":[{"subject":"INTRUDER","title":"x","body":"y"}]}`
	if _, err := ApplyMerge(manifest, resp); err == nil {
		t.Fatal("expected rejection of a page outside the write set")
	}
}

// TestApplyMergeRejectsUnwrittenWriteSetPage proves every write-set page must be
// written (merge owes a body for each subject it was handed).
func TestApplyMergeRejectsUnwrittenWriteSetPage(t *testing.T) {
	manifest := manifestForMerge()
	resp := `{"pages":[]}`
	if _, err := ApplyMerge(manifest, resp); err == nil {
		t.Fatal("expected rejection: write-set page was not written")
	}
}

// TestApplyMergeRejectsEmptyBody proves a page with an empty body is rejected (a
// merge that paraphrased a page to nothing is a failed call, not a blank page).
func TestApplyMergeRejectsEmptyBody(t *testing.T) {
	manifest := manifestForMerge()
	resp := `{"pages":[{"subject":"subj-1","title":"Acme","body":"   "}]}`
	if _, err := ApplyMerge(manifest, resp); err == nil {
		t.Fatal("expected rejection of an empty-body page")
	}
}
