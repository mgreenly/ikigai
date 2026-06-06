// Package push is notify's domain: it turns event-plane events into best-effort
// ntfy.sh notifications. It is the consumer-side mirror of crm's internal/contacts
// producer domain — where crm owns the contact.created payload shape, notify owns
// the effect that reacts to it.
//
// The effect is a **best-effort external hop** (event-protocol.md §11.2): notify
// attempts the ntfy POST once, fire-and-forget, logs the result, and never
// retries. The eventplane consumer engine commits the cursor regardless (decision
// 1, 8), so the controlled leg (crm → notify) stays at-least-once while end-user
// delivery is intentionally unreliable. There is no pending table, no retry
// state, and no dedup — duplicate pushes on reconnect are expected and acceptable
// (decision 2).
package push

import (
	"context"
	"encoding/json"
	"fmt"
	"io"
	"log/slog"
	"net/http"
	"strings"
	"time"

	"eventplane/consumer"
)

// pushTimeout bounds a single ntfy request so a fire-and-forget goroutine always
// terminates even against a black-holed connection (decision 16). The push is
// async — the handler returns immediately and the engine commits the cursor
// without waiting — so this timeout is the only thing that reaps the goroutine.
const pushTimeout = 10 * time.Second

// Client is a thin ntfy.sh publisher. The base URL is configuration
// (NOTIFY_NTFY_BASE_URL, so tests can point it at a mock); the topic and token
// are deployment SECRETS injected as environment configuration at the
// composition root (§11.2) — this package only carries the values it is handed
// and never logs them.
type Client struct {
	base  string
	topic string
	token string
	http  *http.Client
	log   *slog.Logger
}

// NewClient builds a push Client. baseURL defaults the scheme/host (e.g.
// "https://ntfy.sh"); topic is the ntfy topic; token is the bearer credential.
func NewClient(baseURL, topic, token string, logger *slog.Logger) *Client {
	if logger == nil {
		logger = slog.Default()
	}
	return &Client{
		base:  strings.TrimRight(baseURL, "/"),
		topic: topic,
		token: token,
		http:  &http.Client{Timeout: pushTimeout},
		log:   logger,
	}
}

// Send fires one best-effort ntfy push (§11.2): POST <base>/<topic> with the
// message as the request body, the Title header, and Authorization: Bearer
// <token>. A non-2xx response or a transport error is logged at WARN and dropped
// — never retried, never surfaced. The topic and token are never logged.
func (c *Client) Send(ctx context.Context, title, message string) {
	url := c.base + "/" + c.topic
	req, err := http.NewRequestWithContext(ctx, http.MethodPost, url, strings.NewReader(message))
	if err != nil {
		c.log.Warn("push: build request failed", "err", err)
		return
	}
	req.Header.Set("Title", title)
	if c.token != "" {
		req.Header.Set("Authorization", "Bearer "+c.token)
	}

	resp, err := c.http.Do(req)
	if err != nil {
		// Best-effort: a failed external call is simply lost (§11.2).
		c.log.Warn("push: ntfy request failed (dropped)", "err", err)
		return
	}
	defer resp.Body.Close()
	io.Copy(io.Discard, resp.Body) //nolint:errcheck // drain to allow connection reuse
	if resp.StatusCode/100 != 2 {
		c.log.Warn("push: ntfy returned non-2xx (dropped)", "status", resp.StatusCode)
		return
	}
	c.log.Debug("push: sent", "title", title)
}

// contactCreated is the slice of crm's contact.created payload (event-protocol.md
// §8.6) that notify needs: just the display name (decision 6/7 — Title "New
// contact", body = display_name only).
type contactCreated struct {
	DisplayName string `json:"display_name"`
}

// Subscription is notify's single declared event-plane in-edge: it listens to
// crm's contact.created and fires a best-effort ntfy push in reaction. It is the
// ONE source of truth — the consumer Handler matches each event against it
// (sub.Match) and the reflection tool reports it via Spec.Subscriptions, so the
// runtime filter and what reflection advertises cannot drift (decision 10). The
// Handler field is left unset here; the engine wiring uses Subscription only as a
// declared graph edge, while Handler builds the effect separately.
func Subscription() consumer.Subscription {
	return consumer.Subscription{
		Source:      "crm",
		Filter:      "contact.created",
		Description: "fires a best-effort ntfy.sh push (Title \"New contact\", body = display_name) for every contact created",
	}
}

// Handler returns the consumer.Handler notify hands to the engine. It runs the
// effect only for contact.created (consumer-side filtering, §7.3) and ignores
// every other type — the engine still commits the cursor for those, so they do
// not re-arrive (§7.3, §10). For a matched event it fires the push ASYNCHRONOUSLY
// (decision 16): the POST runs in its own goroutine on a detached, timeout-bound
// context so the handler returns immediately and the engine commits the cursor
// without waiting. A push therefore never blocks the cursor advance, and a slow
// or dead ntfy never stalls the feed.
func Handler(c *Client, logger *slog.Logger) consumer.Handler {
	if logger == nil {
		logger = slog.Default()
	}
	sub := Subscription() // the SAME declared in-edge reflection reports (decision 10)
	return func(ctx context.Context, ev consumer.Event) error {
		if !sub.Match(ev.Type) {
			return nil // not ours — the engine advances the cursor anyway (§7.3)
		}
		var p contactCreated
		if err := json.Unmarshal(ev.Payload, &p); err != nil {
			// A malformed payload is logged and dropped; best-effort tolerates the
			// loss and the engine still advances the cursor (decision 8).
			return fmt.Errorf("push: decode contact.created %s: %w", ev.ID, err)
		}
		go func(displayName string) {
			// Detached from the engine's request context (the handler has already
			// returned) but bounded by pushTimeout via the client and this ctx, so
			// the goroutine always terminates (decision 16).
			ctx, cancel := context.WithTimeout(context.Background(), pushTimeout)
			defer cancel()
			c.Send(ctx, "New contact", displayName)
		}(p.DisplayName)
		return nil
	}
}
