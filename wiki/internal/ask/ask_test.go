package ask

import (
	"context"
	"encoding/json"
	"errors"
	"reflect"
	"strings"
	"testing"

	agentkit "github.com/ikigenba/agentkit"

	"wiki/internal/llm"
	"wiki/internal/retrieve"
)

func TestAskReturnsHonestEmptyWithoutLLMWhenSearchHasNoHits(t *testing.T) {
	// R-5THH-I3WL
	ctx := context.Background()
	prov := &askProvider{responses: []*agentkit.RoundTrip{textRoundTrip(`{"found":true}`)}}
	rs := retrieve.NewService(nil, &fakeRetriever{}, retrieve.SearchLimits{Default: 8, Cap: 8})

	got, err := New(rs, nil, nil, llm.New(prov, nil), llm.CallSite{Model: "ask-model"}, 0, 0).Ask(ctx, "owner-1", "what is missing?")
	if err != nil {
		t.Fatalf("Ask returned error: %v", err)
	}
	if got.Found || got.Text != honestEmptyText || len(got.Citations) != 0 || len(got.Sources) != 0 {
		t.Fatalf("Ask = %+v, want honest empty answer with no citations or sources", got)
	}
	if len(prov.requests) != 0 {
		t.Fatalf("provider requests = %d, want no LLM call when retrieval is empty", len(prov.requests))
	}
}

func TestAskSendsRetrievedContextAndRequiresGroundedCitation(t *testing.T) {
	// R-5UPD-VVNA
	ctx := context.Background()
	prov := &askProvider{responses: []*agentkit.RoundTrip{textRoundTrip(`{
		"found": true,
		"text": "Ada wrote the note.",
		"citations": [{"Subject":"subject-ada","Title":"Ada"}]
	}`)}}
	rs := retrieve.NewService(nil, &fakeRetriever{hits: []retrieve.Hit{{
		SubjectID: "subject-ada",
		PageID:    "page-ada",
		Title:     "Ada",
		Snippet:   "Ada wrote the note. [job-1]",
	}}}, retrieve.SearchLimits{Default: 8, Cap: 8})

	got, err := New(rs, nil, nil, llm.New(prov, nil), llm.CallSite{Model: "ask-model", System: "ask system"}, 0, 0).Ask(ctx, "owner-1", "who wrote the note?")
	if err != nil {
		t.Fatalf("Ask returned error: %v", err)
	}
	if !got.Found || got.Text != "Ada wrote the note." {
		t.Fatalf("Ask = %+v, want found answer text", got)
	}
	if want := []Citation{{Subject: "subject-ada", Title: "Ada"}}; !reflect.DeepEqual(got.Citations, want) {
		t.Fatalf("citations = %+v, want %+v", got.Citations, want)
	}
	if len(prov.requests) != 1 {
		t.Fatalf("provider requests = %d, want 1", len(prov.requests))
	}
	req := prov.requests[0]
	if req.Model != "ask-model" || req.System != "ask system" {
		t.Fatalf("request model/system = %q/%q, want ask-model/ask system", req.Model, req.System)
	}
	text := requestText(req)
	if !strings.Contains(text, "who wrote the note?") || !strings.Contains(text, "Ada wrote the note. [job-1]") {
		t.Fatalf("request text = %q, want question and retrieved context", text)
	}
}

func TestAskRejectsUngroundedCitations(t *testing.T) {
	// R-5VXA-9NDZ
	ctx := context.Background()
	prov := &askProvider{responses: []*agentkit.RoundTrip{textRoundTrip(`{
		"found": true,
		"text": "Grace wrote it.",
		"citations": [{"Subject":"subject-grace","Title":"Grace"}]
	}`)}}
	rs := retrieve.NewService(nil, &fakeRetriever{hits: []retrieve.Hit{{
		SubjectID: "subject-ada",
		Title:     "Ada",
		Snippet:   "Ada wrote the note.",
	}}}, retrieve.SearchLimits{Default: 8, Cap: 8})

	_, err := New(rs, nil, nil, llm.New(prov, nil), llm.CallSite{Model: "ask-model"}, 0, 0).Ask(ctx, "owner-1", "who wrote the note?")
	if err == nil || !strings.Contains(err.Error(), "citation not in retrieved context") {
		t.Fatalf("Ask error = %v, want ungrounded citation error", err)
	}
}

func TestAskExposesOnlyReadSourceToolAndReturnsReadSourceIDs(t *testing.T) {
	// R-5X56-NF4O
	ctx := context.Background()
	prov := &askProvider{responses: []*agentkit.RoundTrip{
		toolRoundTrip("toolu_read", "read_source", `{"job_id":"job-2"}`),
		textRoundTrip(`{
			"found": true,
			"text": "The source says Ada approved it.",
			"citations": [{"Subject":"subject-ada","Title":"Ada"}]
		}`),
	}}
	src := &fakeSourceReader{text: map[string]string{"job-2": "Ada approved it in the original note."}}
	rs := retrieve.NewService(nil, &fakeRetriever{hits: []retrieve.Hit{{
		SubjectID: "subject-ada",
		Title:     "Ada",
		Snippet:   "Ada approved it. [job-2]",
	}}}, retrieve.SearchLimits{Default: 8, Cap: 8})

	got, err := New(rs, nil, src, llm.New(prov, nil), llm.CallSite{Model: "ask-model"}, 2, 2).Ask(ctx, "owner-1", "who approved it?")
	if err != nil {
		t.Fatalf("Ask returned error: %v", err)
	}
	if !reflect.DeepEqual(got.Sources, []string{"job-2"}) {
		t.Fatalf("sources = %#v, want read source job id", got.Sources)
	}
	if !reflect.DeepEqual(src.calls, []sourceCall{{owner: "owner-1", jobID: "job-2"}}) {
		t.Fatalf("source calls = %+v, want owner-scoped job read", src.calls)
	}
	if len(prov.requests) != 2 {
		t.Fatalf("provider requests = %d, want tool turn and final turn", len(prov.requests))
	}
	if names := toolNames(prov.requests[0].Tools); !reflect.DeepEqual(names, []string{"read_source"}) {
		t.Fatalf("tools = %#v, want only read_source", names)
	}
}

func TestAskPropagatesRetrievalErrors(t *testing.T) {
	// R-5YD3-16VD
	ctx := context.Background()
	wantErr := errors.New("search failed")
	rs := retrieve.NewService(nil, &fakeRetriever{err: wantErr}, retrieve.SearchLimits{Default: 8, Cap: 8})

	_, err := New(rs, nil, nil, nil, llm.CallSite{}, 0, 0).Ask(ctx, "owner-1", "anything?")
	if !errors.Is(err, wantErr) {
		t.Fatalf("Ask error = %v, want retrieval error %v", err, wantErr)
	}
}

func TestAskParsesDecoratedFinalAnswerJSON(t *testing.T) {
	// R-7SXQ-B9AX
	cases := []struct {
		name string
		text string
	}{
		{
			name: "fenced",
			text: "```json\n" + answerText("Ada wrote the note.") + "\n```",
		},
		{
			name: "preamble and trailing commentary",
			text: "Here is the grounded answer:\n" + answerText("Ada wrote the note.") + "\nI used only the wiki context.",
		},
		{
			name: "stray leading backtick",
			text: "`" + answerText("Ada wrote the note."),
		},
	}

	for _, tc := range cases {
		t.Run(tc.name, func(t *testing.T) {
			ctx := context.Background()
			prov := &askProvider{responses: []*agentkit.RoundTrip{textRoundTrip(tc.text)}}
			rs := retrieve.NewService(nil, &fakeRetriever{hits: []retrieve.Hit{{
				SubjectID: "subject-ada",
				Title:     "Ada",
				Snippet:   "Ada wrote the note.",
			}}}, retrieve.SearchLimits{Default: 8, Cap: 8})

			got, err := New(rs, nil, nil, llm.New(prov, nil), llm.CallSite{Model: "ask-model"}, 0, 0).Ask(ctx, "owner-1", "who wrote the note?")
			if err != nil {
				t.Fatalf("Ask returned error: %v", err)
			}
			if !got.Found || got.Text != "Ada wrote the note." {
				t.Fatalf("Ask = %+v, want found answer text from decorated JSON", got)
			}
			if want := []Citation{{Subject: "subject-ada", Title: "Ada"}}; !reflect.DeepEqual(got.Citations, want) {
				t.Fatalf("citations = %+v, want %+v", got.Citations, want)
			}
		})
	}
}

func answerText(text string) string {
	return `{"found":true,"text":"` + text + `","citations":[{"Subject":"subject-ada","Title":"Ada"}]}`
}

type fakeRetriever struct {
	hits []retrieve.Hit
	err  error
}

func (r *fakeRetriever) Search(context.Context, string, int) ([]retrieve.Hit, error) {
	return append([]retrieve.Hit(nil), r.hits...), r.err
}

type sourceCall struct {
	owner string
	jobID string
}

type fakeSourceReader struct {
	text  map[string]string
	calls []sourceCall
}

func (r *fakeSourceReader) ReadSource(_ context.Context, owner, jobID string) (string, error) {
	r.calls = append(r.calls, sourceCall{owner: owner, jobID: jobID})
	return r.text[jobID], nil
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

func toolRoundTrip(id, name, input string) *agentkit.RoundTrip {
	return agentkit.NewRoundTrip(agentkit.Message{
		Role: agentkit.RoleAssistant,
		Blocks: []agentkit.Block{agentkit.ToolUseBlock{
			ID:    id,
			Name:  name,
			Input: json.RawMessage(input),
		}},
	}, agentkit.FinishToolUse, agentkit.Usage{InputUncached: 1, Output: 1, Total: 2}, nil, nil)
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

func toolNames(tools []agentkit.Tool) []string {
	out := make([]string, 0, len(tools))
	for _, tool := range tools {
		out = append(out, tool.Name())
	}
	return out
}
