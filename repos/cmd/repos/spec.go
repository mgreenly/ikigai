package main

import (
	"context"
	"fmt"
	"net/http"
	"os"
	"path/filepath"
	"time"

	"appkit"
	"appkit/config"
	"appkit/web"
	"eventplane/consumer"
	"eventplane/outbox"
	"registry"

	reposdb "repos/internal/db"
	"repos/internal/mcp"
	"repos/internal/repos"
	"repos/internal/runner"
)

type wallClock struct{}

func (wallClock) Now() time.Time { return time.Now() }

type domainAdapter struct {
	lifecycle *repos.Service
	store     *repos.Store
	runner    *runner.Runner
	reaper    *repos.Reaper
}

func (d *domainAdapter) CloneRepo(ctx context.Context, owner, name string) error {
	return d.lifecycle.CloneRepo(ctx, owner, name)
}
func (d *domainAdapter) GetRepo(ctx context.Context, name string) (repos.Repo, error) {
	return d.store.GetRepo(ctx, name)
}
func (d *domainAdapter) ListRepos(ctx context.Context, owner string) ([]repos.Repo, error) {
	return d.store.ListRepos(ctx, owner)
}
func (d *domainAdapter) DeleteRepo(ctx context.Context, name string) error {
	return d.reaper.DeleteRepo(ctx, name)
}
func (d *domainAdapter) Enqueue(ctx context.Context, request runner.SessionRequest) (repos.Session, error) {
	if d.runner == nil {
		return repos.Session{}, fmt.Errorf("repos: runner is not initialized")
	}
	return d.runner.Enqueue(ctx, request)
}
func (d *domainAdapter) GetSession(ctx context.Context, id string) (repos.Session, error) {
	return d.store.GetSession(ctx, id)
}
func (d *domainAdapter) ListSessions(ctx context.Context, repoName, owner string) ([]repos.Session, error) {
	return d.store.ListSessions(ctx, repoName, owner)
}
func (d *domainAdapter) Cancel(id string) bool { return d.runner.Cancel(id) }

func reposSpec() appkit.Spec {
	domain := &domainAdapter{}
	clock := wallClock{}
	var intake *repos.Intake
	var engine *runner.Runner
	var runnerConfig runner.Config

	consumers := []appkit.Consumer{{
		Source:        "webhooks",
		Subscriptions: repos.Subscriptions(os.Getenv("REPOS_GITHUB_HOOK")),
		Handler: func(*appkit.Router) consumer.Handler {
			return intake.Handle
		},
	}}

	return appkit.Spec{
		App:        "repos",
		Mount:      "/srv/repos/",
		Port:       registry.MustPort("repos"),
		MCP:        true,
		WWW:        true,
		Feed:       "/feed",
		Migrations: reposdb.FS,
		Events:     repos.Events,
		Consumers:  consumers,
		Handlers: func(rt *appkit.Router) error {
			if rt.DB() == nil {
				return fmt.Errorf("repos: no DB handle on router")
			}

			model := runner.DefaultModelConfig(os.Getenv("ANTHROPIC_API_KEY"))
			model.Provider = config.EnvOr(os.Getenv, "REPOS_PROVIDER", model.Provider)
			model.Model = config.EnvOr(os.Getenv, "REPOS_MODEL", model.Model)
			if _, err := runner.ValidateModel(model); err != nil {
				return err
			}
			ttl, err := config.EnvOrDuration(os.Getenv, "REPOS_SESSION_TTL", 30*time.Minute)
			if err != nil {
				return err
			}
			maxRun, err := config.EnvOrInt(os.Getenv, "REPOS_MAX_SESSIONS", 2)
			if err != nil {
				return err
			}

			stateRoot, err := repos.ResolveStateRoot(os.Getenv)
			if err != nil {
				return err
			}
			store := repos.NewStore(rt.DB())
			tokens := repos.NewHTTPTokenSource(http.DefaultClient, clock.Now)
			git := repos.NewGit(filepath.Join(stateRoot, "repos"), tokens)
			lifecycle := repos.NewService(store, git, clock,
				config.EnvOr(os.Getenv, "REPOS_GITHUB_ORG", "ikigenba"))
			protocol := repos.NewProtocol(repos.NewGitHubPeer(http.DefaultClient))
			domain.lifecycle = lifecycle
			domain.store = store
			intake = repos.NewIntake(store, lifecycle, domain, os.Getenv("REPOS_BOT_LOGIN"), rt.Logger())
			runnerConfig = runner.Config{
				Store: store, Git: git, Protocol: protocol, Clock: clock,
				StateRoot: stateRoot, TTL: ttl, MaxRun: maxRun, Model: model,
			}

			handler, err := mcp.NewHandler(domain, rt)
			if err != nil {
				return err
			}
			rt.Handle("POST /mcp", rt.RequireIdentity(handler))
			rt.Handle("GET /{$}", landingHandler(rt.WWW(), rt.Service(), rt.Version()))
			return nil
		},
		Producer: func(producer *outbox.Outbox) error {
			if domain.store == nil {
				return fmt.Errorf("repos: Producer called before Handlers")
			}
			reaper, err := repos.NewReaper(runnerConfig.Store, runnerConfig.Git, clock,
				runnerConfig.StateRoot, repos.DefaultWorktreeTTL)
			if err != nil {
				return err
			}
			runnerConfig.Outbox = producer
			runnerConfig.Reaper = reaper
			engine, err = runner.New(runnerConfig)
			if err != nil {
				return err
			}
			domain.reaper = reaper
			domain.runner = engine
			return nil
		},
		Workers: []func(context.Context) error{func(ctx context.Context) error {
			if engine == nil {
				return fmt.Errorf("repos: Worker called before Producer")
			}
			if _, err := engine.Recover(ctx); err != nil {
				return fmt.Errorf("repos: recover sessions: %w", err)
			}
			return engine.Dispatch(ctx)
		}},
		Health: func(ctx context.Context) (map[string]any, error) {
			if domain.store == nil {
				return nil, fmt.Errorf("repos: health called before Handlers")
			}
			running, err := domain.store.CountRunning(ctx)
			if err != nil {
				return nil, err
			}
			sessions, err := domain.store.ListSessions(ctx, "", "")
			if err != nil {
				return nil, err
			}
			queued := 0
			for _, session := range sessions {
				if session.Status == repos.StatusQueued {
					queued++
				}
			}
			return map[string]any{"running": running, "queued": queued}, nil
		},
	}
}

func landingHandler(site *web.Site, service, version string) http.Handler {
	return http.HandlerFunc(func(w http.ResponseWriter, request *http.Request) {
		if request.URL.Path != "/" {
			http.NotFound(w, request)
			return
		}
		if err := site.Render(w, "landing.html", struct {
			Service string
			Version string
		}{Service: service, Version: version}); err != nil {
			http.Error(w, "template error", http.StatusInternalServerError)
		}
	})
}
