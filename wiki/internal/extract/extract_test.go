package extract

import (
	"context"
	"reflect"
	"strings"
	"testing"
	"time"

	agentkit "github.com/ikigenba/agentkit"

	"wiki/internal/llm"
)

func TestExtractRendersDocumentHeaderAndReturnsSubjects(t *testing.T) {
	prov := &scriptedProvider{responses: []string{`{
		"subjects": [
			{
				"type": "entity",
				"kind": "company",
				"name": "Acme Robotics",
				"occurred_at": "",
				"claims": ["Acme Robotics opened a research lab in Tulsa."]
			}
		]
	}`}}
	extractor := New(llm.New(prov, nil), llm.CallSite{Model: "extract-model", System: "extract system"})
	header := DocumentHeader{
		Source:     "mcp:ingest_text",
		Title:      "Tulsa robotics notes",
		Tags:       []string{"robotics", "tulsa"},
		ReceivedAt: time.Date(2026, 6, 20, 19, 45, 0, 0, time.FixedZone("CDT", -5*60*60)),
	}

	got, err := extractor.Extract(context.Background(), header, "Acme Robotics opened a research lab in Tulsa.")
	if err != nil {
		t.Fatalf("Extract returned error: %v", err)
	}
	if len(got) != 1 {
		t.Fatalf("subjects len = %d, want 1", len(got))
	}
	if got[0].Type != "entity" || got[0].Kind != "company" || got[0].Name != "Acme Robotics" {
		t.Fatalf("subject = %#v, want decoded extracted entity", got[0])
	}
	// R-W19T-38SB
	if got[0].OccurredAt != "" {
		t.Fatalf("occurred_at = %q, want honest empty date when source states no defining date", got[0].OccurredAt)
	}
	if len(got[0].Claims) != 1 || got[0].Claims[0] != "Acme Robotics opened a research lab in Tulsa." {
		t.Fatalf("claims = %#v, want decoded claims", got[0].Claims)
	}

	prompt := onlyPrompt(t, prov)
	for _, want := range []string{
		"source: mcp:ingest_text",
		"title: Tulsa robotics notes",
		"tags: robotics, tulsa",
		"received on: 2026-06-20",
		"occurred_at is required for events, optional for entities and concepts",
		"Acme Robotics opened a research lab in Tulsa.",
	} {
		if !strings.Contains(prompt, want) {
			t.Fatalf("prompt %q does not contain %q", prompt, want)
		}
	}
	if strings.Contains(strings.ToLower(prompt), "today is") {
		t.Fatalf("prompt = %q, want received date rendered without relative today wording", prompt)
	}
}

func TestExtractUsesCustomPromptInstructionsAndAppendsSourceContext(t *testing.T) {
	// R-ODAP-34N6
	prov := &scriptedProvider{responses: []string{`{
		"subjects": [
			{
				"type": "concept",
				"kind": "method",
				"name": "prompt experiments",
				"occurred_at": "",
				"claims": ["Prompt experiments compare extraction instructions."]
			}
		]
	}`}}
	extractor := New(
		llm.New(prov, nil),
		llm.CallSite{Model: "extract-model"},
		WithPromptInstructions("CUSTOM JSON CONTRACT"),
	)

	_, err := extractor.Extract(context.Background(), validHeader(), "Prompt experiments compare extraction instructions.")
	if err != nil {
		t.Fatalf("Extract returned error: %v", err)
	}

	prompt := onlyPrompt(t, prov)
	if !strings.HasPrefix(prompt, "CUSTOM JSON CONTRACT\n\nDocument header:\n") {
		t.Fatalf("prompt = %q, want custom instructions before generated document context", prompt)
	}
	for _, want := range []string{
		"source: mcp:ingest_text",
		"title: Source",
		"Source text:\nPrompt experiments compare extraction instructions.",
	} {
		if !strings.Contains(prompt, want) {
			t.Fatalf("prompt %q does not contain %q", prompt, want)
		}
	}
	if strings.Contains(prompt, DefaultPromptInstructions) {
		t.Fatalf("prompt = %q, want custom instructions to replace the default prompt", prompt)
	}
}

func TestExtractDefaultsToExportedPromptInstructions(t *testing.T) {
	// R-OGYE-8FV9
	prov := &scriptedProvider{responses: []string{`{"subjects":[{"type":"entity","kind":"company","name":"Acme Robotics","occurred_at":"","claims":["Acme Robotics opened a research lab."]}]}`}}
	extractor := New(llm.New(prov, nil), llm.CallSite{Model: "extract-model"})

	if _, err := extractor.Extract(context.Background(), validHeader(), "Acme Robotics opened a research lab."); err != nil {
		t.Fatalf("Extract returned error: %v", err)
	}

	prompt := onlyPrompt(t, prov)
	if !strings.HasPrefix(prompt, DefaultPromptInstructions+"\n\nDocument header:\n") {
		t.Fatalf("prompt = %q, want exported default instructions followed by generated document context", prompt)
	}
}

func TestExtractRejectsInvalidSubjectTypesAndEmptyClaims(t *testing.T) {
	tests := []struct {
		name     string
		response string
		wantErr  string
	}{
		{
			name: "invalid type",
			// R-VYU0-BPAX
			response: `{"subjects":[{
				"type":"place",
				"kind":"city",
				"name":"Tulsa",
				"occurred_at":"",
				"claims":["Tulsa hosted the meeting."]
			}]}`,
			wantErr: "type",
		},
		{
			name: "empty claims",
			response: `{"subjects":[{
				"type":"concept",
				"kind":"method",
				"name":"retrieval augmented generation",
				"occurred_at":"",
				"claims":[]
			}]}`,
			wantErr: "claims required",
		},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			prov := &scriptedProvider{responses: []string{tt.response}}
			extractor := New(llm.New(prov, nil), llm.CallSite{Model: "extract-model"})

			got, err := extractor.Extract(context.Background(), validHeader(), "source text")
			if err == nil {
				t.Fatal("Extract returned nil error")
			}
			if got != nil {
				t.Fatalf("subjects = %#v, want nil on invalid response", got)
			}
			if !strings.Contains(err.Error(), tt.wantErr) {
				t.Fatalf("error = %v, want %q", err, tt.wantErr)
			}
		})
	}
}

func TestExtractRetainsNonEventOccurredAt(t *testing.T) {
	// R-XJBY-H8JZ
	prov := &scriptedProvider{responses: []string{`{"subjects":[{
		"type":"entity",
		"kind":"company",
		"name":"Acme Robotics",
		"occurred_at":"2026-06",
		"claims":["Acme Robotics was founded in June 2026."]
	}]}`}}
	extractor := New(llm.New(prov, nil), llm.CallSite{Model: "extract-model"})

	got, err := extractor.Extract(context.Background(), validHeader(), "Acme Robotics was founded in June 2026.")
	if err != nil {
		t.Fatalf("Extract returned error: %v", err)
	}
	if len(got) != 1 {
		t.Fatalf("subjects len = %d, want 1", len(got))
	}
	if got[0].Type != "entity" || got[0].OccurredAt != "2026-06" {
		t.Fatalf("subject = %#v, want entity occurred_at retained exactly", got[0])
	}
}

func TestExtractGaryGygaxDocumentAcceptsEntityYears(t *testing.T) {
	// R-XJBY-H8JZ
	prov := &scriptedProvider{responses: []string{`{"subjects":[
		{
			"type":"entity",
			"kind":"person",
			"name":"Gary Gygax",
			"occurred_at":"1938",
			"claims":["Gary Gygax was born in 1938."]
		},
		{
			"type":"entity",
			"kind":"company",
			"name":"TSR",
			"occurred_at":"1973",
			"claims":["TSR was founded in 1973."]
		},
		{
			"type":"entity",
			"kind":"game",
			"name":"Dungeons & Dragons",
			"occurred_at":"1974",
			"claims":["Dungeons & Dragons was first published in 1974."]
		}
	]}`}}
	extractor := New(llm.New(prov, nil), llm.CallSite{Model: "extract-model"})

	got, err := extractor.Extract(context.Background(), validHeader(), "Gary Gygax was born in 1938. TSR was founded in 1973. Dungeons & Dragons was first published in 1974.")
	if err != nil {
		t.Fatalf("Extract returned error: %v", err)
	}
	if len(got) != 3 {
		t.Fatalf("subjects = %#v, want three extracted Gary Gygax subjects", got)
	}
	for _, subject := range got {
		if subject.Type != "entity" {
			t.Fatalf("subject = %#v, want only entities", subject)
		}
		if subject.OccurredAt == "" || !isISOPrefix(subject.OccurredAt) {
			t.Fatalf("subject = %#v, want entity year retained as an ISO prefix", subject)
		}
	}
}

func TestExtractValidatesOccurredAtOnEverySubjectType(t *testing.T) {
	// R-XKJU-V0AO
	tests := []struct {
		name      string
		subject   string
		wantError bool
	}{
		{
			name: "event accepts ISO year month day prefix",
			subject: `{
				"type":"event",
				"kind":"launch",
				"name":"Acme Robotics lab opening",
				"occurred_at":"2026-06-20",
				"claims":["Acme Robotics opened a research lab on June 20, 2026."]
			}`,
		},
		{
			name: "event rejects non ISO prefix",
			subject: `{
				"type":"event",
				"kind":"launch",
				"name":"Acme Robotics lab opening",
				"occurred_at":"June 20, 2026",
				"claims":["Acme Robotics opened a research lab on June 20, 2026."]
			}`,
			wantError: true,
		},
		{
			name: "event rejects empty occurred at",
			subject: `{
				"type":"event",
				"kind":"launch",
				"name":"Acme Robotics lab opening",
				"occurred_at":"",
				"claims":["Acme Robotics opened a research lab on June 20, 2026."]
			}`,
			wantError: true,
		},
		{
			name: "entity accepts ISO year prefix",
			subject: `{
				"type":"entity",
				"kind":"company",
				"name":"Acme Robotics",
				"occurred_at":"2026",
				"claims":["Acme Robotics opened a research lab."]
			}`,
		},
		{
			name: "concept accepts ISO year month prefix",
			subject: `{
				"type":"concept",
				"kind":"method",
				"name":"claim extraction",
				"occurred_at":"2026-06",
				"claims":["Claim extraction was documented in June 2026."]
			}`,
		},
		{
			name: "entity rejects non ISO prefix",
			subject: `{
				"type":"entity",
				"kind":"company",
				"name":"Acme Robotics",
				"occurred_at":"June 2026",
				"claims":["Acme Robotics opened a research lab."]
			}`,
			wantError: true,
		},
		{
			name: "concept rejects non ISO prefix",
			subject: `{
				"type":"concept",
				"kind":"method",
				"name":"claim extraction",
				"occurred_at":"Summer 2026",
				"claims":["Claim extraction was documented in summer 2026."]
			}`,
			wantError: true,
		},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			prov := &scriptedProvider{responses: []string{`{"subjects":[` + tt.subject + `]}`}}
			extractor := New(llm.New(prov, nil), llm.CallSite{Model: "extract-model"})

			got, err := extractor.Extract(context.Background(), validHeader(), "Acme Robotics opened a research lab.")
			if tt.wantError {
				if err == nil {
					t.Fatal("Extract returned nil error")
				}
				if got != nil {
					t.Fatalf("subjects = %#v, want nil on invalid occurred_at", got)
				}
				return
			}
			if err != nil {
				t.Fatalf("Extract returned error: %v", err)
			}
			if len(got) != 1 {
				t.Fatalf("subjects = %#v, want one subject", got)
			}
			if got[0].OccurredAt == "" || !isISOPrefix(got[0].OccurredAt) {
				t.Fatalf("subjects = %#v, want valid ISO occurred_at retained", got)
			}
		})
	}
}

func TestExtractRepromptsAfterOccurredAtValidationFailure(t *testing.T) {
	// R-XKJU-V0AO
	prov := &scriptedProvider{responses: []string{
		`{"subjects":[{
			"type":"entity",
			"kind":"company",
			"name":"Acme Robotics",
			"occurred_at":"June 2026",
			"claims":["Acme Robotics opened a research lab."]
		}]}`,
		`{"subjects":[{
			"type":"entity",
			"kind":"company",
			"name":"Acme Robotics",
			"occurred_at":"2026",
			"claims":["Acme Robotics opened a research lab."]
		}]}`,
	}}
	extractor := New(llm.New(prov, nil), llm.CallSite{Model: "extract-model", MaxParseRetries: 1})

	got, err := extractor.Extract(context.Background(), validHeader(), "Acme Robotics opened a research lab in 2026.")
	if err != nil {
		t.Fatalf("Extract returned error after retry: %v", err)
	}
	if len(got) != 1 || got[0].OccurredAt != "2026" {
		t.Fatalf("subjects = %#v, want corrected ISO occurred_at after retry", got)
	}
	if len(prov.requests) != 2 {
		t.Fatalf("requests len = %d, want validation failure to trigger one re-prompt", len(prov.requests))
	}
	corrective := requestTexts(prov.requests[1])
	correctiveText := strings.Join(corrective, "\n")
	if !strings.Contains(correctiveText, "previous response") || !strings.Contains(correctiveText, "occurred_at must be an ISO-8601 prefix") {
		t.Fatalf("corrective prompt = %#v, want validation error in re-prompt", corrective)
	}
}

func TestDefaultCallSiteRetriesBadThenGoodExtraction(t *testing.T) {
	// R-4CK8-E688
	prov := &scriptedProvider{responses: []string{
		`{"subjects":[{
			"type":"place",
			"kind":"city",
			"name":"Tulsa",
			"occurred_at":"",
			"claims":["Tulsa hosted the meeting."]
		}]}`,
		`{"subjects":[{
			"type":"event",
			"kind":"meeting",
			"name":"Tulsa planning meeting",
			"occurred_at":"2026-06-20",
			"claims":["Tulsa hosted the planning meeting on June 20, 2026."]
		}]}`,
	}}
	site := DefaultCallSite()
	site.Model = "extract-model"
	// R-GGIG-AN7W
	if site.Stage != "extract" {
		t.Fatalf("stage = %q, want extract", site.Stage)
	}
	if site.Temperature == nil || *site.Temperature != 0 {
		t.Fatalf("temperature = %#v, want 0", site.Temperature)
	}
	if !reflect.DeepEqual(site.Reasoning, llm.DisableReasoning()) {
		t.Fatalf("reasoning = %#v, want disabled", site.Reasoning)
	}
	if site.MaxTokens != 16384 {
		t.Fatalf("MaxTokens = %d, want 16384", site.MaxTokens)
	}
	if site.MaxParseRetries != 2 {
		t.Fatalf("MaxParseRetries = %d, want 2", site.MaxParseRetries)
	}
	extractor := New(llm.New(prov, nil), site)

	got, err := extractor.Extract(context.Background(), validHeader(), "Tulsa hosted the planning meeting on June 20, 2026.")
	if err != nil {
		t.Fatalf("Extract returned error after default retry: %v", err)
	}
	if len(got) != 1 || got[0].Type != "event" || got[0].OccurredAt != "2026-06-20" {
		t.Fatalf("subjects = %#v, want corrected event from second response", got)
	}
	if len(prov.requests) != 2 {
		t.Fatalf("requests len = %d, want bad response to trigger one re-prompt", len(prov.requests))
	}
	req := prov.requests[0]
	if req.Gen.Temperature == nil || *req.Gen.Temperature != 0 || !req.Gen.Reasoning.Disabled() {
		t.Fatalf("gen settings = %#v, want default temperature 0 and disabled reasoning", req.Gen)
	}
	corrective := strings.Join(requestTexts(prov.requests[1]), "\n")
	if !strings.Contains(corrective, "previous response") || !strings.Contains(corrective, "type must be entity, event, or concept") {
		t.Fatalf("corrective prompt = %q, want validation error in re-prompt", corrective)
	}
}

func TestExtractUsesInjectedLLMCallSiteWithoutTools(t *testing.T) {
	// R-W2HP-H0J0
	prov := &scriptedProvider{responses: []string{`{"subjects":[{
		"type":"concept",
		"kind":"method",
		"name":"claim extraction",
		"occurred_at":"",
		"claims":["Claim extraction turns source text into self-contained statements."]
	}]}`}}
	site := DefaultCallSite()
	site.Model = "extract-model"
	extractor := New(llm.New(prov, nil), site)

	if _, err := extractor.Extract(context.Background(), validHeader(), "Claim extraction turns source text into statements."); err != nil {
		t.Fatalf("Extract returned error: %v", err)
	}
	if len(prov.requests) != 1 {
		t.Fatalf("requests len = %d, want 1", len(prov.requests))
	}
	req := prov.requests[0]
	if req.Model != site.Model {
		t.Fatalf("request model = %q, want %q", req.Model, site.Model)
	}
	if len(req.Tools) != 0 {
		t.Fatalf("request tools len = %d, want tool-less extract generation", len(req.Tools))
	}
	if req.Gen.Temperature == nil || *req.Gen.Temperature != 0 || !req.Gen.Reasoning.Disabled() {
		t.Fatalf("gen settings = %#v, want default temperature 0 and disabled reasoning", req.Gen)
	}
}

func validHeader() DocumentHeader {
	return DocumentHeader{
		Source:     "mcp:ingest_text",
		Title:      "Source",
		Tags:       []string{"tag"},
		ReceivedAt: time.Date(2026, 6, 20, 0, 0, 0, 0, time.UTC),
	}
}

func onlyPrompt(t *testing.T, prov *scriptedProvider) string {
	t.Helper()
	if len(prov.requests) != 1 {
		t.Fatalf("requests len = %d, want 1", len(prov.requests))
	}
	texts := requestTexts(prov.requests[0])
	if len(texts) != 1 {
		t.Fatalf("request texts = %#v, want one user prompt", texts)
	}
	return texts[0]
}

type scriptedProvider struct {
	responses []string
	requests  []agentkit.Request
}

func (p *scriptedProvider) RoundTrip(ctx context.Context, req *agentkit.Request) *agentkit.RoundTrip {
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

func (p *scriptedProvider) Name() string {
	return "scripted"
}

func (p *scriptedProvider) Pricing(string) (agentkit.Pricing, bool) {
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

func requestTexts(req agentkit.Request) []string {
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
	return out
}
