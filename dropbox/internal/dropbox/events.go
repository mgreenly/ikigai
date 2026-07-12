package dropbox

import (
	"database/sql"
	"encoding/json"
	"fmt"
	"net/url"

	"eventplane/outbox"
	"registry"
)

// events.go holds the event payload builders and the producer seam (PLAN.md §5,
// §8). The dropbox service is an event-plane producer: every change to a file's
// mirror state emits one event, appended on the SAME tx as the index change so
// the event is emitted iff the mirror state changed.
//
// The three events — file.created / file.modified / file.deleted — carry a
// REFERENCE to the bytes, never the bytes themselves: a consumer fetches the
// current bytes over the loopback /content endpoint via content_url. Mirrors
// ledger's producer seam: an EventSink interface the Service appends to inside
// the index tx, with a concrete outboxProducer wrapping outbox.Append/Ring. The
// interface lets the engine run with emission DISABLED (Outbox == nil) in unit
// tests without importing the library.

// Event type names (PLAN.md §5).
const (
	EventFileCreated  = "file.created"
	EventFileModified = "file.modified"
	EventFileDeleted  = "file.deleted"
	// OriginDropbox identifies a file change pulled from Dropbox, rather than
	// one written by a suite service.
	OriginDropbox = "dropbox"
)

// Events is the published-event Registry for the reflection tool and Append-time
// validation (wired via Spec.Events). Each entry carries a filled-in Sample
// instance of its real payload struct (filePayload) — the single source for both
// the reflected JSON Schema and the worked example, so schema/example/wire shape
// can't diverge. All three events share the filePayload shape; only the `event`
// discriminator and (for delete) the last-known field semantics differ.
var Events = outbox.Registry{
	{
		Kind:        EventFileCreated,
		Description: "A path not previously in the mirror index now exists. Carries a REFERENCE to the bytes (content_url), never the bytes themselves — fetch current bytes over the loopback /content endpoint. origin is \"dropbox\" for a pulled change or the writing service's client id for a service write.",
		Sample:      sampleFilePayload(EventFileCreated),
	},
	{
		Kind:        EventFileModified,
		Description: "A known path's rev changed (includes a case-only rename). Carries the current rev/content_hash/size and a content_url reference to the bytes. origin is \"dropbox\" for a pulled change or the writing service's client id for a service write.",
		Sample:      sampleFilePayload(EventFileModified),
	},
	{
		Kind:        EventFileDeleted,
		Description: "A known path is gone; one event per indexed file removed (including every file beneath a deleted folder). Carries the file's LAST-KNOWN rev/content_hash/size, read before the in-tx delete. origin is \"dropbox\" for a pulled change or the writing service's client id for a service write.",
		Sample:      sampleFilePayload(EventFileDeleted),
	},
}

// sampleFilePayload is a filled-in filePayload used as the reflection Sample for
// the file.* events. The `event` discriminator is set to the type being sampled.
func sampleFilePayload(eventType string) filePayload {
	return filePayload{
		Event:       eventType,
		Path:        "/notes/meeting.md",
		Rev:         "0123456789abcdef0123456789",
		ContentHash: "9b71d224bd62f3785d96d46ad3ea3d73319bfbc2890caadae2dff72519673ca7",
		Size:        4096,
		ContentURL:  contentURL(registry.BaseURL("dropbox"), "/notes/meeting.md"),
		OccurredAt:  "2026-06-03T12:00:00.000000000Z",
		Origin:      OriginDropbox,
	}
}

// eventTimeFormat matches the read API's timestamp rendering so the event
// payload occurred_at is the same shape the rest of the suite renders.
const eventTimeFormat = "2006-01-02T15:04:05.000000000Z07:00"

// FileEvent is the in-memory shape of a file lifecycle event before it is
// marshaled into the outbox payload. The Service builds one per applied change
// and hands it to the EventSink; the builders below turn it into the wire
// payload with a URL-encoded content_url.
type FileEvent struct {
	Type        string // EventFileCreated | EventFileModified | EventFileDeleted
	Path        string // literal Dropbox path within the app folder
	Rev         string // last-known rev (current on create/modify, last-known on delete)
	ContentHash string // verified Dropbox block-SHA256
	Size        int64  // bytes (last-known on delete)
	OccurredAt  string // RFC3339Nano UTC
	Origin      string // writing service's X-Client-Id, or OriginDropbox
}

// filePayload is the wire shape of a file lifecycle event (PLAN.md §5). The
// `path` field is the literal Dropbox path; the `path` inside content_url is
// URL-encoded.
type filePayload struct {
	Event       string `json:"event"`
	Path        string `json:"path"`
	Rev         string `json:"rev"`
	ContentHash string `json:"content_hash"`
	Size        int64  `json:"size"`
	ContentURL  string `json:"content_url"`
	OccurredAt  string `json:"occurred_at"`
	Origin      string `json:"origin"`
}

// contentURL builds the §5 content_url for a literal Dropbox path: the service's
// /content route under contentBase with the path carried URL-encoded in the
// `path` query value. Resolution of that query through the index (case-fold)
// happens in the /content handler (Phase 5); the builder only encodes.
func contentURL(contentBase, path string) string {
	return contentBase + "/content?path=" + url.QueryEscape(path)
}

// buildFilePayload marshals a FileEvent into the outbox payload, computing the
// URL-encoded content_url from contentBase.
func buildFilePayload(contentBase string, ev FileEvent) (outbox.Event, error) {
	p := filePayload{
		Event:       ev.Type,
		Path:        ev.Path,
		Rev:         ev.Rev,
		ContentHash: ev.ContentHash,
		Size:        ev.Size,
		ContentURL:  contentURL(contentBase, ev.Path),
		OccurredAt:  ev.OccurredAt,
		Origin:      ev.Origin,
	}
	raw, err := json.Marshal(p)
	if err != nil {
		return outbox.Event{}, fmt.Errorf("marshal %s payload: %w", ev.Type, err)
	}
	return outbox.Event{Kind: ev.Type, Payload: raw}, nil
}

// EventSink is the producer seam the Service appends to inside the index tx. The
// concrete implementation wraps the eventplane outbox (outboxProducer). It is an
// interface so the Service can run with emission disabled (Outbox == nil) — and
// so a unit test can inject a recording fake that captures emitted events without
// a real outbox (the engine-test backbone, PLAN.md §10).
type EventSink interface {
	// AppendFileEvent appends one file lifecycle event on the caller's tx, atomic
	// with the files-index change.
	AppendFileEvent(tx *sql.Tx, ev FileEvent) error
	// Ring wakes parked feed connections; called after a successful commit.
	Ring()
}

// outboxProducer adapts the eventplane outbox to the EventSink seam. It holds
// the content base URL so it can render content_url at append time.
type outboxProducer struct {
	ob          *outbox.Outbox
	contentBase string
}

// NewOutboxProducer wraps an eventplane outbox as an EventSink. contentBase is
// the scheme+host(+port) dropbox's own loopback /content route lives at (its
// registry-resolved address); the builders append "/content?path=...". main
// wires the result onto Service.Outbox to make the service an event-plane
// producer.
func NewOutboxProducer(ob *outbox.Outbox, contentBase string) EventSink {
	return &outboxProducer{ob: ob, contentBase: contentBase}
}

// AppendFileEvent appends one file lifecycle event on the caller's tx, atomic
// with the files-index change (PLAN.md §5).
func (o *outboxProducer) AppendFileEvent(tx *sql.Tx, ev FileEvent) error {
	e, err := buildFilePayload(o.contentBase, ev)
	if err != nil {
		return err
	}
	return o.ob.Append(tx, e)
}

// Ring wakes parked feed connections after commit.
func (o *outboxProducer) Ring() { o.ob.Ring() }
