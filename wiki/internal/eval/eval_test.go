package eval

import (
	"context"
	"encoding/json"
	"os"
	"path/filepath"
	"reflect"
	"strings"
	"testing"
	"time"

	agentkit "github.com/ikigenba/agentkit"

	"wiki/internal/extract"
	"wiki/internal/llm"
)

func TestLoadCaseLoadsWellFormedCase(t *testing.T) {
	// R-VXAT-MMTX
	dir := writeCase(t, "tulsa-lab", "Acme Robotics opened a Tulsa lab.\nSecond line stays verbatim.\n", `{
		"difficulty": "medium",
		"header": {
			"source": "mcp:ingest_text",
			"title": "Tulsa lab note",
			"tags": ["robotics", "tulsa"],
			"received_at": "2026-06-20T19:45:00-05:00"
		},
		"gold": [
			{
				"type": "entity",
				"name": "Acme Robotics",
				"claims": ["Acme Robotics opened a Tulsa lab."]
			},
			{
				"type": "concept",
				"name": "robotics lab",
				"claims": ["The robotics lab is in Tulsa.", "The lab opened before the note was received."]
			}
		]
	}`)

	got, err := LoadCase(dir)
	if err != nil {
		t.Fatalf("LoadCase returned error: %v", err)
	}
	if got.Name != "tulsa-lab" || got.Difficulty != "medium" {
		t.Fatalf("case name/difficulty = %q/%q, want tulsa-lab/medium", got.Name, got.Difficulty)
	}
	if got.Text != "Acme Robotics opened a Tulsa lab.\nSecond line stays verbatim.\n" {
		t.Fatalf("text = %q, want document.txt verbatim", got.Text)
	}
	wantTime := time.Date(2026, 6, 20, 19, 45, 0, 0, time.FixedZone("", -5*60*60))
	if !got.Header.ReceivedAt.Equal(wantTime) {
		t.Fatalf("received_at = %v, want %v from gold.json", got.Header.ReceivedAt, wantTime)
	}
	if got.Header.Source != "mcp:ingest_text" || got.Header.Title != "Tulsa lab note" {
		t.Fatalf("header = %#v, want source and title from gold.json", got.Header)
	}
	if !reflect.DeepEqual(got.Header.Tags, []string{"robotics", "tulsa"}) {
		t.Fatalf("tags = %#v, want parsed tags", got.Header.Tags)
	}
	wantGold := []GoldSubject{
		{Type: "entity", Name: "Acme Robotics", Claims: []string{"Acme Robotics opened a Tulsa lab."}},
		{Type: "concept", Name: "robotics lab", Claims: []string{"The robotics lab is in Tulsa.", "The lab opened before the note was received."}},
	}
	if !reflect.DeepEqual(got.Gold, wantGold) {
		t.Fatalf("gold = %#v, want parsed subjects and claims", got.Gold)
	}
}

func TestLoadCaseRejectsBoundaryViolations(t *testing.T) {
	// R-VYIQ-0EKM
	tests := []struct {
		name     string
		text     string
		gold     string
		wantErr  string
		noDoc    bool
		noGold   bool
		emptyDoc bool
	}{
		{
			name:    "missing document",
			gold:    validGoldJSON("easy"),
			wantErr: "document.txt",
			noDoc:   true,
		},
		{
			name:    "missing gold",
			text:    "body",
			wantErr: "gold.json",
			noGold:  true,
		},
		{
			name:     "empty document",
			text:     " \n\t",
			gold:     validGoldJSON("easy"),
			wantErr:  "document.txt required",
			emptyDoc: true,
		},
		{
			name:    "malformed gold",
			text:    "body",
			gold:    `{"difficulty":`,
			wantErr: "gold.json",
		},
		{
			name:    "unknown gold field",
			text:    "body",
			gold:    strings.Replace(validGoldJSON("easy"), `"gold":`, `"extra": true, "gold":`, 1),
			wantErr: "unknown field",
		},
		{
			name:    "bad difficulty",
			text:    "body",
			gold:    validGoldJSON("expert"),
			wantErr: "difficulty",
		},
		{
			name:    "missing source",
			text:    "body",
			gold:    strings.Replace(validGoldJSON("easy"), `"source": "mcp:ingest_text"`, `"source": ""`, 1),
			wantErr: "header.source",
		},
		{
			name:    "missing received at",
			text:    "body",
			gold:    strings.Replace(validGoldJSON("easy"), `"received_at": "2026-06-20T19:45:00Z"`, `"received_at": ""`, 1),
			wantErr: "header.received_at",
		},
		{
			name:    "invalid received at",
			text:    "body",
			gold:    strings.Replace(validGoldJSON("easy"), `"received_at": "2026-06-20T19:45:00Z"`, `"received_at": "2026-06-20"`, 1),
			wantErr: "RFC3339",
		},
		{
			name:    "invalid subject type",
			text:    "body",
			gold:    strings.Replace(validGoldJSON("easy"), `"type": "entity"`, `"type": "place"`, 1),
			wantErr: "gold[0].type",
		},
		{
			name:    "missing subject name",
			text:    "body",
			gold:    strings.Replace(validGoldJSON("easy"), `"name": "Acme Robotics"`, `"name": ""`, 1),
			wantErr: "gold[0].name",
		},
		{
			name:    "missing subject claims",
			text:    "body",
			gold:    strings.Replace(validGoldJSON("easy"), `"claims": ["Acme Robotics opened a Tulsa lab."]`, `"claims": []`, 1),
			wantErr: "gold[0].claims",
		},
		{
			name:    "empty subject claim",
			text:    "body",
			gold:    strings.Replace(validGoldJSON("easy"), `"claims": ["Acme Robotics opened a Tulsa lab."]`, `"claims": [" "]`, 1),
			wantErr: "gold[0].claims[0]",
		},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			dir := t.TempDir()
			if !tt.noDoc {
				text := tt.text
				if text == "" && !tt.emptyDoc {
					text = "body"
				}
				if err := os.WriteFile(filepath.Join(dir, "document.txt"), []byte(text), 0o600); err != nil {
					t.Fatalf("write document: %v", err)
				}
			}
			if !tt.noGold {
				if err := os.WriteFile(filepath.Join(dir, "gold.json"), []byte(tt.gold), 0o600); err != nil {
					t.Fatalf("write gold: %v", err)
				}
			}

			got, err := LoadCase(dir)
			if err == nil {
				t.Fatalf("LoadCase returned nil error and case %#v", got)
			}
			if !strings.Contains(err.Error(), tt.wantErr) {
				t.Fatalf("error = %v, want it to contain %q", err, tt.wantErr)
			}
		})
	}
}

func TestLoadDatasetLoadsImmediateSubdirectories(t *testing.T) {
	// R-VZQM-E6BB
	root := t.TempDir()
	writeCaseAt(t, filepath.Join(root, "b-case"), "B text", validGoldJSON("hard"))
	writeCaseAt(t, filepath.Join(root, "a-case"), "A text", validGoldJSON("easy"))
	if err := os.WriteFile(filepath.Join(root, "README.txt"), []byte("ignored"), 0o600); err != nil {
		t.Fatalf("write root file: %v", err)
	}

	got, err := LoadDataset(root)
	if err != nil {
		t.Fatalf("LoadDataset returned error: %v", err)
	}
	if len(got) != 2 {
		t.Fatalf("cases len = %d, want one case per immediate subdirectory", len(got))
	}
	if got[0].Name != "a-case" || got[1].Name != "b-case" {
		t.Fatalf("case order/names = %q, %q; want sorted immediate subdirectories", got[0].Name, got[1].Name)
	}
	if got[0].Text != "A text" || got[1].Text != "B text" {
		t.Fatalf("case texts = %q, %q; want loaded from subdirectory document.txt files", got[0].Text, got[1].Text)
	}
}

func TestRunFeedsCaseToProductionExtractor(t *testing.T) {
	// R-W26F-5PSP
	prov := &capturingProvider{responses: []string{`{"subjects":[{
		"type":"event",
		"kind":"opening",
		"name":"Acme Robotics Tulsa lab opening",
		"occurred_at":"2026-06-19",
		"claims":["Acme Robotics opened the Tulsa lab on June 19, 2026."]
	}]}`}}
	site := extract.DefaultCallSite()
	site.Model = "extract-model"
	ex := extract.New(llm.New(prov, nil), site)
	c := Case{
		Header: extract.DocumentHeader{
			Source:     "mcp:ingest_text",
			Title:      "Tulsa lab note",
			Tags:       []string{"robotics"},
			ReceivedAt: time.Date(2026, 6, 20, 19, 45, 0, 0, time.FixedZone("CDT", -5*60*60)),
		},
		Text: "Acme Robotics opened the Tulsa lab on June 19, 2026.",
	}

	got, err := Run(context.Background(), ex, c)
	if err != nil {
		t.Fatalf("Run returned error: %v", err)
	}
	want := []extract.ExtractedSubject{{
		Type:       "event",
		Kind:       "opening",
		Name:       "Acme Robotics Tulsa lab opening",
		OccurredAt: "2026-06-19",
		Claims:     []string{"Acme Robotics opened the Tulsa lab on June 19, 2026."},
	}}
	if !reflect.DeepEqual(got, want) {
		t.Fatalf("subjects = %#v, want extractor subjects unchanged", got)
	}
	if len(prov.requests) != 1 {
		t.Fatalf("requests len = %d, want one production extract call", len(prov.requests))
	}
	req := prov.requests[0]
	if req.Model != site.Model || len(req.Tools) != 0 || req.Gen.MaxTokens != site.MaxTokens {
		t.Fatalf("request = %#v, want production extract call site and no tools", req)
	}
	prompt := onlyRequestText(t, req)
	for _, wantText := range []string{
		"Extract subjects and claims from the source text.",
		"source: mcp:ingest_text",
		"title: Tulsa lab note",
		"tags: robotics",
		"received on: 2026-06-20",
		"Acme Robotics opened the Tulsa lab on June 19, 2026.",
	} {
		if !strings.Contains(prompt, wantText) {
			t.Fatalf("prompt %q does not contain %q", prompt, wantText)
		}
	}
}

func TestScorePartitionsSubjectsByTypeAndNormalizedName(t *testing.T) {
	// R-DRME-T4FA
	prov := &capturingProvider{responses: []string{`{
		"covered":[{"gold":"Cafe Noir opened a Tulsa lab.","predicted":"The Tulsa lab was opened by Cafe Noir."}],
		"missed":[],
		"extra":[]
	}`}}
	j := NewJudge(llm.New(prov, nil), DefaultJudgeCallSite())
	c := Case{
		Name:       "subject-partition",
		Difficulty: "easy",
		Gold: []GoldSubject{
			{Type: "entity", Name: "Café   Noir", Claims: []string{"Cafe Noir opened a Tulsa lab."}},
			{Type: "event", Name: "Lab opening", Claims: []string{"The lab opening happened in Tulsa."}},
		},
	}
	predicted := []extract.ExtractedSubject{
		{Type: "entity", Kind: "company", Name: "cafe noir", Claims: []string{"The Tulsa lab was opened by Cafe Noir."}},
		{Type: "concept", Kind: "topic", Name: "Café Noir", Claims: []string{"Cafe Noir is discussed as a concept."}},
	}

	got, err := Score(context.Background(), j, c, predicted)
	if err != nil {
		t.Fatalf("Score returned error: %v", err)
	}
	if !reflect.DeepEqual(got.Subjects.Found, []string{"entity/cafe-noir"}) {
		t.Fatalf("found = %#v, want normalized entity match", got.Subjects.Found)
	}
	if !reflect.DeepEqual(got.Subjects.Missed, []string{"event/lab-opening"}) {
		t.Fatalf("missed = %#v, want unmatched gold event", got.Subjects.Missed)
	}
	if !reflect.DeepEqual(got.Subjects.Hallucinated, []string{"concept/cafe-noir"}) {
		t.Fatalf("hallucinated = %#v, want same normalized name with different type to stay separate", got.Subjects.Hallucinated)
	}
	if got.Claims != (ClaimScore{Covered: 1, Missed: 1, Extra: 1}) {
		t.Fatalf("claims = %#v, want matched claim plus unmatched missed/extra claims", got.Claims)
	}
}

func TestScoreCallsJudgeForMatchedSubjectAndUsesVerdict(t *testing.T) {
	// R-DSUB-6W5Z
	prov := &capturingProvider{responses: []string{`{
		"covered":[{"gold":"Acme Robotics opened a lab in Tulsa.","predicted":"The Tulsa facility was opened by Acme Robotics."}],
		"missed":[],
		"extra":[]
	}`}}
	rec := &recordingRecorder{}
	site := DefaultJudgeCallSite()
	j := NewJudge(llm.New(prov, nil, rec), site)
	c := Case{
		Name:       "judge-call",
		Difficulty: "medium",
		Gold: []GoldSubject{{
			Type:   "entity",
			Name:   "Acme Robotics",
			Claims: []string{"Acme Robotics opened a lab in Tulsa."},
		}},
	}
	predicted := []extract.ExtractedSubject{{
		Type:   "entity",
		Kind:   "company",
		Name:   "acme robotics",
		Claims: []string{"The Tulsa facility was opened by Acme Robotics."},
	}}

	got, err := Score(context.Background(), j, c, predicted)
	if err != nil {
		t.Fatalf("Score returned error: %v", err)
	}
	if got.Claims != (ClaimScore{Covered: 1}) {
		t.Fatalf("claims = %#v, want scripted judge verdict to mark different wording covered", got.Claims)
	}
	if len(prov.requests) != 1 {
		t.Fatalf("requests len = %d, want one judge llm.JSON call for matched subject", len(prov.requests))
	}
	if len(rec.records) != 1 || rec.records[0].Stage != "judge" {
		t.Fatalf("records = %#v, want one Stage judge call record", rec.records)
	}
	req := prov.requests[0]
	if req.Model != site.Model || req.Gen.MaxTokens != site.MaxTokens {
		t.Fatalf("request model/max_tokens = %q/%d, want %q/%d", req.Model, req.Gen.MaxTokens, site.Model, site.MaxTokens)
	}
	prompt := onlyRequestText(t, req)
	for _, wantText := range []string{
		"entity/acme-robotics",
		"Acme Robotics opened a lab in Tulsa.",
		"The Tulsa facility was opened by Acme Robotics.",
	} {
		if !strings.Contains(prompt, wantText) {
			t.Fatalf("prompt %q does not contain %q", prompt, wantText)
		}
	}
}

func TestScoreSkipsJudgeForUnmatchedSubjectsAndClassifiesClaims(t *testing.T) {
	// R-DU27-KNWO
	prov := &capturingProvider{}
	j := NewJudge(llm.New(prov, nil), DefaultJudgeCallSite())
	c := Case{
		Name:       "unmatched",
		Difficulty: "hard",
		Gold: []GoldSubject{{
			Type: "entity",
			Name: "Acme Robotics",
			Claims: []string{
				"Acme Robotics opened a Tulsa lab.",
				"Acme Robotics hired robotics engineers.",
			},
		}},
	}
	predicted := []extract.ExtractedSubject{{
		Type:   "concept",
		Kind:   "topic",
		Name:   "Tulsa robotics",
		Claims: []string{"Tulsa robotics attracted investor interest."},
	}}

	got, err := Score(context.Background(), j, c, predicted)
	if err != nil {
		t.Fatalf("Score returned error: %v", err)
	}
	if len(prov.requests) != 0 {
		t.Fatalf("requests len = %d, want no judge call for unmatched subjects", len(prov.requests))
	}
	if !reflect.DeepEqual(got.Subjects.Missed, []string{"entity/acme-robotics"}) {
		t.Fatalf("missed = %#v, want unmatched gold subject", got.Subjects.Missed)
	}
	if !reflect.DeepEqual(got.Subjects.Hallucinated, []string{"concept/tulsa-robotics"}) {
		t.Fatalf("hallucinated = %#v, want unmatched predicted subject", got.Subjects.Hallucinated)
	}
	if got.Claims != (ClaimScore{Missed: 2, Extra: 1}) {
		t.Fatalf("claims = %#v, want unmatched gold claims missed and predicted claims extra", got.Claims)
	}
}

func TestScoreErrorsAfterBoundedMalformedOrOutOfRangeJudgeVerdicts(t *testing.T) {
	// R-DVA3-YFND
	prov := &capturingProvider{responses: []string{
		`not json`,
		`{"covered":[{"gold":"not a gold claim","predicted":"Predicted claim."}],"missed":[],"extra":[]}`,
		`{"covered":[],"missed":["Gold claim."],"extra":["not a predicted claim"]}`,
	}}
	site := DefaultJudgeCallSite()
	site.MaxParseRetries = 2
	j := NewJudge(llm.New(prov, nil), site)
	c := Case{
		Name:       "bad-verdict",
		Difficulty: "easy",
		Gold: []GoldSubject{{
			Type:   "entity",
			Name:   "Acme Robotics",
			Claims: []string{"Gold claim."},
		}},
	}
	predicted := []extract.ExtractedSubject{{
		Type:   "entity",
		Kind:   "company",
		Name:   "Acme Robotics",
		Claims: []string{"Predicted claim."},
	}}

	got, err := Score(context.Background(), j, c, predicted)
	if err == nil {
		t.Fatalf("Score returned nil error and result %#v", got)
	}
	if !strings.Contains(err.Error(), "after 3 attempt") {
		t.Fatalf("error = %v, want bounded retry failure", err)
	}
	if len(prov.requests) != 3 {
		t.Fatalf("requests len = %d, want MaxParseRetries+1 attempts", len(prov.requests))
	}
	if !strings.Contains(requestTexts(prov.requests[1]), "previous response could not be parsed") {
		t.Fatalf("second prompt = %q, want corrective re-prompt", requestTexts(prov.requests[1]))
	}
}

func TestAggregateComputesOverallAndPerDifficultyMetrics(t *testing.T) {
	// R-DXPW-PZ4R
	got := Aggregate([]CaseResult{
		{
			Difficulty: "easy",
			Subjects: SubjectScore{
				Found:        []string{"entity/a", "entity/b"},
				Missed:       []string{"entity/c"},
				Hallucinated: []string{"entity/d"},
			},
			Claims: ClaimScore{Covered: 3, Missed: 1, Extra: 2},
		},
		{
			Difficulty: "hard",
			Subjects: SubjectScore{
				Found:  []string{"event/e"},
				Missed: []string{"event/f", "event/g"},
			},
			Claims: ClaimScore{Covered: 1, Missed: 3},
		},
		{
			Difficulty: "easy",
			Subjects: SubjectScore{
				Found:        []string{"concept/h"},
				Hallucinated: []string{"concept/i"},
			},
			Claims: ClaimScore{Covered: 2, Extra: 1},
		},
	})

	assertMetrics(t, "overall subjects", got.Overall.Subjects, Metrics{Precision: 4.0 / 6.0, Recall: 4.0 / 7.0})
	assertMetrics(t, "overall claims", got.Overall.Claims, Metrics{Precision: 6.0 / 9.0, Recall: 6.0 / 10.0})
	if got.Overall.Cases != 3 {
		t.Fatalf("overall cases = %d, want 3", got.Overall.Cases)
	}
	easy := got.ByDifficulty["easy"]
	assertMetrics(t, "easy subjects", easy.Subjects, Metrics{Precision: 3.0 / 5.0, Recall: 3.0 / 4.0})
	assertMetrics(t, "easy claims", easy.Claims, Metrics{Precision: 5.0 / 8.0, Recall: 5.0 / 6.0})
	if easy.Cases != 2 {
		t.Fatalf("easy cases = %d, want 2", easy.Cases)
	}
	hard := got.ByDifficulty["hard"]
	assertMetrics(t, "hard subjects", hard.Subjects, Metrics{Precision: 1, Recall: 1.0 / 3.0})
	assertMetrics(t, "hard claims", hard.Claims, Metrics{Precision: 1, Recall: 1.0 / 4.0})
	if hard.Cases != 1 {
		t.Fatalf("hard cases = %d, want 1", hard.Cases)
	}
}

func writeCase(t *testing.T, name, text, gold string) string {
	t.Helper()
	dir := filepath.Join(t.TempDir(), name)
	writeCaseAt(t, dir, text, gold)
	return dir
}

func writeCaseAt(t *testing.T, dir, text, gold string) {
	t.Helper()
	if err := os.MkdirAll(dir, 0o700); err != nil {
		t.Fatalf("mkdir case: %v", err)
	}
	if err := os.WriteFile(filepath.Join(dir, "document.txt"), []byte(text), 0o600); err != nil {
		t.Fatalf("write document: %v", err)
	}
	if err := os.WriteFile(filepath.Join(dir, "gold.json"), []byte(gold), 0o600); err != nil {
		t.Fatalf("write gold: %v", err)
	}
}

func validGoldJSON(difficulty string) string {
	return `{
		"difficulty": ` + strconvQuote(difficulty) + `,
		"header": {
			"source": "mcp:ingest_text",
			"title": "Tulsa lab note",
			"tags": ["robotics"],
			"received_at": "2026-06-20T19:45:00Z"
		},
		"gold": [
			{
				"type": "entity",
				"name": "Acme Robotics",
				"claims": ["Acme Robotics opened a Tulsa lab."]
			}
		]
	}`
}

func strconvQuote(s string) string {
	raw, err := json.Marshal(s)
	if err != nil {
		panic(err)
	}
	return string(raw)
}

type capturingProvider struct {
	responses []string
	requests  []agentkit.Request
}

func (p *capturingProvider) RoundTrip(ctx context.Context, req *agentkit.Request) *agentkit.RoundTrip {
	p.requests = append(p.requests, cloneRequest(req))
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

func (p *capturingProvider) Name() string {
	return "capturing"
}

func (p *capturingProvider) Pricing(string) (agentkit.Pricing, bool) {
	return agentkit.Pricing{Tiers: []agentkit.RateTier{{MinInputTokens: 0}}}, true
}

func cloneRequest(req *agentkit.Request) agentkit.Request {
	if req == nil {
		return agentkit.Request{}
	}
	return agentkit.Request{
		Model:    req.Model,
		System:   req.System,
		Messages: append([]agentkit.Message(nil), req.Messages...),
		Tools:    append([]agentkit.Tool(nil), req.Tools...),
		Gen:      req.Gen,
	}
}

func onlyRequestText(t *testing.T, req agentkit.Request) string {
	t.Helper()
	var out []string
	for _, msg := range req.Messages {
		var b strings.Builder
		for _, block := range msg.Blocks {
			if text, ok := block.(agentkit.TextBlock); ok {
				b.WriteString(text.Text)
			}
		}
		if b.Len() > 0 {
			out = append(out, b.String())
		}
	}
	if len(out) != 1 {
		t.Fatalf("request texts = %#v, want one user prompt", out)
	}
	return out[0]
}

func requestTexts(req agentkit.Request) string {
	var out []string
	for _, msg := range req.Messages {
		var b strings.Builder
		for _, block := range msg.Blocks {
			if text, ok := block.(agentkit.TextBlock); ok {
				b.WriteString(text.Text)
			}
		}
		if b.Len() > 0 {
			out = append(out, b.String())
		}
	}
	return strings.Join(out, "\n")
}

type recordingRecorder struct {
	records []llm.CallRecord
}

func (r *recordingRecorder) Record(_ context.Context, rec llm.CallRecord) error {
	r.records = append(r.records, rec)
	return nil
}

func assertMetrics(t *testing.T, name string, got, want Metrics) {
	t.Helper()
	if got != want {
		t.Fatalf("%s = %#v, want %#v", name, got, want)
	}
}
