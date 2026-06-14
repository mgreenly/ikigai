//go:build integration

// P11k fixture capture for the lint call-site prompt gates (judge, fold, stale).
// See docs/wiki-redesign-plan.md, "P11k — Keyed Part-I validation gate": capture
// one REAL-model response per gate from the live pinned triple and commit it as
// testdata/<site>_response.json's recorded sibling, so the offline (a-ii) gate
// runs against real output. Runs only with WIKI_CAPTURE_FIXTURES=1 and keys
// present; reuses the production stage path verbatim and tees the raw model JSON.
package lint

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

type teeCaller struct {
	inner caller
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

func TestCaptureJudgeAndFoldFixtures(t *testing.T) {
	skipUnlessCapturing(t)
	cfg, err := config.Load(os.Getenv)
	if err != nil {
		t.Fatalf("config.Load: %v", err)
	}
	w := llm.New(liveFactory(), nil)

	// Blunt obviously-same pair → near-certain merge; we tee judge then fold.
	sameA := page.DupSubject{SubjectID: "01A", Type: "entity", CanonicalName: "Apple Inc.",
		Aliases: []string{"apple", "apple inc"},
		Body:    "Apple Inc. is a technology company headquartered in Cupertino, California. [01HXBLUNTINBOX0000000000001]"}
	sameB := page.DupSubject{SubjectID: "01B", Type: "entity", CanonicalName: "Apple Incorporated",
		Aliases: []string{"apple incorporated"},
		Body:    "Apple Incorporated, the Cupertino technology company, makes the iPhone. [01HXBLUNTINBOX0000000000002]"}

	judgeTee := &teeCaller{inner: NewWrapperCaller(w), path: "judge_response.json", t: t}
	jj := NewDupsJob(judgeTee, nil, cfg.LLM.LintDupJudge, cfg.LLM.LintFold)

	ctx, cancel := context.WithTimeout(context.Background(), 120*time.Second)
	defer cancel()
	vr, err := jj.Judge(ctx, sameA, sameB)
	if err != nil {
		t.Fatalf("capture judge: %v", err)
	}
	if vr.Verdict != VerdictMerge {
		t.Fatalf("blunt obviously-same pair did not merge (got %s) — re-run; the (a-ii) gate needs a merge fixture", vr.Verdict)
	}

	foldTee := &teeCaller{inner: NewWrapperCaller(w), path: "fold_response.json", t: t}
	fj := NewDupsJob(foldTee, nil, cfg.LLM.LintDupJudge, cfg.LLM.LintFold)
	if _, err := fj.Fold(ctx, vr.CanonicalName, sameA, sameB); err != nil {
		t.Fatalf("capture fold: %v", err)
	}
}

func TestCaptureStaleFixture(t *testing.T) {
	skipUnlessCapturing(t)
	cfg, err := config.Load(os.Getenv)
	if err != nil {
		t.Fatalf("config.Load: %v", err)
	}
	w := llm.New(liveFactory(), nil)
	tee := &teeCaller{inner: NewWrapperCaller(w), path: "stale_response.json", t: t}
	j := NewStaleJob(tee, nil, nil, cfg.LLM.LintStale)

	subj := page.StaleSubject{
		SubjectID: "01HSUBJINITECH000000000001",
		Title:     "Initech",
		Body:      "Initech is an independent software company based in Austin. [01HXBLUNTINBOX0000000000010]",
		Notes: []page.StaleNote{{
			ID:    "01HNOTE0000000000000000001",
			Note:  "Initech is described here as independent, but Globex Corporation acquired it in 2021.",
			Cites: "01HXBLUNTINBOX0000000000011",
		}},
	}
	ctx, cancel := context.WithTimeout(context.Background(), 120*time.Second)
	defer cancel()
	if _, err := j.Repair(ctx, subj); err != nil {
		t.Fatalf("capture stale: %v", err)
	}
}
