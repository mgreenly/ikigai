// Command ralph is the loopback-only domain service behind nginx. It trusts the
// X-Owner-Email / X-Client-Id headers nginx injects after a successful
// auth_request against the dashboard's authorization server, and performs no
// token logic of its own.
//
// The uniform chassis — the fixed subcommands (serve/version/manifest/migrate/
// backup/restore), config-from-env, the migration runner + downgrade guard, the
// loopback HTTP server + PRM + identity gate (ikigenba_ralph_health), and graceful
// shutdown — is owned by appkit. main.go declares only ralph's identity (the
// Spec) and wires its domain surface through the Handlers hook: the session
// store, per-session sandbox tree, async runner, the boot-time crash-recovery
// sweep, and the ralph_* MCP surface. RESOURCE_ID / AUTH_SERVER are composed
// in-binary by appkit/config from METASPOT_DOMAIN + MOUNT (was the deleted
// bin/build run-wrapper's job).
//
// ralph is an LLM service: it uses agentkit (the LLM engine + tool surface) for
// the agent loop, kept strictly separate from appkit (the deploy/serve chassis).
// It is neither an event-plane producer nor a consumer — no /feed, no consumer
// loop, no background worker; the async runner is spawned per-run, not a
// long-running task. Its only secret, ANTHROPIC_API_KEY, is read env-only inside
// the runner/session domain at the point of use and never logged (§2.8).
package main

import (
	"context"
	"fmt"
	"os"
	"path/filepath"
	"time"

	"appkit"
	"appkit/config"

	"ralph/internal/db"
	"ralph/internal/mcp"
	"ralph/internal/runner"
	"ralph/internal/sandbox"
	"ralph/internal/session"
)

func main() {
	appkit.Main(appkit.Spec{
		App:        "ralph",
		Mount:      "/srv/ralph/",
		Port:       3004,
		MCP:        true,
		Migrations: db.FS,
		// Handlers builds ralph's domain over appkit's shared single-writer DB
		// handle, runs the boot-time crash-recovery sweep (after migrate, before
		// serving), and mounts the ralph_* MCP surface gated behind nginx-injected
		// identity. ralph is neither producer nor consumer, so there is no
		// Producer/Workers hook — the async runner is spawned per-run by the
		// session service, not run as a long-lived background worker.
		Handlers: registerRoutes,
	})
}

// registerRoutes wires ralph's domain on appkit's server. It is the seam where
// the chassis (appkit) hands off to the domain: appkit has already resolved
// config, opened, and migrated the shared single-writer DB before calling this.
func registerRoutes(rt *appkit.Router) error {
	conn := rt.DB()
	if conn == nil {
		return fmt.Errorf("ralph: no DB handle on router")
	}

	// RALPH_RUN_TTL bounds each run's wall-clock — the runaway-goroutine backstop
	// (§5.3). Parsed as a Go duration (e.g. "30m", "2h"). Read here at the domain
	// boundary, reusing appkit/config's env helper.
	runTTL, err := config.EnvOrDuration(os.Getenv, "RALPH_RUN_TTL", 30*time.Minute)
	if err != nil {
		return err
	}

	// The session data tree (sandboxes + run logs) lives alongside the db file —
	// the same RALPH_DB_PATH appkit resolved (default ./tmp/ralph.db; /opt/ralph/
	// data/ralph.db on the box) — mirroring the on-box /opt/ralph/data layout.
	dbPath := config.EnvOr(os.Getenv, "RALPH_DB_PATH", "./tmp/ralph.db")
	dataDir := filepath.Join(filepath.Dir(dbPath), "data")
	sb, err := sandbox.New(filepath.Join(dataDir, "sandboxes"))
	if err != nil {
		return fmt.Errorf("ralph: sandbox: %w", err)
	}
	runsDir := filepath.Join(dataDir, "runs")

	store := session.NewStore(conn)
	run := runner.New(store, sb, runTTL)
	svc := session.NewService(store, sb, runsDir, run)

	// Crash-recovery sweep (§5.3): runs left 'running' by a previous process are
	// orphaned — mark them failed and return their sessions to idle before
	// serving, so the single-flight gate starts clean. Runs after migrate (appkit
	// migrated the shared conn before calling Handlers) and before the server
	// begins listening.
	if swept, err := run.Recover(context.Background()); err != nil {
		return fmt.Errorf("ralph: crash-recovery sweep: %w", err)
	} else if swept > 0 {
		rt.Logger().Warn("crash-recovery: swept orphaned runs", "count", swept)
	}

	rt.Handle("POST /mcp", rt.RequireIdentity(mcp.NewHandler(svc, rt.Version(), rt.Service(), rt.Health())))
	return nil
}
