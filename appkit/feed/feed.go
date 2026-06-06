// Package feed is the producer-side orchestration of the event plane: it
// constructs the eventplane/outbox over appkit's DB handle, starts retention,
// and hands back the unauthenticated SSE FeedHandler the server mounts. It is
// thin glue — appkit CALLS eventplane, it does not re-implement or wrap the
// event protocol (PLAN §B1 map: appkit/feed is orchestration only).
//
// This package is compiled into every appkit app but only EXERCISED when a
// service declares Spec.Feed (a producer). Consumers (notify/wiki) wire
// eventplane/consumer.Run themselves service-side; appkit owns only the producer
// mount (the producer/consumer asymmetry, PLAN §B1 map §3 risk 2).
package feed

import (
	"context"
	"database/sql"
	"fmt"
	"log/slog"
	"net/http"

	"eventplane/outbox"
)

// Options configures Start.
type Options struct {
	Source           string       // emitting service name, stamped into every envelope (required)
	DBPath           string       // SQLite file, used for the §5.3 startup probe
	GenerationPath   string       // event-plane epoch sidecar, outside the DB file
	Logger           *slog.Logger // feed/retention observability
	RetentionDays    int          // 0 = library default (7 days)
	RetentionMaxRows int          // 0 = library default (1,000,000 rows)
	// Registry is the producer's published event-type registry. When non-empty it
	// is forwarded into outbox.Options so Append rejects an unregistered ev.Type
	// (§5.3 fail-loud); empty preserves today's unvalidated behavior.
	Registry outbox.Registry
}

// Producer bundles the constructed outbox and its mountable feed handler. The
// service's Handlers hook receives the *outbox.Outbox so its domain writes can
// Append events on the same transaction (the producer's payload builders stay
// app-side).
type Producer struct {
	Outbox  *outbox.Outbox
	Handler http.Handler
}

// Start runs the producer startup probe, loads/mints the generation token, kicks
// off background retention bound to ctx, and returns the producer handle. A
// startup-probe or generation failure is returned so the process crashes rather
// than serving a stream it cannot order correctly (eventplane semantics).
func Start(ctx context.Context, conn *sql.DB, opts Options) (*Producer, error) {
	ob, err := outbox.New(conn, outbox.Options{
		Source:           opts.Source,
		DBPath:           opts.DBPath,
		GenerationPath:   opts.GenerationPath,
		Logger:           opts.Logger,
		RetentionDays:    opts.RetentionDays,
		RetentionMaxRows: int64(opts.RetentionMaxRows),
		Registry:         opts.Registry,
	})
	if err != nil {
		return nil, fmt.Errorf("event plane: %w", err)
	}
	go ob.StartRetention(ctx)
	return &Producer{Outbox: ob, Handler: ob.FeedHandler()}, nil
}
