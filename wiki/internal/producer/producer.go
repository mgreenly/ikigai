// Package producer is wiki's event-plane emit side: the small helper that writes
// the two wiki.* events (design §8) onto the producer outbox. wiki publishes
// EXACTLY two events:
//
//   - wiki.ingest_refused    — a plain, pre-accept outbox write at a door when an
//     oversized ingest is refused (this package owns it: a standalone tx +
//     Append + Ring, since there is no domain transaction to ride on);
//   - wiki.row_dead_lettered — emitted by the failure-path code in the SAME
//     transaction that sets dead_at (P4/P5); that one is appended on the caller's
//     tx, so this package exposes Append for it rather than owning the tx.
//
// The outbox + DB are injected at the composition root (the appkit Producer hook
// hands over the constructed *outbox.Outbox; cmd/wiki captures the shared DB).
package producer

import (
	"context"
	"database/sql"
	"encoding/json"
	"fmt"

	"eventplane/outbox"

	"wiki/internal/events"
)

// Producer emits wiki's outbox events. A nil Producer (or a nil ob) makes every
// emit a no-op — the same nil-Outbox tolerance the rest of the suite uses so
// tests and key-less paths run without an event plane.
type Producer struct {
	db *sql.DB
	ob *outbox.Outbox
}

// New builds a Producer over the shared DB and the injected outbox.
func New(db *sql.DB, ob *outbox.Outbox) *Producer {
	return &Producer{db: db, ob: ob}
}

// SetOutbox injects the outbox after construction. This exists for the appkit
// hook ordering: the ingest front doors and consumer doors are constructed in the
// Handlers hook (which has the DB but not yet the outbox), while the outbox
// arrives later in the Producer hook. Constructing the Producer empty in Handlers
// and filling its outbox here keeps the doors' Refuser dependency stable while
// honoring that ordering; until the outbox is set every emit is a safe no-op.
func (p *Producer) SetOutbox(db *sql.DB, ob *outbox.Outbox) {
	p.db = db
	p.ob = ob
}

// IngestRefused writes wiki.ingest_refused in its own transaction and rings the
// feed (§8). It is the pre-accept door refusal — there is no domain write to ride
// on, so the producer owns the whole tx. A nil producer/outbox is a no-op.
func (p *Producer) IngestRefused(ctx context.Context, ev events.IngestRefused) error {
	if p == nil || p.ob == nil {
		return nil
	}
	payload, err := json.Marshal(ev)
	if err != nil {
		return fmt.Errorf("producer: marshal ingest_refused: %w", err)
	}
	tx, err := p.db.BeginTx(ctx, nil)
	if err != nil {
		return fmt.Errorf("producer: begin: %w", err)
	}
	defer tx.Rollback()
	if err := p.ob.Append(tx, outbox.Event{Type: events.TypeIngestRefused, Payload: payload}); err != nil {
		return fmt.Errorf("producer: append ingest_refused: %w", err)
	}
	if err := tx.Commit(); err != nil {
		return fmt.Errorf("producer: commit: %w", err)
	}
	p.ob.Ring()
	return nil
}

// AppendRowDeadLettered appends wiki.row_dead_lettered onto the caller's existing
// transaction (§8) — the failure-path code sets dead_at and emits the event
// atomically in one tx, then Rings after its own Commit. The caller owns the tx
// and the Ring; this method only stages the Append. A nil producer/outbox is a
// no-op. (The failure-path caller lands in P4/P5.)
func (p *Producer) AppendRowDeadLettered(tx *sql.Tx, ev events.RowDeadLettered) error {
	if p == nil || p.ob == nil {
		return nil
	}
	payload, err := json.Marshal(ev)
	if err != nil {
		return fmt.Errorf("producer: marshal row_dead_lettered: %w", err)
	}
	if err := p.ob.Append(tx, outbox.Event{Type: events.TypeRowDeadLettered, Payload: payload}); err != nil {
		return fmt.Errorf("producer: append row_dead_lettered: %w", err)
	}
	return nil
}

// Ring wakes parked feed connections after the caller commits a tx carrying a
// row_dead_lettered append. Exposed so the P4/P5 failure path can ring its own
// transaction. A nil producer/outbox is a no-op.
func (p *Producer) Ring() {
	if p == nil || p.ob == nil {
		return
	}
	p.ob.Ring()
}
