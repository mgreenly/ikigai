package main

import (
	"bytes"
	"context"
	"encoding/json"
	"os"
	"path/filepath"
	"strings"
	"testing"

	agentkit "github.com/ikigenba/agentkit"

	"wiki/internal/eval"
	"wiki/internal/extract"
	"wiki/internal/llm"
)

func TestParseConfigLayersFlagsOntoDefaults(t *testing.T) {
	// R-34NV-WDIP
	defaults, err := parseConfig(nil)
	if err != nil {
		t.Fatalf("parseConfig defaults returned error: %v", err)
	}
	if defaults.extract.Model != "claude-sonnet-4-6" || defaults.judge.Model != "claude-opus-4-8" {
		t.Fatalf("default models = %q/%q, want Sonnet 4.6 extract and Opus 4.8 judge", defaults.extract.Model, defaults.judge.Model)
	}
	if !strings.Contains(eval.CallSiteParamsJSON(defaults.extract), `"reasoning":"disabled"`) || !strings.Contains(eval.JudgeStampJSON(defaults.judge), `"reasoning":"high"`) || strings.Contains(eval.JudgeStampJSON(defaults.judge), `"temperature"`) {
		t.Fatalf("default stamps = %s / %s, want extract reasoning disabled and judge high with no temperature", eval.CallSiteParamsJSON(defaults.extract), eval.JudgeStampJSON(defaults.judge))
	}

	got, err := parseConfig([]string{
		"-dataset", "cases",
		"-model", "extract-model",
		"-reasoning", "low",
		"-temperature", "0.25",
		"-max-tokens", "1234",
		"-judge-model", "judge-model",
		"-judge-reasoning", "medium",
		"-record", "run.jsonl",
		"-json",
	})
	if err != nil {
		t.Fatalf("parseConfig returned error: %v", err)
	}
	if got.dataset != "cases" || got.record != "run.jsonl" || !got.json {
		t.Fatalf("paths/json = %#v, want flags layered", got)
	}
	if got.extract.Model != "extract-model" || got.extract.Temperature == nil || *got.extract.Temperature != 0.25 || got.extract.MaxTokens != 1234 {
		t.Fatalf("extract site = %#v, want model/temp/max tokens from flags", got.extract)
	}
	if !strings.Contains(eval.CallSiteParamsJSON(got.extract), `"reasoning":"low"`) {
		t.Fatalf("extract config = %s, want low reasoning", eval.CallSiteParamsJSON(got.extract))
	}
	if got.judge.Model != "judge-model" || !strings.Contains(eval.JudgeStampJSON(got.judge), `"reasoning":"medium"`) {
		t.Fatalf("judge stamp = %s, want judge model and medium reasoning", eval.JudgeStampJSON(got.judge))
	}
	if _, err := parseConfig([]string{"-reasoning", "sideways"}); err == nil || !strings.Contains(err.Error(), "-reasoning") {
		t.Fatalf("malformed reasoning err = %v, want loud -reasoning failure", err)
	}
}

func TestRunRequiresAPIKeyBeforeProviderOrDataset(t *testing.T) {
	// R-39JH-FGHH
	var stdout, stderr bytes.Buffer
	providers := 0
	evaluations := 0
	code := run(context.Background(), []string{"-dataset", filepath.Join(t.TempDir(), "missing")}, func(string) string { return "" }, &stdout, &stderr, runDeps{
		newProvider: func(string) agentkit.Provider {
			providers++
			return &scriptedProvider{}
		},
		evaluate: func(context.Context, string, *extract.Extractor, llm.CallSite, *eval.Judge, llm.CallSite) (eval.Scorecard, error) {
			evaluations++
			return eval.Scorecard{}, nil
		},
	})
	if code == 0 {
		t.Fatal("run exit code = 0, want failure without ANTHROPIC_API_KEY")
	}
	if providers != 0 || evaluations != 0 {
		t.Fatalf("providers/evaluations = %d/%d, want no provider construction or dataset evaluation before API key", providers, evaluations)
	}
	if !strings.Contains(stderr.String(), "ANTHROPIC_API_KEY") {
		t.Fatalf("stderr = %q, want API key error", stderr.String())
	}
}

func TestRunUsesInternalEvalWithMockBackedClient(t *testing.T) {
	// R-35VS-A59E
	root := t.TempDir()
	writeCase(t, filepath.Join(root, "one"))
	prov := &scriptedProvider{responses: []string{
		`{"subjects":[{"type":"entity","kind":"company","name":"Acme Robotics","claims":["Acme Robotics opened a Tulsa lab."]}]}`,
		`{"covered":[{"gold":"Acme Robotics opened a Tulsa lab.","predicted":"Acme Robotics opened a Tulsa lab."}],"missed":[],"extra":[]}`,
	}}
	recordPath := filepath.Join(t.TempDir(), "run.jsonl")
	var stdout, stderr bytes.Buffer
	code := run(context.Background(), []string{"-dataset", root, "-model", "mock-extract", "-judge-model", "mock-judge", "-record", recordPath, "-json"}, func(key string) string {
		if key == "ANTHROPIC_API_KEY" {
			return "test-key"
		}
		return ""
	}, &stdout, &stderr, runDeps{
		newProvider: func(string) agentkit.Provider { return prov },
	})
	if code != 0 {
		t.Fatalf("run exit code = %d, stderr = %q", code, stderr.String())
	}
	var scorecard eval.Scorecard
	if err := json.Unmarshal(stdout.Bytes(), &scorecard); err != nil {
		t.Fatalf("stdout did not contain JSON scorecard: %v\n%s", err, stdout.String())
	}
	if scorecard.Model != "mock-extract" || !strings.Contains(scorecard.Judge, `"model":"mock-judge"`) || scorecard.Totals.Overall.Cases != 1 {
		t.Fatalf("scorecard = %#v, want mock-stamped one-case eval", scorecard)
	}
	rawLog, err := os.ReadFile(recordPath)
	if err != nil {
		t.Fatalf("read record file: %v", err)
	}
	if got := strings.Count(string(rawLog), "\n"); got != 2 {
		t.Fatalf("record lines = %d, want extract and judge JSONL records:\n%s", got, string(rawLog))
	}
}

type scriptedProvider struct {
	responses []string
	requests  []agentkit.Request
}

func (p *scriptedProvider) RoundTrip(ctx context.Context, req *agentkit.Request) *agentkit.RoundTrip {
	p.requests = append(p.requests, *req)
	text := `{"subjects":[]}`
	if len(p.responses) > 0 {
		text = p.responses[0]
		p.responses = p.responses[1:]
	}
	return agentkit.NewRoundTrip(
		agentkit.Message{Role: agentkit.RoleAssistant, Blocks: []agentkit.Block{agentkit.TextBlock{Text: text}}},
		agentkit.FinishStop,
		agentkit.Usage{InputUncached: 1, Output: 1, Total: 2},
		nil,
		nil,
	)
}

func (p *scriptedProvider) Name() string {
	return "scripted"
}

func (p *scriptedProvider) Pricing(string) (agentkit.Pricing, bool) {
	return agentkit.Pricing{Tiers: []agentkit.RateTier{{MinInputTokens: 0}}}, true
}

func writeCase(t *testing.T, dir string) {
	t.Helper()
	if err := os.MkdirAll(dir, 0o700); err != nil {
		t.Fatalf("mkdir case: %v", err)
	}
	if err := os.WriteFile(filepath.Join(dir, "document.txt"), []byte("Acme Robotics opened a Tulsa lab."), 0o600); err != nil {
		t.Fatalf("write document: %v", err)
	}
	if err := os.WriteFile(filepath.Join(dir, "gold.json"), []byte(`{
		"difficulty": "easy",
		"header": {
			"source": "mcp:ingest_text",
			"title": "Tulsa lab note",
			"tags": ["robotics"],
			"received_at": "2026-06-20T19:45:00Z"
		},
		"gold": [
			{"type": "entity", "name": "Acme Robotics", "claims": ["Acme Robotics opened a Tulsa lab."]}
		]
	}`), 0o600); err != nil {
		t.Fatalf("write gold: %v", err)
	}
}
