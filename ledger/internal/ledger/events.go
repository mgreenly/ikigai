package ledger

import (
	"database/sql"
	"encoding/json"
	"fmt"
	"strings"
	"time"

	"eventplane/outbox"
	"ledger/internal/ids"
)

// eventTimeFormat matches the read API's timestamp rendering (internal/mcp
// tools.go) so the event payload is the same shape the read API returns.
const eventTimeFormat = "2006-01-02T15:04:05.000000000Z07:00"

// eventTransactionRecorded is the one first-wave event type string, declared
// once and referenced at both the emit site and the reflection Registry so the
// two cannot drift (the reflection plan's single-source-of-truth rule).
const eventTransactionRecorded = "transaction.recorded"

// Events is the published-event Registry for the reflection tool and Append-time
// validation (wired via Spec.Events). Each entry carries a filled-in Sample
// instance of its real payload struct — the single source for both the reflected
// JSON Schema and the worked example, so schema/example/wire shape can't diverge.
var Events = outbox.Registry{
	{
		Kind:        eventTransactionRecorded,
		Description: "A balanced double-entry transaction was committed to the immutable journal (including a reversal mirror, whose reverses_id is non-null). Carries the whole transaction and its postings so a consumer can rebuild balances off the feed.",
		Sample:      sampleTransactionRecorded,
	},
}

// sampleTransactionRecorded is a filled-in transactionRecordedPayload used as the
// reflection Sample for transaction.recorded.
var sampleTransactionRecorded = transactionRecordedPayload{
	ID:          "01J9Z2K7P3QC8M4R6T0V2X5YA",
	Date:        "2026-06-01",
	Description: "Acme — June hosting",
	CreatedAt:   "2026-06-01T12:00:00.000000000Z",
	ReversesID:  nil,
	ExternalRef: stringPtr("dropbox:/bills/aws/2026-06.pdf@content_hash"),
	Postings: []postingPayload{
		{ID: "01J9Z2K7P3QC8M4R6T0V2X5YB", Account: "Assets:Bank:Checking", AmountCents: 12000, Status: "pending", Ord: 0},
		{ID: "01J9Z2K7P3QC8M4R6T0V2X5YC", Account: "Income:Sales", AmountCents: -12000, Status: "pending", Ord: 1},
	},
}

func stringPtr(s string) *string { return &s }

// outboxProducer adapts the eventplane outbox to the Service's EventSink seam.
// It is the concrete implementation of EventSink; the Service holds it as an
// interface so the domain can run with emission disabled without importing the
// library.
type outboxProducer struct {
	ob *outbox.Outbox
}

// NewOutboxProducer wraps an eventplane outbox as an EventSink. main wires the
// result onto Service.Outbox to make the service an event-plane producer.
func NewOutboxProducer(ob *outbox.Outbox) EventSink {
	return &outboxProducer{ob: ob}
}

// AppendRecorded appends the transaction.recorded event on the caller's tx,
// atomic with the journal write (PLAN.md §6). Every committed transaction emits
// exactly one such event — reversal mirrors included, because Record and Reverse
// share the same persist helper, so a consumer rebuilding balances off the feed
// never silently misses a correction.
func (o *outboxProducer) AppendRecorded(tx *sql.Tx, t Transaction) error {
	ev, err := transactionRecordedEvent(t)
	if err != nil {
		return err
	}
	if err := o.ob.Append(tx, ev); err == nil {
		return nil
	} else if !strings.Contains(err.Error(), "no column named kind") {
		return err
	}
	// Existing ledger databases retain the immutable pre-kind outbox migration.
	// Keep those databases writable while eventplane's current producer supports
	// the newer kind/subject table shape.
	_, err = tx.Exec(`INSERT INTO outbox (event_id, type, payload, created_at) VALUES (?, ?, ?, ?)`,
		ids.NewULID(), eventTransactionRecorded, string(ev.Payload), time.Now().UTC().Format(time.RFC3339Nano))
	if err != nil {
		return fmt.Errorf("outbox: append %s to legacy table: %w", eventTransactionRecorded, err)
	}
	return nil
}

// Ring wakes parked feed connections after commit.
func (o *outboxProducer) Ring() { o.ob.Ring() }

// transactionRecordedPayload is the transaction.recorded snapshot: the whole
// transaction and its postings. reverses_id is non-null on a reversal mirror so
// a consumer can already tell a correction from an original before the
// second-wave transaction.reversed event exists.
type transactionRecordedPayload struct {
	ID          string           `json:"id"`
	Date        string           `json:"date"`
	Description string           `json:"description"`
	CreatedAt   string           `json:"created_at"`
	ReversesID  *string          `json:"reverses_id"`
	ExternalRef *string          `json:"external_ref"`
	Postings    []postingPayload `json:"postings"`
}

type postingPayload struct {
	ID          string `json:"id"`
	Account     string `json:"account"`
	AmountCents int64  `json:"amount_cents"`
	Status      string `json:"status"`
	Ord         int    `json:"ord"`
}

// transactionRecordedEvent builds the transaction.recorded outbox event from a
// freshly persisted transaction. The library wraps this opaque payload in the
// uniform envelope at serialize time; ledger owns only the payload shape.
func transactionRecordedEvent(t Transaction) (outbox.Event, error) {
	p := transactionRecordedPayload{
		ID:          t.ID,
		Date:        t.Date,
		Description: t.Description,
		CreatedAt:   t.CreatedAt.UTC().Format(eventTimeFormat),
		ReversesID:  t.ReversesID,
		ExternalRef: t.ExternalRef,
		Postings:    make([]postingPayload, 0, len(t.Postings)),
	}
	for _, pg := range t.Postings {
		p.Postings = append(p.Postings, postingPayload{
			ID:          pg.ID,
			Account:     pg.Account,
			AmountCents: pg.AmountCents,
			Status:      pg.Status,
			Ord:         pg.Ord,
		})
	}
	raw, err := json.Marshal(p)
	if err != nil {
		return outbox.Event{}, fmt.Errorf("marshal transaction.recorded payload: %w", err)
	}
	return outbox.Event{Kind: eventTransactionRecorded, Payload: raw}, nil
}
