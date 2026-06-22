package llm

import (
	"bytes"
	"context"
	"encoding/json"
	"errors"
	"strings"
	"testing"
	"time"

	agentkit "github.com/ikigenba/agentkit"
)

func TestJobIDContextCarriesTrimmedJobID(t *testing.T) {
	// R-VNS0-1Z85
	ctx := WithJobID(context.Background(), " job-123 ")

	if got := JobID(ctx); got != "job-123" {
		t.Fatalf("JobID() = %q, want trimmed job id", got)
	}
	if got := JobID(context.Background()); got != "" {
		t.Fatalf("JobID(background) = %q, want empty", got)
	}
}

func TestJSONNilRecorderIsNoOp(t *testing.T) {
	// R-VOZW-FQYU
	prov := &scriptedProvider{responses: []string{`{"title":"ok","count":2}`}}

	got, err := JSON(context.Background(), New(prov, nil), CallSite{Stage: "extract", Model: "json-model"}, "make json", nilJSONFixture)
	if err != nil {
		t.Fatalf("JSON returned error without recorder: %v", err)
	}
	if got.Title != "ok" || len(prov.requests) != 1 {
		t.Fatalf("JSON result = %#v, requests = %d; want successful provider call", got, len(prov.requests))
	}
}

func TestJSONRecordsSuccessfulProviderCallFootprint(t *testing.T) {
	// R-VRFP-7AG8
	temp := 0.25
	rec := &captureRecorder{}
	prov := &scriptedProvider{responses: []string{`{"title":"ok","count":2}`}}
	client := New(prov, nil, rec)
	client.now = sequenceTimes(
		time.Date(2026, 6, 22, 7, 0, 0, 0, time.UTC),
		time.Date(2026, 6, 22, 7, 0, 1, 0, time.UTC),
	)
	client.newID = sequenceLLMIDs("call-1")
	ctx := WithJobID(context.Background(), "job-1")

	got, err := JSON(ctx, client, CallSite{
		Stage:       "extract",
		Model:       "json-model",
		Temperature: &temp,
		Reasoning:   DisableReasoning(),
		System:      "system prompt",
	}, "make json", nilJSONFixture)
	if err != nil {
		t.Fatalf("JSON returned error: %v", err)
	}
	if got.Title != "ok" {
		t.Fatalf("JSON result = %#v, want parsed response", got)
	}
	if len(rec.records) != 1 {
		t.Fatalf("records len = %d, want 1", len(rec.records))
	}
	call := rec.records[0]
	if call.ID != "call-1" || call.Stage != "extract" || call.JobID != "job-1" || call.Attempt != 1 ||
		call.Provider != "scripted" || call.Model != "json-model" || call.Response != `{"title":"ok","count":2}` || call.Err != "" {
		t.Fatalf("record = %+v, want successful extract footprint", call)
	}
	assertJSONField(t, call.Params, "temperature", float64(temp))
	assertJSONField(t, call.Params, "reasoning", "disabled")
	assertJSONField(t, call.Request, "system", "system prompt")
	assertJSONField(t, call.Request, "user", "make json")
	assertJSONField(t, call.Usage, "Total", float64(2))
	if !call.StartedAt.Equal(time.Date(2026, 6, 22, 7, 0, 0, 0, time.UTC)) ||
		!call.EndedAt.Equal(time.Date(2026, 6, 22, 7, 0, 1, 0, time.UTC)) {
		t.Fatalf("record times = %v/%v, want fixed clock times", call.StartedAt, call.EndedAt)
	}
}

func TestJSONRecordsCorrectiveRetryAttempt(t *testing.T) {
	// R-VSNL-L26X
	rec := &captureRecorder{}
	prov := &scriptedProvider{responses: []string{
		`not-json`,
		`{"title":"fixed","count":4}`,
	}}
	client := New(prov, nil, rec)
	client.now = sequenceTimes(
		time.Date(2026, 6, 22, 7, 1, 0, 0, time.UTC),
		time.Date(2026, 6, 22, 7, 1, 1, 0, time.UTC),
		time.Date(2026, 6, 22, 7, 1, 2, 0, time.UTC),
		time.Date(2026, 6, 22, 7, 1, 3, 0, time.UTC),
	)
	client.newID = sequenceLLMIDs("call-1", "call-2")

	got, err := JSON(context.Background(), client, CallSite{Stage: "compile", Model: "json-model", MaxParseRetries: 1}, "make json", nilJSONFixture)
	if err != nil {
		t.Fatalf("JSON returned error: %v", err)
	}
	if got.Title != "fixed" {
		t.Fatalf("JSON result = %#v, want retry response", got)
	}
	if len(rec.records) != 2 {
		t.Fatalf("records len = %d, want initial plus retry", len(rec.records))
	}
	if rec.records[0].Attempt != 1 || rec.records[0].Request == rec.records[1].Request {
		t.Fatalf("records = %+v, want distinct attempt requests", rec.records)
	}
	second := rec.records[1]
	if second.ID != "call-2" || second.Stage != "compile" || second.Attempt != 2 {
		t.Fatalf("second record = %+v, want compile attempt 2", second)
	}
	var req callRequest
	if err := json.Unmarshal([]byte(second.Request), &req); err != nil {
		t.Fatalf("decode retry request: %v", err)
	}
	if !strings.Contains(req.User, "previous response") || !strings.Contains(req.User, "make json") {
		t.Fatalf("retry user prompt = %q, want corrective prompt with original request", req.User)
	}
}

func TestJSONRecordsTransportError(t *testing.T) {
	// R-VTVH-YTXM
	rec := &captureRecorder{}
	prov := &scriptedProvider{errs: []error{errors.New("network down")}}
	client := New(prov, nil, rec)
	client.now = sequenceTimes(
		time.Date(2026, 6, 22, 7, 2, 0, 0, time.UTC),
		time.Date(2026, 6, 22, 7, 2, 1, 0, time.UTC),
	)
	client.newID = sequenceLLMIDs("call-err")

	_, err := JSON[jsonFixture](context.Background(), client, CallSite{Stage: "ask", Model: "json-model"}, "make json", nilJSONFixture)
	if err == nil {
		t.Fatal("JSON returned nil error, want transport error")
	}
	if len(rec.records) != 1 {
		t.Fatalf("records len = %d, want 1 failed call", len(rec.records))
	}
	call := rec.records[0]
	if call.ID != "call-err" || call.Stage != "ask" || call.Attempt != 1 || call.Response != "" || !strings.Contains(call.Err, "network down") {
		t.Fatalf("failed record = %+v, want transport error footprint", call)
	}
}

func TestConverseBuildsFreshConfiguredConversation(t *testing.T) {
	var log bytes.Buffer
	prov := &scriptedProvider{}
	client := New(prov, &log)
	temp := 0.25
	site := CallSite{
		Model:       "model-a",
		Temperature: &temp,
		Reasoning:   agentkit.Level("low"),
		System:      "system prompt",
	}
	tool := agentkit.RawTool("lookup", "Lookup", json.RawMessage(`{"type":"object"}`), func(context.Context, json.RawMessage) (string, error) {
		return "ok", nil
	})

	first := client.Converse(site, []agentkit.Tool{tool})
	second := client.Converse(site, nil)

	if first == second {
		t.Fatal("Converse returned the same conversation twice")
	}
	if first.Provider != prov || first.Model != site.Model || first.System != site.System || first.Log != &log {
		t.Fatalf("conversation config = %#v, want shared provider, model, system, and log", first)
	}
	if first.Gen.Temperature == nil || *first.Gen.Temperature != temp {
		t.Fatalf("temperature = %v, want %v", first.Gen.Temperature, temp)
	}
	if level, ok := first.Gen.Reasoning.Level(); !ok || level != "low" {
		t.Fatalf("reasoning level = %q/%v, want low/true", level, ok)
	}
	if len(first.Tools) != 1 || first.Tools[0].Name() != "lookup" {
		t.Fatalf("tools = %#v, want lookup tool", first.Tools)
	}
	if len(second.Tools) != 0 || len(first.History) != 0 || len(second.History) != 0 {
		t.Fatalf("fresh conversations should start without unrelated tools/history: first=%#v second=%#v", first, second)
	}
}

func TestConverseSnapshotsToolSlice(t *testing.T) {
	// R-JDMA-UHS3
	client := New(&scriptedProvider{}, nil)
	lookup := agentkit.RawTool("lookup", "Lookup", json.RawMessage(`{"type":"object"}`), func(context.Context, json.RawMessage) (string, error) {
		return "lookup", nil
	})
	replace := agentkit.RawTool("replace", "Replace", json.RawMessage(`{"type":"object"}`), func(context.Context, json.RawMessage) (string, error) {
		return "replace", nil
	})

	tools := []agentkit.Tool{lookup}
	conv := client.Converse(CallSite{Model: "json-model"}, tools)
	tools[0] = replace

	if len(conv.Tools) != 1 {
		t.Fatalf("conversation tools len = %d, want 1", len(conv.Tools))
	}
	if conv.Tools[0].Name() != "lookup" {
		t.Fatalf("conversation tool = %q, want snapshot of original lookup tool", conv.Tools[0].Name())
	}
}

func TestExtractJSONFromFencesAndBareJSON(t *testing.T) {
	// R-J8QP-BETB
	tests := []struct {
		name string
		in   string
		want string
	}{
		{
			name: "json fence",
			in:   " \n```json\n{\"title\":\"fenced\",\"count\":3}\n```\n",
			want: `{"title":"fenced","count":3}`,
		},
		{
			name: "bare fence",
			in:   "```\n{\"title\":\"bare fence\",\"count\":4}\n```",
			want: `{"title":"bare fence","count":4}`,
		},
		{
			name: "inline json fence",
			in:   " ```JSON {\"title\":\"inline\",\"count\":6} ``` ",
			want: `{"title":"inline","count":6}`,
		},
		{
			name: "inline bare fence",
			in:   "```{\"title\":\"inline bare\",\"count\":7}```",
			want: `{"title":"inline bare","count":7}`,
		},
		{
			name: "bare json",
			in:   " \n{\"title\":\"bare\",\"count\":5}\n",
			want: `{"title":"bare","count":5}`,
		},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			got := ExtractJSON(tt.in)
			if got != tt.want {
				t.Fatalf("ExtractJSON() = %q, want %q", got, tt.want)
			}
			var decoded jsonFixture
			if err := json.Unmarshal([]byte(got), &decoded); err != nil {
				t.Fatalf("stripped response is not parseable JSON: %v", err)
			}
		})
	}
}

func TestExtractJSONFromDecoratedReplies(t *testing.T) {
	// R-4BCC-0EHJ
	tests := []struct {
		name string
		in   string
		want string
	}{
		{
			name: "prose preamble and trailing commentary",
			in:   "Here is the result:\n{\"title\":\"decorated\",\"count\":8}\nThat should do it.",
			want: `{"title":"decorated","count":8}`,
		},
		{
			name: "extra backtick fence",
			in:   "````json\n{\"title\":\"extra ticks\",\"count\":9}\n````",
			want: `{"title":"extra ticks","count":9}`,
		},
		{
			name: "stray leading backtick",
			in:   "`{\"title\":\"stray tick\",\"count\":10}",
			want: `{"title":"stray tick","count":10}`,
		},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			var unmodified jsonFixture
			if err := json.Unmarshal([]byte(tt.in), &unmodified); err == nil {
				t.Fatalf("unmodified reply parsed unexpectedly as %#v", unmodified)
			} else if !strings.Contains(err.Error(), "invalid character") {
				t.Fatalf("unmodified parse error = %v, want leading-character failure", err)
			}

			got := ExtractJSON(tt.in)
			if got != tt.want {
				t.Fatalf("ExtractJSON() = %q, want %q", got, tt.want)
			}
			var decoded jsonFixture
			if err := json.Unmarshal([]byte(got), &decoded); err != nil {
				t.Fatalf("carved response is not parseable JSON: %v", err)
			}
		})
	}
}

func TestJSONSendsToollessGenerationAndValidates(t *testing.T) {
	// R-J9YL-P6K0
	temp := 0.0
	prov := &scriptedProvider{responses: []string{"```json\n{\"title\":\"ok\",\"count\":2}\n```"}}
	site := CallSite{
		Model:       "json-model",
		Temperature: &temp,
		Reasoning:   agentkit.DisableReasoning(),
		System:      "json only",
	}
	validated := false
	got, err := JSON(context.Background(), New(prov, nil), site, "make json", func(v *jsonFixture) error {
		validated = true
		if v.Title == "" {
			return errors.New("title required")
		}
		return nil
	})
	if err != nil {
		t.Fatalf("JSON returned error: %v", err)
	}
	if got.Title != "ok" || got.Count != 2 {
		t.Fatalf("JSON result = %#v, want parsed response", got)
	}
	if !validated {
		t.Fatal("validate was not called")
	}
	if len(prov.requests) != 1 {
		t.Fatalf("requests len = %d, want 1", len(prov.requests))
	}
	req := prov.requests[0]
	if req.Model != site.Model || req.System != site.System || len(req.Tools) != 0 {
		t.Fatalf("request config = %#v, want model/system and no tools", req)
	}
	if req.Gen.Temperature == nil || *req.Gen.Temperature != temp || !req.Gen.Reasoning.Disabled() {
		t.Fatalf("gen settings = %#v, want pinned temperature and disabled reasoning", req.Gen)
	}
	if texts := requestTexts(req); len(texts) != 1 || texts[0] != "make json" {
		t.Fatalf("request texts = %#v, want original prompt only", texts)
	}
}

func TestJSONUsesRequestContextDeadline(t *testing.T) {
	// R-J9YL-P6K0
	prov := &scriptedProvider{responses: []string{`{"title":"deadline","count":3}`}}
	deadline := time.Now().Add(time.Minute).Round(0)
	ctx, cancel := context.WithDeadline(context.Background(), deadline)
	defer cancel()

	got, err := JSON(ctx, New(prov, nil), CallSite{Model: "json-model"}, "make json", nilJSONFixture)
	if err != nil {
		t.Fatalf("JSON returned error: %v", err)
	}
	if got.Title != "deadline" || got.Count != 3 {
		t.Fatalf("JSON result = %#v, want parsed response", got)
	}
	if len(prov.deadlines) != 1 {
		t.Fatalf("provider deadlines len = %d, want 1", len(prov.deadlines))
	}
	if !prov.deadlines[0].Equal(deadline) {
		t.Fatalf("provider deadline = %v, want request deadline %v", prov.deadlines[0], deadline)
	}
}

func TestJSONCarriesCallSiteConfiguration(t *testing.T) {
	// R-JEU7-89IS
	temp := 0.75
	prov := &scriptedProvider{responses: []string{`{"title":"configured","count":9}`}}
	site := CallSite{
		Model:       "configured-model",
		Temperature: &temp,
		Reasoning:   agentkit.Level("medium"),
		System:      "configured system",
	}

	got, err := JSON(context.Background(), New(prov, nil), site, "make configured json", nilJSONFixture)
	if err != nil {
		t.Fatalf("JSON returned error: %v", err)
	}
	if got.Title != "configured" || got.Count != 9 {
		t.Fatalf("JSON result = %#v, want configured response", got)
	}
	if len(prov.requests) != 1 {
		t.Fatalf("requests len = %d, want 1", len(prov.requests))
	}
	req := prov.requests[0]
	if req.Model != site.Model || req.System != site.System {
		t.Fatalf("request config = %#v, want callsite model and system", req)
	}
	if req.Gen.Temperature == nil || *req.Gen.Temperature != temp {
		t.Fatalf("temperature = %v, want %v", req.Gen.Temperature, temp)
	}
	if level, ok := req.Gen.Reasoning.Level(); !ok || level != "medium" {
		t.Fatalf("reasoning level = %q/%v, want medium/true", level, ok)
	}
}

func TestJSONRetriesWithCorrectivePromptOnValidationFailure(t *testing.T) {
	// R-JCEE-GQ1E
	prov := &scriptedProvider{responses: []string{
		`{"title":"","count":1}`,
		`{"title":"fixed","count":4}`,
	}}
	site := CallSite{Model: "json-model", MaxParseRetries: 1}

	got, err := JSON(context.Background(), New(prov, nil), site, "make validated json", func(v *jsonFixture) error {
		if v.Title == "" {
			return errors.New("title required")
		}
		return nil
	})
	if err != nil {
		t.Fatalf("JSON returned error: %v", err)
	}
	if got.Title != "fixed" || got.Count != 4 {
		t.Fatalf("JSON result = %#v, want retry response", got)
	}
	if len(prov.requests) != 2 {
		t.Fatalf("requests len = %d, want initial plus retry", len(prov.requests))
	}
	texts := requestTexts(prov.requests[1])
	if len(texts) != 3 {
		t.Fatalf("retry conversation texts = %#v, want original user, failed assistant, corrective user", texts)
	}
	corrective := texts[len(texts)-1]
	if !strings.Contains(corrective, "previous response") || !strings.Contains(corrective, "title required") || !strings.Contains(corrective, "make validated json") {
		t.Fatalf("corrective prompt = %q, want parse note, validation error, and original request", corrective)
	}
}

func TestJSONRetriesWithCorrectivePromptOnMalformedJSON(t *testing.T) {
	// R-JCEE-GQ1E
	temp := 0.0
	prov := &scriptedProvider{responses: []string{
		`not-json`,
		`{"title":"parsed","count":7}`,
	}}
	site := CallSite{
		Model:           "json-model",
		Temperature:     &temp,
		Reasoning:       agentkit.DisableReasoning(),
		System:          "json only",
		MaxParseRetries: 1,
	}

	got, err := JSON(context.Background(), New(prov, nil), site, "make parseable json", nilJSONFixture)
	if err != nil {
		t.Fatalf("JSON returned error: %v", err)
	}
	if got.Title != "parsed" || got.Count != 7 {
		t.Fatalf("JSON result = %#v, want retry response", got)
	}
	if len(prov.requests) != 2 {
		t.Fatalf("requests len = %d, want initial plus retry", len(prov.requests))
	}
	for i, req := range prov.requests {
		if req.Model != site.Model || req.System != site.System || len(req.Tools) != 0 {
			t.Fatalf("request %d config = %#v, want model/system and no tools", i, req)
		}
		if req.Gen.Temperature == nil || *req.Gen.Temperature != temp || !req.Gen.Reasoning.Disabled() {
			t.Fatalf("request %d gen settings = %#v, want pinned temperature and disabled reasoning", i, req.Gen)
		}
	}
	texts := requestTexts(prov.requests[1])
	if len(texts) != 3 {
		t.Fatalf("retry conversation texts = %#v, want original user, malformed assistant, corrective user", texts)
	}
	corrective := texts[len(texts)-1]
	if !strings.Contains(corrective, "previous response") || !strings.Contains(corrective, "invalid character") || !strings.Contains(corrective, "make parseable json") {
		t.Fatalf("corrective prompt = %q, want parse note, JSON error, and original request", corrective)
	}
}

func TestJSONReturnsErrorAfterRetryBudgetWithoutSilentZero(t *testing.T) {
	// R-JCEE-GQ1E
	prov := &scriptedProvider{responses: []string{`not-json`, `also-not-json`}}
	site := CallSite{Model: "json-model", MaxParseRetries: 1}

	got, err := JSON(context.Background(), New(prov, nil), site, "make json", nilJSONFixture)
	if err == nil {
		t.Fatal("JSON returned nil error after exhausting parse retries")
	}
	if got != (jsonFixture{}) {
		t.Fatalf("JSON result = %#v, want zero value on error", got)
	}
	if len(prov.requests) != 2 {
		t.Fatalf("requests len = %d, want initial plus one retry", len(prov.requests))
	}
	if !strings.Contains(err.Error(), "2 attempt") {
		t.Fatalf("error = %v, want retry count context", err)
	}
}

func TestJSONDoesNotRetryWhenRetryBudgetIsZero(t *testing.T) {
	// R-JCEE-GQ1E
	prov := &scriptedProvider{responses: []string{`not-json`, `{"title":"unused","count":9}`}}
	site := CallSite{Model: "json-model", MaxParseRetries: 0}

	got, err := JSON(context.Background(), New(prov, nil), site, "make json", nilJSONFixture)
	if err == nil {
		t.Fatal("JSON returned nil error with zero retries after malformed response")
	}
	if got != (jsonFixture{}) {
		t.Fatalf("JSON result = %#v, want zero value on parse error", got)
	}
	if len(prov.requests) != 1 {
		t.Fatalf("requests len = %d, want exactly one attempt with zero retry budget", len(prov.requests))
	}
}

func TestJSONBuildsFreshConversationForEachCall(t *testing.T) {
	// R-JDMA-UHS3
	prov := &scriptedProvider{responses: []string{
		`{"title":"first","count":1}`,
		`{"title":"second","count":2}`,
	}}
	client := New(prov, nil)
	site := CallSite{Model: "json-model"}

	first, err := JSON(context.Background(), client, site, "make first json", nilJSONFixture)
	if err != nil {
		t.Fatalf("first JSON returned error: %v", err)
	}
	second, err := JSON(context.Background(), client, site, "make second json", nilJSONFixture)
	if err != nil {
		t.Fatalf("second JSON returned error: %v", err)
	}
	if first.Title != "first" || second.Title != "second" {
		t.Fatalf("JSON results = %#v and %#v, want independent responses", first, second)
	}
	if len(prov.requests) != 2 {
		t.Fatalf("requests len = %d, want one provider call per JSON call", len(prov.requests))
	}
	firstTexts := requestTexts(prov.requests[0])
	secondTexts := requestTexts(prov.requests[1])
	if len(firstTexts) != 1 || firstTexts[0] != "make first json" {
		t.Fatalf("first request texts = %#v, want only first prompt", firstTexts)
	}
	if len(secondTexts) != 1 || secondTexts[0] != "make second json" {
		t.Fatalf("second request texts = %#v, want no accumulated history from first call", secondTexts)
	}
}

type jsonFixture struct {
	Title string `json:"title"`
	Count int    `json:"count"`
}

func nilJSONFixture(*jsonFixture) error {
	return nil
}

type scriptedProvider struct {
	responses []string
	errs      []error
	requests  []agentkit.Request
	deadlines []time.Time
}

func (p *scriptedProvider) RoundTrip(ctx context.Context, req *agentkit.Request) *agentkit.RoundTrip {
	p.requests = append(p.requests, cloneRequest(req))
	if deadline, ok := ctx.Deadline(); ok {
		p.deadlines = append(p.deadlines, deadline)
	}
	if len(p.errs) > 0 {
		err := p.errs[0]
		p.errs = p.errs[1:]
		return agentkit.NewRoundTrip(agentkit.Message{}, agentkit.FinishOther, agentkit.Usage{}, nil, err)
	}
	text := `{}`
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

type captureRecorder struct {
	records []CallRecord
}

func (r *captureRecorder) Record(_ context.Context, rec CallRecord) error {
	r.records = append(r.records, rec)
	return nil
}

func sequenceLLMIDs(ids ...string) func() string {
	i := 0
	return func() string {
		if i >= len(ids) {
			return ids[len(ids)-1]
		}
		id := ids[i]
		i++
		return id
	}
}

func sequenceTimes(times ...time.Time) func() time.Time {
	i := 0
	return func() time.Time {
		if i >= len(times) {
			return times[len(times)-1]
		}
		t := times[i]
		i++
		return t
	}
}

func assertJSONField(t *testing.T, raw, field string, want any) {
	t.Helper()

	var got map[string]any
	if err := json.Unmarshal([]byte(raw), &got); err != nil {
		t.Fatalf("decode %s: %v; raw=%q", field, err, raw)
	}
	if got[field] != want {
		t.Fatalf("%s[%q] = %#v, want %#v; raw=%s", field, field, got[field], want, raw)
	}
}
