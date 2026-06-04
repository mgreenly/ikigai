package ledger

import (
	"database/sql"
	"encoding/json"
	"fmt"

	"eventplane/outbox"
)

// eventTimeFormat matches the read API's timestamp rendering (internal/mcp
// tools.go) so the event payload is the same shape the read API returns.
const eventTimeFormat = "2006-01-02T15:04:05.000000000Z07:00"

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
	return o.ob.Append(tx, ev)
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
	return outbox.Event{Type: "transaction.recorded", Payload: raw}, nil
}
