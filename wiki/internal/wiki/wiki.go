// Package wiki wires the service skeleton into the shared appkit chassis.
package wiki

import (
	"context"
	"fmt"
	"os"
	"strconv"
	"time"

	"appkit"
	"github.com/ikigenba/agentkit/anthropic"

	"wiki/internal/db"
	"wiki/internal/mcp"
	"wiki/internal/worker"
)

const (
	App   = "wiki"
	Mount = "/srv/wiki/"
	Port  = 3006

	ModelID           = anthropic.ModelSonnet46
	WorkerConcurrency = 1
	SearchDefault     = 8
	SearchCap         = 32
)

// Spec returns the production-shaped appkit service declaration.
func Spec() appkit.Spec {
	return appkit.Spec{
		App:   App,
		Mount: Mount,
		Port:  Port,
		MCP:   true,
		ManifestExtras: []appkit.ManifestKV{
			{Key: "MODEL_ID", Value: ModelID},
			{Key: "WORKER_CONCURRENCY", Value: strconv.Itoa(WorkerConcurrency)},
			{Key: "SEARCH_DEFAULT", Value: strconv.Itoa(SearchDefault)},
			{Key: "SEARCH_CAP", Value: strconv.Itoa(SearchCap)},
		},
		Migrations: db.FS,
		Config: func(getenv func(string) string) (any, error) {
			return NewConfig(getenv)
		},
		Handlers: func(rt *appkit.Router) error {
			if rt.DB() == nil {
				return fmt.Errorf("wiki: no DB handle on router")
			}
			dbPath, err := db.Path(context.Background(), rt.DB())
			if err != nil {
				return err
			}
			read, err := db.OpenRead(dbPath)
			if err != nil {
				return err
			}
			svc := NewService(Conns{Read: read, Write: rt.DB()}, nil, nil, time.Now)
			rt.Handle("POST /mcp", rt.RequireIdentity(
				mcp.NewHandler(rt.Version(), rt.Service(), rt.Health(),
					mcp.WithIngestService(svc),
					mcp.WithJobStatusService(svc),
					mcp.WithSubjectsService(svc),
					mcp.WithClaimsService(svc),
					mcp.WithPageService(svc),
				)))
			return nil
		},
		Workers: []func(ctx context.Context) error{
			func(ctx context.Context) error { return worker.Run(ctx) },
		},
	}
}

// Main enters the shared appkit dispatcher.
func Main() {
	if serveCommand(os.Args[1:]) {
		if _, err := NewConfig(os.Getenv); err != nil {
			fmt.Fprintf(os.Stderr, "wiki: %v\n", err)
			os.Exit(1)
		}
	}
	appkit.Main(Spec())
}

func serveCommand(args []string) bool {
	if len(args) == 0 {
		return true
	}
	return args[0] == "serve"
}
