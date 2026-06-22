package compile

import (
	"context"
	"reflect"
	"strings"
	"testing"
	"unicode/utf8"

	agentkit "github.com/ikigenba/agentkit"

	"wiki/internal/extract"
	"wiki/internal/llm"
	"wiki/internal/wiki"
)

func TestCompileRendersSubjectIdentityAndCompleteClaimSet(t *testing.T) {
	// R-FQLB-QWS6
	prov := &scriptedProvider{responses: []string{`{"title":"Acme Robotics","body":"Acme Robotics opened a Tulsa lab and hired Mira Patel."}`}}
	compiler := New(llm.New(prov, nil), llm.CallSite{Model: "compile-model", System: "compile system"}, nil)

	title, body, err := compiler.Compile(context.Background(), acmeSubject(), []wiki.Claim{
		{ID: "claim-001", SubjectID: "subj-acme", Body: "Acme Robotics opened a research lab in Tulsa."},
		{ID: "claim-002", SubjectID: "subj-acme", Body: "Mira Patel leads Acme Robotics' Tulsa lab."},
	})
	if err != nil {
		t.Fatalf("Compile returned error: %v", err)
	}
	if title != "Acme Robotics" || body != "Acme Robotics opened a Tulsa lab and hired Mira Patel." {
		t.Fatalf("Compile result = %q/%q, want decoded page", title, body)
	}

	prompt := onlyPrompt(t, prov, 0)
	for _, want := range []string{
		"Compile one wiki page from the subject identity and complete claim set below.",
		"Use only the subject identity and claims",
		"do not use previous pages, prior page bodies, source documents, or unstated facts",
	} {
		if !strings.Contains(prompt, want) {
			t.Fatalf("prompt %q does not contain compile boundary %q", prompt, want)
		}
	}
	for _, want := range []string{
		"id: subj-acme",
		"name: Acme Robotics",
		"norm_name: acme robotics",
		"type: entity",
		"[claim-001] Acme Robotics opened a research lab in Tulsa.",
		"[claim-002] Mira Patel leads Acme Robotics' Tulsa lab.",
	} {
		if !strings.Contains(prompt, want) {
			t.Fatalf("prompt %q does not contain %q", prompt, want)
		}
	}
}

func TestCompileUsesInjectedCallSiteWithoutTools(t *testing.T) {
	// R-FT14-IG9K
	temp := 0.0
	prov := &scriptedProvider{responses: []string{`{"title":"Acme Robotics","body":"Acme Robotics operates a Tulsa research lab."}`}}
	site := llm.CallSite{
		Model:       "compile-model",
		Temperature: &temp,
		Reasoning:   agentkit.DisableReasoning(),
		System:      "compile from claims",
	}
	compiler := New(llm.New(prov, nil), site, nil)

	if _, _, err := compiler.Compile(context.Background(), acmeSubject(), acmeClaims()); err != nil {
		t.Fatalf("Compile returned error: %v", err)
	}
	if len(prov.requests) != 1 {
		t.Fatalf("requests len = %d, want 1", len(prov.requests))
	}
	req := prov.requests[0]
	if req.Model != site.Model || req.System != site.System {
		t.Fatalf("request config = %#v, want injected call site model/system", req)
	}
	if len(req.Tools) != 0 {
		t.Fatalf("request tools len = %d, want tool-less compile generation", len(req.Tools))
	}
	if req.Gen.Temperature == nil || *req.Gen.Temperature != temp || !req.Gen.Reasoning.Disabled() {
		t.Fatalf("gen settings = %#v, want injected temperature and disabled reasoning", req.Gen)
	}
}

func TestDefaultCallSiteUsesDeterministicReasoningOffSettings(t *testing.T) {
	// R-4DS4-RXYX
	site := DefaultCallSite("compile-model")
	if site.Model != "compile-model" {
		t.Fatalf("model = %q, want compile-model", site.Model)
	}
	if site.Temperature == nil || *site.Temperature != 0 {
		t.Fatalf("temperature = %#v, want 0", site.Temperature)
	}
	if !reflect.DeepEqual(site.Reasoning, llm.DisableReasoning()) {
		t.Fatalf("reasoning = %#v, want disabled", site.Reasoning)
	}
	if site.MaxTokens <= 0 {
		t.Fatalf("MaxTokens = %d, want non-zero output ceiling", site.MaxTokens)
	}

	prov := &scriptedProvider{responses: []string{`{"title":"Acme Robotics","body":"Acme Robotics operates a Tulsa research lab."}`}}
	compiler := New(llm.New(prov, nil), site, nil)
	if _, _, err := compiler.Compile(context.Background(), acmeSubject(), acmeClaims()); err != nil {
		t.Fatalf("Compile returned error: %v", err)
	}
	if len(prov.requests) != 1 {
		t.Fatalf("requests len = %d, want 1", len(prov.requests))
	}
	req := prov.requests[0]
	if req.Gen.Temperature == nil || *req.Gen.Temperature != 0 || !req.Gen.Reasoning.Disabled() {
		t.Fatalf("gen settings = %#v, want default temperature 0 and disabled reasoning", req.Gen)
	}
	if req.Gen.MaxTokens != site.MaxTokens {
		t.Fatalf("request max tokens = %d, want default ceiling %d", req.Gen.MaxTokens, site.MaxTokens)
	}
}

func TestExtractAndCompileDefaultCallSitesCarryOutputTokenCeilings(t *testing.T) {
	// R-MW86-M158
	prov := &scriptedProvider{responses: []string{
		`{"subjects":[]}`,
		`{"title":"Acme Robotics","body":"Acme Robotics operates a Tulsa research lab."}`,
	}}
	extractSite := extract.DefaultCallSite("extract-model")
	compileSite := DefaultCallSite("compile-model")
	if extractSite.MaxTokens <= 0 || compileSite.MaxTokens <= 0 {
		t.Fatalf("default max tokens = extract:%d compile:%d, want both non-zero", extractSite.MaxTokens, compileSite.MaxTokens)
	}

	extractor := extract.New(llm.New(prov, nil), extractSite)
	if _, err := extractor.Extract(context.Background(), extract.DocumentHeader{}, "source text"); err != nil {
		t.Fatalf("Extract returned error: %v", err)
	}
	compiler := New(llm.New(prov, nil), compileSite, nil)
	if _, _, err := compiler.Compile(context.Background(), acmeSubject(), acmeClaims()); err != nil {
		t.Fatalf("Compile returned error: %v", err)
	}
	if len(prov.requests) != 2 {
		t.Fatalf("requests len = %d, want extract and compile calls", len(prov.requests))
	}
	if prov.requests[0].Gen.MaxTokens != extractSite.MaxTokens {
		t.Fatalf("extract request max tokens = %d, want %d", prov.requests[0].Gen.MaxTokens, extractSite.MaxTokens)
	}
	if prov.requests[1].Gen.MaxTokens != compileSite.MaxTokens {
		t.Fatalf("compile request max tokens = %d, want %d", prov.requests[1].Gen.MaxTokens, compileSite.MaxTokens)
	}
}

func TestCompileRebuildsFromClaimsWithoutPriorGeneratedBody(t *testing.T) {
	// R-FU90-W809
	prov := &scriptedProvider{responses: []string{
		`{"title":"Acme Robotics","body":"STALE GENERATED BODY should not be reused."}`,
		`{"title":"Acme Robotics","body":"Acme Robotics opened a Denver lab."}`,
	}}
	compiler := New(llm.New(prov, nil), llm.CallSite{Model: "compile-model"}, nil)

	if _, _, err := compiler.Compile(context.Background(), acmeSubject(), []wiki.Claim{
		{ID: "claim-001", SubjectID: "subj-acme", Body: "Acme Robotics opened a Tulsa lab."},
	}); err != nil {
		t.Fatalf("first Compile returned error: %v", err)
	}
	title, body, err := compiler.Compile(context.Background(), acmeSubject(), []wiki.Claim{
		{ID: "claim-003", SubjectID: "subj-acme", Body: "Acme Robotics opened a Denver lab."},
	})
	if err != nil {
		t.Fatalf("second Compile returned error: %v", err)
	}
	if title != "Acme Robotics" || body != "Acme Robotics opened a Denver lab." {
		t.Fatalf("second result = %q/%q, want rebuilt page from second claim set", title, body)
	}

	secondPrompt := onlyPrompt(t, prov, 1)
	if strings.Contains(secondPrompt, "STALE GENERATED BODY") {
		t.Fatalf("second prompt contains prior generated body: %q", secondPrompt)
	}
	if strings.Contains(secondPrompt, "Tulsa lab") || !strings.Contains(secondPrompt, "Denver lab") {
		t.Fatalf("second prompt = %q, want only the new complete claim set", secondPrompt)
	}
}

func TestCompileTightensOverCapBodyFromClaims(t *testing.T) {
	// R-FVGX-9ZQY
	tooLong := strings.Repeat("a", PageCharCap+1)
	prov := &scriptedProvider{responses: []string{
		`{"title":"Acme Robotics","body":"` + tooLong + `"}`,
		`{"title":"Acme Robotics","body":"Acme Robotics runs a concise Tulsa lab page."}`,
	}}
	compiler := New(llm.New(prov, nil), llm.CallSite{Model: "compile-model"}, nil)

	_, body, err := compiler.Compile(context.Background(), acmeSubject(), acmeClaims())
	if err != nil {
		t.Fatalf("Compile returned error: %v", err)
	}
	if body != "Acme Robotics runs a concise Tulsa lab page." {
		t.Fatalf("body = %q, want tightened response", body)
	}
	if len(prov.requests) != 2 {
		t.Fatalf("requests len = %d, want initial compile plus tighten", len(prov.requests))
	}
	secondPrompt := onlyPrompt(t, prov, 1)
	if !strings.Contains(secondPrompt, "previous body was 12001 characters") || !strings.Contains(secondPrompt, "[claim-001]") {
		t.Fatalf("tighten prompt = %q, want cap warning and original claims", secondPrompt)
	}
	if strings.Contains(secondPrompt, tooLong) {
		t.Fatalf("tighten prompt should not include over-cap generated body")
	}
}

func TestCompileDeterministicallyEnforcesRuneCap(t *testing.T) {
	// R-FWOT-NRHN
	body := strings.Repeat("é", PageCharCap+7)
	prov := &scriptedProvider{responses: []string{`{"title":"Acme Robotics","body":"` + body + `"}`}}
	site := DefaultCallSite("compile-model")
	compiler := New(llm.New(prov, nil), site, nil)
	compiler.maxTighten = 0

	_, got, err := compiler.Compile(context.Background(), acmeSubject(), acmeClaims())
	if err != nil {
		t.Fatalf("Compile returned error: %v", err)
	}
	if utf8.RuneCountInString(got) != PageCharCap {
		t.Fatalf("body rune count = %d, want %d", utf8.RuneCountInString(got), PageCharCap)
	}
	if got != strings.Repeat("é", PageCharCap) {
		t.Fatalf("body was not truncated on rune boundaries")
	}
	if len(prov.requests) != 1 {
		t.Fatalf("requests len = %d, want 1", len(prov.requests))
	}
	req := prov.requests[0]
	if req.Model != site.Model {
		t.Fatalf("request model = %q, want %q", req.Model, site.Model)
	}
	if req.Gen.Temperature == nil || *req.Gen.Temperature != 0 || !req.Gen.Reasoning.Disabled() {
		t.Fatalf("gen settings = %#v, want default temperature 0 and disabled reasoning", req.Gen)
	}
}

func acmeSubject() wiki.Subject {
	return wiki.Subject{ID: "subj-acme", Name: "Acme Robotics", NormName: "acme robotics", Type: "entity"}
}

func acmeClaims() []wiki.Claim {
	return []wiki.Claim{
		{ID: "claim-001", SubjectID: "subj-acme", Body: "Acme Robotics opened a research lab in Tulsa."},
		{ID: "claim-002", SubjectID: "subj-acme", Body: "Mira Patel leads Acme Robotics' Tulsa lab."},
	}
}

func onlyPrompt(t *testing.T, prov *scriptedProvider, i int) string {
	t.Helper()
	if len(prov.requests) <= i {
		t.Fatalf("requests len = %d, want request index %d", len(prov.requests), i)
	}
	texts := requestTexts(prov.requests[i])
	if len(texts) != 1 {
		t.Fatalf("request texts = %#v, want one user prompt", texts)
	}
	return texts[0]
}

type scriptedProvider struct {
	responses []string
	requests  []agentkit.Request
}

func (p *scriptedProvider) RoundTrip(_ context.Context, req *agentkit.Request) *agentkit.RoundTrip {
	p.requests = append(p.requests, cloneRequest(req))
	text := `{"title":"Untitled","body":"Empty."}`
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
		text := agentkitText(msg)
		if text != "" {
			out = append(out, text)
		}
	}
	return out
}

func agentkitText(message agentkit.Message) string {
	var b strings.Builder
	for _, block := range message.Blocks {
		if text, ok := block.(agentkit.TextBlock); ok {
			b.WriteString(text.Text)
		}
	}
	return b.String()
}
