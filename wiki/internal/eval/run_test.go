package eval

import (
	"bufio"
	"bytes"
	"context"
	"encoding/json"
	"os"
	"path/filepath"
	"strings"
	"testing"

	"wiki/internal/extract"
	"wiki/internal/llm"
)

func TestRunDatasetBuildsStampedScorecardWithMockLLMs(t *testing.T) {
	// R-35VS-A59E
	root := t.TempDir()
	writeCaseAt(t, filepath.Join(root, "one"), "Acme Robotics opened a Tulsa lab.", validGoldJSON("easy"))
	prov := &capturingProvider{responses: []string{
		`{"subjects":[{"type":"entity","kind":"company","name":"Acme Robotics","claims":["Acme Robotics opened a Tulsa lab."]}]}`,
		`{"covered":[{"gold":"Acme Robotics opened a Tulsa lab.","predicted":"Acme Robotics opened a Tulsa lab."}],"missed":[],"extra":[]}`,
	}}
	extractSite := extract.DefaultCallSite()
	extractSite.Model = "mock-extract"
	judgeSite := DefaultJudgeCallSite()
	judgeSite.Model = "mock-judge"
	client := llm.New(prov, nil)

	got, err := RunDataset(context.Background(), root, extract.New(client, extractSite), extractSite, NewJudge(client, judgeSite), judgeSite)
	if err != nil {
		t.Fatalf("RunDataset returned error: %v", err)
	}
	if got.Dataset != root || got.Model != "mock-extract" || got.Prompt != "default" || !strings.Contains(got.Config, `"max_tokens":16384`) || !strings.Contains(got.Judge, `"model":"mock-judge"`) {
		t.Fatalf("scorecard stamp = dataset %q model %q prompt %q config %s judge %s", got.Dataset, got.Model, got.Prompt, got.Config, got.Judge)
	}
	if len(got.Cases) != 1 || got.Cases[0].Case != "one" {
		t.Fatalf("cases = %#v, want one scored case", got.Cases)
	}
	if got.Totals.Overall.Cases != 1 || got.Totals.Overall.Subjects.Precision != 1 || got.Totals.Overall.Claims.Recall != 1 {
		t.Fatalf("totals = %#v, want perfect one-case aggregate", got.Totals)
	}
	if len(prov.requests) != 2 {
		t.Fatalf("requests len = %d, want one extract call and one judge call", len(prov.requests))
	}
}

func TestScorecardWritersEmitStampCasesAggregateAndRoundTripJSON(t *testing.T) {
	// R-373O-NX03
	// R-OFQH-UO4K
	scorecard := Scorecard{
		Dataset: "testdata/eval/extract",
		Model:   "mock-extract",
		Prompt:  "prompts/strict.md",
		Config:  `{"temperature":0,"reasoning":"disabled","max_tokens":16384}`,
		Judge:   `{"model":"mock-judge","params":{"reasoning":"low","max_tokens":2048}}`,
		Cases: []CaseResult{{
			Case:       "one",
			Difficulty: "hard",
			Subjects:   SubjectScore{Found: []string{"entity/acme"}, Missed: []string{"event/opening"}},
			Claims:     ClaimScore{Covered: 2, Missed: 1, Extra: 1},
			ClaimText:  []SubjectClaimResult{{Subject: "entity/acme", Covered: []ClaimMatch{{Gold: "g", Predicted: "p"}}, Extra: []string{"extra"}}},
		}},
	}
	scorecard.Totals = Aggregate(scorecard.Cases)

	var human bytes.Buffer
	scorecard.WriteHuman(&human)
	for _, want := range []string{
		"dataset: testdata/eval/extract",
		"model: mock-extract",
		"prompt: prompts/strict.md",
		"config: {\"temperature\":0,\"reasoning\":\"disabled\",\"max_tokens\":16384}",
		"judge: {\"model\":\"mock-judge\"",
		"- one (hard)",
		"aggregate:",
		"by difficulty:",
		"hard",
	} {
		if !strings.Contains(human.String(), want) {
			t.Fatalf("human scorecard %q does not contain %q", human.String(), want)
		}
	}

	var raw bytes.Buffer
	scorecard.WriteJSON(&raw)
	var decoded Scorecard
	if err := json.Unmarshal(raw.Bytes(), &decoded); err != nil {
		t.Fatalf("scorecard JSON did not decode: %v\n%s", err, raw.String())
	}
	if decoded.Dataset != scorecard.Dataset || decoded.Model != scorecard.Model || decoded.Prompt != "prompts/strict.md" || decoded.Cases[0].Case != "one" || decoded.Totals.ByDifficulty["hard"].Cases != 1 {
		t.Fatalf("decoded scorecard = %#v, want stamp, case, and per-difficulty totals", decoded)
	}
}

func TestJSONLRecorderCapturesExtractAndJudgeRoundTrips(t *testing.T) {
	// R-38BL-1OQS
	var log bytes.Buffer
	prov := &capturingProvider{responses: []string{
		`{"subjects":[{"type":"entity","kind":"company","name":"Acme Robotics","claims":["Acme Robotics opened a Tulsa lab."]}]}`,
		`{"covered":[{"gold":"Acme Robotics opened a Tulsa lab.","predicted":"Acme Robotics opened a Tulsa lab."}],"missed":[],"extra":[]}`,
	}}
	client := llm.New(prov, nil, NewJSONLRecorder(&log))
	extractSite := extract.DefaultCallSite()
	extractSite.Model = "mock-extract"
	judgeSite := DefaultJudgeCallSite()
	judgeSite.Model = "mock-judge"
	c := Case{
		Name:       "one",
		Difficulty: "easy",
		Header:     mustHeader(t, validGoldJSON("easy")),
		Text:       "Acme Robotics opened a Tulsa lab.",
		Gold:       []GoldSubject{{Type: "entity", Name: "Acme Robotics", Claims: []string{"Acme Robotics opened a Tulsa lab."}}},
	}

	predicted, err := Run(context.Background(), extract.New(client, extractSite), c)
	if err != nil {
		t.Fatalf("Run returned error: %v", err)
	}
	if _, err := Score(context.Background(), NewJudge(client, judgeSite), c, predicted); err != nil {
		t.Fatalf("Score returned error: %v", err)
	}

	records := decodeJSONLLines(t, log.String())
	if len(records) != 2 {
		t.Fatalf("record count = %d, want extract and judge records in JSONL:\n%s", len(records), log.String())
	}
	for i, record := range records {
		for _, field := range []string{"Stage", "Model", "Params", "Request", "Response"} {
			if _, ok := record[field]; !ok {
				t.Fatalf("record %d = %#v, missing %s", i, record, field)
			}
		}
	}
	if records[0]["Stage"] != "extract" || records[1]["Stage"] != "judge" {
		t.Fatalf("stages = %q, %q; want extract then judge", records[0]["Stage"], records[1]["Stage"])
	}
	if !strings.Contains(records[0]["Params"].(string), `"max_tokens":16384`) || !strings.Contains(records[1]["Request"].(string), "gold_claims") {
		t.Fatalf("records = %#v, want request and params footprints", records)
	}
}

func TestCommittedGoldCaseLoadsCleanly(t *testing.T) {
	// R-3ARD-T886
	got, err := LoadCase(committedCasePath())
	if err != nil {
		t.Fatalf("LoadCase returned error: %v", err)
	}
	switch got.Difficulty {
	case "easy", "medium", "hard":
	default:
		t.Fatalf("difficulty = %q, want easy, medium, or hard", got.Difficulty)
	}
	if len(got.Gold) == 0 {
		t.Fatal("gold subjects len = 0, want at least one")
	}
	if len(got.Gold[0].Claims) == 0 {
		t.Fatalf("first gold subject = %#v, want at least one claim", got.Gold[0])
	}
	if strings.TrimSpace(got.Text) == "" || got.Header.Source == "" || got.Header.Title == "" {
		t.Fatalf("case = %#v, want document text and header", got)
	}
}

func mustHeader(t *testing.T, gold string) extract.DocumentHeader {
	t.Helper()
	dir := writeCase(t, "header", "body", gold)
	c, err := LoadCase(dir)
	if err != nil {
		t.Fatalf("LoadCase for header: %v", err)
	}
	return c.Header
}

func decodeJSONLLines(t *testing.T, text string) []map[string]any {
	t.Helper()
	var out []map[string]any
	sc := bufio.NewScanner(strings.NewReader(text))
	for sc.Scan() {
		var record map[string]any
		if err := json.Unmarshal(sc.Bytes(), &record); err != nil {
			t.Fatalf("decode JSONL line %q: %v", sc.Text(), err)
		}
		out = append(out, record)
	}
	if err := sc.Err(); err != nil {
		t.Fatalf("scan JSONL: %v", err)
	}
	return out
}

func TestJSONLRecorderIgnoresNilWriter(t *testing.T) {
	if err := NewJSONLRecorder(nil).Record(context.Background(), llm.CallRecord{}); err != nil {
		t.Fatalf("nil writer Record error = %v, want nil", err)
	}
}

func TestLoadCommittedGoldUsesRealFiles(t *testing.T) {
	if _, err := os.Stat(filepath.Join(committedCasePath(), "gold.json")); err != nil {
		t.Fatalf("committed gold.json missing: %v", err)
	}
}

func committedCasePath() string {
	return filepath.Join("..", "..", "testdata", "eval", "extract", "tulsa-lab-opening")
}
