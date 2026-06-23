package wiki_test

import (
	"context"
	"database/sql"
	"strings"
	"sync"
	"testing"
	"time"

	agentkit "github.com/ikigenba/agentkit"

	"wiki/internal/compile"
	"wiki/internal/extract"
	"wiki/internal/llm"
	wikidomain "wiki/internal/wiki"
	"wiki/internal/worker"
)

func TestIngestReturnsPendingThenWorkerCommitsPage(t *testing.T) {
	// R-M8RN-87WV
	// R-M9ZJ-LZNK
	// R-MB7F-ZRE9
	// R-MCFC-DJ4Y
	ctx := context.Background()
	conn := migratedWikiDB(t, ctx)
	defer conn.Close()

	prov := &scriptedProvider{responses: []string{
		`{"subjects":[{
			"type":"entity",
			"kind":"company",
			"name":"Acme Robotics",
			"occurred_at":"",
			"claims":["Acme Robotics opened a research lab in Tulsa."]
		}]}`,
		`{"title":"Acme Robotics","body":"Acme Robotics opened a research lab in Tulsa."}`,
	}}
	svc := scriptedService(conn, prov, time.Date(2026, 6, 20, 22, 0, 0, 0, time.UTC))

	jobID, err := svc.Ingest(ctx, " owner@example.com ", "Acme Robotics opened a research lab in Tulsa.", " Tulsa lab ", []string{"robotics"})
	if err != nil {
		t.Fatalf("Ingest: %v", err)
	}
	if jobID == "" {
		t.Fatal("jobID is empty, want durable job handle")
	}
	if got := len(prov.Requests()); got != 0 {
		t.Fatalf("provider requests before worker start = %d, want 0", got)
	}
	status, err := svc.JobStatus(ctx, jobID)
	if err != nil {
		t.Fatalf("JobStatus before worker: %v", err)
	}
	if status.Status != wikidomain.JobPending || status.StartedAt != nil || status.FinishedAt != nil {
		t.Fatalf("status before worker = %+v, want pending without worker timestamps", status)
	}

	stop := startWorker(t, ctx, svc)
	defer stop()

	status = waitJobStatus(t, ctx, svc, jobID, wikidomain.JobDone)
	if status.StartedAt == nil || status.FinishedAt == nil {
		t.Fatalf("status after worker = %+v, want started and finished timestamps", status)
	}
	if len(status.Subjects) != 1 {
		t.Fatalf("subjects = %#v, want one integrated subject", status.Subjects)
	}
	if got := len(prov.Requests()); got != 2 {
		t.Fatalf("provider requests = %d, want extract plus compile", got)
	}

	page, err := wikidomain.NewPageStore(conn).Get(ctx, status.Subjects[0])
	if err != nil {
		t.Fatalf("Get page: %v", err)
	}
	if page.Title != "Acme Robotics" || !strings.Contains(page.Body, "Tulsa") {
		t.Fatalf("page = %+v, want compiled Tulsa page", page)
	}
}

func TestWorkerReusesSubjectAndCompilesCompleteClaims(t *testing.T) {
	// R-MDN8-RAVN
	ctx := context.Background()
	conn := migratedWikiDB(t, ctx)
	defer conn.Close()

	prov := &scriptedProvider{responses: []string{
		`{"subjects":[{
			"type":"entity",
			"kind":"company",
			"name":"Acme Robotics",
			"occurred_at":"",
			"claims":["Acme Robotics opened a Tulsa lab."]
		}]}`,
		`{"title":"Acme Robotics","body":"Acme Robotics opened a Tulsa lab."}`,
		`{"subjects":[{
			"type":"entity",
			"kind":"company",
			"name":" ACME   ROBOTICS ",
			"occurred_at":"",
			"claims":["Acme Robotics hired Mira Patel."]
		}]}`,
		`{"title":"Acme Robotics","body":"Acme Robotics opened a Tulsa lab.\nAcme Robotics hired Mira Patel."}`,
	}}
	svc := scriptedService(conn, prov, time.Date(2026, 6, 20, 22, 5, 0, 0, time.UTC))
	stop := startWorker(t, ctx, svc)
	defer stop()

	firstID, err := svc.Ingest(ctx, "owner@example.com", "Acme Robotics opened a Tulsa lab.", "One", nil)
	if err != nil {
		t.Fatalf("first Ingest: %v", err)
	}
	first := waitJobStatus(t, ctx, svc, firstID, wikidomain.JobDone)
	if len(first.Subjects) != 1 {
		t.Fatalf("first subjects = %#v, want one subject", first.Subjects)
	}

	secondID, err := svc.Ingest(ctx, "owner@example.com", "Acme Robotics hired Mira Patel.", "Two", nil)
	if err != nil {
		t.Fatalf("second Ingest: %v", err)
	}
	second := waitJobStatus(t, ctx, svc, secondID, wikidomain.JobDone)
	if len(second.Subjects) != 1 {
		t.Fatalf("second subjects = %#v, want one subject", second.Subjects)
	}
	if second.Subjects[0] != first.Subjects[0] {
		t.Fatalf("second subject = %q, want reused subject %q", second.Subjects[0], first.Subjects[0])
	}

	requests := prov.Requests()
	if len(requests) != 4 {
		t.Fatalf("provider requests = %d, want two extract and two compile calls", len(requests))
	}
	secondCompilePrompt := requestText(requests[3])
	if !strings.Contains(secondCompilePrompt, "Acme Robotics opened a Tulsa lab.") ||
		!strings.Contains(secondCompilePrompt, "Acme Robotics hired Mira Patel.") {
		t.Fatalf("second compile prompt = %q, want complete claim set", secondCompilePrompt)
	}

	page, err := wikidomain.NewPageStore(conn).Get(ctx, first.Subjects[0])
	if err != nil {
		t.Fatalf("Get page: %v", err)
	}
	if !strings.Contains(page.Body, "opened a Tulsa lab") || !strings.Contains(page.Body, "hired Mira Patel") {
		t.Fatalf("page body = %q, want recompiled page with both claims", page.Body)
	}
}

func TestWorkerRecordsFailedExtractStatus(t *testing.T) {
	// R-MG31-IUD1
	ctx := context.Background()
	conn := migratedWikiDB(t, ctx)
	defer conn.Close()

	prov := &scriptedProvider{responses: []string{
		`{"subjects":[{
			"type":"entity",
			"kind":"company",
			"name":"",
			"occurred_at":"",
			"claims":["Acme Robotics opened a Tulsa lab."]
		}]}`,
	}}
	svc := scriptedService(conn, prov, time.Date(2026, 6, 20, 22, 10, 0, 0, time.UTC))
	stop := startWorker(t, ctx, svc)
	defer stop()

	jobID, err := svc.Ingest(ctx, "owner@example.com", "bad source", "Bad source", nil)
	if err != nil {
		t.Fatalf("Ingest: %v", err)
	}
	status := waitJobStatus(t, ctx, svc, jobID, wikidomain.JobFailed)
	if status.StartedAt == nil || status.FinishedAt == nil {
		t.Fatalf("status = %+v, want started and finished timestamps", status)
	}
	if !strings.Contains(status.Error, "name required") {
		t.Fatalf("error = %q, want extract validation failure", status.Error)
	}
	if len(status.Subjects) != 0 {
		t.Fatalf("subjects = %#v, want none for failed extract", status.Subjects)
	}
	if got := len(prov.Requests()); got != 1 {
		t.Fatalf("provider requests = %d, want one failed extract call", got)
	}
}

func scriptedService(conn *sql.DB, prov *scriptedProvider, now time.Time) *wikidomain.Service {
	client := llm.New(prov, nil)
	return wikidomain.NewService(
		conn,
		extract.New(client, llm.CallSite{Model: "extract-model"}),
		compile.New(client, llm.CallSite{Model: "compile-model"}, nil),
		func() time.Time { return now },
	)
}

func startWorker(t *testing.T, ctx context.Context, svc *wikidomain.Service) func() {
	t.Helper()

	runCtx, cancel := context.WithCancel(ctx)
	done := make(chan error, 1)
	go func() { done <- worker.Run(runCtx, svc) }()

	return func() {
		cancel()
		select {
		case err := <-done:
			if err != nil {
				t.Fatalf("worker.Run returned error: %v", err)
			}
		case <-time.After(2 * time.Second):
			t.Fatal("worker.Run did not stop after context cancellation")
		}
	}
}

func waitJobStatus(t *testing.T, ctx context.Context, svc *wikidomain.Service, jobID, want string) wikidomain.JobStatus {
	t.Helper()

	deadline := time.Now().Add(3 * time.Second)
	var last wikidomain.JobStatus
	for time.Now().Before(deadline) {
		status, err := svc.JobStatus(ctx, jobID)
		if err != nil {
			t.Fatalf("JobStatus: %v", err)
		}
		last = status
		if status.Status == want {
			return status
		}
		time.Sleep(10 * time.Millisecond)
	}
	t.Fatalf("job status = %+v, want %s", last, want)
	return wikidomain.JobStatus{}
}

// scriptedProvider returns a queued list of responses in order, recording each
// request it receives so tests can assert on call counts and prompt contents.
type scriptedProvider struct {
	mu        sync.Mutex
	responses []string
	requests  []agentkit.Request
}

func (p *scriptedProvider) RoundTrip(_ context.Context, req *agentkit.Request) *agentkit.RoundTrip {
	p.mu.Lock()
	p.requests = append(p.requests, cloneAgentKitRequest(req))
	text := `{"subjects":[]}`
	if len(p.responses) > 0 {
		text = p.responses[0]
		p.responses = p.responses[1:]
	}
	p.mu.Unlock()

	return agentkit.NewRoundTrip(
		agentkit.Message{Role: agentkit.RoleAssistant, Blocks: []agentkit.Block{agentkit.TextBlock{Text: text}}},
		agentkit.FinishStop,
		agentkit.Usage{InputUncached: 1, Output: 1, Total: 2},
		nil,
		nil,
	)
}

func (p *scriptedProvider) Requests() []agentkit.Request {
	p.mu.Lock()
	defer p.mu.Unlock()
	return append([]agentkit.Request(nil), p.requests...)
}

func (p *scriptedProvider) Name() string {
	return "scripted"
}

func (p *scriptedProvider) Pricing(string) (agentkit.Pricing, bool) {
	return agentkit.Pricing{Tiers: []agentkit.RateTier{{MinInputTokens: 0}}}, true
}

func requestText(req agentkit.Request) string {
	var b strings.Builder
	for _, msg := range req.Messages {
		for _, block := range msg.Blocks {
			if text, ok := block.(agentkit.TextBlock); ok {
				b.WriteString(text.Text)
				b.WriteByte('\n')
			}
		}
	}
	return b.String()
}
