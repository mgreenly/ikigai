# Event Plane — Decisions

> **Status: DECIDED for the items below.** The normative contract for the event
> plane — the wire protocol plus the correctness invariants every producer and
> consumer must uphold — is **`event-protocol.md`**, and it is the authority:
> **on any conflict, `event-protocol.md` wins and this doc is corrected to
> match.** This doc is narrower: the current intent for *what we build first*
> (the concrete `crm → notify` slice), not the full contract. It depends on the
> protocol for every shared mechanism and defers to it for rationale.
>
> This is still a **research tech demo**, not production. The goal of the first
> slice is to prove the producer→SSE→consumer→effect chain end to end, the way
> the gated `ikigenba_<svc>_health` MCP tool proves the auth chain (it echoes the
> caller's `owner_email`/`client_id`).
>
> Still-open items are collected at the end — do not treat them as settled.

## Build plan

- **New service `notify`** — a path-routed service `/srv/notify/`, loopback port
  **3003**, started as a clone of `ledger` (health-only skeleton). It then gains
  a second role: an **event consumer** that watches `crm` for new contacts and
  sends a notification.
- **First proof-of-concept event: `contact.created`.** Emitted by `crm` when a
  contact is created; consumed by `notify`, which reacts by sending a push.
- **Shared library in its own (sixth) repo.** The outbox writer, the SSE feed
  handler, the offset-tracking consumer, the in-process notifier, and the
  per-engine `Dialect` live in one Go module, `require`d by `crm` / `ledger` /
  `notify` / `dashboard`. A `go.work` at the `ikigai/` root wires it for local dev.
  It is a *library*, not an on-box service — no port, no nginx fragment.

## Two planes

The suite has two independent communication planes with different trust models:

- **North/south — external, owner-facing.** Claude client → nginx (TLS +
  `auth_request` introspection) → MCP. Request/response. Owner-scoped. *Already
  built.* Unchanged by any of this.
- **East/west — internal, service-to-service.** The **event plane**: outbox
  feeds over **loopback-direct SSE**, consumer offset cursors. Streaming.

**The event feed needs no auth and does not go through nginx.** One box = one
customer = **one owner**, so a service consuming another's feed never crosses an
owner boundary. The only requirement is "not reachable externally," which the
existing loopback binding already provides. nginx's introspection contract
applies solely to the external MCP plane; it does not touch the event plane.
Direct loopback also avoids nginx buffering/timeouts fighting SSE backpressure.

## The outbox (write / publish side)

- **Publish = one `INSERT` into a local outbox table, inside the domain
  transaction that's already happening.** For `contact.created`, the existing
  `CreateContact` transaction gains one statement before `COMMIT`. Same `*sql.Tx`
  as the domain writes → the contact and its event commit together or not at all.
  Atomic outbox, no dual-write, no lost/phantom messages.
- **The write path is subscriber-blind.** It records; it never pushes. Whether
  anyone is connected, and how many, is irrelevant to the producer's write code.
  Adding a consumer later touches no producer write code.
- **`seq INTEGER PRIMARY KEY AUTOINCREMENT`.** The `AUTOINCREMENT` is
  load-bearing, not decoration: without it, if retention ever empties the table,
  the rowid restarts at 1 and a consumer cursored at 500 silently ignores all new
  rows. `AUTOINCREMENT` keeps a persistent high-water mark so `seq` only climbs.
- **The doorbell rings after `COMMIT`, and carries no data.** The notifier
  (`sync.Cond` broadcast) wakes parked feed goroutines *after* the commit returns
  — never inside the tx (the row isn't visible yet). It says "look sooner"; it is
  never the delivery mechanism. A reader that misses the bell loses nothing — the
  durable row is waiting to be queried. Doorbell, not mail.
- **Payload is a self-contained snapshot, not a pointer.** `contact.created`
  carries the fields a consumer needs (e.g. display name, primary email at emit
  time), so a consumer never has to call back into the producer, and the event
  survives the contact later being deleted.

## The ordering invariant

> **Outbox writes are serialized such that seq-assignment order == commit order
> == visibility order.** The consumer's "read everything after my cursor, in
> order" is correct *only* while this holds.

- **SQLite gives this for free.** SQLite permits exactly one write transaction at
  a time (both rollback-journal and WAL — WAL adds concurrent *readers*, never
  concurrent *writers*). A writer holds the write lock from its first write until
  commit, so no second writer can assign the next `seq` until the first commits.
  Assignment order therefore *equals* commit and visibility order, by
  construction. This is a property of the **engine**, not of our pool settings.
- **`SetMaxOpenConns(1)` + `BEGIN IMMEDIATE` are ergonomics, not the guarantee.**
  Single-writer ordering would hold even with a larger pool; those two settings
  just keep SQLite's built-in serialization from surfacing as `SQLITE_BUSY` (pool
  size 1 queues writes in Go) or a deferred-transaction upgrade deadlock
  (`IMMEDIATE` takes the write lock up front).
- **Postgres would NOT get this for free.** Concurrent writers there can commit
  out of `seq` order; correctness requires ordering on the transaction id and a
  stable-frontier watermark (`txid < txid_snapshot_xmin(txid_current_snapshot())`).
  This is exactly the work SQLite makes disappear, and is why the SQLite dialect
  is the simple one. (Detail lives in the dialect, below.)

### Protecting the invariant (defense in depth, all mechanical, none on the hot path)

1. **Encapsulate.** The shared library owns the outbox DB handle, its pool size,
   and its transaction style. Services receive a correctly-configured outbox;
   they can't misconfigure what they don't control.
2. **Concurrency stress test = the executable spec (primary net).** N goroutines
   hammering `Append`+commit concurrently while a reader consumes via the real
   `Fetch`; assert the reader sees a strictly-increasing sequence with **no row
   ever appearing behind its cursor.** Passes on single-writer SQLite; goes red
   in CI the moment anyone introduces concurrent writers or an incorrect dialect.
   Dialect-agnostic — it's the contract every `Dialect` must satisfy.
3. **Startup behavioral probe (fail loudly).** At boot, confirm a second
   concurrent write transaction is actually refused (two `BEGIN IMMEDIATE`; the
   second must get `SQLITE_BUSY`). If it succeeds, the ordering guarantee is gone
   — crash rather than silently corrupt the stream. Tests the *behavior*, so it
   survives a future engine change.
4. **Named invariant documented at the single chokepoint** (`Append`), pointing
   at the test and probe.

Deliberately **not** done: per-write runtime ordering checks (hot-path cost, no
benefit) and contiguity/gap auditing (rollbacks legitimately leave holes; the
invariant is *no row behind the cursor*, not *no gaps*). Note the consumer cannot
detect this failure itself — its `seq > cursor` query filters out the very row
that would prove a violation — which is why detection lives in the test and probe.

## Database-agnostic: the dialect seam + the opaque cursor

We rely on SQLite's free ordering and do **not** build a second backend now. But
the seam is defined so a future Postgres/MySQL backend is one small file, not a
rewrite.

- **`Dialect` interface — the only engine-specific surface.** Two operations:
  `Append(tx, evt)` (write one event into the outbox in the caller's tx) and
  `Fetch(after Cursor, limit)` (return stable events strictly after the cursor,
  in order, plus the next cursor; never past the engine's stability frontier).
  Everything above it — feed handler, doorbell, consumer loop, retention — is
  written once against the interface.
  - **SQLite dialect (build now):** `seq`-only, no watermark. `Fetch` is
    `WHERE seq > ? ORDER BY seq LIMIT ?`. The cursor encodes the `seq` wrapped
    with the mandatory generation token (below), **not** a bare `seq`.
  - **Postgres dialect (documented/stub, not built):** carries `txid`, applies
    the `xmin` watermark, orders by `(txid, seq)`. Cursor encodes `txid:seq`.
- **The cursor is opaque to the consumer.** Contract: *the consumer stores,
  presents, and never inspects it; only the producing dialect mints and
  interprets it.* It is a string; the consumer's only verbs are receive (rides
  with each event), persist (atomically with the effect), and present (on
  (re)connect). No arithmetic, no comparison, no construction → no hidden
  dependency on its shape.
  - **Keyed per upstream.** A consumer of multiple feeds stores one opaque cursor
    *per feed*; opacity is what prevents cross-applying one feed's cursor to
    another.
  - **The one non-opaque bit:** on *first* subscription only, earliest vs latest
    — expressed as the **absence** of a cursor (from the beginning) or a
    **`?from=tail` query parameter** (only new), **never** a command token placed
    in the opaque-cursor channel. A per-consumer product choice, not arithmetic.
  - **Maps 1:1 onto SSE and costs nothing extra.** SSE's `id:` field is already
    an opaque string and `Last-Event-ID` is it echoed back; we adopt that native
    mechanism and add the one rule SSE doesn't enforce: present the **committed**
    cursor, not the last-received one.
  - **Consumer's offset column is `TEXT`, not `INTEGER`** (it stores the opaque
    string).
  - **Mandatory here, free with opacity: the generation/epoch token.** Because
    the cursor is opaque, the producer embeds a **generation/epoch token** inside
    it at no cost to the consumer. This is **not optional** on the metaspot box:
    its lifecycle includes restore-from-snapshot / rebuild (`bin/restore` is a
    mandated verb), and without the token a file-level restore is silently unsafe
    — `sqlite_sequence` rolls back with the DB file, `seq` re-climbs through values
    it already issued, and a consumer would resume onto unrelated post-restore
    events as their natural successors, losing the originals undetectably. So the
    producer **MUST** mint a generation id at DB init, **MUST** re-mint it on every
    restore/rebuild, and **MUST** embed it in every cursor; the connect-time epoch
    check (reject a stale cursor with `resync` reason `stale-epoch`) is required
    **from day one**. Detection is entirely producer-side; the consumer stays none
    the wiser.

## Read model: streaming over SSE (consumer-driven via TCP)

**Decided: streaming, not pull-polling.** A consumer opens **one** long-lived
HTTP connection, presents its cursor once, and the producer streams every event
past it and keeps shipping new ones down that same connection, parking on the
in-process doorbell when caught up.

**This is still consumer-driven — the "pull" intuition is preserved, the
mechanism is just TCP backpressure.** The consumer "asks for the next event" by
*reading the next bytes off the socket*, not by sending a request. When it stops
reading (busy processing), its TCP receive window fills, the producer's next
`write()` blocks in the kernel, and the producer stops sending. "If it doesn't
ask, nothing happens" becomes "if it doesn't read, the producer blocks." The
backlog stays on the producer's disk; consumer memory is O(1); a parked,
caught-up consumer is a goroutine blocked on the netpoller at ~0 CPU.

> **This zero-busy-wait property is the headline of the whole design** — the
> single reason to build this rather than reach for a broker. A quiet box banks
> burstable credits because nobody polls and nobody spins. When we write the
> protocol up properly, this is the lede, not a footnote.

Rejected: pure polling (latency + burns idle CPU/credits, defeating the point)
and long-polling (push-like latency but one connection per event, reinventing
SSE with worse ergonomics).

## Wire format

- **Transport is SSE over a plain loopback HTTP `GET`.** The `GET /feed` request
  *is* the consumer's registration — there is **no** separate "new consumer"
  announcement message or handshake round-trip. The request is self-describing:
  - `X-Consumer-Id: <uuid>` — the forever-UUID (header).
  - `Last-Event-ID: <cursor>` — carries **only** the committed opaque cursor, or
    is absent; **never** a command token. The one-time earliest-vs-latest choice
    is made *outside* this channel: `Last-Event-ID` absent (and no `?from=tail`)
    = "from the beginning"; **`?from=tail`** (a query parameter, with
    `Last-Event-ID` absent) = "only new"; any `Last-Event-ID` value = resume
    strictly after it. That query-parameter choice is the entire non-opaque
    surface of the cursor.
- **Each event is one SSE frame:**
  ```
  id: <opaque cursor>
  event: <message type>
  data: <payload>

  ```
  - `id:` = the opaque cursor (echoed back as `Last-Event-ID` on reconnect).
  - `event:` = the **message type**, used for consumer filtering (below).
  - `data:` = the envelope (below).
- **Lightweight envelope carried in `data:`, designed below.** One outbox can
  carry multiple message types, and a consumer filters to the types it wants. The
  type lives in the SSE `event:` line *and* the envelope, so a consumer can filter
  without parsing the body while the event stays self-describing if logged or
  forwarded. Full shape in **Message envelope**, below.
  - **Filtering runs consumer-side** (for now): the producer streams all types;
    the consumer matches on the `event:` line and runs the domain effect only for
    types it wants. Keeps the producer passive, no query params. Producer-side
    `?types=` is a later bandwidth optimization. **The cursor still advances over
    skipped events** (see Consumer side) — "skip" means "no effect," never "don't
    commit the cursor."
- **Payload encoding: JSON, for now.** Debuggable and `curl`-able, which matches
  the "standard HTTP you can watch" goal. Revisit only if payload size ever
  justifies a binary encoding; the envelope/transport decisions don't depend on
  it.

## Message envelope & the first message (`contact.created`)

**Two layers, kept separate:** a **uniform envelope** (identical shape for every
message type) wrapping a per-type **`payload`** body (the domain snapshot).
Filtering, dedup, ordering, and observability all operate on the envelope; only
the body changes per type.

**The envelope is CloudEvents-aligned** (the CNCF event-envelope spec —
`id`/`source`/`type`/`time`/…), so it's battle-tested and we can adopt the fuller
spec later without a rewrite. We deliberately deviate on one name: the body key
is **`payload`**, not CloudEvents' `data`, to avoid misreading it against SSE's
`data:` wire line.

```json
{
  "id":      "01HQ8ZK9...EVENT",          // stable unique event identity (ULID)
  "type":    "contact.created",            // message type (mirrors the SSE event: line)
  "source":  "crm",                        // emitting service
  "time":    "2026-06-02T15:04:05.123Z",   // when emitted (RFC3339)
  "payload": { ...domain snapshot... }
}
```

Each field earns its place:

- **`id` — required, and *forced by the opaque cursor*.** A consumer dedups
  (idempotency) on *something*, and it cannot use the cursor — the cursor is
  opaque and on another engine would be a compound token it can't key on. So
  opacity *requires* an explicit, non-opaque identity field. A ULID, generated
  once at emit and **stored**, so it is byte-identical on every replay.
- **`type` — required.** Drives filtering/dispatch; mirrored in the SSE `event:`
  line (filter without parsing the body) and kept here (self-describing off-wire).
- **`source` — included** (the most droppable field, but cheap): disambiguates a
  consumer that merges multiple feeds; just a producer-side constant.
- **`time` — included.** Emit timestamp. Distinct from a domain `created_at`:
  `time` is "when emitted" (for `contact.updated` it would differ from the
  entity's creation time).

### Three `id`s at three layers — do not conflate

| Where | What it is | Job | Opaque? |
|---|---|---|---|
| SSE `id:` line | the **cursor** (= `seq` on SQLite, wrapped with the generation token) | stream **position** / resume | yes |
| envelope `id` | the **event** ULID | **identity** / idempotency / dedup | no |
| `payload.id` | the **contact** ULID | domain entity identity | no |

Position vs. identity is the crucial split: the cursor says *where you are*
(opaque); the event id says *which event this is* (explicit, so you can dedup).
They had to separate the moment the cursor became opaque.

### `contact.created` payload

Self-contained snapshot — and the same DTO crm's `GetContact` returns, so the
read API and the event share one shape:

```json
"payload": {
  "id": "01HQ8ZK9...CONTACT",
  "display_name": "Jane Doe",
  "given_name": "Jane",
  "family_name": "Doe",
  "emails": [ { "email": "jane@example.com", "label": "work",   "primary": true } ],
  "phones": [ { "phone": "+15551234567",     "label": "mobile", "primary": true } ],
  "created_at": "2026-06-02T15:04:05.123Z"
}
```

### Full wire frame

The `id:` is the opaque cursor; on SQLite it is the `seq` wrapped with the
generation token (illustrated below as `<generation>.<seq>` — the consumer never
parses it).

```
id: 0190f3a1c2.42
event: contact.created
data: {"id":"01HQ...EVENT","type":"contact.created","source":"crm","time":"2026-06-02T15:04:05.123Z","payload":{"id":"01HQ...CONTACT","display_name":"Jane Doe","given_name":"Jane","family_name":"Doe","emails":[{"email":"jane@example.com","label":"work","primary":true}],"phones":[{"phone":"+15551234567","label":"mobile","primary":true}],"created_at":"2026-06-02T15:04:05.123Z"}}

```

### Outbox schema — store the parts, assemble the envelope on read

The outbox stores the envelope **decomposed into columns**; the producer
assembles the frame on serialize. `source` isn't stored (producer constant);
`event_id` and `created_at` are, so they're stable across replays.

```sql
CREATE TABLE outbox (
  seq        INTEGER PRIMARY KEY AUTOINCREMENT,  -- ordering/cursor; the SSE id:
  event_id   TEXT    NOT NULL,                   -- ULID; envelope "id"; generated once
  type       TEXT    NOT NULL,                   -- "contact.created"; enables filtering
  payload    TEXT    NOT NULL,                   -- JSON: the domain payload object
  created_at TEXT    NOT NULL                    -- emit time; envelope "time"
);
```

`type` as a column is what makes producer-side filtering (`… AND type IN (…)`)
possible later.

### Deliberate omissions (on the record, not forgotten)

- **`specversion` / payload version** — skipped now (YAGNI). Evolve by additive
  fields + ignore-unknown-keys; introduce a version (or bump the `type` string)
  at the first *breaking* change, coordinated in a maintenance window.
- **`correlation` / `causation` ids** — for tracing multi-hop event chains (the
  DAG/partial-chain story). Add when a consumer first emits events *caused by*
  events, not before.

## Control & liveness frames

**Governing principle: the consumer never compares positions; the producer —
the sole interpreter of opaque cursors — sends conclusions, not coordinates.**
This is what keeps cursor opacity intact while still giving the consumer
everything it needs. The consumer's contract stays "receive, persist, present,
**obey**."

| Concern | Who decides | Wire form | Consumer interprets cursor? |
|---|---|---|---|
| Liveness (pipe alive) | neither — just bytes | `: keepalive` comment | no |
| Caught-up | producer (its own send state) | `event: caught-up` | no |
| Lag (observability) | producer computes `head − sent` | `event: status` + integer | no |
| Divergence / expiry (correctness) | producer compares presented cursor vs head / horizon / epoch | `event: resync` | no |

- **Keepalive (`: keepalive`) — mandatory.** A periodic comment proves the pipe
  is alive and lets both ends detect a silently-dead TCP connection (failed write
  → producer closes and releases the retention pin; stalled read → consumer
  reconnects). Carries no position.
- **Caught-up (`event: caught-up`).** The producer states, as a fact about its
  *own* outbox, "I have sent you everything through my current head" (its
  `SELECT ... WHERE seq > last_sent` came back empty). No number, no comparison
  by the consumer.
- **Lag (`event: status`, e.g. `data: {"behind": 5}`) — included as an
  observability/diagnostic tool.** The producer computes `behind = head − (what
  it has sent on this connection)` — it owns both numbers — and emits it as a
  plain integer. The consumer *logs* it for monitoring; it never compares it to
  its own cursor. Pure telemetry, and a cheap, valuable lens on whether a
  consumer is keeping up.
- **Resync (`event: resync`, with a `reason`) — correctness.** The producer does
  the position comparison (it interprets both its head and the cursor the
  consumer presented) and tells a consumer to discard its position and reconnect
  fresh. Four reasons are defined: **`diverged`** (cursor ahead of head — e.g.
  producer restored from an older backup), **`past-horizon`** (cursor below the
  retention horizon — the events after it were trimmed and are gone),
  **`stale-epoch`** (cursor's generation token ≠ the producer's live generation,
  post-restore/rebuild), and **`unintelligible-cursor`** (the producer cannot
  parse the cursor: garbage, truncated, or minted by a *different* feed). The
  consumer's mechanical response to all four is identical — discard the position
  and reconnect fresh — but `past-horizon` is special: it is the one reason that
  denotes **real, detected-but-unrecovered loss** on the controlled leg (the
  events are already gone; resync *reports* the loss, it does not redeliver), so
  the producer **SHOULD alarm** on it rather than treat it as a routine
  reposition. The cheap, high-value check runs at *connect time* — one comparison
  of the presented cursor against head/horizon/epoch when the connection opens,
  **including the epoch check** — and is worth having **from day one**; periodic
  mid-stream emission is an easy add-on later.

The mental model: **the consumer never asks "am I caught up?"; the producer tells
it "you're current," "you're 5 behind," or "you've diverged — start over."**

## Consumer side

- **Durable offset cursor per upstream**, stored as opaque text in the consumer's
  own SQLite.
- **Apply the effect and commit the cursor in one local transaction.** The
  committed cursor is the entire recovery state.
- **Present the committed cursor on (re)connect**, never the last-received one (a
  crash between receive and commit would otherwise silently drop an event).
- **In-order processing.** Strictly in sequence per upstream; resolve one before
  advancing. This is what lets the whole position be a single (opaque) cursor.
- **Commit the cursor for *every* event; run the effect only on a type match.**
  With consumer-side filtering, the loop advances and commits the cursor over
  every event in order, and performs the domain effect only for wanted types. A
  filtered-out event commits just the cursor advance (for notify's two-stage
  model: no `notifications` row). Skipping the effect must never skip the commit,
  or the event re-arrives on every reconnect.

## notify: best-effort external delivery (ntfy.sh)

`notify` delivers via **ntfy.sh** (an internet push service). The push is the
**external hop** — outside the at-least-once boundary (see *Delivery guarantee &
retention*) — so notify treats it as **fire-and-forget**:

1. **Consume:** for each `contact.created`, notify attempts **one** `POST` to
   ntfy.sh, ignores the result beyond logging, and commits its cursor. There is
   **no** `notifications` table, **no** draining sender, **no** retry/backoff, and
   **no** pending state. A failed push is simply lost.
2. Because best-effort tolerates *both* loss and duplicates, there is nothing to
   keep transactionally consistent — the POST and the cursor commit are not
   coordinated. This is why the offset-atomicity problem that a *durable* side
   effect would create does not arise here.

- **notify still keeps a durable committed cursor and commits *after*
  processing**, so the *controlled-service* leg (crm → notify) is at-least-once:
  a restart resumes from the committed cursor and re-delivers anything not yet
  committed. The fire-and-forget POST is the only best-effort part.
- **First outbound internet dependency on the data plane.** If ntfy is
  unreachable the push is dropped (no retry); the inter-service guarantee is
  unaffected.
- **The ntfy topic/token is a secret.** Per the secrets rule it lives under
  `~/.secrets/`, is injected via `.envrc` → env var, and notify reads it with
  `getenv` at its composition root. The topic name *is* the access control on
  ntfy.sh, so it is secret material, not config.
- **Deferred, per service, later:** any robustness for the external hop —
  retries, a local pending/outbox for ntfy, external-side dedup — is explicitly
  out of scope now. How a service behaves toward outside systems is left
  undefined and decided per service when we harden it.

## Delivery guarantee & retention

**The guarantee boundary: at-least-once from a producer's outbox commit through a
consumer's cursor commit — the leg between services we control — *provided no
consumer falls below the retention horizon*.** That horizon proviso is
load-bearing (see *Retention*, below): day-one retention is a blunt horizon, not
retain-until-committed, so a consumer offline longer than the horizon falls below
the floor and the producer answers `resync` reason `past-horizon` — real,
detected-but-unrecovered loss *inside* the controlled leg. Beyond the boundary —
a service's interaction with an outside system (e.g. notify → ntfy.sh) —
delivery is **undefined / best-effort**, decided per service, later. The event
plane is work-bearing; external hops are not.

At-least-once between controlled services means three things, and **duplicates
are possible** (a crash between processing and committing reprocesses on
restart):

- the consumer keeps a **durable committed cursor** and commits **after**
  processing;
- the producer **retains each event at least until it falls below the retention
  horizon** (day-one retention is the blunt horizon below, *not*
  retain-until-committed — that exact model is deferred);
- a consumer with a *controlled-side* effect **dedups on the envelope `id`** (the
  reason that field exists). notify's only effect is the external POST, so a
  duplicate is at worst a double-push — acceptable.

**Retention — generous horizon now; exact retention deferred.** For a single,
co-located, normally-up consumer like notify, "keep the outbox long enough that
the consumer always catches up" *is* at-least-once in practice, achieved with a
**dumb time/size horizon** and a plain `DELETE`:

```
trim where seq ≤ horizon_floor        -- generous: N days / M rows
```

No consumer registry, no acks, no TTL — but the horizon is a contract, not a
hope: it **MUST** exceed the maximum tolerable consumer downtime, because a
consumer that crosses it loses the events trimmed beneath it — at-least-once is
broken for that consumer, reported as `resync` reason `past-horizon`.

**Deferred to "more robust later" — variant B (exact retention).** When disk
pressure, or many / untrusted / long-offline consumers, make a generous horizon
untenable, upgrade to *retain-until-committed*:

```
trim where seq ≤ MAX( horizon_floor , MIN over *live* consumers of committed )
```

This needs a producer-side registry (`consumer_id → committed`), **stable
consumer IDs** as its key (the `X-Consumer-Id` UUID — kept in the protocol for
this and for observability), **TTL/lease eviction** so a dead consumer can't pin
the log, the horizon as the ultimate backstop (never ship min-offset logic
without it), and — because SSE is one-directional — a **max-age cap** (or an
out-of-band ack) so the producer learns a *busy* consumer's advanced committed
offset rather than only the offset it presented at connect.

## Still open (NOT decided)

- **Event inventory** — the real cross-service reactions in the suite, and
  whether they're state-based/reconcilable or occurrence/side-effect. The whole
  cost depends on this.
- **Topology (Axis 1)** — monolith vs. services — still open and upstream of all
  of this (carried from the exploration draft).
- **Spec collision with metaspot** — the authoritative path-routing/auth/Service
  specs. Mostly *relieved* by the two-plane model above (the event plane is
  internal/loopback and outside the nginx auth contract), but worth a deliberate
  confirmation pass.
