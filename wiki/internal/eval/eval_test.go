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
