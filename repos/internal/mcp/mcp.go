// Package mcp exposes the repository lifecycle and session engine through the
// shared appkit MCP transport.
package mcp

import (
	"context"
	"fmt"
	"net/http"

	"appkit"
	appkitmcp "appkit/mcp"

	"repos/internal/repos"
	"repos/internal/runner"
)

// Instructions describes the repository workflow to MCP clients.
const Instructions = "Clone and manage GitHub repositories, then start, inspect, and cancel autonomous repository sessions. Repository and session reads are scoped to the authenticated owner."

// Service is the narrow domain seam used by the MCP surface. Composition
// delegates these methods to the lifecycle service, store, runner, and reaper.
type Service interface {
	CloneRepo(context.Context, string, string) error
	GetRepo(context.Context, string) (repos.Repo, error)
	ListRepos(context.Context, string) ([]repos.Repo, error)
	DeleteRepo(context.Context, string) error
	Enqueue(context.Context, runner.SessionRequest) (repos.Session, error)
	GetSession(context.Context, string) (repos.Session, error)
	ListSessions(context.Context, string, string) ([]repos.Session, error)
	Cancel(string) bool
}

// NewHandler assembles repos' domain tools with the chassis-owned health and
// reflection tools.
func NewHandler(svc Service, rt *appkit.Router) (http.Handler, error) {
	if svc == nil {
		panic("mcp: repos service is required")
	}
	if rt == nil {
		return nil, fmt.Errorf("mcp: router is required")
	}
	return appkitmcp.New(appkitmcp.Options{
		Service:       rt.Service(),
		Version:       rt.Version(),
		Instructions:  Instructions,
		Tools:         Tools(svc),
		Health:        rt.Health(),
		Events:        rt.Events(),
		Publishes:     rt.Publishes(),
		Subscriptions: rt.Subscriptions(),
	})
}
