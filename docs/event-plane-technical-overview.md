# Event Plane Technical Overview

This document is a first technical look at the ikigenba event plane: what it
is, how the current `crm -> notify` implementation works, why it is shaped this
way, and how it compares with more common messaging tools and protocols.

The short version: this is an internal, single-box messaging design built from
an atomic database outbox and a loopback Server-Sent Events feed. It is not a
general-purpose message broker. It deliberately avoids a separate broker process,
consumer polling loops, cross-service token machinery, and highly available
cluster assumptions. The design goal is a quiet, low-resource appliance-style
system where services stay independent, consumers recover after restarts, and
backup/restore is part of normal operations rather than an untested emergency
procedure.

The normative protocol is `docs/event-protocol.md`; the concrete first slice is
described in `docs/event-plane-decisions.md`. The shared implementation lives in
`eventplane/`, the producer integration is in `crm/`, and the first consumer is
`notify/`.

## System Context

The suite runs on one box per customer. The public surface is the dashboard
plus path-routed services behind nginx. That north/south plane handles external
owner-facing traffic: Claude clients reach MCP/REST endpoints through nginx,
nginx authenticates through the dashboard, strips `/srv/<service>/`, and injects
identity headers. Services trust those headers and do no token validation of
their own.

The event plane is separate. It is east/west service-to-service traffic inside
the same customer box. A service that wants to react to another service's facts
connects directly to that producer's loopback feed. The feed is not exposed
through nginx, is unauthenticated, and is reachable only on `127.0.0.1`.

That split is central. The event plane has no second owner or client principal
to authenticate, because one box equals one customer. Adding OAuth, service
tokens, or dashboard introspection to the feed would add complexity without
changing the trust boundary. It would also place nginx and proxy buffering in the
middle of a streaming path whose value depends on direct TCP backpressure.

Topology lives outside each service except for the upstream feed addresses it is
given. A consumer does not discover peer ports by inspecting the filesystem and
does not know the whole graph. It reads an injected URL such as
`NOTIFY_FEED_URL=http://127.0.0.1:3001/feed` and connects. That keeps the service
code independent of deployment layout while making the dependencies explicit at
the composition root.

## The Core Pattern

The producer side is the atomic outbox pattern. When `crm` creates a contact, it
inserts the contact and inserts one `contact.created` outbox row in the same
SQLite transaction. The transaction commits both or neither. There is no
dual-write window where the contact exists but the event was lost, or where an
event exists for a rolled-back contact.

`crm/internal/contacts/service.go` is the important integration point:
`CreateContact` performs the contact writes, calls `Outbox.Append(tx, ev)` on
the same `*sql.Tx`, commits, and only then calls `Outbox.Ring()`. The ring is an
in-process doorbell that wakes parked feed connections. It carries no event
data. If the doorbell is missed, nothing is lost, because the outbox row is the
mail and the doorbell only says "look sooner."

The event payload is a snapshot, not a pointer back into `crm`. The current
message type is `contact.created`, with the contact id, display name, name
fields, emails, phones, and creation timestamp. `notify` can act from the event
alone; it does not call back into `crm` to interpret the message. This is one of
the main independence properties: producers publish facts, consumers react to
facts, and the two do not coordinate synchronously.

The shared producer library in `eventplane/outbox` owns the durable table shape,
the feed handler, generation-wrapped cursors, retention, and the startup
ordering probe. Services apply the library DDL through their own migration
runners, and tests assert the local migrations match the library constants.

## The Feed Protocol

A producer exposes `GET /feed` on its loopback HTTP server. The response is a
standard SSE stream. A consumer opens one long-lived connection, sends
`X-Consumer-Id`, and optionally sends `Last-Event-ID` containing the last
durably committed cursor. If no cursor exists, the first subscription can start
from the beginning or use `?from=tail` to start after the current head.

Each event frame contains:

```text
id: <opaque cursor>
event: <message type>
data: <event envelope JSON>
```

The `id:` is stream position. The envelope `id` is event identity. The
`payload.id` is the domain entity identity. Those are deliberately separate.
The cursor is opaque to consumers: they store it, present it, and never parse or
compare it. Today the SQLite cursor is internally encoded as
`<generation>.<seq>`, but consumers must not depend on that.

The envelope is CloudEvents-aligned without adopting the full CloudEvents spec:
`id`, `type`, `source`, `time`, and `payload`. The body key is `payload` rather
than `data` to avoid confusion with the SSE `data:` wire field.

The feed also sends control and liveness frames:

- `event: caught-up` with `data: {}` when the producer has drained to its current
  head.
- `event: status` with `{"behind": N}` as optional lag telemetry.
- `event: resync` with a reason when the presented cursor cannot be honored.
- `: keepalive` comments while idle so dead pipes are noticed.

Control frames carry no cursor and therefore cannot advance consumer position.
Only real event frames with an `id:` line can move the cursor.

## Zero Busy Waiting

The headline property is zero-busy-wait. A caught-up consumer does not poll. It
holds a single HTTP connection and blocks in a read. The producer does not spin
on its database either; it parks feed goroutines on the doorbell and wakes them
when a committed row exists. Keepalive comments are the only periodic activity
on a quiet connection.

Backpressure is inherited from TCP. The consumer asks for the next event by
reading bytes. If it stops reading because it is busy, the TCP receive window
fills, the producer's write blocks, and the backlog remains in the producer's
SQLite outbox. The producer streams bounded batches and flushes each frame, and
tests assert that a slow reader does not cause the producer to buffer the whole
backlog in memory.

This is why the design uses SSE instead of periodic polling or long polling.
Pure polling is simple but burns CPU and burst credits on quiet boxes. Long
polling reduces idle cost but recreates a request/response cycle per event or
batch. SSE gives one ordinary HTTP connection, standard framing, built-in resume
vocabulary through `Last-Event-ID`, and direct TCP flow control.

## Ordering and SQLite

The correctness invariant is: outbox sequence assignment order equals commit
order equals visibility order. A consumer asks for rows after its cursor, in
order. That is only safe if a row can never commit behind a cursor the consumer
has already advanced past.

SQLite gives this property naturally because it allows exactly one writer at a
time. WAL mode allows concurrent readers, not concurrent writers. Once a writer
has the write lock, no second writer can assign the next outbox sequence until
the first commits or rolls back. That makes the simple SQLite fetch correct:

```sql
WHERE seq > ?
ORDER BY seq
LIMIT ?
```

The implementation still protects the assumption mechanically. The database is
opened with single-writer-friendly settings, the outbox library runs a startup
probe that proves a second concurrent `BEGIN IMMEDIATE` is refused, and the test
suite includes a concurrency stress test that continuously writes and fetches to
ensure no row appears behind the reader's cursor.

This is one of the places where the design would be harder on Postgres or
another concurrent-writer engine. A naive sequence column is not enough there:
transactions can receive sequence values in one order and commit in another. A
future dialect would need a stable transaction frontier and a compound cursor.
That is why the cursor is opaque and why the code is structured around a dialect
seam even though only SQLite is implemented now.

## Recovery, Restore, and Self-Healing

Consumers keep durable offset state in their own SQLite database. For `notify`,
that is the `feed_offset` table: one row per upstream source, an opaque cursor,
a `subscribed` marker, and an update timestamp.

On normal transport failure, the consumer reconnects indefinitely with
exponential backoff and presents the last committed cursor. It does not require
systemd startup ordering to be correct. If `notify` starts before `crm`, it keeps
retrying. If the connection drops, it reconnects. If the process restarts, it
loads the committed cursor and resumes strictly after it.

The consumer must present the committed cursor, not the last received cursor.
That is the difference between at-least-once and silent loss. If it receives an
event and crashes before committing the cursor, it must receive that event again
after restart.

The producer embeds a generation or epoch token in every cursor. This exists
because backup/restore is a normal lifecycle operation. SQLite's `AUTOINCREMENT`
high-water mark lives inside the database file, so restoring an older database
snapshot can roll sequence numbers back. Without a generation token, a consumer
with cursor 500 could reconnect after a producer restore and treat an unrelated
new sequence 501 as the natural successor to the old lineage. The generation
sidecar lives outside the database file; a restore deletes or remints it, and
the producer rejects old cursors with `resync` reason `stale-epoch`.

Other `resync` reasons cover other invalid positions:

- `diverged`: cursor is ahead of the producer head, as after an older producer
  restore.
- `past-horizon`: cursor is below the retention floor, meaning real events were
  trimmed before this consumer caught up.
- `unintelligible-cursor`: cursor cannot be parsed or belongs to another feed.
- `stale-epoch`: cursor belongs to an old producer lineage.

The consumer response is mechanically similar: discard the stored position and
re-bootstrap. The meanings are not equivalent. `past-horizon` is real,
detected-but-unrecovered loss inside the controlled service-to-service leg.

The design also handles consumer restore cleanly. If a consumer is restored from
an older snapshot, its cursor rolls back. The producer still has the same
lineage, so it replays from that older point. A consumer with durable
controlled-side effects would deduplicate on the envelope event id. `notify`
does not have such a durable effect; duplicate pushes are acceptable.

## Retention and the Nightly Downtime Model

Day-one retention is a generous blunt horizon: keep events by time and row count,
trim old rows in a background job, and reclaim SQLite space with checkpoint and
`VACUUM`. There is no producer-side registry of every consumer's committed
offset and no acknowledgement protocol.

That choice fits the current operating model: a small single-tenant box, a small
number of known services, and a lifecycle that intentionally includes scheduled
nightly downtime for backup/restore practice. The system is not pretending to be
an always-on highly available cluster. The backup/restore path is exercised
regularly so restore semantics, including producer generation reminting and
consumer replay, are part of normal operation.

The tradeoff is explicit. At-least-once delivery between controlled services
holds only while consumers stay within the retention horizon. If a consumer is
offline past that floor, the producer can detect the condition and emit
`past-horizon`, but it cannot redeliver events it has already trimmed.

A future exact-retention model is documented but deferred. It would retain until
the slowest live consumer has committed, requiring a producer-side consumer
registry, leases or TTLs so dead consumers do not pin the log forever, and some
acknowledgement path or forced reconnect cadence because SSE is one-way.

## `notify`: Best-Effort External Delivery

`notify` is the first consumer and intentionally uses weaker semantics for its
external hop. It subscribes to `crm`'s feed, filters for `contact.created`, and
fires one ntfy.sh POST with title `New contact` and the contact display name as
the body. It does this asynchronously in a timeout-bound goroutine.

The eventplane consumer engine used by `notify` commits the cursor after every
event regardless of handler outcome. Handler errors are logged and ignored.
Non-matching events still advance the cursor. There is no pending notification
table, retry loop, or dedup table.

That is correct for this first effect because ntfy delivery is explicitly
best-effort. The controlled leg is `crm` outbox commit through `notify` cursor
commit. The external leg, `notify -> ntfy.sh`, is outside that guarantee. A slow
or unavailable ntfy service must not stall the feed or burn resources. A missed
push is acceptable; a duplicate push after replay is acceptable.

This is not the right engine semantics for every future consumer. A future
consumer with a durable controlled-side effect would need to apply the effect,
insert the event id into a dedup table, and commit the cursor in one local
transaction. On effect failure it would stall and retry the same event rather
than advance.

## Comparison With Common Messaging Tools

Compared with Kafka, this design borrows the idea of an append-only log and
consumer-held offsets, but removes the broker cluster. Kafka is built for high
throughput, partitioning, retention policies, consumer groups, replay, and
multi-node operation. The ikigenba event plane is intentionally smaller: one
producer's SQLite outbox is the log, one SSE connection is the subscription, and
the service database is the persistence layer. There are no partitions, group
rebalancing, broker metadata, or separate operational surface.

Compared with RabbitMQ or AMQP queues, this is less broker-centric and less
command-oriented. RabbitMQ models queues, exchanges, bindings, acknowledgements,
redelivery, and routing policies inside a broker. The ikigenba design keeps
routing outside the producer, has no broker acknowledgements, and makes
consumers own their offsets. It is closer to "read my durable feed" than "the
broker owns pending deliveries."

Compared with NATS or NATS JetStream, this design is much less general. Core
NATS is excellent lightweight pub/sub but volatile unless JetStream is added.
JetStream adds persistence, replay, acknowledgements, retention policies, and
consumer state. The ikigenba event plane implements only the subset needed for
one box: durable producer log, replay after cursor, and low idle cost. It avoids
running and configuring another daemon.

Compared with Redis Streams, the concepts are familiar: stream ids, consumers,
pending messages, and replay. Redis Streams would provide richer consumer-group
semantics and operational tooling, but it introduces another stateful service to
backup, restore, secure, and monitor. Here the state lives in each service's
SQLite file, which aligns with the suite's existing backup/restore discipline.

Compared with AWS SQS/SNS or cloud pub/sub systems, this is intentionally local.
Managed queues solve durability and fanout outside the box and are a good fit
for cloud-native HA services. They also add network dependencies, IAM, billing,
delivery semantics to learn, and another restore story. The ikigenba design favors a
single appliance that can run through local downtime and restore practice without
depending on external infrastructure for internal coordination.

Compared with Postgres `LISTEN/NOTIFY`, this design is more durable.
`LISTEN/NOTIFY` is useful as a wakeup signal but not a durable event log; if a
listener is down, notifications are missed. The ikigenba doorbell is similarly
only a wakeup signal, but the actual event is stored in the outbox. Missing the
doorbell is harmless.

Compared with webhooks, the direction is inverted. Webhooks push HTTP requests
from producer to consumer and require retry policy, endpoint auth, timeout
handling, and failure queues if they need durability. Here the consumer opens
the feed and controls its own recovery position. The producer never calls the
consumer and does not know who is listening.

Compared with polling a REST endpoint, the event plane avoids idle work. Polling
is easy and often good enough, but on burstable or low-resource hardware a fleet
of idle pollers slowly spends CPU for no state change. The SSE feed parks when
quiet and wakes on actual commits.

Compared with WebSockets, SSE is narrower and better matched. WebSockets are
bidirectional and useful for interactive sessions. This protocol is
producer-to-consumer only, uses ordinary HTTP GET, has simple text framing, and
gets `Last-Event-ID` as a native resume channel. The one-way limitation is also
why exact retain-until-committed retention is deferred: if the producer needs to
learn committed offsets mid-connection, SSE needs an additional ack path or
periodic reconnects.

Compared with CloudEvents, this design uses a CloudEvents-like envelope but not
the full protocol family. That is pragmatic: the fields are familiar and useful,
while the transport and cursor rules are specific to this SSE outbox feed.

## Strategy and Tradeoffs

The design optimizes for a specific product shape: small single-tenant boxes,
independent services, SQLite state, local loopback communication, predictable
nightly downtime, and low idle cost. It avoids general-purpose broker complexity
because most broker features are not free operationally: they need their own
storage, backups, upgrades, credentials, monitoring, and failure modes.

The independence goal is mostly achieved by three boundaries:

- Producers publish self-contained facts into their own outbox and do not know
  who consumes them.
- Consumers know only configured upstream feed URLs and their own durable cursor.
- The platform owns topology and injects addresses; services do not infer the
  graph.

The self-healing goal is addressed by committed cursors, indefinite reconnect,
generation-aware producer restore detection, resync frames, and consumer replay.
The scheduled downtime and backup/restore strategy is not an afterthought; it is
part of the correctness model. Restores are expected, practiced, and represented
in the cursor design.

The design's main limits are equally clear:

- It is single-box and loopback-trust oriented, not multi-tenant or cross-host.
- Day-one retention is horizon-based, so very long consumer downtime can produce
  detected loss.
- The current consumer engine is best-effort-effect oriented and will need a
  stricter variant for durable controlled-side effects.
- There is no producer-side filtering yet; consumers receive all event types and
  advance over skipped ones.
- Intra-box isolation is accepted as a demo risk: any local process that can
  reach loopback can read the feed.

Those limits are coherent with the current goals. This event plane is not trying
to compete with Kafka, RabbitMQ, or managed queues. It is a small, inspectable,
standard-HTTP event log for a single appliance, designed to stay quiet when idle,
recover from ordinary service restarts, and make restore behavior explicit
instead of accidental.
