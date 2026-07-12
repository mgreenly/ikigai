# Design — Event Routing Keys (source:kind + subject)

This document revises how events on the suite's event plane are *addressed*.
It changes the envelope's type field and how consumers select events; it does
not change the outbox, the SSE transport, cursors, resync, or delivery
semantics, all of which stand as built in `eventplane`.

The short version: every event gets a **routing key** of the form
`<source>:<kind><subject>`, and consumers select events with a single glob
against that key. `dropbox:create/bills/aws/2026-06.pdf` is an event; a prompt
triggered by `dropbox:create/bills/**/*.pdf` fires for exactly the files it
cares about.

## Context — what we are replacing

Today an event's envelope is `{id, source, time, type, payload}`, where `type`
comes from a **closed per-service registry** (`outbox.Registry`): dropbox
publishes `file.created|modified|deleted`, ledger publishes
`transaction.recorded`, and so on. Reflection enumerates the registry; prompts
validates a trigger's `(source, event_filter)` pair against a static copy of
each producer's vocabulary and matches the filter against `type` with
`path.Match`.

Two pressures broke this model:

1. **cron already violates it, by design.** cron's types are `cron.<name>`,
   minted dynamically from the live crontab. There is no closed vocabulary, so
   prompts special-cases cron as "any `cron.*` filter is accepted." The
   *subject* (the schedule name) is baked into the type, and that is exactly
   what makes cron triggers precise and cheap.
2. **Every other producer's subject is out of reach.** A dropbox
   `file.created` event names its path only in the payload, where filters
   cannot see it. A prompt that cares about `/bills/**` must be triggered by
   *every* file event on the box and spend a full agent run discovering the
   event is not for it. Routing precision exists for cron alone.

The revision generalizes cron's shape to every producer: cron was not the
special case, it was the correct model.

## The core principle

> **An event is addressed by what happened and to what — `kind` and `subject`
> — chosen by the producer as a routing key. The payload remains the detail
> record; the key exists to be matched.**

`kind` is the operation or fact class (`create`, `delete`, `tick`,
`recorded`). `subject` is the producer's name for the thing it happened to (a
mirror path, a schedule name, a prompt name). Together with `source` they form
a key that is *structural in the envelope* and *rendered canonically for
matching*, so producers never build the key by string concatenation and
consumers never parse it apart.

## The envelope

The wire envelope becomes `{id, source, time, kind, subject, payload}`:

- `source` — the producing service, unchanged (`dropbox`, `cron`, `ledger`).
- `kind` — lowercase `[a-z0-9_.-]+`, the fact class. The old `type` names drop
  their redundant noun prefix: `file.created` becomes `create` (the source
  already says dropbox deals in files), `transaction.recorded` becomes
  `recorded`, cron's `cron.<name>` becomes kind `tick`.
- `subject` — a `/`-rooted, `/`-separated routing path, or empty when the kind
  has no natural subject. It is a *name*, never data: dropbox's is the mirror
  path (`/bills/aws/2026-06.pdf`), cron's is the schedule name
  (`/bill-sweep`), prompts' is the prompt name (`/collect-bills`).
- `id`, `time`, `payload` — unchanged. Everything a consumer acts on beyond
  routing stays in the payload (`content_url`, `rev`, amounts, identifiers).

## The canonical key and the matcher

The `eventplane` library owns a single join rule and a single matcher; nothing
else in the suite renders or interprets keys.

**Rendering:**

```
key(source, kind, subject) = source + ":" + kind + subject
```

Since `subject` is either empty or `/`-rooted, the key needs no further
delimiter:

```
dropbox:create/bills/aws/2026-06-inv-1234567.pdf
dropbox:delete/statements/chase/2026-05.pdf
cron:tick/bill-sweep
prompts:run.succeeded/collect-bills
ledger:recorded
```

**Matching** is one glob over the whole key, doublestar dialect:

- `*` matches any run of characters **within** a path segment (never `/`).
- `**` matches across segments.
- `?` and character classes as in standard glob; no brace expansion.

```
dropbox:create/bills/**/*.pdf      files under /bills/, any depth, PDFs only
dropbox:*/bills/aws/**            any operation under one vendor's folder
cron:tick/bill-sweep              one schedule, exactly
ledger:recorded                   a subjectless kind matches literally
```

The stdlib `path.Match` cannot express `**`; the matcher is implemented once
in `eventplane` and consumed by every selector in the suite: prompts triggers,
`consumer.Config` filters, and any future subscriber. Filter semantics can
never diverge between consumers because there is exactly one implementation.

## Reflection: families, not enumerations

A producer's registry stops being a closed list of concrete types and becomes
a list of **families**: `{kind, subject pattern, description, payload
schema/sample}`. dropbox declares `create/<mirror path>`, `modify/<mirror
path>`, `delete/<mirror path>`; cron declares `tick/<schedule name>` and
reflection may additionally enumerate the live schedule names, as it does
today.

Trigger and filter validation follows: a filter is accepted when it is
well-formed and **could match some family** of the named source (glob
intersection with the family's `kind`, subject left open). This replaces
prompts' static per-source vocabulary and deletes the cron special case; cron
becomes an ordinary producer whose subjects happen to be dynamic, which is now
true of dropbox too.

## Producer key map

The full current vocabulary, revised:

| today (`source` + `type`) | revised key |
|---|---|
| crm `contact.created` (etc.) | `crm:contact.created/<contact-id>` |
| ledger `transaction.recorded` | `ledger:recorded` |
| dropbox `file.created/modified/deleted` | `dropbox:create\|modify\|delete/<mirror path>` |
| cron `cron.<name>` | `cron:tick/<name>` |
| scripts `scripts.succeeded/failed` | `scripts:succeeded\|failed/<script name>` |
| prompts `run.succeeded/failed` | `prompts:run.succeeded\|run.failed/<prompt name>` |
| gmail `mail.received` | `gmail:received` (subject open until a routing need appears) |
| webhooks (inbound fire) | `webhooks:received/<hook name>` |

Each producer's exact kinds and subject choice are that service's design
decisions, made in its own spec loop; this table records the direction agreed
suite-wide, not a binding rename list.

## Migration

There is no compatibility mode. The envelope field change, the canonical key,
the matcher, producer registries, prompts trigger storage/validation, and
every consumer's filter move together, coordinated as one revision across the
affected services' spec loops (eventplane first; producers and consumers
after). The suite is inside its no-live-data migration window: consumer
cursors and trigger rows can be wiped and re-created rather than migrated.
This is precisely the kind of protocol change that is cheap now and expensive
after real customers exist, which is why it lands now.

(The `docs/event-protocol.md` wire contract that `eventplane/CLAUDE.md` and
the root README cite does not exist in the tree; when it is written, it writes
against *this* addressing model.)

## Decisions resolved

- **Structure in the envelope, string for matching.** Events carry `kind` and
  `subject` as fields; the canonical `source:kind<subject>` rendering exists
  for matching, logs, and human eyes. Nobody parses a key.
- **Subjects are producer-chosen routing names**, `/`-rooted or empty, and
  carry no data that belongs in the payload.
- **One matcher, doublestar dialect**, implemented in `eventplane`, used by
  every selector.
- **Reflection describes families**; validation checks a filter against
  families; the cron special case dissolves.
- **Old `type` names lose their redundant prefixes** (`file.created` →
  `create`), since `source` carries that information in the key.
- **Hard cutover inside the migration window**, no dual-format period.

## Non-goals

- **Payload querying.** The subject is the only routable dimension beyond
  source and kind. Filters never reach into payload fields; a consumer that
  needs payload-level selection filters in its handler, as today.
- **Delivery-semantics changes.** Ordering, at-least-once, stall/skip, resync,
  and retention are untouched.
- **A broker or topic infrastructure.** Keys are matched by each consumer
  against each upstream's feed, exactly as filters are today; nothing central
  routes.
- **Cross-source subscription syntax.** A filter still applies to one
  upstream's feed. `source:` in the key makes filters self-describing and
  future-proofs a multi-source selector, but none is being built.
