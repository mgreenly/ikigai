// Package consume holds wiki's event-plane consumer doors: the handlers that map
// an upstream event into a single inbox.Accept call (design §2.1). Each upstream
// is one eventplane/consumer.Run loop (wired in cmd/wiki) with its own
// feed_offset cursor; the cursor commits ONLY after Accept returns, giving the
// at-least-once / hash-dedup contract (§2.1). Three handler shapes (§2.1):
//
//   - a suite DOMAIN event (crm, ledger, …) → Accept(kind=event) with the event
//     envelope JSON verbatim as the bytes — the event IS the knowledge;
//   - a DROPBOX file event → fetch content_url, then Accept(kind=document) with
//     the fetched bytes — the event is the delivery mechanism, the FILE is the
//     knowledge, and the envelope is used and discarded;
//   - a cron.<name> tick → Accept(kind=event, source="cron:<name>") — the tick
//     itself is the fact.
//
// Every consumer door stamps the SYSTEM identity in owner (no human acted, §2.2)
// — the literal SystemOwner. Doors are non-interactive (nobody is watching the
// MCP call), so on an oversized refusal they NOTIFY via the producer outbox
// (wiki.ingest_refused, §8) rather than failing silently, then advance the cursor
// (consumer.ErrSkip) so the poison row never stalls the consumer forever.
package consume

import (
	"context"
	"encoding/json"
	"errors"
	"fmt"
	"io"
	"log/slog"
	"net/http"

	"eventplane/consumer"

	"wiki/internal/events"
	"wiki/internal/inbox"
)

// SystemOwner is the attribution stamped on every autonomously-ingested arrival
// (no human acted — §2.2 / design §12.3, the provisional system-identity literal).
const SystemOwner = "system@ikigenba"

// Accepter is the inbox write surface the doors depend on (just inbox.Store's
// Accept + the cap), narrowed to an interface so the doors are unit-testable
// against a fake.
type Accepter interface {
	Accept(ctx context.Context, owner, kind, source, mime, title, tags string, bytes []byte) (inbox.Receipt, error)
	MaxBytes() int64
}

// Refuser emits the wiki.ingest_refused producer event for a door's oversized
// refusal (§8). It is the plain pre-accept outbox write, lifted to an interface
// so the doors can be tested without a live outbox. A nil Refuser makes a refusal
// log-only (the cursor still advances).
type Refuser interface {
	IngestRefused(ctx context.Context, ev events.IngestRefused) error
}

// Doors builds the per-upstream consumer.Handler closures. It carries the shared
// inbox writer, the refusal emitter, an HTTP client for dropbox content fetches,
// and a logger. Constructed once at the composition root (cmd/wiki).
type Doors struct {
	inbox  Accepter
	refuse Refuser
	http   *http.Client
	log    *slog.Logger
}

// New builds a Doors. A nil http client defaults to http.DefaultClient; a nil
// logger to slog.Default(); a nil refuser makes refusals log-only.
func New(in Accepter, refuse Refuser, httpc *http.Client, log *slog.Logger) *Doors {
	if httpc == nil {
		httpc = http.DefaultClient
	}
	if log == nil {
		log = slog.Default()
	}
	return &Doors{inbox: in, refuse: refuse, http: httpc, log: log}
}

// DomainHandler maps a suite domain event (crm, ledger, …) to an inbox arrival:
// Accept(kind=event) with the raw envelope JSON as the bytes. source is
// "<src>:<type>" (e.g. "crm:contact.created"); the title is the event type.
func (d *Doors) DomainHandler() consumer.Handler {
	return func(ctx context.Context, ev consumer.Event) error {
		source := ev.Source + ":" + ev.Type
		// The arrival bytes are the full envelope JSON, verbatim (§2.1).
		body, err := json.Marshal(map[string]any{
			"type":    ev.Type,
			"id":      ev.ID,
			"source":  ev.Source,
			"time":    ev.Time,
			"payload": ev.Payload,
		})
		if err != nil {
			d.log.Error("consume: marshal domain envelope", "type", ev.Type, "err", err)
			return fmt.Errorf("%w: marshal envelope", consumer.ErrSkip)
		}
		return d.accept(ctx, "domain", inbox.KindEvent, source, "application/json", ev.Type, body)
	}
}

// DropboxHandler maps a dropbox file.created / file.modified event to a document
// arrival: fetch content_url, then Accept(kind=document) with the bytes. A
// file.deleted event carries no knowledge to ingest and is skipped (advance the
// cursor). The handler filters by Type itself (the engine delivers EVERY event).
func (d *Doors) DropboxHandler() consumer.Handler {
	return func(ctx context.Context, ev consumer.Event) error {
		switch ev.Type {
		case "file.created", "file.modified":
			// proceed
		default:
			return nil // file.deleted / unrelated → advance, nothing to ingest
		}
		var p struct {
			Path       string `json:"path"`
			ContentURL string `json:"content_url"`
			Size       int64  `json:"size"`
		}
		if err := json.Unmarshal(ev.Payload, &p); err != nil {
			d.log.Error("consume: unmarshal dropbox event", "id", ev.ID, "err", err)
			return fmt.Errorf("%w: bad dropbox payload", consumer.ErrSkip)
		}
		body, err := d.fetch(ctx, p.ContentURL)
		if err != nil {
			// A transient fetch failure must STALL (return a plain error) so the
			// event re-delivers and the file is not silently lost (§2.1 at-least-once).
			d.log.Warn("consume: fetch dropbox content", "url", p.ContentURL, "err", err)
			return fmt.Errorf("fetch dropbox content: %w", err)
		}
		source := "dropbox:" + p.Path
		return d.accept(ctx, "dropbox", inbox.KindDocument, source, "", p.Path, body)
	}
}

// CronHandler maps a cron.<name> tick to Accept(kind=event,
// source="cron:<name>") (§2.1). name is the schedule name (e.g. "daily"); the
// tick envelope is the bytes.
func (d *Doors) CronHandler(name string) consumer.Handler {
	return func(ctx context.Context, ev consumer.Event) error {
		source := "cron:" + name
		body, err := json.Marshal(map[string]any{
			"type":   ev.Type,
			"id":     ev.ID,
			"source": ev.Source,
			"time":   ev.Time,
		})
		if err != nil {
			return fmt.Errorf("%w: marshal cron tick", consumer.ErrSkip)
		}
		return d.accept(ctx, "cron", inbox.KindEvent, source, "application/json", ev.Type, body)
	}
}

// accept is the shared tail of every door: stamp the system identity, call
// Accept, and translate an oversized refusal into a notify-and-skip (a
// non-interactive door must not stall forever on a poison the pipeline provably
// cannot keep, §2.2). Any other Accept error STALLS (returned plain) so the event
// re-delivers — a transient DB failure must not lose the arrival.
func (d *Doors) accept(ctx context.Context, door, kind, source, mime, title string, body []byte) error {
	_, err := d.inbox.Accept(ctx, SystemOwner, kind, source, mime, title, "[]", body)
	if err == nil {
		return nil
	}
	if errors.Is(err, inbox.ErrTooLarge) {
		d.notifyRefused(ctx, door, source, int64(len(body)))
		// Oversized is permanent poison: skip (advance the cursor) so it never
		// stalls the consumer forever (§2.2).
		return fmt.Errorf("%w: oversized arrival from %s", consumer.ErrSkip, source)
	}
	// Any other failure (DB busy, etc.) is transient — STALL and replay.
	return fmt.Errorf("inbox accept (%s): %w", source, err)
}

// notifyRefused emits wiki.ingest_refused for a non-interactive door's oversized
// refusal (§2.2 "non-interactive doors notify on refusal"). A nil refuser makes
// it log-only; an emit failure is logged, never propagated (the cursor still
// advances — the notify is best-effort, the skip is the durable decision).
func (d *Doors) notifyRefused(ctx context.Context, door, source string, size int64) {
	if d.refuse == nil {
		d.log.Warn("consume: oversized refused (no refuser wired)", "door", door, "source", source, "size", size)
		return
	}
	if err := d.refuse.IngestRefused(ctx, events.IngestRefused{
		Door:   door,
		Source: source,
		Size:   size,
		Cap:    d.inbox.MaxBytes(),
	}); err != nil {
		d.log.Error("consume: emit ingest_refused", "source", source, "err", err)
	}
}

// fetch GETs the dropbox content_url over the loopback and returns the bytes.
// content_url is a direct 127.0.0.1 reference (the event plane bypasses nginx).
func (d *Doors) fetch(ctx context.Context, url string) ([]byte, error) {
	req, err := http.NewRequestWithContext(ctx, http.MethodGet, url, nil)
	if err != nil {
		return nil, err
	}
	resp, err := d.http.Do(req)
	if err != nil {
		return nil, err
	}
	defer resp.Body.Close()
	if resp.StatusCode != http.StatusOK {
		return nil, fmt.Errorf("content fetch: status %d", resp.StatusCode)
	}
	return io.ReadAll(resp.Body)
}
