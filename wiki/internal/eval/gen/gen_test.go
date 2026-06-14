package gen

import (
	"context"
	"encoding/json"
	"os"
	"path/filepath"
	"testing"

	"wiki/internal/eval"
)

// The adversarial verification pass (plan P15: "LLM-author the goldens, then an
// adversarial verification pass over them"), made a deterministic offline check.
// For every gen-1 case we synthesize the IDEAL call-site output implied by the
// case's own gold, score it through the REAL P14 scorer, and assert the dataset is
// internally consistent: an ideal answer must earn the perfect headline and fire
// NO dangerous axis. If a case's gold were self-contradictory (e.g. a match case
// whose gold "same" id is not among its candidates, or a merge gold whose
// must-survive cite is not derivable), the ideal output would fail its own scorer
// and this test goes red — catching an authoring bug at the boundary that
// introduced it, exactly the redesign's verify pattern.

// idealOutput builds the perfect call-site output a case's gold implies, in the
// site's raw-output shape (what the scorer unmarshals). It is the adversary's
// "best possible model": if even this fails the scorer, the gold is malformed.
func idealOutput(t *testing.T, site string, gold json.RawMessage) []byte {
	t.Helper()
	switch site {
	case "extract", "compile":
		// Output shape == gold shape (subjects[]). The ideal output IS the gold.
		return gold

	case "match", "canonical_name":
		// Output field names match gold field names; gold doubles as ideal output.
		return gold

	case "dup_judge":
		var g struct {
			Verdict string `json:"verdict"`
		}
		mustUnmarshal(t, gold, &g)
		return mustMarshal(t, map[string]any{"verdict": g.Verdict})

	case "candidates", "search", "sweep":
		var g retrievalGoldRec
		mustUnmarshal(t, gold, &g)
		// Ideal: return exactly the relevant set as the ranked results.
		return mustMarshal(t, map[string]any{"results": g.Relevant})

	case "merge":
		var g mergeGoldRec
		mustUnmarshal(t, gold, &g)
		// Ideal: one page per write-set member, body = old body (preserving every old
		// cite) plus any must-survive cites not already present, each claim cited.
		type page struct {
			Subject    string   `json:"subject"`
			Title      string   `json:"title"`
			Body       string   `json:"body"`
			Superseded []string `json:"superseded"`
		}
		type claim struct {
			Text  string   `json:"text"`
			Cites []string `json:"cites"`
		}
		out := struct {
			Pages  []page  `json:"pages"`
			Claims []claim `json:"claims"`
		}{}
		// Build the merged body for the single page: old body + every must-survive cite.
		body := ""
		for _, sub := range g.WriteSet {
			body = g.OldBodies[sub]
		}
		for _, c := range g.MustSurvive {
			body += " [" + c + "]"
		}
		for _, sub := range g.WriteSet {
			out.Pages = append(out.Pages, page{Subject: sub, Title: g.LeadName, Body: body, Superseded: []string{}})
		}
		for _, c := range g.MustSurvive {
			out.Claims = append(out.Claims, claim{Text: "fact", Cites: []string{c}})
		}
		return mustMarshal(t, out)

	case "ask":
		var g askGoldRec
		mustUnmarshal(t, gold, &g)
		if g.ShouldAbstain {
			return mustMarshal(t, map[string]any{"answer": "", "abstained": true})
		}
		return mustMarshal(t, map[string]any{
			"answer":    g.Answer,
			"citations": g.Supporting,
			"abstained": false,
			"retrieved": g.Supporting,
		})
	}
	t.Fatalf("idealOutput: no builder for site %q", site)
	return nil
}

// TestGoldensSelfConsistent is the core adversarial pass: every gen-1 case's ideal
// output scores headline 1 with zero dangerous-axis activation. A red here means a
// generator authored a self-contradictory gold.
func TestGoldensSelfConsistent(t *testing.T) {
	// A keyword-rule judge so the judged scorer halves (merge rubric, ask answer
	// correctness, set-alignment claim recall) resolve deterministically and POSITIVE
	// for the ideal output — the adversary's strongest model.
	judge := perfectJudge{}
	for _, g := range Generators() {
		for _, c := range g.cases() {
			out := idealOutput(t, c.Site, c.Gold)
			scorer, err := eval.ScorerFor(c.Site, judge, 10)
			if err != nil {
				t.Fatalf("%s/%s: ScorerFor: %v", c.Site, c.CaseID, err)
			}
			sc := scorer.Score(context.Background(), out, c.Gold)
			if sc.Headline < 0.999 {
				t.Errorf("%s/%s (%s): ideal output headline = %v, want 1 (self-inconsistent gold); errs=%v",
					c.Site, c.CaseID, c.FailureTag, sc.Headline, sc.Errs)
			}
			for axis, rate := range sc.Dangerous {
				// Recall-style axes on the dangerous channel (e.g. dup_pairs_recall) are
				// "high is good": the ideal output should MAXIMIZE them. Every other
				// dangerous axis is an error rate the ideal output must leave at 0.
				if isRecallAxis(axis) {
					if rate != 1 {
						t.Errorf("%s/%s (%s): ideal output recall axis %q = %v (want 1); errs=%v",
							c.Site, c.CaseID, c.FailureTag, axis, rate, sc.Errs)
					}
					continue
				}
				if rate != 0 {
					t.Errorf("%s/%s (%s): ideal output fired dangerous axis %q = %v (want 0); errs=%v",
						c.Site, c.CaseID, c.FailureTag, axis, rate, sc.Errs)
				}
			}
		}
	}
}

// TestDangerousCasesAreReal proves each blunt dangerous trap actually fires its
// axis when given the dangerous WRONG answer — so the dataset genuinely exercises
// the asymmetry the harness exists to measure (not just the happy path).
func TestDangerousCasesAreReal(t *testing.T) {
	// match: the identical-name-different-thing case must register a false_merge when
	// the model wrongly says "same" with a candidate id.
	mc := findCase(t, "match", "match-0003")
	var mi matchIn
	mustUnmarshal(t, mc.Input, &mi)
	wrong := mustMarshal(t, map[string]any{"same": mi.Candidates[0].SubjectID, "dup_pairs": []any{}})
	sc, _ := eval.ScorerFor("match", perfectJudge{}, 10)
	got := sc.Score(context.Background(), wrong, mc.Gold)
	if got.Dangerous["false_merge"] != 1 {
		t.Errorf("match-0003 wrong answer should fire false_merge, got %v", got.Dangerous["false_merge"])
	}

	// ask: the gap case must register a fabrication when the model answers instead of
	// abstaining.
	ac := findCase(t, "ask", "ask-0002")
	fab := mustMarshal(t, map[string]any{"answer": "It is $2 million.", "abstained": false})
	asc, _ := eval.ScorerFor("ask", perfectJudge{}, 10)
	ag := asc.Score(context.Background(), fab, ac.Gold)
	if ag.Dangerous["fabrication"] != 1 {
		t.Errorf("ask-0002 fabricated answer should fire fabrication, got %v", ag.Dangerous["fabrication"])
	}

	// merge: an undeclared dropped citation must fire undeclared_cite_loss.
	mgc := findCase(t, "merge", "merge-0002")
	var mg mergeGoldRec
	mustUnmarshal(t, mgc.Gold, &mg)
	drop := mustMarshal(t, map[string]any{
		"pages":  []any{map[string]any{"subject": mg.WriteSet[0], "title": "T", "body": "rewritten with no old cite", "superseded": []any{}}},
		"claims": []any{},
	})
	mgsc, _ := eval.ScorerFor("merge", perfectJudge{}, 10)
	mgg := mgsc.Score(context.Background(), drop, mgc.Gold)
	if mgg.Dangerous["undeclared_cite_loss"] != 1 {
		t.Errorf("merge-0002 dropped-cite output should fire undeclared_cite_loss, got %v", mgg.Dangerous["undeclared_cite_loss"])
	}
}

// TestEverySiteHasAGenerator asserts a gen-1 generator exists for every one of the
// ten inference sites (the P15 Verify: "a gen-1 bundle exists for every site").
func TestEverySiteHasAGenerator(t *testing.T) {
	want := []string{"ask", "candidates", "canonical_name", "compile", "dup_judge",
		"extract", "match", "merge", "search", "sweep"}
	got := SiteNames()
	if len(got) != len(want) {
		t.Fatalf("generators cover %v sites, want %v", got, want)
	}
	for i := range want {
		if got[i] != want[i] {
			t.Errorf("site[%d] = %q, want %q", i, got[i], want[i])
		}
	}
	// Every generator must emit at least one case spanning blunt -> subtle (>1 case).
	for _, g := range Generators() {
		cs := g.cases()
		if len(cs) < 2 {
			t.Errorf("site %q has %d cases; gen-1 spans blunt->subtle, want >=2", g.site, len(cs))
		}
		for _, c := range cs {
			if c.Site != g.site {
				t.Errorf("site %q emitted a case tagged site=%q", g.site, c.Site)
			}
			if c.Generation != 1 {
				t.Errorf("%s/%s generation = %d, want 1", g.site, c.CaseID, c.Generation)
			}
			if c.FailureTag == "" {
				t.Errorf("%s/%s has no failure_tag", g.site, c.CaseID)
			}
		}
	}
}

// TestSharedCorporaFeedMultipleConsumers asserts the structural saving: the
// identity corpus is referenced by all four identity sites, and the synthetic wiki
// by both wiki sites (eval design "Shared corpora": one truth, four/two consumers).
func TestSharedCorporaFeedMultipleConsumers(t *testing.T) {
	// Identity corpus: every match/dup/candidates case must reference a corpus id.
	corpusIDs := map[string]bool{}
	for _, s := range identityCorpus {
		corpusIDs[s.ID] = true
	}
	identitySites := map[string]bool{"match": true, "dup_judge": true, "candidates": true, "sweep": true}
	identityRefs := 0
	for _, g := range Generators() {
		if !identitySites[g.site] {
			continue
		}
		for _, c := range g.cases() {
			if referencesAny(string(c.Input), corpusIDs) || referencesAny(string(c.Gold), corpusIDs) {
				identityRefs++
			}
		}
	}
	if identityRefs == 0 {
		t.Error("no identity-site case references the shared identity corpus")
	}

	// Synthetic wiki: ask + search must reference a wiki page id.
	wikiIDs := map[string]bool{}
	for _, p := range syntheticWiki {
		wikiIDs[p.ID] = true
	}
	wikiSites := map[string]bool{"ask": true, "search": true}
	wikiRefs := 0
	for _, g := range Generators() {
		if !wikiSites[g.site] {
			continue
		}
		for _, c := range g.cases() {
			if referencesAny(string(c.Input), wikiIDs) || referencesAny(string(c.Gold), wikiIDs) {
				wikiRefs++
			}
		}
	}
	if wikiRefs == 0 {
		t.Error("no wiki-site case references the shared synthetic wiki")
	}
}

// TestWriteAllAndLoadByName proves the harness loads a written bundle by name (the
// P15 Verify: "the harness loads a bundle by name"). It writes the full tree to a
// temp dir and loads every site's gen-1 bundle through the real eval loader.
func TestWriteAllAndLoadByName(t *testing.T) {
	root := t.TempDir()
	if err := WriteAll(root); err != nil {
		t.Fatalf("WriteAll: %v", err)
	}
	for _, g := range Generators() {
		siteDir := filepath.Join(root, g.site)
		bundlePath := filepath.Join(siteDir, "bundles", "gen-1.json")
		b, ds, dsBytes, promptBytes, err := eval.LoadBundle(siteDir, bundlePath)
		if err != nil {
			t.Errorf("%s: LoadBundle: %v", g.site, err)
			continue
		}
		if b.Dataset != "datasets/gen-1.json" {
			t.Errorf("%s: bundle dataset = %q", g.site, b.Dataset)
		}
		if len(ds.Cases) < 2 {
			t.Errorf("%s: loaded %d cases, want >=2", g.site, len(ds.Cases))
		}
		if len(dsBytes) == 0 {
			t.Errorf("%s: empty dataset bytes", g.site)
		}
		if len(promptBytes) == 0 {
			t.Errorf("%s: empty prompt bytes (bundle should name a prompt artifact)", g.site)
		}
	}
}

// --- helpers ---

// perfectJudge is the adversary's strongest model: it agrees with every positive
// rubric criterion, denies hallucination, and judges every claim/answer similar to
// its gold — so the ideal output earns the full rubric headline offline.
type perfectJudge struct{}

func (perfectJudge) YesNo(_ context.Context, question, _, _ string) (bool, int, int) {
	// The hallucination criterion is phrased as "does the page assert an UNsupported
	// fact" — the ideal output does not, so vote NO there; every other (positive)
	// criterion votes YES.
	if containsCI(question, "hallucination") || containsCI(question, "does NOT support") {
		return false, 0, 3
	}
	return true, 3, 3
}
func (perfectJudge) Similar(context.Context, string, string) bool { return true }

func containsCI(s, sub string) bool {
	return len(s) >= len(sub) && indexCI(s, sub) >= 0
}
func indexCI(s, sub string) int {
	for i := 0; i+len(sub) <= len(s); i++ {
		if equalFoldASCII(s[i:i+len(sub)], sub) {
			return i
		}
	}
	return -1
}
func equalFoldASCII(a, b string) bool {
	if len(a) != len(b) {
		return false
	}
	for i := range a {
		ca, cb := a[i], b[i]
		if 'A' <= ca && ca <= 'Z' {
			ca += 'a' - 'A'
		}
		if 'A' <= cb && cb <= 'Z' {
			cb += 'a' - 'A'
		}
		if ca != cb {
			return false
		}
	}
	return true
}

// isRecallAxis reports whether a dangerous-channel axis is a recall metric (high
// is good), as opposed to an error rate (zero is good).
func isRecallAxis(axis string) bool {
	const suffix = "_recall"
	return len(axis) >= len(suffix) && axis[len(axis)-len(suffix):] == suffix
}

func referencesAny(s string, ids map[string]bool) bool {
	for id := range ids {
		if containsCI(s, id) {
			return true
		}
	}
	return false
}

func findCase(t *testing.T, site, caseID string) eval.Case {
	t.Helper()
	for _, g := range Generators() {
		if g.site != site {
			continue
		}
		for _, c := range g.cases() {
			if c.CaseID == caseID {
				return c
			}
		}
	}
	t.Fatalf("case %s/%s not found", site, caseID)
	return eval.Case{}
}

func mustUnmarshal(t *testing.T, b []byte, v any) {
	t.Helper()
	if err := json.Unmarshal(b, v); err != nil {
		t.Fatalf("unmarshal: %v (%s)", err, string(b))
	}
}
func mustMarshal(t *testing.T, v any) []byte {
	t.Helper()
	b, err := json.Marshal(v)
	if err != nil {
		t.Fatalf("marshal: %v", err)
	}
	return b
}

// ensure os is referenced (WriteAll path test uses temp dirs only; keep import
// honest if a future check reads a committed file).
var _ = os.ReadFile
