package gmail

import (
	"database/sql"
	"encoding/json"
	"fmt"

	"eventplane/outbox"
)

// events.go holds the producer's event payload builders and the EventSink seam.
// The gmail service is an event-plane producer: each poll of the Gmail History
// API derives zero or more mail-change events, appended on the SAME tx as the
// cursor advance so the events are emitted iff the cursor moves (decisions §1,
// the "emitted == recorded as emitted" pattern).
//
// The three event types — mail.received / mail.sent / mail.deleted — and their
// payload shapes are the producer's complete published set (decisions §1 table).
// The published-event Registry used for reflection + Append-time validation
// lives in internal/mcp (mcp.Events, wired via Spec.Events); the wire payload
// structs here MUST match those registry Sample shapes field-for-field.

// Event type names (decisions §1). These mirror the constants in internal/mcp;
// kept here too so the producer engine has no import dependency on the mcp
// package (mcp is the HTTP transport layer, the producer is the domain layer).
const (
	EventMailReceived = "mail.received"
	EventMailSent     = "mail.sent"
	EventMailDeleted  = "mail.deleted"
)

// eventTimeFormat matches the suite's RFC3339Nano UTC rendering used across the
// other producers' payload timestamps.
const eventTimeFormat = "2006-01-02T15:04:05.000000000Z07:00"

// mailReceivedPayload is the wire shape of a mail.received event (decisions §1
// table): an inbound message that landed in INBOX.
type mailReceivedPayload struct {
	ID         string `json:"id"`
	ThreadID   string `json:"thread_id"`
	From       string `json:"from"`
	Subject    string `json:"subject"`
	Snippet    string `json:"snippet"`
	ReceivedAt string `json:"received_at"`
}

// mailSentPayload is the wire shape of a mail.sent event: a message carrying
// SENT (and not INBOX) — our own sends, via MCP or the Gmail UI.
type mailSentPayload struct {
	ID       string `json:"id"`
	ThreadID string `json:"thread_id"`
	To       string `json:"to"`
	Subject  string `json:"subject"`
	Snippet  string `json:"snippet"`
	SentAt   string `json:"sent_at"`
}

// mailDeletedPayload is the wire shape of a mail.deleted event: a message moved
// to Trash (labelsAdded: TRASH), not a permanent expunge.
type mailDeletedPayload struct {
	ID        string `json:"id"`
	ThreadID  string `json:"thread_id"`
	Subject   string `json:"subject"`
	DeletedAt string `json:"deleted_at"`
}

// MailEvent is the in-memory shape of a derived mail event before it is
// marshaled into the outbox payload. The engine builds one per applicable
// history record (after MessageGet enrichment) and hands it to the EventSink.
// Fields not relevant to a given Type are simply left empty (From for sent, To
// for received, etc.). OccurredAt is RFC3339Nano UTC.
type MailEvent struct {
	Type       string // EventMailReceived | EventMailSent | EventMailDeleted
	ID         string
	ThreadID   string
	From       string // mail.received only
	To         string // mail.sent only
	Subject    string
	Snippet    string // mail.received / mail.sent only
	OccurredAt string
}

// buildPayload marshals a MailEvent into the outbox event for its type. The
// per-type payload struct keeps the wire shape identical to the mcp.Events
// registry Sample (decisions §1 table).
func buildPayload(ev MailEvent) (outbox.Event, error) {
	var v any
	switch ev.Type {
	case EventMailReceived:
		v = mailReceivedPayload{
			ID:         ev.ID,
			ThreadID:   ev.ThreadID,
			From:       ev.From,
			Subject:    ev.Subject,
			Snippet:    ev.Snippet,
			ReceivedAt: ev.OccurredAt,
		}
	case EventMailSent:
		v = mailSentPayload{
			ID:       ev.ID,
			ThreadID: ev.ThreadID,
			To:       ev.To,
			Subject:  ev.Subject,
			Snippet:  ev.Snippet,
			SentAt:   ev.OccurredAt,
		}
	case EventMailDeleted:
		v = mailDeletedPayload{
			ID:        ev.ID,
			ThreadID:  ev.ThreadID,
			Subject:   ev.Subject,
			DeletedAt: ev.OccurredAt,
		}
	default:
		return outbox.Event{}, fmt.Errorf("gmail: unknown event type %q", ev.Type)
	}
	raw, err := json.Marshal(v)
	if err != nil {
		return outbox.Event{}, fmt.Errorf("marshal %s payload: %w", ev.Type, err)
	}
	return outbox.Event{Kind: ev.Type, Payload: raw}, nil
}

// EventSink is the producer seam the engine appends to inside the per-poll tx.
// The concrete implementation wraps the eventplane outbox (outboxProducer). It
// is an interface so a unit test can inject a recording fake that captures
// emitted events on a real tx without a live outbox, and so emission can be
// disabled (nil sink) when desired.
type EventSink interface {
	// AppendMailEvent appends one mail event on the caller's tx, atomic with the
	// cursor advance.
	AppendMailEvent(tx *sql.Tx, ev MailEvent) error
	// Ring wakes parked feed connections; called after a successful commit.
	Ring()
}

// outboxProducer adapts the eventplane outbox to the EventSink seam.
type outboxProducer struct {
	ob *outbox.Outbox
}

// NewOutboxProducer wraps an eventplane outbox as an EventSink. main wires the
// result onto the Engine to make the service an event-plane producer.
func NewOutboxProducer(ob *outbox.Outbox) EventSink {
	return &outboxProducer{ob: ob}
}

// AppendMailEvent appends one mail event on the caller's tx (decisions §1).
func (o *outboxProducer) AppendMailEvent(tx *sql.Tx, ev MailEvent) error {
	e, err := buildPayload(ev)
	if err != nil {
		return err
	}
	return o.ob.Append(tx, e)
}

// Ring wakes parked feed connections after commit.
func (o *outboxProducer) Ring() { o.ob.Ring() }
