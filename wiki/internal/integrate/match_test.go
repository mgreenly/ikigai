package integrate

import (
	"context"
	"encoding/json"
	"os"
	"path/filepath"
	"reflect"
	"strings"
	"testing"

	"agentkit/provider"

	"wiki/internal/config"
	"wiki/internal/page"
)

// fakeExcerptReader is the unit gate's stub registry: it returns a canned excerpt
// per subject id so match is exercised without a DB.
type fakeExcerptReader struct {
	excerpts  map[string]page.Excerpt
	lastChars int
	err       error
}

func (f *fakeExcerptReader) ReadExcerpt(_ context.Context, subjectID string, chars int) (page.Excerpt, error) {
	f.lastChars = chars
	if f.err != nil {
		return page.Excerpt{}, f.err
	}
	if ex, ok := f.excerpts[subjectID]; ok {
		return ex, nil
	}
	return page.Excerpt{SubjectID: subjectID, CanonicalName: subjectID}, nil
}

func matchTestSubject() Subject {
	return Subject{
		Type:    TypeEntity,
		Name:    "Globex Corp",
		Aliases: []string{"globex"},
		Claims:  []Claim{{Text: "Globex Corp acquired Initech in 2021.", Cites: []string{"01HXINBOX"}}},
	}
}

func matchTestCandidates() []page.Candidate {
	return []page.Candidate{
		{SubjectID: "01A", Type: TypeEntity, CanonicalName: "Globex Corporation"},
		{SubjectID: "01B", Type: TypeEntity, CanonicalName: "Globex Inc"},
	}
}

// TestMatchSame drives the full Match path with a mocked LLM returning a same
// verdict and asserts the binary verdict plus the injected triple and excerpt
// length flow through (obligation 1/2).
func TestMatchSame(t *testing.T) {
	mock := &mockCaller{resp: `{"verdict":{"same":"01A"},"dup_pairs":[]}`}
	reg := &fakeExcerptReader{excerpts: map[string]page.Excerpt{
		"01A": {SubjectID: "01A", CanonicalName: "Globex Corporation", Aliases: []string{"globex", "globex corporation"}, Body: "Globex Corporation is a conglomerate."},
		"01B": {SubjectID: "01B", CanonicalName: "Globex Inc", Body: "Globex Inc is a design studio."},
	}}
	site := config.CallSite{Name: "match", Prompt: config.DefaultMatchPrompt, Model: "claude-haiku-4-5"}
	m := NewMatcher(mock, reg, site, 600)

	v, err := m.Match(context.Background(), matchTestSubject(), matchTestCandidates())
	if err != nil {
		t.Fatalf("Match: %v", err)
	}
	if v.Same != "01A" {
		t.Errorf("verdict.Same = %q, want 01A", v.Same)
	}
	if len(v.DupPairs) != 0 {
		t.Errorf("DupPairs = %v, want empty", v.DupPairs)
	}
	if mock.gotSite.Model != "claude-haiku-4-5" {
		t.Errorf("triple not injected: %+v", mock.gotSite)
	}
	if reg.lastChars != 600 {
		t.Errorf("excerpt chars = %d, want 600", reg.lastChars)
	}
	// The evidence message carries the incoming subject and both candidates.
	got := mock.gotMsgs[0].Blocks[0].(provider.TextBlock).Text
	for _, want := range []string{"Globex Corp", "id: 01A", "id: 01B", "page excerpt:"} {
		if !strings.Contains(got, want) {
			t.Errorf("evidence message missing %q\n%s", want, got)
		}
	}
}

func TestMatchNoMatch(t *testing.T) {
	mock := &mockCaller{resp: `{"verdict":{"no_match":true},"dup_pairs":[]}`}
	reg := &fakeExcerptReader{}
	site := config.CallSite{Name: "match", Prompt: config.DefaultMatchPrompt, Model: "claude-haiku-4-5"}
	m := NewMatcher(mock, reg, site, 600)

	v, err := m.Match(context.Background(), matchTestSubject(), matchTestCandidates())
	if err != nil {
		t.Fatalf("Match: %v", err)
	}
	if v.Same != "" {
		t.Errorf("no_match verdict must have empty Same, got %q", v.Same)
	}
}

// TestMatchDupPairsSideChannel asserts the side channel is preserved as a distinct
// output (obligation 3), canonicalized (smaller id first) and de-duped.
func TestMatchDupPairsSideChannel(t *testing.T) {
	mock := &mockCaller{resp: `{"verdict":{"no_match":true},"dup_pairs":[{"a":"01B","b":"01A"},{"a":"01A","b":"01B"}]}`}
	reg := &fakeExcerptReader{}
	site := config.CallSite{Name: "match", Prompt: config.DefaultMatchPrompt, Model: "claude-haiku-4-5"}
	m := NewMatcher(mock, reg, site, 600)

	v, err := m.Match(context.Background(), matchTestSubject(), matchTestCandidates())
	if err != nil {
		t.Fatalf("Match: %v", err)
	}
	want := []DupPair{{SubjectA: "01A", SubjectB: "01B"}}
	if !reflect.DeepEqual(v.DupPairs, want) {
		t.Errorf("DupPairs = %v, want %v (canonical + deduped)", v.DupPairs, want)
	}
}

func TestNewMatcherClampsExcerptChars(t *testing.T) {
	m := NewMatcher(&mockCaller{}, &fakeExcerptReader{}, config.CallSite{}, 0)
	if m.excerptChars != DefaultMatchExcerptChars {
		t.Errorf("non-positive excerpt chars not clamped: got %d", m.excerptChars)
	}
}

// --- ParseMatch unit tests (offline, no client) ---

func validIDs(ids ...string) map[string]struct{} {
	out := make(map[string]struct{}, len(ids))
	for _, id := range ids {
		out[id] = struct{}{}
	}
	return out
}

func TestParseMatchRejectsBothArms(t *testing.T) {
	_, err := ParseMatch(`{"verdict":{"same":"01A","no_match":true},"dup_pairs":[]}`, validIDs("01A"))
	if err == nil {
		t.Fatal("expected error for both arms set")
	}
}

func TestParseMatchRejectsNeitherArm(t *testing.T) {
	_, err := ParseMatch(`{"verdict":{},"dup_pairs":[]}`, validIDs("01A"))
	if err == nil {
		t.Fatal("expected error for neither arm set")
	}
}

func TestParseMatchRejectsUnknownSameID(t *testing.T) {
	_, err := ParseMatch(`{"verdict":{"same":"99Z"},"dup_pairs":[]}`, validIDs("01A", "01B"))
	if err == nil {
		t.Fatal("expected error when same id is not an offered candidate")
	}
}

func TestParseMatchStripsCodeFence(t *testing.T) {
	v, err := ParseMatch("```json\n{\"verdict\":{\"same\":\"01A\"},\"dup_pairs\":[]}\n```", validIDs("01A"))
	if err != nil {
		t.Fatalf("fenced JSON should parse: %v", err)
	}
	if v.Same != "01A" {
		t.Fatalf("Same = %q, want 01A", v.Same)
	}
}

func TestMatchSchemaIsValidJSON(t *testing.T) {
	var v any
	if err := json.Unmarshal(MatchSchema, &v); err != nil {
		t.Fatalf("MatchSchema is not valid JSON: %v", err)
	}
}

// --- Standing prompt-default gate (offline, no key — see Prompt-default
// validation). Asserts the match config-default prompt is non-placeholder and
// carries its five §4.3 sections, and that the parser schema-validates a committed,
// hand-authored, schema-faithful response fixture into the binary verdict plus the
// dup_pairs side channel. ---

func TestMatchPromptDefaultGate(t *testing.T) {
	p := config.DefaultMatchPrompt

	if len(strings.TrimSpace(p)) < 400 {
		t.Fatalf("match default prompt too short to be real (%d chars)", len(p))
	}
	if strings.Contains(p, "PLACEHOLDER") {
		t.Fatal("match default prompt is still a placeholder")
	}

	sections := []string{
		"## 1. Task framing",
		"## 2. The evidence",
		"## 3. Decision rule",
		"## 4. Candidate-pair side channel",
		"## 5. Output schema",
	}
	for _, s := range sections {
		if !strings.Contains(p, s) {
			t.Errorf("match default prompt missing section %q", s)
		}
	}
	// The doubt-is-no_match polarity is the load-bearing identity-not-similarity rule.
	if !strings.Contains(strings.ToLower(p), "no_match") || !strings.Contains(strings.ToLower(p), "identity") {
		t.Error("match default prompt missing the identity / doubt-is-no_match polarity")
	}

	// (a-ii) the parser + schema validate the committed RECORDED real-model fixture
	// (P11k) — so parser/schema drift is caught against real output. The recorded
	// blunt-pair drive yields a same-verdict resolving to an offered candidate; the
	// dup_pairs side channel is exercised independently by
	// TestMatchDupPairsSideChannel.
	raw, err := os.ReadFile(filepath.Join("testdata", "match_recorded.json"))
	if err != nil {
		t.Fatalf("read recorded fixture: %v", err)
	}
	v, err := ParseMatch(string(raw), validIDs("01HXCANDIDATEAPPLEINC000001", "01HXCANDIDATEAPPLEREC000002"))
	if err != nil {
		t.Fatalf("recorded fixture must parse: %v", err)
	}
	if v.Same != "01HXCANDIDATEAPPLEINC000001" {
		t.Fatalf("recorded verdict = %q, want same(01HXCANDIDATEAPPLEINC000001)", v.Same)
	}
}
