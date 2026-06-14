// Command wiki is the loopback-only knowledge-base service behind nginx. It
// trusts the X-Owner-Email / X-Client-Id headers nginx injects after a successful
// auth_request against the dashboard's authorization server, and performs no
// token logic of its own.
//
// The uniform chassis — the fixed subcommands (serve/version/manifest/migrate/
// backup/restore), config-from-env, the migration runner + downgrade guard, the
// loopback HTTP server + PRM + identity gate, and the /feed producer mount — is
// owned by appkit. main.go declares only wiki's identity (the Spec) and wires its
// surface (the wiki MCP tools and the two wiki.* producer events) through the
// Spec hooks.
//
// This is the P2 scaffold: the verb dispatcher, the MCP tool surface (domain
// tools stubbed not-implemented; health + reflection live), the producer outbox +
// the two declared event types, and the per-call-site config-injection seam
// (internal/config + internal/llm). The ingest path (P3), the worker spine (P4+),
// and the read side (P10) fill the stubs.
package main

import (
	"context"
	"fmt"
	"os"
	"path/filepath"

	"appkit"

	"wiki/internal/config"
	"wiki/internal/consume"
	"wiki/internal/db"
	"wiki/internal/events"
	"wiki/internal/ingest"
	"wiki/internal/inbox"
	"wiki/internal/mcp"
	"wiki/internal/producer"

	"eventplane/consumer"
	"eventplane/outbox"
)

// The event-plane upstreams wiki consumes and the stable id it presents on every
// connect. dropbox/crm/ledger feed wiki's consumer doors; cron ticks are
// consumed too (design §2.1). X-Consumer-Id is the literal "wiki" on every loop.
const consumerID = "wiki"

func main() {
	// Captured by the Handlers / Producer hooks so the consumer workers and the
	// ingest front doors can reach the shared DB handle, the config, the inbox
	// writer, and the producer. The workers run strictly after the server is built,
	// so the captures are always set by the time they execute.
	var (
		rt   *appkit.Router
		cfg  *config.Config
		box  *inbox.Store
		prod *producer.Producer
	)

	appkit.Main(appkit.Spec{
		App:        "wiki",
		Mount:      "/srv/wiki/",
		Port:       3006,
		MCP:        true,
		Feed:       "/feed",                              // event-plane producer (design §8)
		Consumes:   []string{"dropbox", "crm", "ledger"}, // consumer doors (P3)
		Migrations: db.FS,
		Events:     events.Registry, // published wiki.* events: reflection + Append validation
		ManifestExtras: []appkit.ManifestKV{
			{Key: "WIKI_INBOX_INLINE_MAX", Value: "4096"},
			{Key: "WIKI_INGEST_MAX_BYTES", Value: "131072"},
			{Key: "WIKI_INTEGRATION_WORKERS", Value: "4"},
			{Key: "WIKI_EMBED_MODEL", Value: "text-embedding-3-large"},
			{Key: "WIKI_EMBED_DIMS", Value: "1024"},
			{Key: "OUTBOX_RETENTION_DAYS", Value: "7"},
			{Key: "OUTBOX_RETENTION_MAX_ROWS", Value: "1000000"},
		},
		// Config is the composition-root hook: read every non-secret knob plus the
		// secrets (ANTHROPIC_API_KEY / OPENAI_API_KEY / WIKI_OWNER) from env and
		// build the validated Config. Validating the per-call-site LLM triples here
		// fails startup loudly on a wrong model / rejected effort (design §10).
		Config: func(getenv func(string) string) (any, error) {
			return config.Load(getenv)
		},
		// Handlers builds wiki's write path (P3) and mounts the MCP surface behind
		// nginx-injected identity. It constructs the inbox writer over the shared DB
		// + the blob dir beside it, the producer (its outbox is injected later in the
		// Producer hook — until then every emit is a safe no-op), and the interactive
		// ingest front doors; the read-side tools remain stubs (P10).
		Handlers: func(r *appkit.Router) error {
			rt = r
			c, err := config.Load(os.Getenv)
			if err != nil {
				return fmt.Errorf("config: %w", err)
			}
			cfg = c

			// The blob dir lives beside the SQLite file (one data dir per box). appkit
			// resolves the DB path as <APP>_DB_PATH (default ./tmp/wiki.db); the blob
			// root is that file's directory.
			blobRoot := filepath.Dir(dbPath(os.Getenv))

			box, err = inbox.New(inbox.Options{
				DB:        rt.DB(),
				BlobRoot:  blobRoot,
				InlineMax: cfg.InboxInlineMax,
				MaxBytes:  cfg.IngestMaxBytes,
				// Nudge wires to the worker doorbell in P4; nil = no-op now.
			})
			if err != nil {
				return fmt.Errorf("inbox: %w", err)
			}

			// The producer is constructed empty here (the outbox arrives in the
			// Producer hook); the front/consumer doors take it as their Refuser now and
			// it becomes live once SetOutbox runs.
			prod = producer.New(rt.DB(), nil)

			ingestSvc := ingest.New(box, rt.DB(), prod, nil)

			rt.Handle("POST /mcp", rt.RequireIdentity(
				mcp.NewHandler(rt.Version(), rt.Service(), rt.Health(),
					rt.Events(), rt.Subscriptions(), ingestSvc)))
			return nil
		},
		// Producer fires after Handlers: inject the constructed outbox into the
		// producer so wiki.ingest_refused / wiki.row_dead_lettered can be emitted.
		// /feed + reflection were already live from the scaffold; this closes the
		// emit seam the ingest doors (P3) and failure path (P4/P5) write through.
		Producer: func(ob *outbox.Outbox) error {
			prod.SetOutbox(rt.DB(), ob)
			return nil
		},
		// Workers carries wiki's event-plane consumer doors (§2.1): one consumer.Run
		// loop per upstream, each with its own feed_offset cursor, mapping events to
		// inbox.Accept. The cursor commits ONLY after Accept returns (at-least-once;
		// hash dedup). appkit launches them on the serve context alongside the HTTP
		// server; a structural fault escapes and brings the server down with it.
		Workers: []func(context.Context) error{
			func(ctx context.Context) error { return runDropboxConsumer(ctx, rt, box, prod) },
			func(ctx context.Context) error { return runCRMConsumer(ctx, rt, box, prod) },
			func(ctx context.Context) error { return runLedgerConsumer(ctx, rt, box, prod) },
		},
	})
}

// dbPath resolves the SQLite file path the same way appkit/config does
// (<APP>_DB_PATH, default ./tmp/wiki.db) so the blob dir lands in the data dir
// beside it.
func dbPath(getenv func(string) string) string {
	if v := getenv("WIKI_DB_PATH"); v != "" {
		return v
	}
	return "./tmp/wiki.db"
}

// feedURL resolves an upstream's loopback /feed address (the event plane bypasses
// nginx — a direct 127.0.0.1 reference) from <UP>_FEED_URL with a dev fallback.
func feedURL(getenv func(string) string, key, fallback string) string {
	if v := getenv(key); v != "" {
		return v
	}
	return fallback
}

// runDropboxConsumer drives wiki's dropbox consumer door: fetch each file event's
// content_url and Accept it as a document (§2.1). ctx cancellation → nil; a
// structural fault escapes.
func runDropboxConsumer(ctx context.Context, rt *appkit.Router, box *inbox.Store, prod *producer.Producer) error {
	doors := consume.New(box, prod, nil, rt.Logger())
	cfg := consumer.Config{
		FeedURL:    feedURL(os.Getenv, "DROPBOX_FEED_URL", "http://127.0.0.1:3005/feed"),
		From:       feedURL(os.Getenv, "WIKI_FROM", "tail"),
		DB:         rt.DB(),
		Source:     "dropbox",
		ConsumerID: consumerID,
		Logger:     rt.Logger(),
	}
	if err := consumer.Run(ctx, cfg, doors.DropboxHandler()); err != nil {
		return fmt.Errorf("event-plane consumer (dropbox): %w", err)
	}
	return nil
}

// runCRMConsumer drives wiki's crm consumer door: Accept each domain event's
// envelope verbatim as an event arrival (§2.1).
func runCRMConsumer(ctx context.Context, rt *appkit.Router, box *inbox.Store, prod *producer.Producer) error {
	doors := consume.New(box, prod, nil, rt.Logger())
	cfg := consumer.Config{
		FeedURL:    feedURL(os.Getenv, "CRM_FEED_URL", "http://127.0.0.1:3001/feed"),
		From:       feedURL(os.Getenv, "WIKI_FROM", "tail"),
		DB:         rt.DB(),
		Source:     "crm",
		ConsumerID: consumerID,
		Logger:     rt.Logger(),
	}
	if err := consumer.Run(ctx, cfg, doors.DomainHandler()); err != nil {
		return fmt.Errorf("event-plane consumer (crm): %w", err)
	}
	return nil
}

// runLedgerConsumer drives wiki's ledger consumer door: same domain-event door as
// crm, keyed by its own feed_offset cursor (source "ledger").
func runLedgerConsumer(ctx context.Context, rt *appkit.Router, box *inbox.Store, prod *producer.Producer) error {
	doors := consume.New(box, prod, nil, rt.Logger())
	cfg := consumer.Config{
		FeedURL:    feedURL(os.Getenv, "LEDGER_FEED_URL", "http://127.0.0.1:3002/feed"),
		From:       feedURL(os.Getenv, "WIKI_FROM", "tail"),
		DB:         rt.DB(),
		Source:     "ledger",
		ConsumerID: consumerID,
		Logger:     rt.Logger(),
	}
	if err := consumer.Run(ctx, cfg, doors.DomainHandler()); err != nil {
		return fmt.Errorf("event-plane consumer (ledger): %w", err)
	}
	return nil
}
