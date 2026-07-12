package webhooks

import (
	"context"
	"encoding/base64"
	"encoding/json"
	"fmt"
	"time"

	"eventplane/outbox"

	"webhooks/internal/db"
)

// kindReceived is the single fact kind this service publishes: one fact per
// accepted inbound webhook trigger.
const kindReceived = "received"

// webhookReceivedPayload is the per-event snapshot marshaled into the outbox
// envelope. owner is always the STORED owner of the webhook (wh.OwnerEmail),
// never caller input, and body is base64.StdEncoding of the raw request bytes so
// a non-UTF8/binary body round-trips byte-for-byte through JSON.
type webhookReceivedPayload struct {
	Name        string `json:"name"`
	Owner       string `json:"owner"`
	ReceivedAt  string `json:"received_at"` // RFC3339Nano UTC from the injected clock
	ContentType string `json:"content_type"`
	Body        string `json:"body"` // base64.StdEncoding of the raw request bytes
}

// Events is the registry of every event family this service may emit. Append
// rejects any kind not declared here, so this is the single source of truth for
// what "webhooks" publishes onto the event plane.
var Events = outbox.Registry{
	{
		Kind:        kindReceived,
		Subject:     "/<hook name>",
		Description: "An inbound webhook trigger was accepted and recorded.",
		Sample: webhookReceivedPayload{
			Name:        "deploy-hook",
			Owner:       "owner@example.com",
			ReceivedAt:  "2026-06-25T12:00:00Z",
			ContentType: "application/json",
			Body:        base64.StdEncoding.EncodeToString([]byte(`{"ok":true}`)),
		},
	},
}

// Record durably publishes one received event and stamps the webhook's
// last_triggered_at in a SINGLE transaction (durable-before-ack, D5): the outbox
// Append and the touch commit together or not at all. The payload's received_at
// and the touch both use one fixed instant from the injected clock, so they are
// equal. owner is the stored wh.OwnerEmail, never caller input. Record returns
// nil only once the row is committed; the doorbell rings best-effort afterward.
func (s *Service) Record(ctx context.Context, wh db.Webhook, contentType string, body []byte) error {
	now := s.clock.Now().UTC()

	raw, err := json.Marshal(webhookReceivedPayload{
		Name:        wh.Name,
		Owner:       wh.OwnerEmail,
		ReceivedAt:  now.Format(time.RFC3339Nano),
		ContentType: contentType,
		Body:        base64.StdEncoding.EncodeToString(body),
	})
	if err != nil {
		return fmt.Errorf("webhooks: marshal payload: %w", err)
	}

	tx, err := s.db.BeginTx(ctx, nil)
	if err != nil {
		return err
	}
	if err := s.Outbox.Append(tx, outbox.Event{Kind: kindReceived, Subject: "/" + wh.Name, Payload: raw}); err != nil {
		tx.Rollback()
		return err
	}
	if err := s.store.TouchLastTriggered(tx, wh.Name, now); err != nil {
		tx.Rollback()
		return err
	}
	if err := tx.Commit(); err != nil {
		tx.Rollback()
		return err
	}

	s.Outbox.Ring() // best-effort doorbell, AFTER commit
	return nil
}
