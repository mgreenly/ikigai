// Command wiki is the loopback-only wiki MCP service behind nginx.
package main

import (
	"context"
	"fmt"
	"os"
	"time"

	"appkit"

	"wiki/internal/ask"
	"wiki/internal/compile"
	"wiki/internal/extract"
	"wiki/internal/llm"
	"wiki/internal/mcp"
	"wiki/internal/retrieve"
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
		compiler := compile.New(cfg.LLM, llm.CallSite{Model: cfg.ModelID}, nil)
		svc = wiki.NewService(db, extractor, compiler, time.Now)
		retriever := retrieve.NewService(db, retrieve.NewKeyword(db), retrieve.SearchLimits{
			Default: cfg.SearchDefault,
			Cap:     cfg.SearchCap,
		})
		asker := ask.New(retriever, wiki.NewPageStore(db), nil, cfg.LLM, llm.CallSite{Model: cfg.ModelID}, 0, 0)
		rt.Handle("POST /mcp", rt.RequireIdentity(
			mcp.NewHandler(rt.Version(), rt.Service(), rt.Health(),
				mcp.WithIngestService(svc),
				mcp.WithJobStatusService(svc),
				mcp.WithSubjectsService(svc),
				mcp.WithClaimsService(svc),
				mcp.WithPageService(svc),
				mcp.WithAskFunc(asker.Ask),
			)))
		return nil
	}
	spec.Workers = []func(ctx context.Context) error{
		func(ctx context.Context) error { return worker.Run(ctx, svc) },
	}
	return spec
}

func serveCommand(args []string) bool {
	if len(args) == 0 {
		return true
	}
	return args[0] == "serve"
}
