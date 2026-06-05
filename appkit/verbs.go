package appkit

import (
	"context"
	"errors"
	"flag"
	"fmt"
	"io"
	"net"
	"net/http"
	"os"
	"os/signal"
	"strconv"
	"strings"
	"syscall"

	"appkit/config"
	"appkit/db"
	"appkit/feed"
	"appkit/logging"
	"appkit/manifest"
	"appkit/server"

	"eventplane/outbox"
)

// emitManifest renders the app's full manifest.env from its Spec, comment-free
// and in fixed order, so `<app> manifest` byte-equals the committed
// etc/manifest.env (PLAN §B1 map §6).
func emitManifest(spec Spec) string {
	extras := make([]manifest.KV, len(spec.ManifestExtras))
	for i, kv := range spec.ManifestExtras {
		extras[i] = manifest.KV{Key: kv.Key, Value: kv.Value}
	}
	return manifest.Emit(manifest.Fields{
		App:      spec.App,
		Mount:    spec.Mount,
		Default:  spec.Default,
		Port:     spec.Port,
		MCP:      spec.MCP,
		Feed:     spec.Feed,
		Consumes: spec.Consumes,
		Extras:   extras,
	})
}

// loadMigrations reads the app's embedded migration set via the db runner.
func loadMigrations(spec Spec) ([]db.Migration, error) {
	return db.LoadMigrations(spec.Migrations, spec.migrationsDir())
}

// runMigrate applies pending migrations against the app's DB and exits. It is
// the standalone `migrate` verb; serve also migrates on start.
func runMigrate(spec Spec, args []string, getenv func(string) string, stdout, stderr io.Writer) error {
	cfg, err := config.Resolve(spec.App, spec.Mount, spec.Port, getenv)
	if err != nil {
		return err
	}
	if err := parseSimpleFlags(spec.App+" migrate", args, stderr); err != nil {
		return err
	}
	migs, err := loadMigrations(spec)
	if err != nil {
		return err
	}
	ctx := context.Background()
	conn, err := db.Open(cfg.DBPath)
	if err != nil {
		return err
	}
	defer conn.Close()
	if err := db.Migrate(ctx, conn, migs); err != nil {
		return fmt.Errorf("migrate: %w", err)
	}
	v, err := db.AppliedVersion(ctx, conn)
	if err != nil {
		return err
	}
	fmt.Fprintf(stdout, "migrated %s to version %d\n", spec.App, v)
	return nil
}

// runSchema reports the DB's applied migration version and the binary's
// max-embedded migration version, as a single byte-stable line:
//
//	applied=<N> embedded=<M>
//
// It is a read-only introspection seam for optctl: install compares applied<M to
// decide whether a deploy advances the schema (→ back up the DB before migrate so
// the matching rollback can restore it). It opens the DB read-only-ish (no
// migration is run) and reports applied=0 when the DB file does not yet exist.
func runSchema(spec Spec, args []string, getenv func(string) string, stdout, stderr io.Writer) error {
	cfg, err := config.Resolve(spec.App, spec.Mount, spec.Port, getenv)
	if err != nil {
		return err
	}
	if err := parseSimpleFlags(spec.App+" schema", args, stderr); err != nil {
		return err
	}
	migs, err := loadMigrations(spec)
	if err != nil {
		return err
	}
	embedded := db.MaxEmbedded(migs)

	// A not-yet-created DB carries applied=0 (first install of a brand-new app).
	applied := 0
	if _, statErr := os.Stat(cfg.DBPath); statErr == nil {
		conn, err := db.Open(cfg.DBPath)
		if err != nil {
			return err
		}
		defer conn.Close()
		applied, err = db.AppliedVersion(context.Background(), conn)
		if err != nil {
			return err
		}
	}
	fmt.Fprintf(stdout, "applied=%d embedded=%d\n", applied, embedded)
	return nil
}

// runServe is the long-running serve verb (the default no-arg invocation). It
// resolves config, opens + migrates the DB, starts the producer feed when the
// service is a producer, builds the loopback server, and runs until interrupted.
func runServe(spec Spec, args []string, getenv func(string) string, stdout, stderr io.Writer) error {
	cfg, err := config.Resolve(spec.App, spec.Mount, spec.Port, getenv)
	if err != nil {
		return err
	}

	// serve flags override env (env already applied as the default in Resolve).
	fs := flag.NewFlagSet(spec.App+" serve", flag.ContinueOnError)
	fs.SetOutput(stderr)
	ip := fs.String("ip", cfg.IP, "listen address — keep loopback (env: "+envKey(spec.App, "IP")+")")
	port := fs.Int("port", cfg.Port, "listen port (env: "+envKey(spec.App, "PORT")+")")
	logLevel := fs.String("log-level", cfg.LogLevel, "log level: debug|info|warn|error (env: "+envKey(spec.App, "LOG_LEVEL")+")")
	if err := fs.Parse(args); err != nil {
		return helpOrErr(err)
	}

	level, err := logging.ParseLevel(*logLevel)
	if err != nil {
		return err
	}
	logger := logging.New(level, stdout)

	ctx, stop := signal.NotifyContext(context.Background(), os.Interrupt, syscall.SIGTERM)
	defer stop()

	conn, err := db.Open(cfg.DBPath)
	if err != nil {
		return err
	}
	defer conn.Close()
	migs, err := loadMigrations(spec)
	if err != nil {
		return err
	}
	if err := db.Migrate(ctx, conn, migs); err != nil {
		return fmt.Errorf("migrate: %w", err)
	}

	// Producer: construct the outbox + mount the unauthenticated feed handler. The
	// outbox is built here (its FeedHandler must be ready for server.New to mount),
	// but the producer-injection hook is called AFTER server.New so the service's
	// Handlers hook has already built its domain Service over the shared DB handle.
	var feedH http.Handler
	var producerOutbox *outbox.Outbox
	if spec.Feed != "" {
		retDays, err := config.EnvOrInt(getenv, "OUTBOX_RETENTION_DAYS", 0)
		if err != nil {
			return err
		}
		retRows, err := config.EnvOrInt(getenv, "OUTBOX_RETENTION_MAX_ROWS", 0)
		if err != nil {
			return err
		}
		prod, err := feed.Start(ctx, conn, feed.Options{
			Source:           spec.App,
			DBPath:           cfg.DBPath,
			GenerationPath:   cfg.GenerationPath,
			Logger:           logger,
			RetentionDays:    retDays,
			RetentionMaxRows: retRows,
		})
		if err != nil {
			return err
		}
		feedH = prod.Handler
		producerOutbox = prod.Outbox
	}

	addr := net.JoinHostPort(*ip, strconv.Itoa(*port))
	srv, err := server.New(server.Options{
		Addr:       addr,
		Logger:     logger,
		ResourceID: cfg.ResourceID,
		AuthServer: cfg.AuthServer,
		Apex:       spec.Default,
		Feed:       feedH,
		FeedPath:   spec.Feed,
		Register:   spec.Handlers,
		DB:         conn,
	})
	if err != nil {
		return err
	}

	// Close the producer-outbox seam (C1): hand the constructed *outbox.Outbox to
	// the service's injection hook now that Handlers has built its domain Service
	// over the shared DB handle. The service attaches the outbox so its domain
	// writes Append events on the same transaction; the payload builders stay
	// app-side (PLAN §B1 map: appkit/feed is orchestration only).
	if producerOutbox != nil && spec.Producer != nil {
		if err := spec.Producer(producerOutbox); err != nil {
			return fmt.Errorf("producer wiring: %w", err)
		}
	}

	logger.Info("starting "+spec.App,
		"addr", addr, "resource_id", cfg.ResourceID, "auth_server", cfg.AuthServer,
		"db_path", cfg.DBPath, "version", versionString())
	return server.Run(ctx, srv, logger)
}

// runBackup dispatches to Spec.Backup, or appkit's default SQLite snapshot.
func runBackup(spec Spec, args []string, getenv func(string) string, stdout, stderr io.Writer) error {
	cfg, err := config.Resolve(spec.App, spec.Mount, spec.Port, getenv)
	if err != nil {
		return err
	}
	req := BackupReq{
		App: spec.App, DBPath: cfg.DBPath, GenerationPath: cfg.GenerationPath,
		Args: args, Stdout: stdout, Stderr: stderr,
	}
	if spec.Backup != nil {
		return spec.Backup(context.Background(), req)
	}
	return defaultBackup(context.Background(), req)
}

// runRestore dispatches to Spec.Restore, or appkit's default SQLite restore.
func runRestore(spec Spec, args []string, getenv func(string) string, stdin io.Reader, stdout, stderr io.Writer) error {
	cfg, err := config.Resolve(spec.App, spec.Mount, spec.Port, getenv)
	if err != nil {
		return err
	}
	req := RestoreReq{
		App: spec.App, DBPath: cfg.DBPath, GenerationPath: cfg.GenerationPath,
		Args: args, Stdout: stdout, Stderr: stderr,
	}
	if spec.Restore != nil {
		return spec.Restore(context.Background(), req)
	}
	return defaultRestore(context.Background(), req)
}

// envKey builds the <APP>_<SUFFIX> env-var name for a flag's help text.
func envKey(app, suffix string) string {
	return strings.ToUpper(app) + "_" + suffix
}

// helpOrErr maps flag.ErrHelp to the package help sentinel (exit 0) and passes
// other errors through.
func helpOrErr(err error) error {
	if errors.Is(err, flag.ErrHelp) {
		return flagHelp
	}
	return err
}

// parseSimpleFlags parses an args slice that carries no flags of its own beyond
// -h/-help, so verbs like migrate fail cleanly on a stray flag.
func parseSimpleFlags(name string, args []string, stderr io.Writer) error {
	fs := flag.NewFlagSet(name, flag.ContinueOnError)
	fs.SetOutput(stderr)
	if err := fs.Parse(args); err != nil {
		return helpOrErr(err)
	}
	return nil
}
