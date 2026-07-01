// Package outbox is the producer half of the suite's internal SSE event plane
// (see ../../docs/event-protocol.md — the normative wire contract). A producer
// writes events into a local outbox table inside the same transaction as the
// domain change that caused them (§4.1 atomic outbox), then serves them to
// loopback consumers as a Server-Sent-Events feed (§7, §8).
//
// The event plane is internal, loopback-only, and UNAUTHENTICATED (§2): one box
// is one owner, so there is no second principal to authenticate. The feed must
// never be reachable externally — that is enforced by binding loopback, an nginx
// exact-match 404 on the public mount, and FeedHandler's own rejection of any
// request that arrives carrying nginx-injected identity headers.
//
// This package ships the producer side only. The consumer half (offset loop,
// dedup, resync handling) is deferred to the first consumer, but the wire
// contract — control frames and the connect-time resync/epoch check — is
// complete from day one so no producer change is needed when a consumer lands.
package outbox

import (
	"context"
	"database/sql"
	"encoding/json"
	"errors"
	"fmt"
	"log/slog"
	"os"
	"strings"
	"sync"
	"time"
)

// defaults for retention (§11.3) and the feed fetch batch (§6.1).
const (
	defaultRetentionDays = 7
	defaultRetentionRows = 1_000_000
	defaultBatchLimit    = 100
)

// Event is what a producer hands to Append. The library is domain-agnostic: the
// caller marshals the per-type payload (§8.6) to opaque JSON and the library
// treats it as an opaque blob, wrapping it in the uniform envelope (§8.3) at
// serialize time.
type Event struct {
	Type    string          // e.g. "contact.created" (§8.5)
	Payload json.RawMessage // domain snapshot, marshaled by the caller
}

// Options configures New.
type Options struct {
	// Source is the emitting service name stamped into every envelope's
	// "source" field (§8.3), e.g. "crm". Required.
	Source string
	// Registry, when non-empty, is the set of event types this producer may
	// emit. Append rejects an event whose Type is not declared here (fail
	// loudly), guaranteeing reflection lists everything emittable. Empty leaves
	// Append unconstrained — today's behavior, so adoption is incremental.
	Registry Registry
	// DBPath is the SQLite database file, used only for the startup behavioural
	// probe (§5.3), which opens its own connections to prove a second concurrent
	// write transaction is refused. Empty or an in-memory DSN skips the probe
	// (the single-writer guarantee cannot be exercised across in-memory handles).
	DBPath string
	// GenerationPath is the sidecar file holding the generation/epoch token
	// (§9.3). It MUST live outside the DB file so a file-level restore does not
	// roll it back. Empty mints an ephemeral in-process token (tests only).
	GenerationPath string
	// Logger is used for feed/retention observability. Defaults to slog.Default().
	Logger *slog.Logger
	// RetentionDays / RetentionMaxRows bound the outbox (§11.3). A row is trimmed
	// only when it is beyond BOTH horizons (the more-conservative floor wins).
	// Zero uses the defaults (7 days / 1,000,000 rows).
	RetentionDays    int
	RetentionMaxRows int64
	// Now is the clock, injectable for tests. Defaults to time.Now.
	Now func() time.Time
}

// Outbox is a producer's handle to its event plane: it owns the durable outbox
// table on db, the live generation token, and the in-process doorbell that wakes
// parked feed connections.
type Outbox struct {
	db            *sql.DB
	source        string
	generation    string
	log           *slog.Logger
	now           func() time.Time
	retentionDays int
	retentionRows int64
	batchLimit    int
	registry      Registry

	mu   sync.Mutex
	bell chan struct{} // closed-and-replaced on Ring; the broadcast doorbell (§4.3)
}

// New validates config, runs the startup behavioural probe (§5.3), loads or
// mints the generation token (§9.3), and returns a ready Outbox. It does not
// create the outbox table — the caller's migration runner applies SchemaSQL.
//
// The probe is a MUST (§5.3): if a second concurrent write transaction is not
// refused, the ordering guarantee is gone and New returns an error so the
// producer crashes rather than serving a stream it cannot order correctly.
func New(db *sql.DB, opts Options) (*Outbox, error) {
	if db == nil {
		return nil, errors.New("outbox: db is required")
	}
	if opts.Source == "" {
		return nil, errors.New("outbox: Source is required")
	}

	if err := runStartupProbe(opts.DBPath); err != nil {
		return nil, err
	}

	generation, err := EnsureGeneration(opts.GenerationPath)
	if err != nil {
		return nil, fmt.Errorf("outbox: generation token: %w", err)
	}

	logger := opts.Logger
	if logger == nil {
		logger = slog.Default()
	}
	now := opts.Now
	if now == nil {
		now = time.Now
	}
	days := opts.RetentionDays
	if days <= 0 {
		days = defaultRetentionDays
	}
	rows := opts.RetentionMaxRows
	if rows <= 0 {
		rows = defaultRetentionRows
	}

	return &Outbox{
		db:            db,
		source:        opts.Source,
		generation:    generation,
		log:           logger,
		now:           now,
		retentionDays: days,
		retentionRows: rows,
		batchLimit:    defaultBatchLimit,
		registry:      opts.Registry,
		bell:          make(chan struct{}),
	}, nil
}

// Generation returns the live generation/epoch token, exposed for observability
// and tests.
func (o *Outbox) Generation() string { return o.generation }

// Append writes one event into the outbox on the caller's existing transaction
// (§4.1). It MUST be called on the same *sql.Tx as the domain write so the event
// and the domain change commit atomically. The library mints the event id (ULID)
// and the emit time here and stores them once, so they are byte-identical on
// every replay (§4.5) — the stable dedup key a consumer relies on.
//
// Append does not signal the doorbell: the row is not visible to readers until
// the caller commits, so the caller invokes Ring AFTER a successful Commit
// (§4.3).
func (o *Outbox) Append(tx *sql.Tx, ev Event) error {
	if tx == nil {
		return errors.New("outbox: Append requires a transaction")
	}
	if ev.Type == "" {
		return errors.New("outbox: event Type is required")
	}
	if len(o.registry) > 0 && !o.registry.has(ev.Type) {
		return fmt.Errorf("outbox: event type %q is not in the registry; declared types: %s",
			ev.Type, strings.Join(o.registry.types(), ", "))
	}
	eventID := newULID()
	createdAt := o.now().UTC().Format(time.RFC3339Nano)
	_, err := tx.Exec(
		`INSERT INTO outbox (event_id, type, payload, created_at) VALUES (?, ?, ?, ?)`,
		eventID, ev.Type, string(ev.Payload), createdAt,
	)
	if err != nil {
		return fmt.Errorf("outbox: append %s: %w", ev.Type, err)
	}
	return nil
}

// Ring wakes every parked feed connection (§4.3). It carries no data — it says
// only "look sooner". The caller invokes it AFTER tx.Commit; a missed ring loses
// nothing because the durable row is the source of truth and the keepalive timer
// re-polls regardless.
func (o *Outbox) Ring() {
	o.mu.Lock()
	close(o.bell)
	o.bell = make(chan struct{})
	o.mu.Unlock()
}

// subscribe returns the current doorbell channel. A feed connection grabs it
// before fetching, so a Ring that fires between the fetch and the park still
// wakes it (the channel it holds is the one that gets closed).
func (o *Outbox) subscribe() <-chan struct{} {
	o.mu.Lock()
	defer o.mu.Unlock()
	return o.bell
}

// eventRow is one outbox row as read for the feed.
type eventRow struct {
	seq       int64
	eventID   string
	typ       string
	payload   string
	createdAt string
}

// fetch returns up to limit events strictly after afterSeq, in seq order
// (§9.2). It returns the rows and the seq of the last row (or afterSeq when
// none), which becomes the next fetch cursor. The bounded limit keeps producer
// memory bounded and the backlog on disk (§6.1).
func (o *Outbox) fetch(ctx context.Context, afterSeq int64, limit int) ([]eventRow, int64, error) {
	rows, err := o.db.QueryContext(ctx,
		`SELECT seq, event_id, type, payload, created_at
		   FROM outbox
		  WHERE seq > ?
		  ORDER BY seq
		  LIMIT ?`, afterSeq, limit)
	if err != nil {
		return nil, afterSeq, fmt.Errorf("outbox: fetch: %w", err)
	}
	defer rows.Close()

	last := afterSeq
	var out []eventRow
	for rows.Next() {
		var r eventRow
		if err := rows.Scan(&r.seq, &r.eventID, &r.typ, &r.payload, &r.createdAt); err != nil {
			return nil, afterSeq, fmt.Errorf("outbox: scan: %w", err)
		}
		out = append(out, r)
		last = r.seq
	}
	if err := rows.Err(); err != nil {
		return nil, afterSeq, fmt.Errorf("outbox: fetch rows: %w", err)
	}
	return out, last, nil
}

// headSeq is the highest seq present (0 when empty).
func (o *Outbox) headSeq(ctx context.Context) (int64, error) {
	var v int64
	err := o.db.QueryRowContext(ctx, `SELECT COALESCE(MAX(seq), 0) FROM outbox`).Scan(&v)
	if err != nil {
		return 0, fmt.Errorf("outbox: head seq: %w", err)
	}
	return v, nil
}

// minSeq is the lowest seq still retained (0 when empty).
func (o *Outbox) minSeq(ctx context.Context) (int64, error) {
	var v int64
	err := o.db.QueryRowContext(ctx, `SELECT COALESCE(MIN(seq), 0) FROM outbox`).Scan(&v)
	if err != nil {
		return 0, fmt.Errorf("outbox: min seq: %w", err)
	}
	return v, nil
}

// checkCursor performs the connect-time resync/epoch check (§10.1) on a
// presented Last-Event-ID. It returns a non-empty resync reason when the cursor
// cannot be honoured, otherwise the seq to resume strictly after. The epoch
// comparison happens FIRST (§10.1): only it catches a cross-restore seq
// collision (§4.5), which the other reasons cannot see.
func (o *Outbox) checkCursor(ctx context.Context, cursor string) (reason string, seq int64, err error) {
	generation, s, ok := parseCursor(cursor)
	if !ok {
		return reasonUnintelligible, 0, nil
	}
	if generation != o.generation {
		return reasonStaleEpoch, 0, nil
	}
	head, err := o.headSeq(ctx)
	if err != nil {
		return "", 0, err
	}
	if s > head {
		// Cursor is ahead of our head — e.g. we were restored from an older
		// backup. Position is invalid; no controlled-leg event was lost.
		return reasonDiverged, 0, nil
	}
	min, err := o.minSeq(ctx)
	if err != nil {
		return "", 0, err
	}
	// The consumer's next expected event is s+1. If that has been trimmed below
	// the retention horizon, events it never received are gone — real loss.
	if min > 0 && s+1 < min {
		return reasonPastHorizon, 0, nil
	}
	return "", s, nil
}

// EnsureGeneration reads the generation token from its sidecar file, minting
// and persisting a fresh one when the file is absent or empty (§9.3). The token
// lives outside the DB file so a file-level restore cannot roll it back; any
// appkit restore re-mints (the restore verb removes the sidecar at the dispatch
// chokepoint), and the operator bin/restore does the same for its out-of-band S3
// path, so the next boot mints a new epoch.
// An empty path yields an ephemeral in-process token (tests only).
func EnsureGeneration(path string) (string, error) {
	if path == "" {
		return newULID(), nil
	}
	b, err := os.ReadFile(path)
	if err == nil {
		if g := strings.TrimSpace(string(b)); g != "" {
			return g, nil
		}
	} else if !errors.Is(err, os.ErrNotExist) {
		return "", err
	}
	g := newULID()
	if err := os.WriteFile(path, []byte(g+"\n"), 0o640); err != nil {
		return "", err
	}
	return g, nil
}

// runStartupProbe confirms the database actually refuses a second concurrent
// write transaction (§5.3 startup behavioural probe). It opens two independent
// connections with busy_timeout(0), takes a write lock on the first with BEGIN
// IMMEDIATE, and asserts the second BEGIN IMMEDIATE is rejected. If the second
// is NOT refused, the ordering guarantee is gone and the producer must crash, so
// this returns an error. An empty or in-memory DSN is skipped — independent
// in-memory handles do not share a database, so the probe is meaningless there.
func runStartupProbe(dbPath string) error {
	if dbPath == "" || strings.Contains(dbPath, ":memory:") || strings.Contains(dbPath, "mode=memory") {
		return nil
	}
	dsn := fmt.Sprintf("file:%s?_pragma=journal_mode(WAL)&_pragma=busy_timeout(0)", dbPath)
	ctx := context.Background()

	open := func() (*sql.Conn, func(), error) {
		db, err := sql.Open("sqlite", dsn)
		if err != nil {
			return nil, nil, err
		}
		db.SetMaxOpenConns(1)
		conn, err := db.Conn(ctx)
		if err != nil {
			db.Close()
			return nil, nil, err
		}
		return conn, func() { conn.Close(); db.Close() }, nil
	}

	a, closeA, err := open()
	if err != nil {
		return fmt.Errorf("outbox: startup probe open A: %w", err)
	}
	defer closeA()
	b, closeB, err := open()
	if err != nil {
		return fmt.Errorf("outbox: startup probe open B: %w", err)
	}
	defer closeB()

	if _, err := a.ExecContext(ctx, "BEGIN IMMEDIATE"); err != nil {
		return fmt.Errorf("outbox: startup probe: first BEGIN IMMEDIATE failed: %w", err)
	}
	defer a.ExecContext(ctx, "ROLLBACK")

	if _, err := b.ExecContext(ctx, "BEGIN IMMEDIATE"); err == nil {
		// The second write transaction was admitted: ordering is not guaranteed.
		b.ExecContext(ctx, "ROLLBACK")
		return errors.New("outbox: startup probe failed: a second concurrent write transaction was NOT refused; single-writer ordering is not guaranteed — refusing to serve")
	}
	return nil
}
