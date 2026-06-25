package wiki

import (
	"context"
	"fmt"
	"testing"
	"time"

	"wiki/internal/page"
)

func TestLLMCallStoreAcceptsCurrentClosedStageSet(t *testing.T) {
	// R-EMWV-6RK5
	ctx := context.Background()
	conn := migratedDB(t, ctx)
	defer conn.Close()

	calls := NewLLMCallStore(conn)
	started := time.Date(2026, 6, 24, 20, 5, 0, 0, time.UTC)
	stages := []string{"extract", "compile", "ask-subject", "ask-synthesis", "judge", "embed-page", "embed-query"}
	for i, stage := range stages {
		rec := CallRecord{
			ID:        fmt.Sprintf("call-%d", i+1),
			Stage:     stage,
			JobID:     "job-allowed",
			Attempt:   1,
			Provider:  "test",
			Model:     "model",
			Params:    `{"temperature":0}`,
			Request:   `{"system":"sys","user":"prompt"}`,
			Response:  `{"ok":true}`,
			Usage:     `{"total":1}`,
			StartedAt: started.Add(time.Duration(i) * time.Second),
			EndedAt:   started.Add(time.Duration(i) * time.Second).Add(time.Millisecond),
		}
		if err := calls.Record(ctx, rec); err != nil {
			t.Fatalf("Record stage %q: %v", stage, err)
		}
	}

	rows, err := conn.QueryContext(ctx, `SELECT stage FROM llm_calls ORDER BY id`)
	if err != nil {
		t.Fatalf("query llm_calls stages: %v", err)
	}
	defer rows.Close()

	var got []string
	for rows.Next() {
		var stage string
		if err := rows.Scan(&stage); err != nil {
			t.Fatalf("scan stage: %v", err)
		}
		got = append(got, stage)
	}
	if err := rows.Err(); err != nil {
		t.Fatalf("iterate stages: %v", err)
	}
	if !sameStrings(got, stages) {
		t.Fatalf("persisted stages = %v, want %v", got, stages)
	}
}

func TestLLMCallStoreAcceptsEmbeddingStages(t *testing.T) {
	// R-ZDYO-05Q2
	ctx := context.Background()
	conn := migratedDB(t, ctx)
	defer conn.Close()

	calls := NewLLMCallStore(conn)
	started := time.Date(2026, 6, 25, 1, 10, 0, 0, time.UTC)
	for i, stage := range []string{"embed-page", "embed-query"} {
		rec := CallRecord{
			ID:        fmt.Sprintf("embed-call-%d", i+1),
			Stage:     stage,
			JobID:     "job-embedding",
			Attempt:   1,
			Provider:  "openai",
			Model:     "text-embedding-3-small",
			Params:    `{"dimensions":512}`,
			Request:   `{"inputs":["text"],"role":"document"}`,
			Response:  `{"vectors":1,"dims":512}`,
			Usage:     `{"InputTokens":3,"Total":3}`,
			StartedAt: started.Add(time.Duration(i) * time.Second),
			EndedAt:   started.Add(time.Duration(i) * time.Second).Add(time.Millisecond),
		}
		if err := calls.Record(ctx, rec); err != nil {
			t.Fatalf("Record stage %q: %v", stage, err)
		}
	}

	got, _, err := calls.List(ctx, LLMCallFilter{JobID: "job-embedding"}, page.Params{Limit: 10})
	if err != nil {
		t.Fatalf("List embedding calls: %v", err)
	}
	if len(got) != 2 || got[0].Stage != "embed-page" || got[1].Stage != "embed-query" {
		t.Fatalf("embedding stages = %+v, want embed-page then embed-query", got)
	}
}

func TestLLMCallStoreRejectsRetiredAndUnknownStages(t *testing.T) {
	// R-EO4R-KJAU
	ctx := context.Background()
	conn := migratedDB(t, ctx)
	defer conn.Close()

	calls := NewLLMCallStore(conn)
	started := time.Date(2026, 6, 24, 20, 10, 0, 0, time.UTC)
	for i, stage := range []string{"ask", "bogus"} {
		id := fmt.Sprintf("rejected-%d", i+1)
		rec := CallRecord{
			ID:        id,
			Stage:     stage,
			JobID:     "job-rejected",
			Attempt:   1,
			Provider:  "test",
			Model:     "model",
			Params:    `{"temperature":0}`,
			Request:   `{"system":"sys","user":"prompt"}`,
			StartedAt: started.Add(time.Duration(i) * time.Second),
			EndedAt:   started.Add(time.Duration(i) * time.Second).Add(time.Millisecond),
		}
		if err := calls.Record(ctx, rec); err == nil {
			t.Fatalf("Record stage %q succeeded, want DB CHECK failure", stage)
		}

		var count int
		if err := conn.QueryRowContext(ctx, `SELECT COUNT(*) FROM llm_calls WHERE id = ?`, id).Scan(&count); err != nil {
			t.Fatalf("count rejected stage %q rows: %v", stage, err)
		}
		if count != 0 {
			t.Fatalf("stage %q stored %d rows, want none", stage, count)
		}
	}
}
