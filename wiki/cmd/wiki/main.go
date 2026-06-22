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
		extractor := extract.New(cfg.LLM, extract.DefaultCallSite(cfg.ModelID))
		compiler := buildCompiler(cfg)
		svc = wiki.NewService(db, extractor, compiler, time.Now)
		asker := ask.New(wiki.NewSubjectStore(db), wiki.NewPageStore(db), cfg.LLM, llm.CallSite{Model: cfg.ModelID}, llm.CallSite{Model: cfg.ModelID})
		pageService := pathPageService{
			subjects: wiki.NewSubjectStore(db),
			service:  svc,
		}
		rt.Handle("POST /mcp", rt.RequireIdentity(
			mcp.NewHandler(rt.Version(), rt.Service(), rt.Health(),
				mcp.WithIngestService(svc),
				mcp.WithJobStatusService(svc),
				mcp.WithSubjectsService(svc),
				mcp.WithClaimsService(svc),
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

type pathPageService struct {
	subjects *wiki.SubjectStore
	service  *wiki.Service
}

func (s pathPageService) PageByPath(ctx context.Context, path string) (wiki.Page, error) {
	subject, err := s.subjects.GetByPath(ctx, path)
	if errors.Is(err, wiki.ErrSubjectNotFound) {
		return wiki.Page{}, sql.ErrNoRows
	}
	if err != nil {
		return wiki.Page{}, err
	}
	page, err := s.service.PageWithLinks(ctx, subject.ID)
	if err != nil {
		return wiki.Page{}, err
	}
	page.Body = wiki.RenderFooter(page.Body, page.Mentions, page.MentionedBy)
	return page.Page, nil
}

func buildCompiler(cfg wiki.Config) *compile.Compiler {
	return compile.New(cfg.LLM, compile.DefaultCallSite(cfg.ModelID), nil)
}

func serveCommand(args []string) bool {
	if len(args) == 0 {
		return true
	}
	return args[0] == "serve"
}
