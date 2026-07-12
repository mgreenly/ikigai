# eventplane — Research

Non-contractual evidence base for the routing-revision design. Two kinds of
ground truth: the governing suite-level contract, and the as-built facts of
this library verified directly in the code on 2026-07-11.

## The governing external contract

**`docs/event-routing-design.md`** (repo root, suite-level) is the addressing
model this spec realizes: the `{id, source, time, kind, subject, payload}`
envelope, the canonical key `source + ":" + kind + subject`, the doublestar
glob dialect, families-not-enumerations reflection, filter-vs-family
validation, and the hard-cutover migration stance. Design uses its values
verbatim and does not re-derive them. Its "Producer key map" table is
direction, not a binding rename list — each service picks its kinds/subjects
in its own spec.

Note: that document (and `eventplane/CLAUDE.md`, and code comments) cite
`docs/event-protocol.md` as the normative wire contract; **that file does not
exist in the tree**. The routing doc acknowledges this and says the protocol
doc, when written, writes against the new addressing model.

## As-built facts (verified in code)

- **Envelope today** (`outbox/feed.go`, `envelope` struct): JSON
  `{id, type, source, time, payload}`. `type` is a flat dotted string
  (e.g. `contact.created`).
- **SSE event frame today** (`outbox/feed.go`, `eventFrame`): the `id:` line
  carries the opaque cursor `<generation>.<seq>`; the **`event:` line carries
  the raw `type` string**; the `data:` line is the compact JSON envelope.
  Reserved control frame names on the `event:` line: `resync`, `caught-up`,
  `status` (plus `: keepalive` comment frames).
- **Consumer dispatch** (`consumer/consumer.go`, `handleFrame`): switches on
  the `event:` line — the three control names are matched literally, anything
  else is a domain event. `parseEvent` takes `Event.Type` **from the `event:`
  line**, and `{id, source, time, payload}` from the envelope JSON.
- **`outbox.SchemaSQL`** (`outbox/schema.go`): table `outbox(seq INTEGER
  PRIMARY KEY AUTOINCREMENT, event_id, type, payload, created_at)` + index
  `idx_outbox_created_at`. `Append` inserts `(event_id, type, payload,
  created_at)`; `fetch` selects the same columns.
- **`consumer.SchemaSQL`** (`consumer/schema.go`): table
  `feed_offset(source PRIMARY KEY, cursor, subscribed, updated_at)`. It holds
  **no event-shape column** — the cursor is opaque TEXT — so the envelope
  revision requires no change to it.
- **`consumer.Config` has no Filter field** (`consumer/consumer.go`): the
  engine invokes the handler for every event; "type filtering is the
  service's job". Filtering lives in consuming services today, not in the
  engine — the revision therefore exposes a matcher, not an engine filter.
- **`consumer.Subscription`** (`consumer/subscription.go`): the declarative
  record `{Source, Filter, Description, Handler}`; `Filter` is documented as
  a stdlib `path.Match` glob over the dotted event type, and
  `Match(eventType) bool` wraps `path.Match`, treating a malformed pattern as
  a silent non-match. Suite-wide grep (2026-07-11): `Match` has exactly
  **one** caller — `notify/internal/push/push.go` (`sub.Match(ev.Type)`).
  appkit (`appkit.go`, `server/server.go`, `verbs.go`, `mcp/mcp.go`) only
  carries `[]consumer.Subscription` and renders `Filter` opaquely in
  reflection, never calling `Match`; prompts/scripts/notify construct
  Subscriptions declaratively.
- **Registry today** (`outbox/registry.go`): `EventType{Type, Description,
  Sample}`; `Registry []EventType` (ordered). `Append` rejects a `Type` not
  declared when the registry is non-empty. `Index()` renders
  `{type, description}` per entry; `Detail(type)` renders
  `{type, description, schema, example}` where the schema is **reflected from
  the Sample's type** and the example is **marshaled from the Sample's
  value** (single source, cannot drift); unknown type yields
  `*UnknownEventTypeError{Type, Valid}`. The schema reflector covers
  string/bool/int*/float*/pointer/slice/struct and panics on anything else.
- **Test style to model** (`consumer/consumer_test.go`,
  `outbox/feed_test.go`): the highest-value tests wire the **real**
  `outbox.FeedHandler()` into an `httptest.Server` and run `consumer.Run`
  against it over a real SQLite database — that substrate is what the
  revision's end-to-end claims must also exercise.
- **Toolchain** (`eventplane/go.mod`, `eventplane/Makefile`): module
  `eventplane`, Go 1.26, sole direct dependency `modernc.org/sqlite`.
  Makefile targets: `test` (`go test ./...`), `vet`, `fmt`. Local dev runs in
  workspace mode via the repo-root `go.work`.

## Matcher: evaluated and not chosen

`path.Match` (stdlib) cannot express `**` — ruled out by the routing doc.
`github.com/bmatcuk/doublestar` implements the dialect but is a third-party
dependency for ~one screen of matching logic; the operator decision is a
**hand-rolled matcher** in this library, with the dialect pinned by an
exhaustive table-driven test instead of by an upstream's semantics.
