// Command ledger is the loopback-only domain service behind nginx. It trusts the
// X-Owner-Email / X-Client-Id headers nginx injects after a successful
// auth_request against the dashboard's authorization server, and performs no
// token logic of its own. See internal/server for the auth contract.
//
// This is the skeleton ledger service: it boots the full chassis (config, db +
// migrations, logging, server) and exposes a single MCP tool, ledger_whoami.
// The database connection is opened and migrated but not yet read by any tool —
// it is the wired seam where real ledger domain logic attaches later, the same
// way crm wires internal/contacts.
package main

import (
	"context"
	"errors"
	"flag"
	"fmt"
	"io"
	"net"
	"os"
	"os/signal"
	"strconv"
	"syscall"

	"ledger/internal/db"
	"ledger/internal/ledger"
	"ledger/internal/logging"
	"ledger/internal/mcp"
	"ledger/internal/server"

	"eventplane/outbox"
)

// version is the product version, overridden at build time via -ldflags.
var version = "dev"

func main() {
	if err := run(os.Args[1:], os.Getenv, os.Stdout, os.Stderr); err != nil {
		fmt.Fprintln(os.Stderr, "ledger:", err)
		os.Exit(1)
	}
}

func run(args []string, getenv func(string) string, stdout, stderr io.Writer) error {
	portDef, err := envOrInt(getenv, "LEDGER_PORT", 3002)
	if err != nil {
		return err
	}

	fs := flag.NewFlagSet("ledger", flag.ContinueOnError)
	fs.SetOutput(stderr)
	showVersion := fs.Bool("version", false, "print version and exit")
	// Bind 127.0.0.1 by default and in production: nginx is the only ingress
	// and sets identity headers authoritatively. Binding a public interface
	// would let anyone connect directly and spoof X-Owner-Email — a security
	// defect. The flag exists only so tests/local runs can override deliberately.
	ip := fs.String("ip", envOr(getenv, "LEDGER_IP", "127.0.0.1"), "listen address — keep loopback (env: LEDGER_IP)")
	port := fs.Int("port", portDef, "listen port (env: LEDGER_PORT)")
	logLevel := fs.String("log-level", envOr(getenv, "LEDGER_LOG_LEVEL", "info"), "log level: debug|info|warn|error (env: LEDGER_LOG_LEVEL)")
	if err := fs.Parse(args); err != nil {
		if errors.Is(err, flag.ErrHelp) {
			return nil
		}
		return err
	}
	if *showVersion {
		fmt.Fprintln(stdout, version)
		return nil
	}

	// LEDGER_RESOURCE_ID is this service's canonical resource identifier (must be
	// byte-equal to the `resource` in the PRM doc and the dashboard's token
	// binding). LEDGER_AUTH_SERVER is the dashboard authorization-server base URL
	// advertised to clients. Both have local-dev defaults; we resolve them here
	// at the boundary so nothing deeper reads the environment.
	resourceID := envOr(getenv, "LEDGER_RESOURCE_ID", "http://localhost:8080/srv/ledger/mcp")
	authServer := envOr(getenv, "LEDGER_AUTH_SERVER", "http://localhost:8080")
	// LEDGER_DB_PATH is the SQLite database file. db.Open pins SetMaxOpenConns(1)
	// for single-writer discipline; we resolve the path here at the boundary.
	dbPath := envOr(getenv, "LEDGER_DB_PATH", "./tmp/ledger.db")
	// LEDGER_GENERATION_PATH is the event-plane generation/epoch token sidecar
	// (event-protocol.md §9.3). It MUST live outside the DB file so a file-level
	// restore does not roll it back; default is the DB path plus ".generation".
	genPath := envOr(getenv, "LEDGER_GENERATION_PATH", dbPath+".generation")
	// Event-plane retention knobs (§11.3). Zero means "use the library default"
	// (7 days / 1,000,000 rows). Set via manifest.env on the box.
	retentionDays, err := envOrInt(getenv, "OUTBOX_RETENTION_DAYS", 0)
	if err != nil {
		return err
	}
	retentionMaxRows, err := envOrInt(getenv, "OUTBOX_RETENTION_MAX_ROWS", 0)
	if err != nil {
		return err
	}

	return serve(*ip, *port, *logLevel, resourceID, authServer, dbPath, genPath, retentionDays, retentionMaxRows, stdout)
}

// serve runs the long-running HTTP server until interrupted.
func serve(ip string, port int, logLevel, resourceID, authServer, dbPath, genPath string, retentionDays, retentionMaxRows int, stdout io.Writer) error {
	level, err := logging.ParseLevel(logLevel)
	if err != nil {
		return err
	}
	logger := logging.New(level, stdout)

	ctx, stop := signal.NotifyContext(context.Background(), os.Interrupt, syscall.SIGTERM)
	defer stop()

	conn, err := db.Open(dbPath)
	if err != nil {
		return err
	}
	defer conn.Close()
	if err := db.Migrate(ctx, conn); err != nil {
		return fmt.Errorf("migrate: %w", err)
	}

	// Event-plane producer (event-protocol.md). New runs the §5.3 startup probe
	// (it crashes us here if a second concurrent write tx is not refused) and
	// loads/mints the generation token. The outbox shares ledger's single-writer
	// SQLite handle so the transaction.recorded event commits atomically with the
	// journal write.
	ob, err := outbox.New(conn, outbox.Options{
		Source:           "ledger",
		DBPath:           dbPath,
		GenerationPath:   genPath,
		Logger:           logger,
		RetentionDays:    retentionDays,
		RetentionMaxRows: int64(retentionMaxRows),
	})
	if err != nil {
		return fmt.Errorf("event plane: %w", err)
	}
	go ob.StartRetention(ctx)

	ledgerSvc := ledger.NewService(conn)
	ledgerSvc.Outbox = ledger.NewOutboxProducer(ob)
	mcpHandler := mcp.NewHandler(ledgerSvc)

	addr := net.JoinHostPort(ip, strconv.Itoa(port))
	srv, err := server.New(server.Options{
		Addr:       addr,
		Logger:     logger,
		ResourceID: resourceID,
		AuthServer: authServer,
		MCP:        mcpHandler,
		Feed:       ob.FeedHandler(),
	})
	if err != nil {
		return err
	}

	logger.Info("starting ledger", "addr", addr, "resource_id", resourceID, "auth_server", authServer, "db_path", dbPath, "generation", ob.Generation(), "version", version)
	return server.Run(ctx, srv, logger)
}

func envOr(getenv func(string) string, key, def string) string {
	if v := getenv(key); v != "" {
		return v
	}
	return def
}

// envOrInt returns def when key is unset/empty, the parsed value when it holds
// a valid integer, and an error naming the variable otherwise — a malformed
// override fails loudly rather than silently reverting to def.
func envOrInt(getenv func(string) string, key string, def int) (int, error) {
	v := getenv(key)
	if v == "" {
		return def, nil
	}
	n, err := strconv.Atoi(v)
	if err != nil {
		return 0, fmt.Errorf("%s: invalid integer %q", key, v)
	}
	return n, nil
}
