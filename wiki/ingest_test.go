package wiki_test

import (
	"context"
	"database/sql"
	"testing"
	"time"

	agentkit "github.com/ikigenba/agentkit"

	"wiki/internal/compile"
	"wiki/internal/db"
	"wiki/internal/extract"
	"wiki/internal/llm"
	wikidomain "wiki/internal/wiki"
)

func TestIngestSmokeReturnsPendingJobWithoutLLMCall(t *testing.T) {
	// R-M8RN-87WV
	ctx := context.Background()
	conn := migratedWikiDB(t, ctx)
	defer conn.Close()

	prov := &capturingProvider{}
	client := llm.New(prov, nil)
	fixed := time.Date(2026, 6, 20, 21, 7, 0, 0, time.UTC)
	svc := wikidomain.NewService(
		conn,
		extract.New(client, llm.CallSite{Model: "extract-model"}),
		compile.New(client, llm.CallSite{Model: "compile-model"}, nil),
		func() time.Time { return fixed },
	)

	jobID, err := svc.Ingest(ctx, " owner@example.com ", "Acme Robotics opened a Tulsa lab.", " Lab notes ", []string{"robotics"})
	if err != nil {
		t.Fatalf("Ingest returned error: %v", err)
	}
	if jobID == "" {
		t.Fatal("jobID is empty, want durable pending job id")
	}
	if len(prov.requests) != 0 {
		t.Fatalf("provider requests = %d, want 0 before worker processes the job", len(prov.requests))
	}

	status, err := svc.JobStatus(ctx, jobID)
	if err != nil {
		t.Fatalf("JobStatus: %v", err)
	}
	if status.Status != wikidomain.JobPending {
		t.Fatalf("status = %q, want pending", status.Status)
	}
	if !status.ReceivedAt.Equal(fixed) {
		t.Fatalf("received_at = %v, want %v", status.ReceivedAt, fixed)
	}
	if status.StartedAt != nil || status.FinishedAt != nil || len(status.Subjects) != 0 {
		t.Fatalf("status = %+v, want no worker fields or subjects before processing", status)
	}
}

func migratedWikiDB(t *testing.T, ctx context.Context) *sql.DB {
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

type capturingProvider struct {
	requests []agentkit.Request
}

func (p *capturingProvider) RoundTrip(_ context.Context, req *agentkit.Request) *agentkit.RoundTrip {
	p.requests = append(p.requests, cloneAgentKitRequest(req))
	return agentkit.NewRoundTrip(
		agentkit.Message{Role: agentkit.RoleAssistant, Blocks: []agentkit.Block{agentkit.TextBlock{Text: `{"subjects":[]}`}}},
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

func cloneAgentKitRequest(req *agentkit.Request) agentkit.Request {
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
