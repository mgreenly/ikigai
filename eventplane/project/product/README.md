# eventplane — Product

**Authority: intent.** This document owns the *why* of the current work — who
it serves, what is in and out of scope, and what we promise — stated in
outcome terms only. It does not state mechanism, formats, exit codes, or test
assertions; those belong to design. Product states the promise; design states
the exact, checkable proof of that promise.

## Problem

Events on the suite's event plane are addressed by a flat `type` string from a
closed per-service list, and the *thing the event happened to* — a file path,
a schedule name, a prompt name — is buried in the payload where selectors
cannot see it. A prompt that cares only about PDFs landing under `/bills/`
must be triggered by *every* dropbox file event on the box and burn a full
agent run discovering each one is not for it. Only cron escapes this, by
baking its subject into the type — which in turn breaks the closed-list model
and forces consumers to special-case it. Routing precision exists for one
producer and by accident.

## Purpose

`eventplane` is the suite's shared event-plane library (producer `outbox` +
consumer engine `consumer`). This revision gives every event a producer-chosen
**routing address** — what happened, and to what — and gives every selector in
the suite one shared way to match against it, so a consumer fires for exactly
the events it cares about and nothing else. It realizes, inside this library,
the suite-level addressing model in `docs/event-routing-design.md`.

## Users

Suite service developers (and the build loops acting for them): producers that
publish events through `outbox`, consumers that receive them through
`consumer`, and services that validate or evaluate event selectors (e.g. a
trigger filter) and need the one shared matcher to do it.

## Scope

This revision covers event **addressing and selection** only: the envelope's
routing fields, the canonical routing key, the single matcher, the producer's
declared vocabulary (families) and its reflection/validation surface, and the
consumer-side visibility of the routing fields. It is a **hard cutover** — the
old addressing is replaced, with no dual-format or compatibility period (the
suite is inside its no-live-data migration window).

Nothing else about the event plane changes: delivery semantics (ordering,
at-least-once, stall/skip), cursors and resync, retention, backoff, and the
outbox's atomicity all stand exactly as built. Filters select on the routing
address only — never on payload contents. Each consuming service adopts the
revised addressing in its own spec; this library ships only its own two
package surfaces and the shared schema constants.

## Contractual constants

These come from the suite addressing model (`docs/event-routing-design.md`)
and are promised verbatim:

- An event is addressed by `source` (the producing service), `kind` (the fact
  class, lowercase `[a-z0-9_.-]+`), and `subject` (either empty or a
  `/`-rooted, `/`-separated routing path).
- The canonical routing key is `source + ":" + kind + subject` — e.g.
  `dropbox:create/bills/aws/2026-06.pdf`, `cron:tick/bill-sweep`,
  `ledger:recorded`.
- Selection is one glob over the whole key: `*` matches within a path segment
  (never across `/`), `**` crosses segments, `?` matches one character,
  character classes as in standard glob, no brace expansion.

## What we promise (user-facing behavior)

- A producer publishes an event addressed by kind and subject; the address
  travels with the event and is visible to every consumer.
- A consumer selects events with a single glob against the canonical key —
  `dropbox:create/bills/**/*.pdf` fires for exactly the PDF files under
  `/bills/`, at any depth, and for nothing else; `ledger:recorded` matches a
  subjectless event literally.
- Filter semantics are identical everywhere in the suite, because there is
  exactly one matcher and one key rendering, both owned by this library —
  no service renders or parses keys itself.
- A producer declares its vocabulary as families (kind + subject shape +
  payload sample); reflection describes those families, and a proposed filter
  can be checked against them — a filter that could never match anything the
  source emits is rejected with the declared vocabulary in hand. cron stops
  being a special case: dynamic subjects are ordinary.
- A malformed address never enters the plane: publishing with an invalid kind
  or subject fails loudly at the producer.

## Success criteria (outcomes)

- Publishing an event with kind `create` and subject
  `/bills/aws/2026-06.pdf` from source `dropbox` yields an event whose
  observable address is `dropbox:create/bills/aws/2026-06.pdf`, end to end
  from producer append to consumer delivery.
- A consumer filtering with `dropbox:create/bills/**/*.pdf` receives the
  matching event above and does not receive `dropbox:create/notes.txt` or
  `dropbox:delete/bills/aws/2026-06.pdf`.
- A subjectless event (`ledger:recorded`) is selectable by its literal key.
- Asking a producer's reflection surface describes each family it emits, with
  a payload schema and worked example that agree with each other.
- Validating the filter `dropbox:delete/**` against a producer that only
  declares `create` reports that it cannot match; validating
  `dropbox:create/**` reports that it can.
- Publishing with an uppercase kind, or a subject that is neither empty nor
  `/`-rooted, is refused with an explanatory error.
- The event plane's delivery behavior is unchanged: events still arrive in
  order, at least once, with the same recovery behavior as before this
  revision.
