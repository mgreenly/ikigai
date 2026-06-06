# Event Plane Protocol

A normative reference for the suite's internal, service-to-service event
transport: an SSE feed over loopback, driven by consumer-held offset cursors.
This document is the **contract**: the wire protocol plus the correctness
invariants every conforming producer and consumer must uphold. It is **not** a
soup-to-nuts implementation manual. Mechanism that is wire-invisible *and* that no
second service must agree on — how the doorbell is wired (§4.3), where a shared
outbox library lives (§5.3), how a restore is detected at boot (§9.3), where a
consumer keeps its `X-Consumer-Id` (§7.1) — is deliberately out of scope: it is
settled once by the **reference service that serves as the implementation
template**, and copies inherit it. The test for what belongs *here* rather than in
that template: a detail is contract only if a second, independently built service
would silently diverge — broken interop, or undetectable event loss — without
agreeing on it. Everything else is the template's to choose.

The key words **MUST**, **MUST NOT**, **SHOULD**, **SHOULD NOT**, and **MAY** are
used in the RFC 2119 sense. Where a number is given as a SHOULD (e.g. a keepalive
interval) it is a recommended default and a tuning knob, not part of the
contract; where a rule is a MUST it is load-bearing for the guarantees below.

> **Status: research tech demo, not production.** The first slice proves the
> producer → SSE → consumer → effect chain end to end. SQLite is the normative
> reference engine; the seam for other engines is defined (see *The dialect
> seam*) but only SQLite is built.

---

## 1. Purpose, scope, and the headline property

The suite runs as one **dashboard** plus N **services** on a single box, one box
per customer. Two kinds of communication exist, with different trust models:

- **North/south — external, owner-facing.** A Claude client reaches a service's
  MCP/REST endpoints through nginx, which terminates TLS and authenticates every
  request. Request/response, owner-scoped. This is *not* the subject of this
  document.
- **East/west — internal, service-to-service.** One service reacts to things
  that happen in another (e.g. a notifier reacts to a contact being created).
  This is the **event plane**, and it is what this document specifies.

The event plane is **streaming, not polling**. A consumer opens one long-lived
HTTP connection to a producer's feed, presents its position once, and the
producer streams every event past that position and keeps shipping new ones down
the same connection — parking when there is nothing left to send.

> **The headline property is zero-busy-wait.** A caught-up consumer costs
> essentially nothing: no polling, no spinning, no per-event CPU. It is a
> goroutine blocked in the kernel on a socket read, woken when a real event
> arrives — plus one cheap, sanctioned exception: a single ~15s liveness
> keepalive (§10.1), the one periodic write on an otherwise quiet connection, a
> one-line frame at ~0 CPU that exists to detect a silently-dead pipe (a
> half-open connection on a feed that may go hours without an event). A quiet box
> stays quiet. The *consumer* side of this is the wire contract — TCP
> backpressure (§6) makes it true for any conforming producer. The *producer*
> side — waking parked connections without itself polling its outbox — is not
> wire-observable and so cannot be a wire guarantee; it is a property the
> reference SQLite producer delivers via the in-process doorbell of §4.3 (a
> timer-driven producer is still conforming but forfeits it). This single
> property — flow control for free, paid by nobody — is the reason this transport
> exists instead of a message broker, and every design choice below protects it.

The mechanism is plain TCP backpressure, explained in *The read model*.

---

## 2. Trust model: why the event plane needs no auth

The event plane is **internal-only and unauthenticated**, and it **does not pass
through nginx.**

One box serves exactly one customer, so every service on the box belongs to the
**same single owner**. A service consuming another service's feed therefore never
crosses an owner boundary — there is no second principal to authenticate against.
The only security requirement is *"not reachable from outside the box,"* and that
is already satisfied: every service binds to loopback (`127.0.0.1`) only.

This is a deliberate separation from the external plane:

- The external plane's authentication contract (nginx introspecting each request
  and injecting an authenticated identity) exists to protect owner-scoped data
  from other principals on the public internet. The event plane has no such
  exposure and no second principal, so that machinery would be cost without
  benefit.
- Routing the feed through the public front door would be actively wrong. The
  external door is internet-facing; exposing the raw event stream there would
  either leak the stream publicly or force per-service tokens — re-introducing
  exactly the authentication problem the single-owner model removes. So the feed
  **MUST** be served directly over loopback, never via the public proxy.
- Connecting directly also keeps a proxy out of a path whose entire value is
  end-to-end TCP backpressure (§1, §6). No intermediary buffers the stream or
  imposes an idle timeout on it.

A producer's feed endpoint **MUST** therefore listen on loopback and **MUST NOT**
be published on any externally reachable interface.

> **Accepted residual risk (intra-box).** Loopback stops *external* reach, but it
> is **not** intra-box isolation: the event-plane trust boundary is effectively
> *any local code on the box*, not *the owning service*. Every service runs under
> its own dedicated `--system` Unix user, yet a loopback TCP feed grants no
> per-user access control — any local process (including a service compromised
> under its own user) can connect to another service's `127.0.0.1:<port>/feed` and
> read its full stream, which carries domain snapshots (names, emails, phones —
> §8.6). On a single-owner box this is **not** a cross-owner breach, but it does
> undercut the platform's per-service least-privilege posture. For this research
> tech demo we **accept** it. The closers — a Unix-domain socket gated by
> filesystem permissions, or a per-upstream local secret — are **deferred** (§12);
> both are deliberately avoided here, since UDS would contradict the TCP feed URL
> of §3 and the end-to-end TCP-backpressure framing of §1/§6, and a per-upstream
> secret would reintroduce exactly the token machinery this section removes.

---

## 3. Addressing: how a consumer finds a producer

A consumer connects to a producer's feed by an address it receives as **injected
configuration**, read at its composition root (the same way it receives secrets
and every other deployment-specific value). It does **not** discover the address
by inspecting the filesystem, and it does **not** hardcode a peer's port.

- The platform that provisions the box owns topology — port assignment and
  on-disk layout. It injects each consumer's upstream feed address(es) as
  environment configuration (e.g. `CRM_FEED_URL=http://127.0.0.1:3001/feed`). The
  exact config-key naming convention is owned by the **metaspot platform spec**
  (which owns config injection — see `AGENTS.md` in the sibling `metaspot` repo),
  not by this document or any one service: the platform injects the key and the
  consumer reads it *by name*, so the two **MUST** agree, and that agreement is a
  platform contract this document only points at.
- A consumer reads that value via `getenv` at startup and connects directly.
- A consumer therefore knows the *names* of the feeds it consumes (one config
  key per upstream) and nothing about where any other service lives on the box.
  No service learns another service's port or directory layout; that knowledge
  stays with the platform.

Because the consumer only ever sees a configured URL, the addressing scheme can
change without touching service code — point the variable at a different address
and the consumer follows. The protocol below is identical regardless of how the
address is resolved.

---

## 4. The producer (write / publish side)

### 4.1 Publish is one INSERT inside the domain transaction

Publishing an event **MUST** be a single `INSERT` into a local **outbox** table,
performed **inside the same database transaction** as the domain write that
caused it. For "a contact was created," the existing create-contact transaction
gains one statement before it commits, using the same transaction handle as the
domain writes.

This is the **atomic outbox** pattern. The domain change and its event commit
together or not at all: there is no second datastore to write to, so there is no
window in which one persists without the other. No lost events, no phantom events
for changes that rolled back.

### 4.2 The write path is subscriber-blind

The write path **records**; it never **pushes**. Whether any consumer is
connected, and how many, is irrelevant to the code that writes an event. Adding,
removing, or restarting a consumer touches no producer write code. The producer
does not know or care who is listening.

### 4.3 The doorbell rings after commit and carries no data

A producer **SHOULD** maintain an in-process **doorbell** (e.g. a condition
variable broadcast) to wake parked feed connections promptly when a new event
lands. This is purely producer-internal and wire-invisible (the write path is
subscriber-blind — §4.2; delivery is always a query against the durable outbox),
so it cannot be a wire-level **MUST**; the **SHOULD** marks it as the production
reference path rather than an optional extra.

- The doorbell **MUST** fire *after* the transaction commits — never inside it.
  Before commit the new row is not yet visible to the readers that would query
  for it; signalling early is a race.
- The doorbell carries **no data**. It says only "look sooner." It is never the
  delivery mechanism — delivery is always a query against the durable outbox.
- Missing a doorbell signal loses nothing. A reader that was not parked at the
  instant it fired will see the row on its next query; the durable row is the
  source of truth. It is a doorbell, not the mail.

The doorbell is a latency optimization, and it is what makes the producer side of
zero-busy-wait real. Because the write path is subscriber-blind (§4.2), a producer
learns of newly-committed rows in exactly one of two ways: an in-process signal
(the doorbell) or a timer-driven re-query — there is no third option. A producer
that omits the doorbell and re-queries on a timer is therefore *polling its own
outbox*: it stays **wire-conformant** (the wire protocol does not depend on the
doorbell), but it forfeits zero-busy-wait — the very thing §6 rejects as
"defeating the whole point." Such a timer-driven producer is a degraded/test mode,
not the production reference.

### 4.4 The payload is a self-contained snapshot

An event's payload **MUST** carry the fields a consumer needs to act, captured at
emit time — not a pointer back to the producer. A consumer **MUST NOT** be
required to call back into the producer to interpret an event. This keeps
consumers decoupled and lets an event remain meaningful even after the underlying
entity is later changed or deleted.

### 4.5 The outbox schema

The outbox stores each event **decomposed into columns**; the producer assembles
the wire envelope (§8) on serialize. The reference SQLite schema:

```sql
CREATE TABLE outbox (
  seq        INTEGER PRIMARY KEY AUTOINCREMENT,  -- ordering / cursor; the SSE id:
  event_id   TEXT    NOT NULL,                   -- envelope "id" (ULID), minted once
  type       TEXT    NOT NULL,                   -- e.g. "contact.created"; enables filtering
  payload    TEXT    NOT NULL,                   -- JSON: the domain snapshot object
  created_at TEXT    NOT NULL                    -- emit time; envelope "time" (RFC 3339)
);
```

- **`AUTOINCREMENT` is load-bearing, not decoration.** It guarantees `seq` is a
  persistent, monotonically climbing high-water mark. Without it, if retention
  (§11) ever empties the table, SQLite's rowid restarts at 1, and a consumer
  cursored at 500 would silently ignore every new row until the counter climbed
  back past its position. `AUTOINCREMENT` makes `seq` never reuse a value — but
  only **within one continuous DB lineage.** Its high-water mark lives in
  SQLite's `sqlite_sequence` table *inside this DB file*, so a file-level
  `bin/restore` from an older snapshot (§9.3) rolls it back with the file, and
  post-restore writes re-mint the same `seq` integers for genuinely different
  events. That cross-restore reuse is made safe not by `AUTOINCREMENT` but by the
  **generation token** (§9.3) the producer embeds in every cursor. `AUTOINCREMENT`
  **MUST** be present.
- **`event_id` and `created_at` are stored**, so they are byte-identical on every
  replay of the same event (required for stable dedup — §10).
- **`source` is not stored** — it is a producer-wide constant supplied at
  serialize time.
- **`type` is a column** so that producer-side filtering becomes possible later
  (`… AND type IN (…)`) without a schema change. It is not used for filtering
  now (§7.3).

---

## 5. The ordering invariant

> **Outbox writes are serialized such that seq-assignment order equals commit
> order equals visibility order.** A consumer's contract — "read everything after
> my cursor, in order" — is correct *only* while this holds. It is the single
> most important property on the write side.

This invariant — and the monotonicity of `seq` it produces — holds **within one
continuous DB lineage.** A file-level restore or rebuild (§9.3) starts a new
lineage: it can roll `seq` back, so the *same* `seq` value can recur across the
restore for an unrelated event. That recurrence does not break a consumer because
the generation token embedded in the cursor (§9.3, §10.1) makes a pre-restore
cursor fail the connect-time check rather than be mistaken for a position in the
new lineage.

### 5.1 SQLite provides it for free

SQLite permits exactly one write transaction at a time. This is true in both the
rollback-journal and WAL modes — WAL adds concurrent *readers*, never concurrent
*writers*. A writer holds the write lock from its first write until it commits,
so no second writer can assign the next `seq` until the first has committed.
Therefore assignment order *equals* commit order *equals* the order in which rows
become visible to readers — by construction, at the engine level.

This is a property of the **engine**, not of any application setting:

- `SetMaxOpenConns(1)` and `BEGIN IMMEDIATE` are **ergonomics, not the
  guarantee.** Single-writer ordering would hold even with a larger pool. Those
  two settings only keep SQLite's built-in serialization from surfacing as
  errors: a pool size of 1 queues writes in the application instead of returning
  `SQLITE_BUSY`, and `BEGIN IMMEDIATE` takes the write lock up front instead of
  risking a deferred-transaction upgrade deadlock. They make the guarantee
  *pleasant to use*; they do not create it.

### 5.2 Another engine would not get it for free

On a database with concurrent writers (e.g. Postgres), two transactions can be
assigned sequence values in one order but commit in another, so a naive "order by
sequence" read can skip a row that has not yet committed. Correctness there
requires ordering on the transaction id and reading only up to a stable frontier
(a watermark below which no transaction is still in flight). That work is exactly
what SQLite makes disappear, and it lives entirely in the dialect (§9), behind
the same `Fetch` contract — the layers above never see the difference.

### 5.3 Protecting the invariant (all mechanical, none on the hot path)

The invariant is protected by defense in depth, not vigilance:

1. **Encapsulation.** The shared outbox library owns the database handle, its
   pool size, and its transaction style. Services receive a correctly-configured
   outbox; they cannot misconfigure what they do not control.
2. **A concurrency stress test — the executable spec and primary net.** N
   goroutines concurrently `Append` + commit while a reader consumes through the
   real `Fetch`. It asserts the reader observes a strictly increasing sequence
   and **never sees a row appear behind its cursor.** It passes on single-writer
   SQLite and goes red the moment anyone introduces concurrent writers or an
   incorrect dialect. It is dialect-agnostic — the contract every dialect must
   satisfy.
3. **A startup behavioral probe — fail loudly.** At boot, the producer confirms
   that a second concurrent write transaction is actually refused (two
   `BEGIN IMMEDIATE`; the second must be rejected). If it is *not* refused, the
   ordering guarantee is gone, so the producer **MUST** crash rather than serve a
   stream it can no longer order correctly. The probe tests behavior, so it
   survives a future engine swap.
4. **A named invariant documented at the single chokepoint** (the `Append`
   operation), pointing at the test and the probe.
5. **A slow-reader backpressure test — for §6, not for ordering.** The stress
   test above asserts *ordering* only; it does not exercise a consumer that stops
   reading. A separate test attaches a reader that drains slowly (or stalls)
   against a large backlog and asserts the producer's memory stays bounded and its
   `write()` blocks rather than buffering — i.e. that the §6.1 streaming rules
   hold and the backlog stays on disk. It goes red the moment a dialect is driven
   with an unbounded `Fetch` or an in-memory queue is interposed.

Deliberately **not** done, and why:

- **No per-write runtime ordering check** — pure hot-path cost for no benefit;
  the test and probe already cover it.
- **No contiguity / gap auditing.** Rolled-back transactions legitimately leave
  holes in `seq`. The invariant is *no row ever appears behind the cursor*, not
  *no gaps* — auditing for gaps would flag correct behavior.

Note the consumer **cannot** detect a violation of this invariant itself: its
"give me rows after my cursor" query filters out the very row that would prove
the violation. That is precisely why detection lives in the producer-side test
and probe, not in the consumer.

---

## 6. The read model: streaming, consumer-driven via TCP backpressure

A consumer opens **one** long-lived HTTP connection, presents its cursor once,
and the producer streams every event past it, then keeps the connection open and
ships new events as they commit, parking on the doorbell (§4.3) when caught up.

**This is still consumer-driven** — the intuition of "the consumer pulls the next
event" is preserved; the mechanism is just TCP flow control rather than a request
per event:

- The consumer "asks for the next event" by **reading the next bytes off the
  socket.**
- When the consumer stops reading (because it is busy processing), its TCP
  receive window fills. The producer's next `write()` blocks in the kernel, and
  the producer stops producing. "If it doesn't ask, nothing happens" becomes
  "if it doesn't read, the producer blocks."
- The backlog therefore stays on the **producer's disk** (the durable outbox);
  only a bounded batch is ever in producer memory (§6.1). Consumer memory is O(1)
  regardless of how far behind it is. A caught-up consumer is a goroutine blocked
  on the network poller at ~0 CPU — the zero-busy-wait property of §1, realized.

Two alternatives were rejected: **pure polling** (adds latency and burns idle CPU
on a quiet box — defeating the whole point) and **long-polling** (push-like
latency, but one connection per event — reinventing SSE with worse ergonomics).

### 6.1 Producer streaming rules — what makes "stays on disk" true

The "backlog stays on disk" guarantee above is not free; it holds only if the
producer never quietly relocates the backlog into its own memory or breaks the
chain that lets a slow reader's stalled `write()` propagate back as backpressure.
These rules are engine-agnostic (they constrain how any dialect is driven, not
how it is implemented):

- The producer **MUST** stream in bounded `Fetch(limit)` batches (§9.2). It
  **MUST NOT** load all rows after the cursor into memory at once — that puts the
  entire backlog in *producer* memory and falsifies "stays on disk."
- The producer **MUST NOT** interpose an unbounded in-memory queue between `Fetch`
  and the socket. If it does, `write()` never blocks, the kernel receive window
  never communicates the slow reader back to the producer, and TCP backpressure
  is defeated.
- The producer **MUST** flush each frame (or each small bounded batch) to the
  socket promptly, so a caught-up consumer is woken per committed event rather
  than after some buffer fills.

How a given runtime clears or sets write deadlines on a long-lived stream, and how
it observes the peer's disconnect, are implementation concerns left to the engine
(the failed keepalive write of §10.1 already bounds liveness detection to ≤15s);
they are not part of this wire contract.

---

## 7. The connection

### 7.1 Endpoint and request

A consumer opens the feed with a long-lived HTTP `GET` against the producer's
configured feed address (§3), over loopback:

```
GET /feed HTTP/1.1
Host: 127.0.0.1:3001
X-Consumer-Id: <uuid>
Last-Event-ID: <opaque cursor>          (optional)
Accept: text/event-stream
```

**The request *is* the registration.** There is no separate "new consumer"
announcement message and no handshake round-trip; the request is self-describing.

- **`X-Consumer-Id`** — a stable, long-lived UUID identifying the consumer. A
  consumer **MUST** send it. The producer uses it for observability now (logging,
  lag attribution) and it is the key a future retain-until-committed model would
  need (§11); requiring it now keeps that door open at no cost.
- **`Last-Event-ID`** — the consumer's committed opaque cursor (§9), echoed from
  a prior connection. It carries **only** an opaque cursor or is absent — never a
  command token. Its absence is meaningful:
  - **Absent** (and no `?from=tail`) ⇒ "from the beginning" — stream the entire
    retained outbox.
  - **`?from=tail`** (a query parameter, with `Last-Event-ID` absent) ⇒ "only new
    events" — stream nothing historical, begin at the current head.
  - **Any `Last-Event-ID` value** ⇒ resume strictly after that opaque cursor.

  This earliest-vs-latest choice is the **one and only** non-opaque decision a
  consumer makes about its cursor. It lives in the `?from=tail` query parameter,
  *not* as a token in the opaque-cursor channel, so the consumer never places a
  command word where a cursor goes (§9.1). It is made only on a *first*
  subscription; thereafter the consumer always presents the cursor it last
  committed.

### 7.2 Response

The producer responds with a standard SSE stream:

```
HTTP/1.1 200 OK
Content-Type: text/event-stream
Cache-Control: no-cache
Connection: keep-alive
```

The producer **MAY** emit an SSE `retry:` line to hint a reconnection delay.
The protocol adopts SSE's resume mechanism, with one deviation: on a dropped
connection, the consumer reconnects and presents its **durably committed**
cursor as `Last-Event-ID` (§9, §10) — never the last one received. A consumer
therefore **MUST NOT** rely on a library that auto-tracks `Last-Event-ID` on
receipt (e.g. a browser `EventSource`); that would replay the last-*received*
id and silently drop an event whose effect had not yet committed. The
committed-cursor rule is the one in §10; this only restates which id to resume
from.

### 7.3 Filtering is consumer-side (for now)

The producer streams **all** event types on the feed. A consumer matches on the
`event:` line (§8) and runs its domain effect only for the types it cares about.
This keeps the producer passive and free of per-consumer query state.

Critically: **the cursor advances over skipped events.** "Skip" means "run no
effect," never "do not commit the cursor." A consumer that filters out an event
**MUST** still advance and commit its cursor past it (§10), or that event
re-arrives on every reconnect forever.

Producer-side filtering (a `?types=` parameter) is a later bandwidth
optimization; the `type` column (§4.5) exists to make it possible without a
schema change.

---

## 8. Wire frames

Three kinds of frame travel on the stream. They are distinguished by which SSE
fields they carry.

### 8.1 Event frames

A real event is the full SSE triple:

```
id: 0190f3a1c2.42
event: contact.created
data: {"id":"01HQ8ZK9...EVENT","type":"contact.created","source":"crm","time":"2026-06-02T15:04:05.123Z","payload":{"id":"01HQ8ZK9...CONTACT","display_name":"Jane Doe","given_name":"Jane","family_name":"Doe","emails":[{"email":"jane@example.com","label":"work","primary":true}],"phones":[{"phone":"+15551234567","label":"mobile","primary":true}],"created_at":"2026-06-02T15:04:05.123Z"}}

```

- **`id:`** — the **opaque cursor** for this event's position in the stream (§9),
  echoed back as `Last-Event-ID` on reconnect; only event frames carry it. It is
  **not** a bare `seq`: on SQLite the cursor is the `seq` wrapped with the
  generation token (§8.4, §9.3), shown here as `<generation>.<seq>` purely to
  illustrate — the exact encoding is producer-internal and the consumer never
  parses it (§9.1).
- **`event:`** — the message **type** (§8.5 naming rules), mirrored inside the
  envelope. It lets a consumer filter without parsing the body.
- **`data:`** — the **envelope** (§8.3), a single line of JSON.

Serialization rules an implementer's parser depends on, all **MUST**:

- The envelope JSON is serialized **compact, on a single line, with no embedded
  newlines** — exactly one `data:` line per event. (SSE concatenates multiple
  `data:` lines with `\n`; a pretty-printed multi-line body would be reassembled
  incorrectly by a strict consumer.)
- The encoding is **UTF-8**.
- An event frame carries all three of `id:`, `event:`, and `data:`.

### 8.2 Control and liveness frames carry no position

Control and liveness frames (§10) **MUST NOT** carry an `id:` line. Per SSE
semantics, a frame without `id:` does not update the client's last-event-id, so a
control frame **physically cannot advance the consumer's cursor.** This is the
wire-level enforcement of "the consumer never moves its own cursor by inference":
only a genuine event frame, with its `id:`, can move it.

### 8.3 The envelope

Every event frame's `data:` is a **uniform envelope** wrapping a per-type
**`payload`** body. The envelope shape is identical for every event type; only
the body changes. Filtering, dedup, ordering, and observability all operate on
the envelope — only the `payload` is type-specific.

The envelope is aligned with the CloudEvents shape (a battle-tested event
envelope) with one deliberate deviation: the body key is **`payload`**, not
CloudEvents' `data`, so it is not confused with SSE's `data:` wire line.

```json
{
  "id":      "01HQ8ZK9...EVENT",          // event identity (ULID) — required
  "type":    "contact.created",            // message type — required
  "source":  "crm",                        // emitting service — required
  "time":    "2026-06-02T15:04:05.123Z",   // emit time, RFC 3339 — required
  "payload": { ...domain snapshot... }      // per-type body — required
}
```

- **`id`** — a stable unique identity for *this event*, minted once at emit
  (a ULID) and stored, so it is byte-identical on every replay. It is **required
  and forced by cursor opacity**: a consumer dedups on a stable id, and it cannot
  use the cursor for that, because the cursor is opaque and on another engine
  would be a compound token it cannot key on. Opacity therefore *requires* an
  explicit, non-opaque identity field. This is the dedup key (§10).
- **`type`** — drives filtering and dispatch; mirrors the SSE `event:` line and
  is repeated here so the event is self-describing if logged or forwarded.
- **`source`** — the emitting service name, a producer-wide constant. It
  disambiguates a consumer that merges multiple feeds.
- **`time`** — when the event was emitted (RFC 3339). Distinct from any domain
  timestamp inside the payload: for an update event, `time` is when the update
  happened, not when the entity was created.

### 8.4 Three ids at three layers — do not conflate

| Where | What it is | Its job | Opaque to consumer? |
|---|---|---|---|
| SSE `id:` line | the **cursor** (= `seq` on SQLite, wrapped with the generation token, §9.3) | stream **position** / resume | **yes** |
| envelope `id` | the **event** ULID | **identity** / idempotency / dedup | no |
| `payload.id` | the **domain entity** id | entity identity within the domain | no |

Position versus identity is the crucial split: the cursor says *where you are*
(opaque), the event id says *which event this is* (explicit, so you can dedup).
They had to separate the moment the cursor became opaque.

### 8.5 Event type naming

Event types **SHOULD** follow `<domain>.<event>`: lowercase, dot-separated, with
a **past-tense** verb — `contact.created`, `contact.updated`, `contact.deleted`.

- Past tense because an event is a fact that already happened (the outbox is
  written *after* the domain change commits).
- The `source` field, not the type, identifies the emitting service; a service
  prefix in the type would be redundant and would couple the type name to which
  service emits it today.
- No central type registry exists yet (there is one type). The naming convention
  is the only rule; a registry or enum is a later addition if the type count
  grows.
- **Naming a type is not specifying it.** `contact.updated` and `contact.deleted`
  above illustrate the *convention* only; their payload shapes are **not yet
  defined** — `contact.created` (§8.6) is the one specified type. A payload shape
  is interop, not a local choice (§4.4): a consumer written later must agree on it
  without reading the producer's source. So a new type's payload shape graduates
  into this contract (or the registry above) when the reference producer first
  emits it — it is not settled merely by the template emitting some shape.

### 8.6 The `contact.created` payload

The payload is a self-contained snapshot. It is the same shape the producer's
read API returns for a contact, so the read model and the event share one
structure:

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

### 8.7 Payload encoding

The payload is **JSON, for now** — debuggable and inspectable with ordinary HTTP
tools, which matches the "standard HTTP you can watch" goal. This is revisited
only if payload size ever justifies a binary encoding; the envelope and transport
decisions do not depend on the encoding.

### 8.8 Deliberate envelope omissions

- **No `specversion` / payload version field yet.** Evolve by adding fields and
  having consumers ignore unknown keys. Introduce an explicit version (or bump
  the `type` string) only at the first *breaking* change, coordinated in a
  maintenance window.
- **No `correlation` / `causation` ids yet.** Those trace multi-hop event chains
  (events caused by other events). Add them when a consumer first emits events
  *caused by* events it received — not before.

---

## 9. The opaque cursor and the dialect seam

### 9.1 The cursor is opaque to the consumer

The cursor is a string the consumer treats as a **black box**. Its contract:

> The consumer **stores** it, **presents** it, and **never inspects** it. Only the
> producing dialect mints and interprets it.

A consumer's only verbs for a cursor are:

- **receive** — it rides on each event frame's `id:` line;
- **persist** — store it durably, atomically with the effect (§10);
- **present** — send it as `Last-Event-ID` on (re)connect.

A consumer **MUST NOT** perform arithmetic on a cursor, compare two cursors,
parse one, or construct one. With no operation that depends on its shape, the
cursor's internal form is free to change per engine with zero consumer impact.

Consequences:

- **Keyed per upstream.** A consumer of multiple feeds stores one cursor *per
  feed*. Opacity is exactly what prevents the bug of cross-applying one feed's
  cursor to another — the consumer has no operation that would let it.
- **Stored as `TEXT`.** The consumer's offset column is text, because it holds an
  opaque string, even when (as on SQLite) that string currently happens to be a
  number.
- **The one non-opaque bit** is the first-subscription earliest-vs-latest choice
  (§7.1), expressed as the *absence* of a cursor or the `?from=tail` query
  parameter — a product choice, not arithmetic on a cursor value, and never a
  command token placed in the opaque-cursor channel.
- **It maps 1:1 onto SSE** at no extra cost: SSE's `id:` field is already an
  opaque string and `Last-Event-ID` is that string echoed back. The protocol
  adopts that native mechanism and adds the one rule SSE does not enforce by
  itself: present the **committed** cursor, not the last one received (§10).

### 9.2 The dialect seam

All engine-specific behavior lives behind one interface, the **dialect**, with
two operations:

- **`Append(tx, event)`** — write one event into the outbox within the caller's
  existing transaction.
- **`Fetch(after cursor, limit)`** — return events strictly after the given
  cursor, in order, never beyond the engine's stability frontier, plus the next
  cursor.

Everything above the dialect — the feed handler, the doorbell, the consumer loop,
retention — is written once against this interface and is engine-agnostic.

- **SQLite dialect (built, normative).** No watermark is needed (§5). `Fetch` is
  `WHERE seq > ? ORDER BY seq LIMIT ?`. The cursor encodes the `seq` (wrapped
  with the generation token of §9.3).
- **Other engines (seam defined, not built).** A concurrent-writer engine's
  dialect carries the transaction id, applies a stable-frontier watermark, orders
  on `(txid, seq)`, and encodes a compound `txid:seq` cursor. It is a single
  small file implementing the same two operations — not a rewrite. This is the
  one place §5.2's extra work would live, and the reason consumers must never
  assume the cursor is a number.

### 9.3 The generation/epoch token (mandatory wherever restore is in the lifecycle)

Because the consumer never inspects the cursor, a producer embeds a **generation
(epoch) token** inside it at no cost to the consumer. A consumer that presents a
cursor minted before a database rebuild or restore-from-snapshot is then detected
and told to resync (§10, reason `stale-epoch`) — a producer-only mechanism the
consumer is none the wiser about.

This is not optional on a box whose lifecycle includes restore-from-snapshot or
rebuild — which the ikigenba box's does (`bin/restore` is a mandated verb, and a
restore is a routine lifecycle event, not an exception). Without the token a
file-level restore is silently unsafe: `sqlite_sequence` rolls back with the DB
file (§4.5), `seq` re-climbs through values it already issued, and a consumer
cursored at, say, 500 resumes onto unrelated post-restore events 501… as their
natural in-order successors — the original events past 500 lost forever, with no
divergence, no past-horizon, and nothing the consumer can self-detect (§5.3). The
token is what distinguishes "seq 500 of this lineage" from "seq 500 of a restored
lineage." Therefore, for the **SQLite dialect** (and any deployment whose
lifecycle includes restore or rebuild):

- The producer **MUST** mint a generation id at DB init — a random value (e.g. a
  UUID) or a fingerprint of the lineage — identifying the current DB lineage.
- The producer **MUST** re-mint it on every restore or rebuild, such that it
  changes whenever the file is rolled back. A value stored *inside* the same DB
  file would itself be rolled back by a file-level restore and so cannot be
  relied on alone; the re-mint **MUST** be triggered by the restore/rebuild
  procedure (`bin/restore`) or detected at boot, not left to survive inside the
  restored file.
- The producer **MUST** embed the live generation token in every cursor it mints,
  so the SQLite cursor encodes the generation alongside the `seq` and a
  pre-restore cursor is recognizable on sight by the producer.
- The producer **MUST** mint every cursor as a single-line, newline-free, UTF-8
  string safe to carry verbatim on an SSE `id:` line (§8.1) and in a
  `Last-Event-ID` request header (§7.1) — the same single-line/UTF-8 discipline
  §8.1 already imposes on the envelope, applied to the `id:` channel. A newline in
  a minted cursor would silently truncate the field on the wire.

The consumer is unaffected: the cursor stays opaque (§9.1), and the token rides
inside it. Detection and rejection happen entirely producer-side at connect time
(§10.1).

---

## 10. The consumer contract

A conforming consumer:

- **Keeps a durable offset cursor per upstream**, stored as opaque text in its
  own database (§9.1).
- **Applies the effect and commits the cursor in one local transaction.** Once a
  cursor has been committed it is *almost* the entire recovery state — the only
  other durable bit is the first-subscription marker below, needed solely to
  resolve the bootstrap case before any cursor exists.
- **Persists its first-subscription choice durably, before it can matter.** The
  earliest-vs-latest decision of §7.1 is made only on a *first* subscription, so
  the consumer **MUST** record durably that the choice was made — either a
  bootstrap marker written before the first connect, or by committing the
  `tail`-resolved cursor on the first event (or on the first `caught-up`, if it
  chose `tail` and nothing has arrived yet). Otherwise a consumer that connects
  with `tail`, processes nothing, then restarts before any commit has *no*
  persisted cursor **and** *no* record that it already chose `tail` — so it
  re-bootstraps as `tail` and silently re-drops the events that arrived while it
  was down.
- **Presents the committed cursor on (re)connect**, never the last one received.
  A crash between receiving an event and committing its effect must re-deliver
  that event, not skip it; presenting the last-received cursor would silently
  drop it.
- **Processes strictly in order**, per upstream — resolve one event fully before
  advancing to the next. In-order processing is what allows the entire position
  to be a single opaque cursor.
- **Commits the cursor for *every* event; runs the effect only on a type match**
  (§7.3). A filtered-out event commits only the cursor advance. Skipping the
  effect **MUST NOT** skip the commit.
- **On a controlled-side effect that *errors without crashing*, neither commits
  nor advances.** When a matched event's at-least-once effect fails (the local
  transaction did not commit), the consumer **MUST NOT** commit the cursor and
  **MUST** retry the *same* event before any later one — an in-order stall, not a
  skip. Committing past a failed effect would silently lose it; the stall is the
  at-least-once-preserving choice. This is the opposite of a **best-effort**
  external hop (§11.2), which commits regardless because it tolerates loss.
- **Dedups controlled-side effects on the envelope `id`** (§8.3). Because
  delivery is at-least-once (§11), a consumer whose effect must not be applied
  twice records the envelope `id` it has acted on. The dedup-record insert
  **MUST** be part of the *same* local transaction as the effect and the cursor
  commit above — all three commit or roll back together. A separate dedup write
  reopens the partial-failure window the single transaction closes: the effect
  applied but no dedup row (crash between them) re-applies on replay; a dedup row
  with no effect (effect rolled back) marks an event done that never happened. On
  detecting a repeat the consumer **ignores it** — and, exactly as for a filtered
  event (§7.3), "ignore" means *run no effect*, **never** *skip the cursor
  commit*: a duplicate **MUST** still advance and commit the cursor, or it
  re-arrives on every reconnect forever. (This is a *successful* skip that commits,
  not the error-stall of a failed effect above.) Because the envelope `id` is a
  ULID — globally unique by construction — keying the dedup record on `id` alone
  is sufficient; a consumer that merges feeds **MAY** key on `(source, id)`
  (matching how the cursor is kept per upstream), but this is tidiness, not a
  correctness requirement.
- **Obeys control frames** (§10.1) and **never times their cadence** (§10.2).

### 10.1 Control and liveness frames — the producer states conclusions

> **Governing principle: the consumer never compares positions. The producer —
> the sole interpreter of opaque cursors — sends conclusions, not coordinates.**

This is what keeps cursor opacity intact while still giving the consumer
everything it needs to stay healthy. The consumer's contract stays "receive,
persist, present, **obey**."

| Concern | Who decides | Wire form | Consumer interprets cursor? |
|---|---|---|---|
| Liveness (pipe alive) | neither — just bytes | `: keepalive` comment | no |
| Caught-up | producer (its own send state) | `event: caught-up` | no |
| Lag (observability) | producer computes `head − sent` | `event: status` + integer | no |
| Position invalid (correctness) | producer compares presented cursor vs head/horizon/epoch | `event: resync` + `reason` | no |

Every control frame *except* the `: keepalive` comment **MUST** carry a
single-line JSON `data:` body — even an empty `data: {}` — because the SSE
parser dispatches a frame only when its data buffer is non-empty at the
terminating blank line; an `event:`-only frame is silently dropped. A keepalive
is an SSE comment, not a dispatched event, and correctly carries no `data:`.
This also makes "fully-received" (§10.1, below) concrete for control frames: a
control frame is complete only once its `data:` line **and** the blank-line
terminator have both arrived.

- **Keepalive — `: keepalive`, mandatory.** A periodic SSE comment proves the
  pipe is alive and lets both ends detect a silently-dead TCP connection: a
  failed write tells the producer to close and release the consumer's retention
  hold; a stalled read tells the consumer to reconnect. It carries no position.
- **Caught-up — `event: caught-up`, with `data: {}`.** The producer states, as a
  fact about its *own* outbox, "I have sent you everything through my current
  head" — i.e. its fetch past the last-sent position came back empty. No number,
  no comparison by the consumer. (A consumer may use this to know it is live —
  e.g. to flip a readiness flag — but it is not required to act on it.) The frame
  carries no payload data, but it **MUST** still write an explicit single-line
  empty-object `data: {}` body: a frame with an empty data buffer is not
  dispatched at all under the SSE parsing rules (the blank line that ends the
  frame fires no event when no `data:` was seen), so `event: caught-up` alone
  would be silently dropped by a conformant consumer.
- **Lag — `event: status`, with `data: {"behind": <int>}`.** Pure telemetry. The
  producer owns both numbers and computes `behind = head − (what it has sent on
  this connection)`, emitting it as a plain integer. The consumer **logs** it for
  monitoring and **MUST NOT** compare it against its own cursor or otherwise act
  on it. The wire shape is normative; emitting it at all is a producer **MAY**.
- **Resync — `event: resync`, with `data: {"reason":"…"}`, correctness.** The
  producer does the position comparison and tells the consumer to discard its
  position and reconnect fresh. The `reason` travels in a single-line JSON `data:`
  body — e.g. `data: {"reason":"past-horizon"}` — for the same dispatch reason as
  above. The defined reasons:
  - `diverged` — the presented cursor is *ahead* of the producer's head (e.g. the
    producer was restored from an older backup);
  - `past-horizon` — the presented cursor is below the retention horizon (§11);
    the events after it **existed, were never delivered, and have been trimmed
    away** — they are gone. This is the **one reason that denotes actual,
    unrecovered loss on the controlled leg** (§11.1), not a mere position-validity
    problem. The `resync` frame *reports* the loss; it does not, and cannot,
    redeliver the missing events. A producer **SHOULD** therefore treat
    `past-horizon` as an **alarmable fault** — surfaced beyond a single log line —
    because it means the retention horizon was crossed and at-least-once was
    broken (§11.1, §11.3), not a routine reposition;
  - `stale-epoch` — the cursor's generation token does not equal the producer's
    live generation (§9.3): it was minted in a DB lineage that a restore or
    rebuild has since replaced. The producer **MUST** reject such a cursor with
    this reason — it is what stops a pre-restore cursor from resuming onto
    unrelated events that happen to reuse the same `seq` values (§4.5);
  - `unintelligible-cursor` — the producer cannot parse the presented cursor at
    all (garbage, truncated, or a cursor minted by a *different* feed — the
    cross-application mistake opacity is meant to prevent).

  From the consumer's side these four share **one mechanical response**: *your
  stored position is no longer valid — discard it and reconnect fresh.* But they
  do **not** share one meaning. `diverged`, `stale-epoch`, and
  `unintelligible-cursor` are **position-validity / lifecycle** faults: the cursor
  cannot be honored, but no event that the controlled leg promised to deliver has
  been lost on account of them. `past-horizon` is different in kind — it is the
  one reason that signals **real, detected-but-unrecovered loss** on the
  controlled leg: events the producer was obliged to retain (§11.1) were trimmed
  before the consumer caught past them, and the `resync` reports that loss rather
  than repairing it. Reconnecting fresh after `past-horizon` does **not** recover
  the gap: whichever earliest-vs-latest choice the consumer makes (§7.1), the
  events between its old cursor and the new horizon floor are permanently
  abandoned. Treating all four identically in code is fine; treating them
  identically in *alarming* is not — see the `past-horizon` SHOULD above.

  On any of the four the producer **MUST** emit `event: resync` (with the reason)
  and then **close** the connection. Because the connect-time check can fail
  immediately — after the `200` + `text/event-stream` response line is already on
  the wire (§7.2) — the producer **MUST flush the `resync` frame before closing**:
  the frame must reach the socket, not sit in a writer's buffer that `close`
  discards. Correspondingly, the consumer **MUST treat a fully-received `resync`
  as authoritative regardless of how the connection then closed** — it processes
  any `resync` it has parsed *before* deciding the close was a transport failure,
  so a near-immediate EOF after the frame does not send it reconnecting on the
  same dead cursor. (Without this, the two permanent reasons — `stale-epoch` and
  `unintelligible-cursor` — would loop forever: the consumer would reconnect with
  the identical bad cursor and re-trigger the identical `resync`.) The consumer
  then discards its stored cursor and reconnects fresh, making its
  earliest-vs-latest choice (§7.1) again as a deliberate, logged act. The producer
  **MUST NOT** silently reposition the consumer; recovery is the consumer's
  explicit reconnection, not the producer's guess.

  **Absent an authoritative `resync` or other control conclusion, every connect
  failure is one thing: a transport failure, recovered by reconnecting with the
  committed cursor.** A frame received incomplete — the connection dropped before
  the blank-line terminator — **MUST** be discarded without advancing the cursor
  and treated as such a transport failure; a half-frame is never dispatched and
  never advances position. The same uniform response covers an upstream that is
  down at boot, a non-200 response, a dropped connection, and a stalled keepalive
  (§10.2): the consumer reconnects with its committed (or first-subscription,
  §7.1) cursor and **retries indefinitely**. This does **not** override the rule
  above — a fully-received `resync` it has already parsed is authoritative
  regardless of how the socket then closed, and only an EOF with *no* such
  conclusion is treated as transport failure. Startup ordering (e.g. systemd
  `After=` the upstream) is a convenience optimization and **MUST NOT** be
  required for correctness; a consumer started before its producer simply
  retries. Reconnect backoff and jitter are a **SHOULD** (a tight reconnect loop
  against a down upstream wastes cycles), with the curve left impl-defined.

  The connect-time check (compare the presented cursor against head / horizon /
  epoch, once, when the connection opens) is cheap and **MUST** be performed from
  day one — and that includes the **epoch comparison**: the producer **MUST**
  reject, with `stale-epoch`, any cursor whose generation token is not equal to
  the live generation (§9.3) before it interprets the cursor's `seq` against head
  or horizon. The epoch check is not deferred or optional; it is the only thing
  that catches a cross-restore `seq` collision (§4.5, §5), which none of the
  other three reasons detect. Periodic mid-stream resync emission is a later
  **MAY**.

The mental model: the consumer never asks "am I caught up?" or "did I diverge?"
The producer tells it "you're current," "you're N behind," or "your position is
void — start over."

### 10.2 Cadence is not contract

The producer **SHOULD** emit a keepalive roughly every 15 seconds while a
connection is idle (parked or caught-up), and **SHOULD** suppress keepalives
while it is actively streaming events (real frames already prove liveness). It
**SHOULD** emit `caught-up` once, on the edge transition to caught-up (not
repeatedly while idle — that is the keepalive's role), and again each time it
re-reaches head after a burst. It **MAY** emit `status` occasionally while a
consumer is behind (e.g. on the caught-up transition and/or on a slow ~30s
timer); it **MUST NOT** emit `status` per event.

These intervals are recommended defaults and tuning knobs. A consumer **MUST
NOT** depend on any particular cadence: it reacts to frames as they arrive and
never times them. That independence is the actual contract; the numbers are not.

### 10.3 Consumer schema (reference SQLite)

The consumer stores its durable state — the per-upstream cursor, the
first-subscription marker, and the dedup record — in its own database, mirroring
how the producer decomposes the outbox (§4.5). The reference SQLite schema:

```sql
CREATE TABLE feed_offset (
  source     TEXT    PRIMARY KEY,           -- the upstream's envelope "source" (§8.3); one row per feed
  cursor     TEXT,                          -- opaque committed cursor (§9.1), TEXT; NULL before the first commit
  subscribed INTEGER NOT NULL DEFAULT 0,    -- first-subscription marker (§7.1, §10): 1 once the bootstrap choice is durable
  updated_at TEXT    NOT NULL               -- last commit time; observability only
);

CREATE TABLE dedup (
  event_id   TEXT    PRIMARY KEY,           -- the envelope "id" (a ULID, §8.3); unique by construction → sufficient alone
  source     TEXT    NOT NULL,              -- envelope "source"; present for the optional (source, event_id) key below
  acted_at   TEXT    NOT NULL               -- when the effect was applied; observability / future trimming
);
```

- **`feed_offset` is keyed per upstream `source`** (§9.1): one row per feed, the
  opaque `cursor` stored as `TEXT`. `cursor` is **nullable** because the entire
  recovery state is the committed cursor (§10), and on a `tail` first
  subscription no cursor exists until the first commit.
- **`subscribed` is the durable first-subscription marker** required by §7.1/§10.
  It exists so a consumer that chose `tail`, processed nothing, then restarted
  *before any cursor commit* does not silently re-bootstrap as `tail` and re-drop
  the gap. A conforming consumer **MUST** make the bootstrap choice durable before
  it can matter — set `subscribed = 1` in the bootstrap row before the first
  connect, **or** commit a `tail`-resolved `cursor` on the first event (or first
  `caught-up`). Either satisfies §10; this column is the explicit-marker form.
- **`dedup` is keyed on the envelope `id` alone, and that is normative.** The
  envelope `id` is a ULID — globally unique by construction (§8.3) — so
  `event_id` as the sole primary key is sufficient for correctness. A consumer
  that merges feeds **MAY** instead use a compound `(source, event_id)` key
  (matching how the cursor is kept per upstream); the `source` column is carried
  for that case. The compound key is tidiness, **not** a correctness requirement
  — do not read it as a MUST.

The effect, the `dedup` insert, and the `feed_offset` cursor advance **MUST**
commit in **one local transaction** (§10) — all three together or none, so no
partial-failure window reopens. A **duplicate** (an `event_id` already in
`dedup`) runs no effect but **MUST** still advance and commit the `cursor`,
exactly as a filtered event does (§7.3, §10): "ignore the repeat" means *run no
effect*, never *skip the commit*. A **best-effort external hop** (§11.2) keeps a
`feed_offset` row and commits its cursor after processing, but needs no `dedup`
row, since it tolerates both loss and duplicates.

A **producer-side** consumer registry (`X-Consumer-Id → committed`) is a separate
table living on the *producer*, part of the deferred retain-until-committed
upgrade (§11.3, §12); it is **not** consumer-side state and is **not** schematized
here.

---

## 11. Delivery guarantee and retention

### 11.1 The guarantee boundary

> **At-least-once from a producer's outbox commit through a consumer's cursor
> commit — the leg between services we control — *provided no consumer falls
> below the retention horizon* (§11.3).**

Beyond that boundary — a service's interaction with an outside system — delivery
is **undefined / best-effort**, decided per service. The event plane is
work-bearing; external hops are not.

The horizon proviso is load-bearing, not a footnote. Day-one retention is a blunt
horizon (§11.3), **not** retain-until-committed (that upgrade is deferred — §12).
So the at-least-once guarantee holds **only while every consumer stays within the
horizon.** A consumer that is offline longer than the horizon falls below the
floor; the producer has already trimmed the events it never received, and on
reconnect it answers with `resync` reason `past-horizon` (§10.1). **Crossing the
horizon breaks at-least-once**, and the break is **detected-but-not-recovered
loss**: the producer detects the stale position and tells the consumer, but the
events are already gone, so the consumer cannot recover them. `resync` *reports*
this loss; it does not redeliver — neither earliest nor latest on reconnect can
reach the abandoned events (§10.1). This is real loss **inside** the controlled
leg, distinct from the best-effort external hop of §11.2. The horizon MUST be
bounded to keep this from happening (§11.3); when it does happen it **SHOULD** be
alarmed, not merely logged (§10.1).

Within that proviso, at-least-once between controlled services means three
things, and **duplicates are possible** (a crash between processing and committing
reprocesses on restart):

- the consumer keeps a **durable committed cursor** and commits **after**
  processing (§10);
- the producer **retains each event at least until it falls below the retention
  horizon** (§11.3) — and the horizon MUST exceed maximum tolerable consumer
  downtime, since a consumer that crosses it loses the events trimmed beneath it
  (above);
- a consumer with a controlled-side effect **dedups on the envelope `id`** (§8.3,
  §10) — the reason that field exists.

A consumer restored from an older snapshot (its own `bin/restore`) is the mirror
of the duplicate case and is correct **by design**: its rolled-back committed
cursor is still a valid position *in the same producer lineage* (the producer's
epoch is unaffected, §9.3, so no `stale-epoch`/`diverged`), so the producer simply
**replays** from that older point and the consumer's envelope-`id` dedup absorbs
any events it had already applied. No consumer-side epoch is needed — replay plus
dedup make consumer rollback safe, just as they make crash-induced duplicates
safe.

An effect performed in reaction to an event is audited as **service-originated**
(actor = the reacting service, with the triggering envelope's `id` and `source`
as the minimal causation breadcrumb), **never** as owner- or client-originated —
the event plane injects no principal (§2), so the effect has no user/client actor
to attribute it to — until full `correlation`/`causation` ids land (§8.8).

### 11.2 External hops are best-effort, by example

A service whose effect is a call to an outside system (e.g. an internet push
service) treats that call as the best-effort, out-of-boundary hop:

- It attempts the external call **once**, fire-and-forget — logs the result but
  does not retry — and then commits its cursor. There is no pending table, no
  draining sender, no backoff, no retry state. A failed external call is simply
  lost.
- Because best-effort tolerates both loss and duplicates, the external call and
  the cursor commit need not be coordinated — the offset-atomicity problem that a
  *durable* side effect would create does not arise.
- The service nonetheless keeps a **durable committed cursor and commits after
  processing**, so the *controlled* leg (producer → this consumer) is still
  at-least-once: a restart resumes from the committed cursor. Only the external
  hop is best-effort.
- Any robustness for the external hop — retries, a local pending store, external
  dedup — is explicitly out of scope now and decided per service when it is
  hardened.

Secrets that gate an external hop (e.g. a push topic or token) are deployment
secrets: injected as environment configuration at the composition root, never in
source or general config.

### 11.3 Retention

**Now: a generous time/size horizon.** For a single, co-located, normally-up
consumer, "keep the outbox long enough that the consumer always catches up"
upholds at-least-once **so long as the consumer stays within the horizon** — and
*only* so long. It is achieved with a dumb horizon and a plain delete:

```
trim where seq ≤ horizon_floor          -- generous: N days / M rows
```

Trimming runs as a **periodic background job, off the hot path** — decoupled from
`Append`, so the write path never pays for it (the §5.3 "none on the hot path"
discipline). And on SQLite a plain `DELETE` does **not** return space to the OS:
under continuous insert-and-trim the database file and its WAL grow without bound
unless reclaim is addressed — so the producer **MUST** reclaim space, by running
under `auto_vacuum` and/or by periodically issuing `wal_checkpoint`/`VACUUM`. This
is a SQLite operational requirement of the dumb horizon, not a wire concern.

No consumer registry, no acks, no TTL. The horizon is what makes at-least-once
real on day one, so it is a contract, not a hope: the horizon **MUST** exceed the
maximum tolerable consumer downtime — sized so that no consumer expected to
recover can fall below the floor before it reconnects. A cursor that nonetheless
falls below the horizon means events were trimmed before that consumer caught past
them: at-least-once is broken for that consumer (§11.1), and the producer reports
it with `resync` reason `past-horizon` (§10.1) — which the producer **SHOULD**
alarm on, because it is detected-but-unrecovered loss, not a routine reposition.
This day-one horizon does **not** retain-until-committed; it cannot recover a
consumer that crosses it. Closing that gap is the deferred upgrade below (and
§12).

**Deferred: exact, retain-until-committed retention.** When disk pressure, or
many / untrusted / long-offline consumers, make a generous horizon untenable, the
upgrade is to retain until the slowest live consumer has committed past an event:

```
trim where seq ≤ MAX( horizon_floor , MIN over live consumers of committed )
```

That requires a producer-side registry (`consumer_id → committed`) keyed on the
stable `X-Consumer-Id`, a TTL/lease so a dead consumer cannot pin the log
forever, the horizon as the ultimate backstop (never ship min-offset logic
without it), and — because SSE is one-directional — a way for the producer to
learn a *busy* consumer's advanced committed offset (an out-of-band ack, or a
max-age cap that forces periodic reconnects carrying the latest committed
cursor). This is why `X-Consumer-Id` is required by the protocol today even
though nothing consumes it yet.

---

## 12. Deferred / non-normative

Explicitly **not** part of the contract today. Listed so the normative boundary
is sharp:

- **Producer-side filtering** (`?types=`) — bandwidth optimization; the `type`
  column already makes it possible. Filtering is consumer-side for now (§7.3).
- **Exact retention (retain-until-committed)** — the registry / stable-id / TTL /
  min-offset / max-age-cap machinery of §11.3.
- **Correlation / causation ids** — multi-hop event-chain tracing (§8.8).
- **Payload versioning (`specversion`)** — additive evolution until the first
  breaking change (§8.8).
- **Binary payload encoding** — JSON for now (§8.7).
- **A built dialect for a concurrent-writer engine** — the seam is defined and
  SQLite is normative; only SQLite is implemented (§9.2).
- **Periodic mid-stream resync** — the connect-time check is normative; periodic
  re-checking is a later option (§10.1).
- **A central event-type registry** — only the naming convention exists (§8.5).
- **Intra-box event-plane isolation** — a Unix-domain socket gated by filesystem
  permissions, or a per-upstream local secret; loopback's accepted residual risk
  (§2) is left unclosed for the demo.
