package llm

import (
	"context"
	"errors"
	"strings"
	"testing"
	"time"

	agentkit "github.com/ikigenba/agentkit"
)

func TestRecordingEmbedderRecordsSuccessfulPageEmbeddingFootprint(t *testing.T) {
	// R-ZBIV-8M8O
	rec := &captureRecorder{}
	prov := &scriptedEmbeddingProvider{
		vectors: [][]float32{{3, 4}},
		usage:   agentkit.EmbeddingUsage{InputTokens: 7, Total: 7},
	}
	embedder := &recordingEmbedder{
		inner:    &agentkit.Embedder{Provider: prov, Model: "embed-model", Dimensions: 512},
		recorder: rec,
		stage:    "embed-page",
		now: sequenceTimes(
			time.Date(2026, 6, 25, 1, 0, 0, 0, time.UTC),
			time.Date(2026, 6, 25, 1, 0, 1, 0, time.UTC),
		),
		newID: sequenceLLMIDs("embed-call-1"),
	}
	ctx := WithJobID(context.Background(), "job-embed")

	result, err := embedder.Embed(ctx, []string{"Page body"}, agentkit.InputDocument)
	if err != nil {
		t.Fatalf("Embed returned error: %v", err)
	}
	if len(result.Vectors) != 1 || len(result.Vectors[0]) != 2 {
		t.Fatalf("vectors = %#v, want one normalized two-dimensional vector", result.Vectors)
	}
	if len(rec.records) != 1 {
		t.Fatalf("records len = %d, want 1", len(rec.records))
	}
	call := rec.records[0]
	if call.ID != "embed-call-1" || call.Stage != "embed-page" || call.JobID != "job-embed" ||
		call.Attempt != 1 || call.Provider != "embedding-scripted" || call.Model != "embed-model" || call.Err != "" {
		t.Fatalf("record = %+v, want successful embed-page footprint", call)
	}
	assertJSONField(t, call.Params, "dimensions", float64(512))
	assertJSONField(t, call.Request, "role", "document")
	assertJSONField(t, call.Response, "vectors", float64(1))
	assertJSONField(t, call.Response, "dims", float64(2))
	assertJSONField(t, call.Usage, "InputTokens", float64(7))
	if !call.StartedAt.Equal(time.Date(2026, 6, 25, 1, 0, 0, 0, time.UTC)) ||
		!call.EndedAt.Equal(time.Date(2026, 6, 25, 1, 0, 1, 0, time.UTC)) {
		t.Fatalf("record times = %v/%v, want fixed embedding call times", call.StartedAt, call.EndedAt)
	}
	if len(prov.requests) != 1 || prov.requests[0].Model != "embed-model" ||
		prov.requests[0].Dimensions != 512 || prov.requests[0].Role != agentkit.InputDocument {
		t.Fatalf("provider request = %#v, want configured model, dims, and document role", prov.requests)
	}
}

func TestRecordingEmbedderRecordsQueryEmbeddingErrors(t *testing.T) {
	// R-ZCQR-MDZD
	rec := &captureRecorder{}
	prov := &scriptedEmbeddingProvider{err: errors.New("embedding transport down")}
	embedder := &recordingEmbedder{
		inner:    &agentkit.Embedder{Provider: prov, Model: "embed-model", Dimensions: 512},
		recorder: rec,
		stage:    "embed-query",
		now: sequenceTimes(
			time.Date(2026, 6, 25, 1, 5, 0, 0, time.UTC),
			time.Date(2026, 6, 25, 1, 5, 1, 0, time.UTC),
		),
		newID: sequenceLLMIDs("embed-call-err"),
	}

	_, err := embedder.Embed(context.Background(), []string{"search terms"}, agentkit.InputQuery)
	if err == nil {
		t.Fatal("Embed returned nil error, want provider error")
	}
	if len(rec.records) != 1 {
		t.Fatalf("records len = %d, want failed call record", len(rec.records))
	}
	call := rec.records[0]
	if call.ID != "embed-call-err" || call.Stage != "embed-query" || call.Response != "" || call.Usage != "" {
		t.Fatalf("failed record = %+v, want embed-query failure footprint without response usage", call)
	}
	if !strings.Contains(call.Err, "embedding transport down") {
		t.Fatalf("record err = %q, want provider error", call.Err)
	}
	assertJSONField(t, call.Request, "role", "query")
}

type scriptedEmbeddingProvider struct {
	vectors  [][]float32
	usage    agentkit.EmbeddingUsage
	err      error
	requests []agentkit.EmbedRequest
}

func (p *scriptedEmbeddingProvider) Embed(_ context.Context, req *agentkit.EmbedRequest) *agentkit.EmbedRoundTrip {
	if req != nil {
		p.requests = append(p.requests, *req)
	}
	return agentkit.NewEmbedRoundTrip(p.vectors, p.usage, nil, p.err)
}

func (p *scriptedEmbeddingProvider) Name() string {
	return "embedding-scripted"
}

func (p *scriptedEmbeddingProvider) Pricing(string) (agentkit.EmbeddingPricing, bool) {
	return agentkit.EmbeddingPricing{InputToken: 1}, true
}
