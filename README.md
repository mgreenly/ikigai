# ikigenba

ikigenba is a deployable application suite for a single customer on a single
modest server. It is one dashboard, a set of small services, and the operating
conventions that let those services be built, shipped, backed up, restored, and
used by agents.

The bet is simple: a lot of useful business software does not need a cluster,
a fleet of brokers, zero-downtime deploys, or every service authenticating to
every other service. If a customer can accept short, scheduled downtime, the
system can be cheaper, easier to change, and easier to recover. Backups and
restores become normal work instead of an emergency ritual, with concrete
targets: at most an hour of lost data, back online within ten minutes.

The box is closer to an appliance than a cloud platform. It runs quietly, stops
and starts in a known way, and gives the owner a place to grow new services from
prompts.

## The Shape

A ikigenba deployment answers at `<account>.ikigenba.com`. On that box:

- the dashboard owns login, OAuth, API keys and grants, service inventory,
  install guidance, health, and operations;
- each service owns one domain, one SQLite database, one loopback HTTP server,
  and one deployment lifecycle;
- services share an event library for service-to-service messaging;
- nginx is the public front door, routing `/srv/<service>/...` to loopback
  services after asking the dashboard to authenticate owner-facing requests.

The web UI stays small. The dashboard is for identity, grants, install steps,
health, and operations. The product surface is MCP: the user connects their
agent to the services and works through tools rather than screens.

Services do not call each other as a chain of private APIs. They publish facts,
react to facts, keep their own state, and move through their own lifecycles.
The result is still one suite, but not one application with a shared database or
a single release train.

The set of services is deliberately open-ended. One customer might run CRM,
bookkeeping, and notifications. Another might run inventory, scheduling,
document intake, and private analytics. ikigenba fixes the operating model, not
the set of domains.

## AI First

ikigenba assumes that most people will reach business software through a
general-purpose agent: ChatGPT, Claude, Codex, or whatever comes next. The owner
asks for work in plain language; the agent calls well-scoped tools.

MCP is therefore the main interface. Services expose their domains as MCP tools.
The dashboard exposes operations the same way: start, stop, deploy, back up,
restore, check health, manage grants, and stand up new services.

Some services will be ordinary deterministic code. Others will call models to
classify data, draft work, make recommendations, or decide which facts to emit.
That does not change the service model: consume facts, apply domain logic, update
local state, publish new facts. Inference is one kind of decision-making inside
that loop, not a separate coordination mechanism.

The intended development loop is:

1. Ask the dashboard agent to create a new service from the skeleton.
2. Implement the domain behavior in an online coding sandbox.
3. Use the operations MCP to build, deploy, start, stop, back up, and restore it.
4. Let the new service expose its tools and join the event plane.

The skeleton matters because it carries the dull parts: migrations, logging,
MCP wiring, event producer and consumer support, the deploy contract, the nginx
fragment, and the manifest. Concretely, a service is one static Go binary built on
the shared `appkit` chassis; a single shared deploy wrapper builds it off-box from
a tag and an on-box CLI installs it into a versioned release directory. A new
service should start with its place in the suite already settled, leaving the
domain as the real work.

## Two Communication Planes

ikigenba separates external and internal traffic.

The north/south plane is public and owner-facing. An MCP client reaches
`https://<account>.ikigenba.com/srv/<service>/mcp` through nginx. nginx asks the
dashboard to authenticate the request, strips the `/srv/<service>/` prefix, and
forwards the request to the loopback service with trusted identity headers.
Services trust those headers; they do not run their own OAuth servers, token
stores, or grant screens.

The east/west plane is internal. It is loopback-only, never touches nginx, and
uses a small SSE outbox protocol described in
[`docs/sse-outbox-protocol.md`](docs/sse-outbox-protocol.md) and
[`docs/event-protocol.md`](docs/event-protocol.md).

The split follows from the deployment model. One box means one customer.
Services on that box are not crossing an ownership boundary when they talk to
each other, so service tokens and dashboard introspection on internal feeds would
add machinery without changing the trust boundary. The edge is strict; behind
it, the services share the box.

## The Event Model

Services coordinate through facts instead of chains of synchronous calls.

When a producer changes state, it writes an event into an outbox table in the
same SQLite transaction as the domain change. The data and the event commit
together or not at all. Consumers subscribe to the producer's loopback
`GET /feed` endpoint over Server-Sent Events, keep a durable cursor, and
reconnect from the last position they actually committed.

The protocol is small:

- the producer owns an append-only outbox in its own database;
- publishing an event is atomic with the domain write;
- consumers pull from producers over one long-lived SSE connection;
- cursors are opaque strings minted by the producer;
- a consumer advances its cursor only after it has handled the event to its own
  durability standard;
- a caught-up feed blocks instead of polling;
- slow consumers apply TCP backpressure, so backlog stays on the producer's
  disk;
- restore, rebuild, and truncation are handled by generation-aware cursors and
  explicit resync frames.

This is not Kafka, RabbitMQ, NATS, Redis Streams, or a hosted queue. It keeps the
useful part of a durable log and removes the broker as a separate thing to run:
no cluster to tune, no broker database with its own backup story, no
consumer-group protocol, no web of cross-service credentials. The log lives
beside the data that produced it and is restored with it.

## Scheduled Downtime

ikigenba does not optimize for staying online through every deploy or every
failure.

That choice saves more than infrastructure. It avoids data replication,
multi-node coordination, online schema migrations, heavier deploy tooling, and
the extra system states that make recovery procedures fragile. For customers who
can accept short downtime, it is better to take it deliberately, take it often,
and use it to rehearse the exact steps that matter during a real incident.

The lifecycle is part of the product contract. Each app binary self-implements
`serve`, `version`, `manifest`, `migrate`, `backup`, and `restore`; the operator
drives `deploy` (build a tagged release off-box and install it on the box),
`start`/`stop`, and `teardown`. Deploys land in versioned release directories with
an atomic swap and a one-command rollback. Backups are written and restored from.
Restoring is a practiced motion, not a last resort.

The goal is recovery the owner can reason about:

- bounded data loss;
- bounded time back online;
- a restore path that has already been exercised;
- state stored as ordinary local files;
- event streams that make replay after a restore explicit.

## Why It Is Fast To Build On

ikigenba is narrow on purpose. Every service can have the same anatomy:

- one static Go binary over SQLite, built on the shared `appkit` chassis, with
  embedded migrations and structured logs;
- a dedicated system user and an `/opt/<service>` install root with versioned
  release directories;
- a loopback HTTP server, with public routing handled by nginx;
- MCP tools for the agent-facing surface;
- producer and consumer roles built on the shared event library;
- the same fixed set of binary subcommands (`serve`/`version`/`manifest`/
  `migrate`/`backup`/`restore`), shipped by one shared deploy wrapper and the
  on-box `opsctl`;
- a manifest the binary emits and a routing fragment that make deployment
  mechanical.

That sameness is where the speed comes from. Once the skeleton is right, local
development, auth, deployment, operations, and the event model are already
handled. A new service is mostly the domain work unique to it.

It also keeps the box cheap. Idle services stay idle, and caught-up feeds do not
spin. On a burstable instance, the box can bank credits while nothing is
happening instead of spending them on an always-on broker or orchestration layer.

ikigenba's host requirements are modest: a conservative Linux VM, DNS, durable
backup storage, and a place to keep secrets. AWS is a convenient place to run it,
not a core dependency. The same suite should run on another provider or on a
plain VPS.

## The Trade

ikigenba is dependable because it is willing to be specific.

It serves one customer per box, not many tenants on a shared cluster. It is for
people who can schedule downtime, not for anyone who must stay up through every
deploy. Its internal plane trusts the services sharing the box. Its first
retention model keeps a bounded horizon of events, so a consumer that stays
offline too long is told it has lost its place instead of being promised a replay
that no longer exists.

Those limits are the design. Giving up high availability where it is not needed
buys back cost, speed, and a system small enough to understand. Making agents and
MCP the way in lets an owner use and extend the suite by asking for what they
want. Making backup and restore routine turns recovery into something they can
count on.

Within those conditions, ikigenba aims to be a small, coherent suite that is
quick to build on, cheap to keep running, and clear about when it is the wrong
choice.
