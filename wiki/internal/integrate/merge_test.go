package integrate

import (
	"context"
	"os"
	"path/filepath"
	"strings"
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

// manifestForMergeFixture builds the write-set the committed merge fixture writes
// into, so the offline prompt-default gate and the eval-hook checks can fold the
// fixture through the real ApplyMerge parser/validator.
func manifestForMergeFixture() *Manifest {
	return &Manifest{
		Subjects: []Subject{{
			Type: TypeEntity, Kind: "org", Name: "Globex Corp", Aliases: []string{"globex"},
			SubjectID: "01HSUBJGLOBEX0000000000001", TargetPage: "01HSUBJGLOBEX0000000000001",
			Claims: []Claim{{Text: "Globex Corp acquired Initech in 2021.", Cites: []string{"01HXINBOX0000000000000001"}}},
		}},
	}
}

// --- Standing prompt-default gate (offline, no key — see Prompt-default
// validation). Asserts the merge config-default prompt is non-placeholder and
// carries its six §4.4 sections (including the §6.1 superseded obligation), and
// that merge's parser schema-validates a committed, hand-authored, schema-faithful
// response fixture into a page-write + superseded list. Deterministic,
// key-independent — holds even when the live checkpoint skips. ---

func TestMergePromptDefaultGate(t *testing.T) {
	p := config.DefaultMergePrompt

	if len(strings.TrimSpace(p)) < 600 {
		t.Fatalf("merge default prompt too short to be real (%d chars)", len(p))
	}
	if strings.Contains(p, "PLACEHOLDER") {
		t.Fatal("merge default prompt is still a placeholder")
	}

	sections := []string{
		"## 1. Task framing",
		"## 2. The write set",
		"## 3. Lead discipline",
		"## 4. Citation preservation",
		"## 5. Stale notes",
		"## 6. Output schema",
	}
	for _, s := range sections {
		if !strings.Contains(p, s) {
			t.Errorf("merge default prompt missing section %q", s)
		}
	}
	// The §6.1 superseded obligation and the match-owed lead discipline are the
	// load-bearing cross-prompt invariants merge carries.
	lp := strings.ToLower(p)
	if !strings.Contains(lp, "superseded") {
		t.Error("merge default prompt missing the §6.1 superseded obligation")
	}
	if !strings.Contains(lp, "identity") || !strings.Contains(lp, "lead") {
		t.Error("merge default prompt missing the lead-discipline (identity-in-the-lead) obligation")
	}

	// (a-ii) the parser + schema validate a committed, schema-faithful fixture into
	// a page-write + superseded list.
	raw, err := os.ReadFile(filepath.Join("testdata", "merge_response.json"))
	if err != nil {
		t.Fatalf("read fixture: %v", err)
	}
	manifest := manifestForMergeFixture()
	if _, err := ApplyMerge(manifest, string(raw)); err != nil {
		t.Fatalf("committed fixture must parse + validate: %v", err)
	}
	s := manifest.Subjects[0]
	if strings.TrimSpace(s.PageBody) == "" {
		t.Fatal("fixture should produce a non-empty page body")
	}
	if len(s.Superseded) == 0 {
		t.Fatal("fixture should exercise the §6.1 superseded declaration")
	}
	if len(manifest.StaleNotes) == 0 {
		t.Fatal("fixture should exercise the §6 stale-notes side channel")
	}
}

// --- Eval hook (obligation 5): merge's two deterministic mechanical invariants —
// write-set conformance and claim-cite presence — exposed as the same pass/fail the
// harness scores merge on (the citation-preservation gate lands in P7b). Plus the
// closed-loop lead obligation merge owes match: the merged lead stays
// match-recoverable (it names the subject's identity). ---

func TestMergeEvalInvariants(t *testing.T) {
	raw, err := os.ReadFile(filepath.Join("testdata", "merge_response.json"))
	if err != nil {
		t.Fatalf("read fixture: %v", err)
	}

	// Write-set conformance: a fixture page outside the manifest's write set is a
	// hard fail (already covered by TestApplyMergeRejectsPageOutsideWriteSet; here we
	// assert the in-write-set fixture folds cleanly — the harness's pass case).
	manifest := manifestForMergeFixture()
	if _, err := ApplyMerge(manifest, string(raw)); err != nil {
		t.Fatalf("write-set-conformant fixture must fold: %v", err)
	}
	s := manifest.Subjects[0]

	// Claim-cite presence: every claim's citation id appears in the folded body, so
	// the page carries its provenance inline (the harness's deterministic check).
	for _, c := range s.Claims {
		for _, cite := range c.Cites {
			if !strings.Contains(s.PageBody, cite) {
				t.Errorf("merged body dropped claim citation %q (provenance must survive)", cite)
			}
		}
	}

	// Lead discipline: the first paragraph names the subject's identity so a later
	// match call can recover this page from its lead. Assert the canonical name
	// appears in the leading body text.
	lead := s.PageBody
	if i := strings.Index(lead, "\n\n"); i >= 0 {
		lead = lead[:i]
	}
	if !strings.Contains(strings.ToLower(lead), strings.ToLower(s.Name)) {
		t.Errorf("merged lead %q does not name the subject %q (lead must stay match-recoverable)", lead, s.Name)
	}
}
