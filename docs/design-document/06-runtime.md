# Pillar 2: Runtime System

> This is the second of [four pillars](02-architecture.md). The Runtime provides the coordination infrastructure that agents use to communicate and synchronize.

---

## Purpose

The runtime system provides platform services that agents and webapps consume: task queues, mailboxes, storage, caching, pub/sub, and telemetry. Agents import `@ikigai/platform` and use its APIs. They're built for Ikigai, but they don't need to know which database their queue lives in.

```typescript
import { Platform } from "@ikigai/platform";

const platform = await Platform.connect();
const task = await platform.queue("my-tasks").claim();
await platform.cache.set("key", value);
await platform.pubsub.publish("topic", message);
```

The platform package is the API contract. Backend implementations are configuration.

---

## Configurable Backends

Each platform service can be configured independently in `ikigai.conf`. The default configuration uses PostgreSQL for everything, one dependency that handles all services adequately for most deployments.

```
# Default configuration - PostgreSQL for everything
[services]
queues = postgres
mailboxes = postgres
cache = postgres
pubsub = postgres
storage = postgres
telemetry = postgres
```

### Why PostgreSQL as Default

PostgreSQL handles queues, pub/sub, caching, and storage in one place. This isn't just convenience; it enables things that are difficult with separate services:

- **Transactional integrity across operations**: Claim a task, write results, send a message, all in one transaction. If the agent crashes mid-operation, everything rolls back cleanly. This consistency is hard to achieve when tasks live in RabbitMQ, results in Postgres, and messages in Redis.
- **Single backup target**: One database to snapshot, replicate, and restore.
- **No network hops**: On a single server, everything moves through Unix sockets. Latency is measured in microseconds.
- **LISTEN/NOTIFY** enables real-time coordination without polling
- **Full-text search** for querying conversation history and logs
- **pgvector extension** enables semantic search and RAG capabilities
- **Mature tooling** for backup, replication, monitoring

PostgreSQL is not a specialized message queue or time-series database. But for typical agent deployments, it handles all these roles well, and "good enough in one place" beats "optimal in six places you have to operate separately."

---

## Alternative Backends

When specific needs arise, individual services can be reconfigured:

```
# Example: specialized backends for specific services
[services]
queues = postgres          # ACID transactions matter here
mailboxes = postgres       # Keep it simple
cache = redis              # Need faster caching
pubsub = nats              # Need higher message throughput
storage = postgres         # Primary data store
telemetry = timescaledb    # Time-series optimized
```

Supported backends (current and planned):

| Service | Default | Alternatives |
|---------|---------|--------------|
| queues | postgres | redis, rabbitmq |
| mailboxes | postgres | redis, nats |
| cache | postgres | redis |
| pubsub | postgres | redis, nats |
| storage | postgres | - |
| telemetry | postgres | timescaledb, influxdb |

**Agent code doesn't change.** The platform configuration does.

---

## Core Abstractions

The runtime provides four core abstractions. The examples below show the PostgreSQL implementation; the concepts are backend-agnostic.

### Task Queues

Agents pull work from task queues. Each queue is a logical grouping of tasks with priority and scheduling.

```sql
CREATE TABLE task_queue (
    id              UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    queue_name      TEXT NOT NULL,
    payload         JSONB NOT NULL,
    priority        INTEGER DEFAULT 0,
    scheduled_for   TIMESTAMPTZ DEFAULT now(),
    claimed_by      TEXT,           -- agent name
    claimed_at      TIMESTAMPTZ,
    completed_at    TIMESTAMPTZ,
    failed_at       TIMESTAMPTZ,
    error           TEXT,
    created_at      TIMESTAMPTZ DEFAULT now()
);
```

Agents claim tasks atomically:

```sql
UPDATE task_queue
SET claimed_by = 'monitoring-agent', claimed_at = now()
WHERE id = (
    SELECT id FROM task_queue
    WHERE queue_name = 'monitoring-tasks'
      AND claimed_by IS NULL
      AND scheduled_for <= now()
    ORDER BY priority DESC, created_at
    FOR UPDATE SKIP LOCKED
    LIMIT 1
)
RETURNING *;
```

### Mailboxes

Agents communicate through mailboxes. A mailbox is a named destination for messages with a single consumer, the agent that owns the mailbox. This follows the actor model: each agent has its own mailbox, and messages are delivered point-to-point.

```sql
CREATE TABLE mailbox (
    id              UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    mailbox_name    TEXT NOT NULL,
    sender          TEXT NOT NULL,  -- agent name
    payload         JSONB NOT NULL,
    created_at      TIMESTAMPTZ DEFAULT now(),
    read_at         TIMESTAMPTZ,
    read_by         TEXT
);
```

Future versions may introduce named topics with multiple subscribers for broadcast patterns.

### Agent Registry

The runtime tracks registered agents and their status.

```sql
CREATE TABLE agent_registry (
    name            TEXT PRIMARY KEY,
    version         TEXT NOT NULL,
    manifest        JSONB NOT NULL,
    deployed_at     TIMESTAMPTZ,
    status          TEXT,           -- running, stopped, failed
    last_heartbeat  TIMESTAMPTZ,
    server          TEXT            -- which server it's deployed to
);
```

### Telemetry

Agents emit telemetry that feeds back to the developer through Ikigai Terminal.

```sql
CREATE TABLE telemetry (
    id              UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    agent_name      TEXT NOT NULL,
    event_type      TEXT NOT NULL,  -- task_completed, error, metric, log
    payload         JSONB NOT NULL,
    created_at      TIMESTAMPTZ DEFAULT now()
);
```

---

## Real-time Coordination

PostgreSQL LISTEN/NOTIFY enables agents to react immediately:

```sql
-- When a task is inserted
NOTIFY monitoring_tasks, '{"id": "...", "priority": 1}';
```

```typescript
// Agent listens
const listener = await db.listen("monitoring_tasks");
for await (const notification of listener) {
    // Wake up and check the queue
}
```

This avoids polling while keeping operational simplicity. Other backends would use their native pub/sub mechanisms (Redis SUBSCRIBE, NATS subjects, etc.).

---

**Next**: [Autonomous Agents](07-agents.md), the long-running processes that consume these services
