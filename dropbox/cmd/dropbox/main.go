// Command dropbox is the loopback-only mirror daemon + event-plane producer
// behind nginx. It trusts the X-Owner-Email / X-Client-Id headers nginx injects
// after a successful auth_request against the dashboard's authorization server,
// and performs no token logic of its own. See appkit/server for the auth
// contract.
//
// The uniform chassis — the fixed subcommands (serve/version/manifest/migrate/
// schema), config-from-env, the migration runner + downgrade guard, the
// loopback HTTP server + PRM + identity gate, and the /feed producer mount — is
// owned by appkit. main.go declares only dropbox's identity (the Spec) and wires
// its domain surface (the bare MCP tools, the private /content route, the
// file.* producer, and the background sync engine) through the Spec hooks.
// RESOURCE_ID / AUTH_SERVER are composed in-binary by appkit/config from
// IKIGENBA_DOMAIN + MOUNT (was the deleted bin/build run-wrapper's job).
//
// dropbox differs from the passive crm/ledger producers: it carries a background
// sync engine (the longpoll → continue → apply loop, "the heart"). That loop runs
// for the whole serve lifecycle, so it is wired through appkit's Workers seam —
// appkit launches it on the serve context, a SIGTERM cancels it alongside the
// server, and a structural fault returning from it brings the server down too
// (event-protocol.md decision 11). The three Dropbox secrets are read here at
// dropbox's own composition root via getenv and never logged (§2.8); appkit never
// touches them.
package main

import (
	"context"
	"fmt"
	"net/http"
	"os"
	"path/filepath"
	"strconv"

	"appkit"
	"appkit/config"

	"dropbox/internal/db"
	"dropbox/internal/dropbox"
	"dropbox/internal/mcp"

	"eventplane/outbox"
	"registry"
)

func main() {
	// The domain Service + sync engine are built once in Handlers (over appkit's
	// shared single-writer DB handle) and shared by the producer-injection hook
	// (which attaches the content-aware outbox so file.* events append atomically
	// with the index write) and the Workers hook (which runs the engine on the
	// serve context). All three close over svc/engine/contentBase; appkit calls
	// Handlers → Producer → Workers in that order.
	var (
		svc         *dropbox.Service
		engine      *dropbox.Engine
		contentBase string
		rt          *appkit.Router
	)

	appkit.Main(appkit.Spec{
		App:        "dropbox",
		Mount:      "/srv/dropbox/",
		Port:       registry.MustPort("dropbox"),
		MCP:        true,
		WWW:        true,
		Feed:       "/feed", // event-plane producer
		Migrations: db.FS,
		Events:     dropbox.Events, // published event types: reflection + Append validation
		ManifestExtras: []appkit.ManifestKV{
			{Key: "OUTBOX_RETENTION_DAYS", Value: "7"},
			{Key: "OUTBOX_RETENTION_MAX_ROWS", Value: "1000000"},
		},
		// Health is dropbox's per-service telemetry reporter (DECISIONS §3/§7).
		// Unlike the other five services, dropbox has real telemetry, so it
		// supplies a reporter: appkit calls it to populate the `details` object of
		// the shared health envelope on BOTH the ungated HTTP /health route and the
		// gated health MCP tool, so the two cannot diverge. The
		// source is svc.Health (the same data the old dropbox_health tool used) —
		// only its mirror/disk telemetry goes under details; identity is NOT
		// included here (the MCP tool adds owner_email/client_id; HTTP carries
		// none). svc is built in Handlers, which appkit calls before serve, so the
		// closure captures a non-nil Service by the time the reporter runs.
		Health: func(ctx context.Context) (map[string]any, error) {
			if svc == nil {
				return nil, fmt.Errorf("dropbox: Health reporter ran before Handlers built the Service")
			}
			info, err := svc.Health("", "")
			if err != nil {
				return nil, err
			}
			return map[string]any{
				"mirror_bytes":     info.MirrorBytes,
				"disk_free_bytes":  info.DiskFreeBytes,
				"disk_total_bytes": info.DiskTotalBytes,
				"failed_files":     info.FailedFiles,
			}, nil
		},
		// Handlers builds dropbox's domain Service + sync engine over appkit's shared
		// DB handle, then mounts the routes dropbox owns: the gated dropbox_* MCP
		// surface (POST /mcp) and the UNAUTHENTICATED, loopback-only private byte
		// routes — GET /content (one file's bytes) and GET /list (its enumeration
		// twin, ADR-import-sync §3). Each byte route is the primary guard, 404ing any
		// nginx-injected identity header, exactly like /feed. The secrets + non-secret
		// sync knobs are read here at dropbox's own composition root; appkit never
		// reads them.
		Handlers: func(r *appkit.Router) error {
			rt = r
			conn := rt.DB()
			if conn == nil {
				return fmt.Errorf("dropbox: no DB handle on router")
			}

			// The loopback /content base URL stamped into event content_url values
			// (PLAN.md §5). It is the service's own loopback address; consumers fetch
			// bytes there. dropbox always binds 127.0.0.1:<port>; compose it from the
			// same env appkit resolves the listen port from.
			port, err := config.EnvOrInt(os.Getenv, "DROPBOX_PORT", registry.MustPort("dropbox"))
			if err != nil {
				return err
			}
			ip := config.EnvOr(os.Getenv, "DROPBOX_IP", "127.0.0.1")
			contentBase = "http://" + ip + ":" + strconv.Itoa(port)

			// The private local mirror (PLAN.md §4): a 0750 subdir beside the durable
			// DB. An explicit DROPBOX_MIRROR_PATH always wins. Otherwise derive it
			// from DROPBOX_DB_PATH: on the box opsctl stamps
			// DROPBOX_DB_PATH=/opt/dropbox/state/dropbox.db, so the mirror lands at
			// /opt/dropbox/state/mirror (owned with the service's durable state)
			// rather than the legacy data/ tree or rebuildable cache/tmp space. Falls
			// back to the dev default ./tmp/mirror when no DB path is set.
			mirrorPath := defaultMirrorPath(os.Getenv)
			mirror, err := dropbox.NewMirror(mirrorPath)
			if err != nil {
				return fmt.Errorf("mirror: %w", err)
			}

			// The three Dropbox secrets arrive via .envrc/direnv in dev or app-config
			// (ikigenba-launch) on the box. They are read here at the boundary and
			// passed into the client — NEVER logged (§2.8).
			cfg := dropbox.Config{
				AppKey:        os.Getenv("DROPBOX_APP_KEY"),
				AppSecret:     os.Getenv("DROPBOX_APP_SECRET"),
				RefreshToken:  os.Getenv("DROPBOX_REFRESH_TOKEN"),
				AppFolderRoot: os.Getenv("DROPBOX_APP_FOLDER_ROOT"),
			}
			cfg.LongpollTimeoutSeconds, err = config.EnvOrInt(os.Getenv, "DROPBOX_LONGPOLL_TIMEOUT", 480)
			if err != nil {
				return err
			}
			maxEntryRetries, err := config.EnvOrInt(os.Getenv, "DROPBOX_MAX_ENTRY_RETRIES", 5)
			if err != nil {
				return err
			}
			client := dropbox.NewClient(cfg, nil)

			svc = dropbox.NewService(conn)
			svc.Mirror = mirror
			svc.Client = client

			engine = dropbox.NewEngine(svc, dropbox.EngineOptions{
				Client:          client,
				Logger:          rt.Logger(),
				MaxEntryRetries: maxEntryRetries,
			})

			// Landing page and assets are human web UI; nginx owns the session
			// gate, so the in-process handlers stay ungated.
			rt.Handle("GET /{$}", http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
				if r.URL.Path != "/" {
					http.NotFound(w, r)
					return
				}
				if err := rt.WWW().Render(w, "landing.html", struct {
					Service string
					Version string
				}{
					Service: rt.Service(),
					Version: rt.Version(),
				}); err != nil {
					http.Error(w, "template error", http.StatusInternalServerError)
				}
			}))
			handler, err := mcp.NewHandler(svc, rt)
			if err != nil {
				return err
			}
			rt.Handle("POST /mcp", rt.RequireIdentity(handler))
			// /content and /list are unauthenticated + loopback-only (each handler
			// self-guards, 404ing any nginx-injected identity header), so they are
			// registered verbatim, NOT behind RequireIdentity. /content delivers a
			// file's bytes; /list is its enumeration twin (the loopback peer-walk
			// route sites' `sync` consumes, ADR-import-sync §3).
			rt.Handle("GET /content", svc.ContentHandler())
			rt.Handle("GET /list", svc.ListHandler())
			return nil
		},
		// Producer fires after Handlers: wrap the outbox with the content base so
		// every committed index change emits its file.* event (carrying a content_url
		// reference, never bytes) on the same tx. The payload builders stay app-side
		// in internal/dropbox/events.go.
		Producer: func(ob *outbox.Outbox) error {
			if svc == nil {
				return fmt.Errorf("dropbox: Producer called before Handlers built the Service")
			}
			svc.Outbox = dropbox.NewOutboxProducer(ob, contentBase)
			return nil
		},
		// Workers carries dropbox's background sync engine. appkit launches it on the
		// serve context alongside the HTTP server: a SIGTERM cancels both; the engine
		// returns nil on clean ctx cancel (so it never falsely takes the server down),
		// and a transport fault (Dropbox unreachable) is retried inside the loop and
		// never returns. The engine is the sole writer of mirror state and the sole
		// caller of svc's apply helpers, so it must run for emission to flow.
		Workers: []func(context.Context) error{
			func(ctx context.Context) error {
				if engine == nil {
					return fmt.Errorf("dropbox: Workers ran before Handlers built the engine")
				}
				return engine.Run(ctx)
			},
		},
	})
}

func defaultMirrorPath(getenv func(string) string) string {
	if mirrorPath := getenv("DROPBOX_MIRROR_PATH"); mirrorPath != "" {
		return mirrorPath
	}
	if dbPath := getenv("DROPBOX_DB_PATH"); dbPath != "" {
		return filepath.Join(filepath.Dir(dbPath), "mirror")
	}
	return "./tmp/mirror"
}
