// Command wiki is the loopback-only wiki MCP service behind nginx.
package main

import (
	"context"
	"database/sql"
	"errors"
	"fmt"
	"os"
	"time"

	"appkit"

	"wiki/internal/ask"
	"wiki/internal/compile"
	"wiki/internal/db"
	"wiki/internal/extract"
	"wiki/internal/llm"
	"wiki/internal/mcp"
	"wiki/internal/page"
	"wiki/internal/wiki"
	"wiki/internal/worker"
)

func main() {
	spec := wiki.Spec()
	if serveCommand(os.Args[1:]) {
		cfg, err := wiki.NewConfig(os.Getenv)
		if err != nil {
			fmt.Fprintf(os.Stderr, "wiki: %v\n", err)
			os.Exit(1)
		}
		spec = buildSpec(cfg)
	}
	appkit.Main(spec)
}

func buildSpec(cfg wiki.Config) appkit.Spec {
	spec := wiki.Spec()
	var svc *wiki.Service
	spec.Handlers = func(rt *appkit.Router) error {
		if rt.DB() == nil {
			return fmt.Errorf("wiki: no DB handle on router")
		}
		write := rt.DB()
		dbPath, err := db.Path(context.Background(), write)
		if err != nil {
			return err
		}
		read, err := db.OpenRead(dbPath)
		if err != nil {
			return err
		}
		conns := wiki.Conns{Read: read, Write: write}
		llmClient := cfg.LLM.WithRecorder(wiki.NewLLMCallStore(conns)).WithClock(time.Now)
		extractor := extract.New(llmClient, extract.DefaultCallSite(cfg.ModelID))
		compiler := buildCompiler(cfg, llmClient)
		svc = wiki.NewService(conns, extractor, compiler, time.Now)
		asker := ask.New(
			wiki.NewSubjectStore(read),
			wiki.NewPageStore(read),
			llmClient,
			llm.CallSite{Stage: "ask", Model: cfg.ModelID},
			llm.CallSite{Stage: "ask", Model: cfg.ModelID},
		)
		pageService := pathPageService{
			subjects: wiki.NewSubjectStore(read),
			service:  svc,
		}
		subjectService := publicSubjectService{
			subjects: wiki.NewSubjectStore(read),
			pages:    wiki.NewPageStore(read),
		}
		claimService := pathClaimService{
			subjects: wiki.NewSubjectStore(read),
			claims:   wiki.NewClaimStore(read),
		}
		statusService := publicStatusService{service: svc}
		rt.Handle("POST /mcp", rt.RequireIdentity(
			mcp.NewHandler(rt.Version(), rt.Service(), rt.Health(),
				mcp.WithIngestService(svc),
				mcp.WithJobStatusService(statusService),
				mcp.WithJobAbortService(svc),
				mcp.WithJobRerunService(svc),
				mcp.WithJobListService(jobListService{jobs: wiki.NewJobStore(conns)}),
				mcp.WithSubjectListService(subjectService),
				mcp.WithClaimListService(claimService),
				mcp.WithPagePathService(pageService),
				mcp.WithLLMCallListService(llmCallListService{calls: wiki.NewLLMCallStore(conns)}),
				mcp.WithAskFunc(asker.Ask),
			)))
		return nil
	}
	spec.Workers = []func(ctx context.Context) error{
		func(ctx context.Context) error { return worker.Run(ctx, svc) },
	}
	return spec
}

type publicSubject struct {
	Path    string
	Type    string
	Name    string
	HasPage bool
}

type publicClaim struct {
	ID   string
	Text string
	Job  string
}

type publicPage struct {
	Path  string
	Title string
	Body  string
}

type publicJobStatus struct {
	ID         string
	Status     string
	ReceivedAt time.Time
	StartedAt  *time.Time
	FinishedAt *time.Time
	Error      string
	Subjects   []string
}

type jobListService struct {
	jobs *wiki.JobStore
}

func (s jobListService) ListJobs(ctx context.Context, f mcp.JobFilter, p page.Params) ([]wiki.Job, string, error) {
	return s.jobs.ListJobs(ctx, wiki.JobFilter{
		Status: f.Status,
		Since:  f.Since,
		Until:  f.Until,
	}, p)
}

type llmCallListService struct {
	calls *wiki.LLMCallStore
}

func (s llmCallListService) List(ctx context.Context, f mcp.LLMCallFilter, p page.Params) ([]wiki.CallRecord, string, error) {
	return s.calls.List(ctx, wiki.LLMCallFilter{
		JobID: f.JobID,
		Stage: f.Stage,
		Since: f.Since,
		Until: f.Until,
	}, p)
}

type publicSubjectService struct {
	subjects *wiki.SubjectStore
	pages    *wiki.PageStore
}

func (s publicSubjectService) List(ctx context.Context, typ, nameContains string, p page.Params) ([]publicSubject, string, error) {
	subjects, next, err := s.subjects.List(ctx, typ, nameContains, p)
	if err != nil {
		return nil, "", err
	}
	out := make([]publicSubject, 0, len(subjects))
	for _, subject := range subjects {
		_, err := s.pages.GetBySubject(ctx, subject.ID)
		if err != nil && !errors.Is(err, sql.ErrNoRows) {
			return nil, "", err
		}
		out = append(out, publicSubject{
			Path:    wiki.Path(subject),
			Type:    subject.Type,
			Name:    subject.Name,
			HasPage: err == nil,
		})
	}
	return out, next, nil
}

type pathClaimService struct {
	subjects *wiki.SubjectStore
	claims   *wiki.ClaimStore
}

func (s pathClaimService) ListBySubject(ctx context.Context, path string, p page.Params) ([]publicClaim, string, error) {
	subject, err := s.subjects.GetByPath(ctx, path)
	if errors.Is(err, wiki.ErrSubjectNotFound) {
		return nil, "", sql.ErrNoRows
	}
	if err != nil {
		return nil, "", err
	}
	claims, next, err := s.claims.ListBySubject(ctx, subject.ID, p)
	if err != nil {
		return nil, "", err
	}
	out := make([]publicClaim, 0, len(claims))
	for _, claim := range claims {
		out = append(out, publicClaim{
			ID:   claim.ID,
			Text: claim.Body,
			Job:  claim.JobID,
		})
	}
	return out, next, nil
}

type publicStatusService struct {
	service *wiki.Service
}

func (s publicStatusService) JobStatus(ctx context.Context, jobID string) (publicJobStatus, error) {
	status, err := s.service.JobStatus(ctx, jobID)
	if err != nil {
		return publicJobStatus{}, err
	}
	subjects, err := s.service.Subjects(ctx, "", "")
	if err != nil {
		return publicJobStatus{}, err
	}
	paths := make(map[string]string, len(subjects))
	for _, subject := range subjects {
		paths[subject.ID] = wiki.Path(subject)
	}
	publicSubjects := make([]string, 0, len(status.Subjects))
	for _, subjectID := range status.Subjects {
		if path, ok := paths[subjectID]; ok {
			publicSubjects = append(publicSubjects, path)
		}
	}
	return publicJobStatus{
		ID:         status.ID,
		Status:     status.Status,
		ReceivedAt: status.ReceivedAt,
		StartedAt:  status.StartedAt,
		FinishedAt: status.FinishedAt,
		Error:      status.Error,
		Subjects:   publicSubjects,
	}, nil
}

type pathPageService struct {
	subjects *wiki.SubjectStore
	service  *wiki.Service
}

func (s pathPageService) PageByPath(ctx context.Context, path string) (publicPage, error) {
	subject, err := s.subjects.GetByPath(ctx, path)
	if errors.Is(err, wiki.ErrSubjectNotFound) {
		return publicPage{}, sql.ErrNoRows
	}
	if err != nil {
		return publicPage{}, err
	}
	page, err := s.service.PageWithLinks(ctx, subject.ID)
	if err != nil {
		return publicPage{}, err
	}
	page.Body = wiki.RenderFooter(page.Body, page.Mentions, page.MentionedBy)
	return publicPage{
		Path:  wiki.Path(subject),
		Title: page.Title,
		Body:  page.Body,
	}, nil
}

func buildCompiler(cfg wiki.Config, c *llm.Client) *compile.Compiler {
	return compile.New(c, compile.DefaultCallSite(cfg.ModelID), nil)
}

func serveCommand(args []string) bool {
	if len(args) == 0 {
		return true
	}
	return args[0] == "serve"
}
