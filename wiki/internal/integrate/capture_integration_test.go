//go:build integration

// P11k fixture capture (see docs/wiki-redesign-plan.md, "P11k — Keyed Part-I
// validation gate"). These are NOT assertions; they are the recorder that
// discharges P11k's second deliverable: capture one REAL-model response per
// call-site prompt gate from the live pinned (prompt, model, effort) triple and
// commit it as testdata/<site>_recorded.json, so from here on the offline (a-ii)
// parser/schema gate runs against real output (catching parser/schema drift the
// hand-authored stub never could).
//
// They run only when WIKI_CAPTURE_FIXTURES=1 (and keys are present); a normal
// `go test -tags=integration` run skips them so the checkpoints stay the gate.
// Each drives the SAME production stage function the integration checkpoint does,
// on an input whose ids the offline gate asserts against, and tees the raw model
// string to disk.
package integrate

import (
	"context"
	"encoding/json"
	"os"
	"path/filepath"
	"testing"
	"time"

	"agentkit/provider"

	"wiki/internal/config"
	"wiki/internal/llm"
	"wiki/internal/page"
)

// teeCaller wraps a structuredCaller and writes every raw response it sees to
// path, so capture reuses the production stage path verbatim.
type teeCaller struct {
	inner structuredCaller
	path  string
	t     *testing.T
}

func (c *teeCaller) Structured(ctx context.Context, site config.CallSite, schema json.RawMessage, msgs []provider.Message) (string, error) {
	raw, err := c.inner.Structured(ctx, site, schema, msgs)
	if err != nil {
		return raw, err
	}
	writeRecorded(c.t, c.path, raw)
	return raw, nil
}

func writeRecorded(t *testing.T, name, raw string) {
	t.Helper()
	// Pretty-print so the committed fixture is reviewable, but keep it the model's
	// own JSON (no field edits).
	var v any
	if err := json.Unmarshal([]byte(stripCodeFence(raw)), &v); err != nil {
		t.Fatalf("recorded response is not valid JSON: %v\n%s", err, raw)
	}
	b, err := json.MarshalIndent(v, "", "  ")
	if err != nil {
		t.Fatalf("marshal recorded: %v", err)
	}
	b = append(b, '\n')
	if err := os.WriteFile(filepath.Join("testdata", name), b, 0o644); err != nil {
		t.Fatalf("write recorded fixture: %v", err)
	}
	t.Logf("recorded real-model fixture testdata/%s (%d bytes)", name, len(b))
}

func skipUnlessCapturing(t *testing.T) {
	if os.Getenv("WIKI_CAPTURE_FIXTURES") != "1" {
		t.Skip("set WIKI_CAPTURE_FIXTURES=1 to (re)record real-model fixtures")
	}
	if os.Getenv("ANTHROPIC_API_KEY") == "" && os.Getenv("OPENAI_API_KEY") == "" {
		t.Skip("no provider keys present")
	}
}

func capLiveFactory() llm.ClientFactory { return liveFactory() }

func TestCaptureExtractFixture(t *testing.T) {
	skipUnlessCapturing(t)
	cfg, err := config.Load(os.Getenv)
	if err != nil {
		t.Fatalf("config.Load: %v", err)
	}
	site := cfg.LLM.Extract
	if site.Prompt == "" {
		site.Prompt = config.DefaultExtractPrompt
	}
	w := llm.New(capLiveFactory(), nil)
	tee := &teeCaller{inner: NewWrapperCaller(w), path: "extract_recorded.json", t: t}
	ex := NewExtractor(tee, site)

	hdr := DocumentHeader{Source: "test", Title: "Blunt extract fixture",
		ReceivedAt: time.Date(2024, 1, 2, 0, 0, 0, 0, time.UTC)}
	ctx, cancel := context.WithTimeout(context.Background(), 90*time.Second)
	defer cancel()
	if _, err := ex.Extract(ctx, hdr, "Tim Cook is the chief executive officer of Apple Inc.", "01HXBLUNTFIXTUREINBOXROW001"); err != nil {
		t.Fatalf("capture extract: %v", err)
	}
}

func TestCaptureMatchFixture(t *testing.T) {
	skipUnlessCapturing(t)
	cfg, err := config.Load(os.Getenv)
	if err != nil {
		t.Fatalf("config.Load: %v", err)
	}
	site := cfg.LLM.Match
	if site.Prompt == "" {
		site.Prompt = config.DefaultMatchPrompt
	}
	reg := stubExcerptReader{ex: map[string]page.Excerpt{
		"01HXCANDIDATEAPPLEINC000001": {SubjectID: "01HXCANDIDATEAPPLEINC000001", CanonicalName: "Apple Inc.",
			Aliases: []string{"apple", "apple inc"},
			Body:    "Apple Inc. is a technology company headquartered in Cupertino, California. Tim Cook is its chief executive officer."},
		"01HXCANDIDATEAPPLEREC000002": {SubjectID: "01HXCANDIDATEAPPLEREC000002", CanonicalName: "Apple Records",
			Aliases: []string{"apple records"},
			Body:    "Apple Records is a record label founded by the Beatles in 1968 in London."},
	}}
	w := llm.New(capLiveFactory(), nil)
	tee := &teeCaller{inner: NewWrapperCaller(w), path: "match_recorded.json", t: t}
	m := NewMatcher(tee, reg, site, cfg.MatchExcerptChars)
	incoming := Subject{Type: TypeEntity, Name: "Apple Inc.", Aliases: []string{"Apple"},
		Claims: []Claim{{Text: "Apple Inc. is a technology company based in Cupertino.", Cites: []string{"01HXBLUNTINBOX0000000000001"}}}}
	cands := []page.Candidate{
		{SubjectID: "01HXCANDIDATEAPPLEINC000001", Type: TypeEntity},
		{SubjectID: "01HXCANDIDATEAPPLEREC000002", Type: TypeEntity},
	}
	ctx, cancel := context.WithTimeout(context.Background(), 90*time.Second)
	defer cancel()
	if _, err := m.Match(ctx, incoming, cands); err != nil {
		t.Fatalf("capture match: %v", err)
	}
}

// captureMergeReader is a blunt pageReader: the target page does not yet exist
// (a fresh subject), so merge writes a new page.
type captureMergeReader struct{}

func (captureMergeReader) ReadPage(context.Context, string) (string, string, bool, error) {
	return "", "", false, nil
}
func (captureMergeReader) ReadVersion(context.Context, string) (int, error) { return 0, nil }

func TestCaptureMergeFixture(t *testing.T) {
	skipUnlessCapturing(t)
	cfg, err := config.Load(os.Getenv)
	if err != nil {
		t.Fatalf("config.Load: %v", err)
	}
	site := cfg.LLM.Merge
	if site.Prompt == "" {
		site.Prompt = config.DefaultMergePrompt
	}
	w := llm.New(capLiveFactory(), nil)
	tee := &teeCaller{inner: NewWrapperCaller(w), path: "merge_recorded.json", t: t}
	mg := NewMerger(tee, captureMergeReader{}, site)
	manifest := &Manifest{Subjects: []Subject{{
		Type: TypeEntity, Kind: "org", Name: "Globex Corporation", Aliases: []string{"Globex"},
		SubjectID: "01HSUBJGLOBEX0000000000001", TargetPage: "01HSUBJGLOBEX0000000000001",
		Claims: []Claim{
			{Text: "Globex Corporation is headquartered in Springfield.", Cites: []string{"01HXINBOX0000000000000001"}},
			{Text: "Globex Corporation acquired Initech in 2021.", Cites: []string{"01HXINBOX0000000000000002"}},
		},
	}}}
	ctx, cancel := context.WithTimeout(context.Background(), 120*time.Second)
	defer cancel()
	if _, err := mg.Merge(ctx, manifest); err != nil {
		t.Fatalf("capture merge: %v", err)
	}
}

func TestCaptureCompileFixture(t *testing.T) {
	skipUnlessCapturing(t)
	cfg, err := config.Load(os.Getenv)
	if err != nil {
		t.Fatalf("config.Load: %v", err)
	}
	site := cfg.LLM.Compile
	if site.Prompt == "" {
		site.Prompt = config.DefaultCompilePrompt
	}
	w := llm.New(capLiveFactory(), nil)
	tee := &teeCaller{inner: NewWrapperCaller(w), path: "compile_recorded.json", t: t}
	c := NewCompiler(tee, site)
	events := []EventRow{
		{ID: "01EVTA", Source: "crm:deal.stage_changed", Payload: []byte(`{"deal":"Globex renewal","to":"negotiation"}`)},
		{ID: "01EVTB", Source: "crm:deal.stage_changed", Payload: []byte(`{"deal":"Globex renewal","to":"closed_won","date":"2024-05-23"}`)},
	}
	ctx, cancel := context.WithTimeout(context.Background(), 120*time.Second)
	defer cancel()
	if _, err := c.Compile(ctx, events); err != nil {
		t.Fatalf("capture compile: %v", err)
	}
}
