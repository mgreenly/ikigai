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
	agentkit "github.com/ikigenba/agentkit"

	"wiki/internal/ask"
	"wiki/internal/compile"
	"wiki/internal/db"
	"wiki/internal/extract"
	"wiki/internal/llm"
	"wiki/internal/mcp"
	"wiki/internal/page"
	"wiki/internal/retrieve"
	"wiki/internal/web"
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
		vectorCache := retrieve.NewVectorCache()
		cacheEntries, err := wiki.LoadVectorCacheEntries(context.Background(), conns)
		if err != nil {
			return err
		}
		vectorCache.Replace(retrieveCacheEntries(cacheEntries))
		embedder := &agentkit.Embedder{
			Provider:   cfg.EmbedSite.Provider,
			Model:      cfg.EmbedSite.Model,
			Dimensions: cfg.EmbedSite.Dims,
		}
		callRecorder := wiki.NewLLMCallStore(conns)
		pageEmbedder, queryEmbedding := recordingEmbedders(embedder, callRecorder, cfg.EmbedSite)
		extractor := extract.New(llmClient, cfg.CallSites.Extract)
		compiler := buildCompiler(cfg, llmClient)
		svc = wiki.NewService(conns, extractor, compiler, time.Now,
			wiki.WithPageEmbedder(cfg.EmbedSite.Model, pageEmbedder),
			wiki.WithVectorCacheUpdater(func(subjectID, title string, vec []float32) {
				vectorCache.Upsert(retrieve.VectorEntry{SubjectID: subjectID, Title: title, Vec: vec})
			}),
			wiki.WithVectorCacheRemover(vectorCache.Remove),
		)
		search := retrieve.NewHybridRetriever(
			retrieve.NewKeywordRetriever(read),
			retrieve.NewVectorRetriever(queryEmbedder(queryEmbedding), vectorCache),
			wiki.NewResolver(read),
			wiki.NewPageStore(read),
			retrieve.FusionConfig{FinalK: cfg.SearchDefault},
		)
		asker := ask.New(
			search,
			wiki.NewSubjectStore(read),
			wiki.NewPageStore(read),
			llmClient,
			cfg.CallSites.AskSubject,
			cfg.CallSites.AskSynthesis,
		)
		pageService := pathPageService{
			resolver: wiki.NewResolver(read),
			service:  svc,
		}
		subjectService := publicSubjectService{
			subjects: wiki.NewSubjectStore(read),
			pages:    wiki.NewPageStore(read),
		}
		claimService := pathClaimService{
			resolver: wiki.NewResolver(read),
			claims:   wiki.NewClaimStore(read),
		}
		mergeResolver := mergePathResolver{subjects: wiki.NewSubjectStore(read)}
		jobs := wiki.NewJobStore(conns)
		aliases := wiki.NewAliasStore(read)
		statusService := publicStatusService{service: svc}
		rt.Handle("/", web.NewHandler(rt.Service(), rt.Version(), wiki.Mount,
			web.WithOrphanLister(orphanAdapter{svc: svc}),
			web.WithAsker(asker),
			web.WithMentioner(mentionAdapter{svc: svc}),
			web.WithPageFinder(pageService),
		))
		rt.Handle("POST /mcp", rt.RequireIdentity(
			mcp.NewHandler(rt.Version(), rt.Service(), rt.Health(),
				mcp.WithIngestService(svc),
				mcp.WithJobStatusService(statusService),
				mcp.WithJobAbortService(svc),
				mcp.WithJobRerunService(svc),
				mcp.WithJobListService(jobListService{jobs: jobs}),
				mcp.WithJobsCountService(jobCountService{jobs: jobs}),
				mcp.WithMergeService(mergeResolver, svc),
				mcp.WithMergeListService(aliases),
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

func recordingEmbedders(inner wiki.PageEmbedder, recorder llm.Recorder, site wiki.EmbedSite) (page, query wiki.PageEmbedder) {
	return llm.NewRecordingEmbedder(inner, recorder, "embed-page", site.Provider, site.Model, site.Dims),
		llm.NewRecordingEmbedder(inner, recorder, "embed-query", site.Provider, site.Model, site.Dims)
}

func retrieveCacheEntries(entries []wiki.VectorCacheEntry) []retrieve.VectorEntry {
	out := make([]retrieve.VectorEntry, 0, len(entries))
	for _, entry := range entries {
		out = append(out, retrieve.VectorEntry{
			SubjectID: entry.SubjectID,
			Title:     entry.Title,
			Vec:       entry.Vec,
		})
	}
	return out
}

func queryEmbedder(embedder wiki.PageEmbedder) func(context.Context, string) ([]float32, error) {
	return func(ctx context.Context, text string) ([]float32, error) {
		result, err := embedder.Embed(ctx, []string{text}, agentkit.InputQuery)
		if err != nil {
			return nil, err
		}
		if result == nil || len(result.Vectors) != 1 {
			return nil, fmt.Errorf("wiki: query embedder returned %d vectors, want 1", vectorCount(result))
		}
		return append([]float32(nil), result.Vectors[0]...), nil
	}
}

func vectorCount(result *agentkit.EmbedResult) int {
	if result == nil {
		return 0
	}
	return len(result.Vectors)
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
		Statuses: f.Statuses,
		Kinds:    f.Kinds,
		Since:    f.Since,
		Until:    f.Until,
	}, p)
}

type jobCountService struct {
	jobs *wiki.JobStore
}

func (s jobCountService) CountJobs(ctx context.Context, f mcp.JobFilter) (int, error) {
	return s.jobs.CountJobs(ctx, wiki.JobFilter{
		Statuses: f.Statuses,
		Kinds:    f.Kinds,
		Since:    f.Since,
		Until:    f.Until,
	})
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

type mergePathResolver struct {
	subjects *wiki.SubjectStore
}

func (r mergePathResolver) GetByPath(ctx context.Context, path string) (wiki.Subject, error) {
	subject, err := r.subjects.GetByPath(ctx, path)
	if errors.Is(err, wiki.ErrSubjectNotFound) {
		return wiki.Subject{}, sql.ErrNoRows
	}
	return subject, err
}

type orphanAdapter struct {
	svc *wiki.Service
}

func (a orphanAdapter) Orphans(ctx context.Context) ([]web.Ref, error) {
	subjects, err := a.svc.Orphans(ctx)
	if err != nil {
		return nil, err
	}
	refs := make([]web.Ref, 0, len(subjects))
	for _, subject := range subjects {
		refs = append(refs, web.Ref{
			Href: "subject/" + wiki.Path(subject),
			Name: subject.Name,
		})
	}
	return refs, nil
}

type mentionAdapter struct {
	svc *wiki.Service
}

func (a mentionAdapter) MentionsIn(ctx context.Context, text string) ([]web.Ref, error) {
	mentions, err := a.svc.MentionsIn(ctx, text)
	if err != nil {
		return nil, err
	}
	refs := make([]web.Ref, 0, len(mentions))
	for _, mention := range mentions {
		refs = append(refs, web.Ref{
			Href: "subject/" + mention.Path,
			Name: mention.Name,
		})
	}
	return refs, nil
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
	resolver *wiki.Resolver
	claims   *wiki.ClaimStore
}

func (s pathClaimService) ListBySubject(ctx context.Context, path string, p page.Params) ([]publicClaim, string, error) {
	subject, err := s.resolver.ResolveByPath(ctx, path)
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
	resolver *wiki.Resolver
	service  *wiki.Service
}

func (s pathPageService) PageByPath(ctx context.Context, path string) (web.SubjectView, error) {
	subject, err := s.resolver.ResolveByPath(ctx, path)
	if errors.Is(err, wiki.ErrSubjectNotFound) {
		return web.SubjectView{}, sql.ErrNoRows
	}
	if err != nil {
		return web.SubjectView{}, err
	}
	page, err := s.service.PageWithLinks(ctx, subject.ID)
	if err != nil {
		return web.SubjectView{}, err
	}
	page.Body = wiki.RenderFooter(page.Body, page.Mentions, page.MentionedBy)
	return web.SubjectView{
		Path:  wiki.Path(subject),
		Title: page.Title,
		Body:  page.Body,
	}, nil
}

func buildCompiler(cfg wiki.Config, c *llm.Client) *compile.Compiler {
	return compile.New(c, cfg.CallSites.Compile, nil)
}

func serveCommand(args []string) bool {
	if len(args) == 0 {
		return true
	}
	return args[0] == "serve"
}
