package ask

import (
	"context"
	"database/sql"
	"encoding/json"
	"reflect"
	"strings"
	"testing"

	agentkit "github.com/ikigenba/agentkit"

	"wiki/internal/db"
	"wiki/internal/llm"
	"wiki/internal/wiki"
)

func TestAskRunsExtractionThenSynthesizesFromResolvedSubjectPages(t *testing.T) {
	// R-644V-3WUS
	// R-65CR-HOLH
	// R-6A8D-0RK9
	// R-05CG-3H6Y
	ctx := context.Background()
	conn := migratedDB(t, ctx)
	defer conn.Close()
	savePage(t, ctx, conn, wiki.Subject{ID: "subject-cafe", Name: "Café Noir", Type: "entity"}, wiki.Page{
		ID:        "page-cafe",
		SubjectID: "subject-cafe",
		Title:     "Café Noir",
		Body:      "Café Noir keeps the deployment checklist.",
	})
	prov := &askProvider{responses: []*agentkit.RoundTrip{
		textRoundTrip(`{"subjects":["  cafe noir  "]}`),
		textRoundTrip(`{
			"found": true,
			"text": "Café Noir keeps the deployment checklist.",
			"citations": [{"subject":"subject-cafe","title":"Café Noir"}]
		}`),
	}}
	extractSite := llm.CallSite{Model: "extract-model", System: "extract system"}
	synthSite := llm.CallSite{Model: "synth-model", System: "synth system"}

	got, err := New(wiki.NewSubjectStore(conn), wiki.NewPageStore(conn), llm.New(prov, nil), extractSite, synthSite).
		Ask(ctx, "owner@example.com", "Where is Café Noir's checklist?")
	if err != nil {
		t.Fatalf("Ask returned error: %v", err)
	}
	if !got.Found || got.Text != "Café Noir keeps the deployment checklist." {
		t.Fatalf("Ask = %+v, want synthesized found answer", got)
	}
	if want := []Citation{{Path: "entity/cafe-noir", Title: "Café Noir"}}; !reflect.DeepEqual(got.Citations, want) {
		t.Fatalf("citations = %+v, want %+v", got.Citations, want)
	}
	citationsJSON, err := json.Marshal(got.Citations)
	if err != nil {
		t.Fatalf("Marshal citations: %v", err)
	}
	if strings.Contains(string(citationsJSON), "subject-cafe") {
		t.Fatalf("citations JSON = %s, want no internal subject id", citationsJSON)
	}
	if len(prov.requests) != 2 {
		t.Fatalf("provider requests = %d, want extract then synth", len(prov.requests))
	}
	if prov.requests[0].Model != "extract-model" || prov.requests[0].System != "extract system" {
		t.Fatalf("extract request model/system = %q/%q", prov.requests[0].Model, prov.requests[0].System)
	}
	if prov.requests[1].Model != "synth-model" || prov.requests[1].System != "synth system" {
		t.Fatalf("synth request model/system = %q/%q", prov.requests[1].Model, prov.requests[1].System)
	}
	extractText := requestText(prov.requests[0])
	if !strings.Contains(extractText, "Where is Café Noir's checklist?") {
		t.Fatalf("extract prompt = %q, want original question", extractText)
	}
	synthText := requestText(prov.requests[1])
	if !strings.Contains(synthText, "subject-cafe") || !strings.Contains(synthText, "Café Noir keeps the deployment checklist.") {
		t.Fatalf("synth prompt = %q, want resolved page context", synthText)
	}
}

func TestDefaultAskCallSitesUseSeparateReasoningLowStages(t *testing.T) {
	// R-GHQC-OEYL
	subject := DefaultSubjectCallSite()
	synthesis := DefaultSynthesisCallSite()
	if subject.Stage != "ask-subject" {
		t.Fatalf("subject stage = %q, want ask-subject", subject.Stage)
	}
	if synthesis.Stage != "ask-synthesis" {
		t.Fatalf("synthesis stage = %q, want ask-synthesis", synthesis.Stage)
	}
	for name, site := range map[string]llm.CallSite{
		"subject":   subject,
		"synthesis": synthesis,
	} {
		if site.MaxTokens != 16384 {
			t.Fatalf("%s MaxTokens = %d, want 16384", name, site.MaxTokens)
		}
		if !reflect.DeepEqual(site.Reasoning, agentkit.Level("low")) {
			t.Fatalf("%s reasoning = %#v, want low level", name, site.Reasoning)
		}
	}
}

func TestAskBestEffortGathersEveryResolvedSubjectPage(t *testing.T) {
	// R-66KN-VGC6
	ctx := context.Background()
	conn := migratedDB(t, ctx)
	defer conn.Close()
	savePage(t, ctx, conn, wiki.Subject{ID: "subject-ada", Name: "Ada", Type: "entity"}, wiki.Page{
		ID:        "page-ada",
		SubjectID: "subject-ada",
		Title:     "Ada",
		Body:      "Ada owns the parser.",
	})
	savePage(t, ctx, conn, wiki.Subject{ID: "subject-grace", Name: "Grace", Type: "entity"}, wiki.Page{
		ID:        "page-grace",
		SubjectID: "subject-grace",
		Title:     "Grace",
		Body:      "Grace owns the scheduler.",
	})
	prov := &askProvider{responses: []*agentkit.RoundTrip{
		textRoundTrip(`{"subjects":["Ada","Missing Person","Grace"]}`),
		textRoundTrip(`{
			"found": true,
			"text": "Ada owns the parser and Grace owns the scheduler.",
			"citations": [
				{"subject":"subject-ada","title":"Ada"},
				{"subject":"subject-grace","title":"Grace"}
			]
		}`),
	}}

	got, err := New(wiki.NewSubjectStore(conn), wiki.NewPageStore(conn), llm.New(prov, nil), testExtractSite(), testSynthSite()).
		Ask(ctx, "owner@example.com", "What do Ada, Missing Person, and Grace own?")
	if err != nil {
		t.Fatalf("Ask returned error: %v", err)
	}
	if !got.Found || len(got.Citations) != 2 {
		t.Fatalf("Ask = %+v, want answer from two resolved subjects", got)
	}
	synthText := requestText(prov.requests[1])
	for _, want := range []string{"Ada owns the parser.", "Grace owns the scheduler."} {
		if !strings.Contains(synthText, want) {
			t.Fatalf("synth prompt = %q, want %q", synthText, want)
		}
	}
	pagesJSON := synthText[strings.Index(synthText, "Pages: "):]
	if strings.Contains(pagesJSON, `"Missing Person"`) {
		t.Fatalf("synth pages = %q, want unresolved subject omitted", pagesJSON)
	}
}

func TestAskReturnsHonestEmptyWhenNoExtractedSubjectResolves(t *testing.T) {
	// R-67SK-982V
	ctx := context.Background()
	conn := migratedDB(t, ctx)
	defer conn.Close()
	prov := &askProvider{responses: []*agentkit.RoundTrip{
		textRoundTrip(`{"subjects":["Unknown One","Unknown Two"]}`),
		textRoundTrip(`{"found":true,"text":"should not be used","citations":[]}`),
	}}

	got, err := New(wiki.NewSubjectStore(conn), wiki.NewPageStore(conn), llm.New(prov, nil), testExtractSite(), testSynthSite()).
		Ask(ctx, "owner@example.com", "What happened to Unknown One?")
	if err != nil {
		t.Fatalf("Ask returned error: %v", err)
	}
	if got.Found || got.Text != honestEmptyText || len(got.Citations) != 0 {
		t.Fatalf("Ask = %+v, want honest empty answer", got)
	}
	if len(prov.requests) != 1 {
		t.Fatalf("provider requests = %d, want extraction only with no synthesis", len(prov.requests))
	}
}

func TestAskSynthesisUsesOnlyGatheredPageBodies(t *testing.T) {
	// R-5UPD-VVNA
	// R-690G-MZTK
	// R-5X56-NF4O
	ctx := context.Background()
	conn := migratedDB(t, ctx)
	defer conn.Close()
	savePage(t, ctx, conn, wiki.Subject{ID: "subject-ada", Name: "Ada", Type: "entity"}, wiki.Page{
		ID:        "page-ada",
		SubjectID: "subject-ada",
		Title:     "Ada",
		Body:      "Compiled page body says Ada approved the release.",
	})
	if err := wiki.NewClaimStore(conn).Save(ctx, wiki.Claim{
		ID:        "claim-raw",
		SubjectID: "subject-ada",
		JobID:     "job-secret",
		Body:      "RAW CLAIM TEXT SHOULD NOT REACH SYNTHESIS",
	}); err != nil {
		t.Fatalf("Save claim: %v", err)
	}
	prov := &askProvider{responses: []*agentkit.RoundTrip{
		textRoundTrip(`{"subjects":["Ada"]}`),
		textRoundTrip(`{
			"found": true,
			"text": "Ada approved the release.",
			"citations": [{"subject":"subject-ada","title":"Ada"}]
		}`),
	}}

	got, err := New(wiki.NewSubjectStore(conn), wiki.NewPageStore(conn), llm.New(prov, nil), testExtractSite(), testSynthSite()).
		Ask(ctx, "owner@example.com", "Who approved the release?")
	if err != nil {
		t.Fatalf("Ask returned error: %v", err)
	}
	if !got.Found || got.Text != "Ada approved the release." {
		t.Fatalf("Ask = %+v, want page-grounded answer", got)
	}
	for i, req := range prov.requests {
		if len(req.Tools) != 0 {
			t.Fatalf("request %d tools = %#v, want tool-less ask pipeline", i, req.Tools)
		}
	}
	synthText := requestText(prov.requests[1])
	if !strings.Contains(synthText, "Compiled page body says Ada approved the release.") {
		t.Fatalf("synth prompt = %q, want compiled page body", synthText)
	}
	for _, forbidden := range []string{"RAW CLAIM TEXT SHOULD NOT REACH SYNTHESIS", "job-secret", "read_source"} {
		if strings.Contains(synthText, forbidden) {
			t.Fatalf("synth prompt = %q, want no %q", synthText, forbidden)
		}
	}
}

func TestAskRejectsUngroundedSynthesisCitations(t *testing.T) {
	// R-5VXA-9NDZ
	ctx := context.Background()
	conn := migratedDB(t, ctx)
	defer conn.Close()
	savePage(t, ctx, conn, wiki.Subject{ID: "subject-ada", Name: "Ada", Type: "entity"}, wiki.Page{
		ID:        "page-ada",
		SubjectID: "subject-ada",
		Title:     "Ada",
		Body:      "Ada wrote the note.",
	})
	prov := &askProvider{responses: []*agentkit.RoundTrip{
		textRoundTrip(`{"subjects":["Ada"]}`),
		textRoundTrip(`{
			"found": true,
			"text": "Grace wrote it.",
			"citations": [{"subject":"subject-grace","title":"Grace"}]
		}`),
	}}

	_, err := New(wiki.NewSubjectStore(conn), wiki.NewPageStore(conn), llm.New(prov, nil), testExtractSite(), testSynthSite()).
		Ask(ctx, "owner@example.com", "Who wrote the note?")
	if err == nil || !strings.Contains(err.Error(), "citation not in gathered pages") {
		t.Fatalf("Ask error = %v, want ungrounded citation error", err)
	}
}

func migratedDB(t *testing.T, ctx context.Context) *sql.DB {
	t.Helper()

	conn, err := db.Open(t.TempDir() + "/wiki.db")
	if err != nil {
		t.Fatalf("Open: %v", err)
	}
	if err := db.Migrate(ctx, conn); err != nil {
		conn.Close()
		t.Fatalf("Migrate: %v", err)
	}
	return conn
}

func savePage(t *testing.T, ctx context.Context, conn *sql.DB, subject wiki.Subject, page wiki.Page) {
	t.Helper()

	if err := wiki.NewSubjectStore(conn).Save(ctx, subject); err != nil {
		t.Fatalf("Save subject %s: %v", subject.ID, err)
	}
	if err := wiki.NewPageStore(conn).Upsert(ctx, page); err != nil {
		t.Fatalf("Upsert page %s: %v", page.ID, err)
	}
}

func testExtractSite() llm.CallSite {
	return llm.CallSite{Model: "extract-model"}
}

func testSynthSite() llm.CallSite {
	return llm.CallSite{Model: "synth-model"}
}

type askProvider struct {
	responses []*agentkit.RoundTrip
	requests  []agentkit.Request
}

func (p *askProvider) RoundTrip(_ context.Context, req *agentkit.Request) *agentkit.RoundTrip {
	p.requests = append(p.requests, cloneRequest(req))
	if len(p.responses) == 0 {
		return textRoundTrip(`{"found":false}`)
	}
	rt := p.responses[0]
	p.responses = p.responses[1:]
	return rt
}

func (p *askProvider) Name() string {
	return "ask-scripted"
}

func (p *askProvider) Pricing(string) (agentkit.Pricing, bool) {
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

func textRoundTrip(text string) *agentkit.RoundTrip {
	return agentkit.NewRoundTrip(agentkit.Message{
		Role:   agentkit.RoleAssistant,
		Blocks: []agentkit.Block{agentkit.TextBlock{Text: text}},
	}, agentkit.FinishStop, agentkit.Usage{InputUncached: 1, Output: 1, Total: 2}, nil, nil)
}

func requestText(req agentkit.Request) string {
	var b strings.Builder
	for _, msg := range req.Messages {
		for _, block := range msg.Blocks {
			if text, ok := block.(agentkit.TextBlock); ok {
				b.WriteString(text.Text)
			}
		}
	}
	return b.String()
}

func TestAskParsesDecoratedJSONResponses(t *testing.T) {
	ctx := context.Background()
	conn := migratedDB(t, ctx)
	defer conn.Close()
	savePage(t, ctx, conn, wiki.Subject{ID: "subject-ada", Name: "Ada", Type: "entity"}, wiki.Page{
		ID:        "page-ada",
		SubjectID: "subject-ada",
		Title:     "Ada",
		Body:      "Ada wrote the note.",
	})
	answer, _ := json.Marshal(answerResult{
		Found:     true,
		Text:      "Ada wrote the note.",
		Citations: []answerCitation{{Subject: "subject-ada", Title: "Ada"}},
	})
	prov := &askProvider{responses: []*agentkit.RoundTrip{
		textRoundTrip("```json\n{\"subjects\":[\"Ada\"]}\n```"),
		textRoundTrip("Here is the answer:\n" + string(answer)),
	}}

	got, err := New(wiki.NewSubjectStore(conn), wiki.NewPageStore(conn), llm.New(prov, nil), testExtractSite(), testSynthSite()).
		Ask(ctx, "owner@example.com", "Who wrote the note?")
	if err != nil {
		t.Fatalf("Ask returned error: %v", err)
	}
	if !got.Found || got.Text != "Ada wrote the note." {
		t.Fatalf("Ask = %+v, want found answer from decorated JSON", got)
	}
}
