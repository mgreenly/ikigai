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
	"wiki/internal/extract"
	"wiki/internal/llm"
	"wiki/internal/mcp"
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
		db := rt.DB()
		llmClient := cfg.LLM.WithRecorder(wiki.NewLLMCallStore(db)).WithClock(time.Now)
		extractor := extract.New(llmClient, extract.DefaultCallSite(cfg.ModelID))
		compiler := buildCompiler(cfg, llmClient)
		svc = wiki.NewService(db, extractor, compiler, time.Now)
		asker := ask.New(
			wiki.NewSubjectStore(db),
			wiki.NewPageStore(db),
			llmClient,
			llm.CallSite{Stage: "ask", Model: cfg.ModelID},
			llm.CallSite{Stage: "ask", Model: cfg.ModelID},
		)
		pageService := pathPageService{
			subjects: wiki.NewSubjectStore(db),
			service:  svc,
		}
		subjectService := publicSubjectService{service: svc}
		claimService := pathClaimService{
			subjects: wiki.NewSubjectStore(db),
			service:  svc,
		}
		statusService := publicStatusService{service: svc}
		rt.Handle("POST /mcp", rt.RequireIdentity(
			mcp.NewHandler(rt.Version(), rt.Service(), rt.Health(),
				mcp.WithIngestService(svc),
				mcp.WithJobStatusService(statusService),
				mcp.WithSubjectsService(subjectService),
				mcp.WithClaimsService(claimService),
				mcp.WithPagePathService(pageService),
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

type publicSubjectService struct {
	service *wiki.Service
}

func (s publicSubjectService) Subjects(ctx context.Context, typ, nameContains string) ([]publicSubject, error) {
	subjects, err := s.service.Subjects(ctx, typ, nameContains)
	if err != nil {
		return nil, err
	}
	out := make([]publicSubject, 0, len(subjects))
	for _, subject := range subjects {
		_, err := s.service.PageBySubject(ctx, subject.ID)
		if err != nil && !errors.Is(err, sql.ErrNoRows) {
			return nil, err
		}
		out = append(out, publicSubject{
			Path:    wiki.Path(subject),
			Type:    subject.Type,
			Name:    subject.Name,
			HasPage: err == nil,
		})
	}
	return out, nil
}

type pathClaimService struct {
	subjects *wiki.SubjectStore
	service  *wiki.Service
}

func (s pathClaimService) ClaimsBySubject(ctx context.Context, path string) ([]publicClaim, error) {
	subject, err := s.subjects.GetByPath(ctx, path)
	if errors.Is(err, wiki.ErrSubjectNotFound) {
		return nil, sql.ErrNoRows
	}
	if err != nil {
		return nil, err
	}
	claims, err := s.service.ClaimsBySubject(ctx, subject.ID)
	if err != nil {
		return nil, err
	}
	out := make([]publicClaim, 0, len(claims))
	for _, claim := range claims {
		out = append(out, publicClaim{
			ID:   claim.ID,
			Text: claim.Body,
			Job:  claim.JobID,
		})
	}
	return out, nil
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
