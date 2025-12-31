# Pillar 2: Runtime System

> This is the second of [four pillars](02-architecture.md). The Runtime provides the coordination infrastructure that agents use to communicate and synchronize.

---

## Purpose

The runtime system provides platform services that agents and webapps consume: task queues, mailboxes, storage, caching, pub/sub, telemetry, and **LLM access via the Ikigai Daemon**. Agents import the `ikigai` package and use its APIs. They're built for Ikigai, but they don't need to know which database their queue lives in or which LLM provider handles their prompts.

```typescript
import { Platform } from "ikigai/platform";

const platform = await Platform.connect();  // connects to ../../daemon.sock
const task = await platform.queue("my-tasks").claim();
await platform.cache.set("key", value);
await platform.pubsub.publish("topic", message);

// LLM access via the daemon
const response = await platform.prompt({
    model: "sonnet-4.5",
    messages: [{ role: "user", content: "Analyze this data..." }],
});
```

The `ikigai` package provides multiple modules (`ikigai/platform`, `ikigai/agent`, etc.) as the API contract. Backend implementations are configuration.

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

## Ikigai Daemon

The Ikigai Daemon exposes `libikigai` to agents over a Unix socket at `.ikigai/daemon.sock`. Agents connect via the relative path `../../daemon.sock` from their service directory. It's the same intelligence that powers the [Terminal](05-terminal.md), available as a platform service.

```
┌─────────────────────────────────────────────────────────────────┐
│                        Agent Process                             │
│  ┌───────────────────────────────────────────────────────────┐  │
│  │              ikigai/platform client                        │  │
│  └─────────────────────────┬─────────────────────────────────┘  │
└────────────────────────────┼────────────────────────────────────┘
                             │ Unix socket (../../daemon.sock)
┌────────────────────────────┴────────────────────────────────────┐
│                       ikigai-daemon                              │
│  ┌───────────────────────────────────────────────────────────┐  │
│  │                      libikigai                             │  │
│  │  LLM Routing • Tools • Conversations • Telemetry          │  │
│  └───────────────────────────────────────────────────────────┘  │
└─────────────────────────────┬───────────────────────────────────┘
                              │
            ┌─────────────────┼─────────────────┐
            ▼                 ▼                 ▼
     LLM Providers       PostgreSQL          Platform
     (OpenAI, etc.)    (conversations)       Services
```

### Why a Daemon?

Agents need LLM capabilities, but duplicating LLM integration in TypeScript would mean:

- Rebuilding provider abstraction, conversation management, tool execution
- Losing unified observability (developer and agent prompts in different systems)
- Managing API keys in multiple places
- Missing platform-native tools that require C-level platform access

The daemon gives agents access to all `libikigai` capabilities:

- **Same LLM routing**: Multi-provider support, failover, model selection
- **Same conversation management**: Threads, history, context optimization
- **Same tool framework**: Platform tools plus agent-defined custom tools
- **Same telemetry**: All LLM interactions logged uniformly
- **Same cost tracking**: Centralized billing attribution

### Agent API

Agents access LLM capabilities through `ikigai/platform`:

```typescript
import { Platform } from "ikigai/platform";

const platform = await Platform.connect();

// Simple prompt
const response = await platform.prompt({
    model: "sonnet-4.5",
    messages: [
        { role: "user", content: "Should I escalate this alert?" }
    ],
});

// With platform tools
const response = await platform.prompt({
    model: "sonnet-4.5",
    messages: [
        { role: "user", content: "Check recent errors and decide if we need to alert." }
    ],
    tools: {
        platform: ["query_telemetry", "send_message"],
        custom: [myCustomToolDefinition],
    },
});

// With conversation threading
const thread = await platform.threads.create({
    context: "Analyzing customer support tickets"
});

await platform.prompt({
    thread: thread.id,
    messages: [{ role: "user", content: "Categorize this ticket..." }],
});

// Follow-up (has context from previous turn)
await platform.prompt({
    thread: thread.id,
    messages: [{ role: "user", content: "What priority did you assign?" }],
});
```

### Platform Tools

The daemon provides built-in tools that agents can use in prompts:

| Tool | Description |
|------|-------------|
| `query_telemetry` | Read platform metrics and logs |
| `create_task` | Queue work for any agent |
| `send_message` | Send to any mailbox |
| `list_agents` | Discover registered agents |
| `read_config` | Access platform configuration |
| `deploy_agent` | Create or update agents (recursive development) |

These tools execute with platform-level access. When an LLM decides to call `create_task`, the daemon executes it with proper PostgreSQL transactions and permissions.

```typescript
// Agent asks LLM to decide and act
const response = await platform.prompt({
    model: "sonnet-4.5",
    messages: [{
        role: "user",
        content: `I received this webhook: ${payload}.
                  Should I create a task for the analysis agent?`
    }],
    tools: {
        platform: ["create_task", "query_telemetry"],
    },
});

// LLM might respond: "Yes, creating a high-priority task..."
// and call create_task automatically
```

### Permission Model

Agent LLM access is controlled via `manifest.json`:

```json
{
    "name": "monitoring-agent",
    "permissions": {
        "llm": {
            "models": ["haiku", "sonnet"],
            "max_tokens_per_request": 4096,
            "max_tokens_per_hour": 100000,
            "platform_tools": ["query_telemetry", "send_message"],
            "allow_custom_tools": true
        }
    }
}
```

The daemon enforces these limits:

- **Model restrictions**: Agent can only use listed models
- **Token quotas**: Per-request and hourly limits
- **Tool access**: Only listed platform tools are available
- **Custom tools**: Can be disabled entirely for sensitive agents

### Observability

All agent LLM interactions are logged to the same telemetry system as developer Terminal sessions:

```sql
-- Query all LLM calls related to a specific task
SELECT * FROM llm_interactions
WHERE metadata->>'task_id' = '...'
ORDER BY created_at;

-- Compare costs across agents
SELECT agent_name, SUM(tokens_used), SUM(cost_usd)
FROM llm_interactions
WHERE created_at > now() - interval '24 hours'
GROUP BY agent_name;
```

This unified observability means:

- Debug agent decisions: "Why did it escalate that alert?"
- Track costs: "Which agent is using the most tokens?"
- Audit: "What did any part of the system send to LLMs?"

### Memory and Context

Agents control their own context window, just as developers do in the Terminal. The daemon exposes both high-level conveniences (equivalent to Terminal commands like `/mark` and `/rewind`) and low-level APIs for direct control.

**High-level API** (Terminal-equivalent operations):

```typescript
// Mark a restoration point
const mark = await platform.context.mark("before-analysis");

// Rewind to a previous mark
await platform.context.rewind(mark);

// Exclude an exchange from future context
await platform.context.exclude(response.id);

// Summarize older history to save context space
await platform.context.summarize({ olderThan: "1h" });
```

**Low-level API** (direct control):

```typescript
// StoredAssets - named, persistent context sections
await platform.storedAssets.create({
    label: "working_state",
    value: "Currently processing batch #1234",
    limit: 2000,
});

await platform.storedAssets.update("working_state", {
    value: "Batch #1234 complete, 47 items processed",
});

const asset = await platform.storedAssets.get("working_state");

// Conversation history management
const messages = await platform.conversation.list({ limit: 50 });
await platform.conversation.delete(messageId);

// Sliding window configuration
await platform.context.window.resize(16000);  // tokens
await platform.context.window.strategy("summarize");  // or "truncate"
```

**LLM-accessible tools** for self-managing context:

```typescript
await platform.prompt({
    messages: [...],
    tools: {
        platform: [
            "memory_read",      // Read a memory block
            "memory_write",     // Update a memory block
            "context_mark",     // Save a restoration point
            "context_exclude",  // Remove an exchange from future context
        ]
    }
});
```

This layered approach means agents can use simple operations for common cases, or drop to low-level APIs when they need precise control over their context and memory.

---

**Next**: [Autonomous Agents](07-agents.md), the long-running processes that consume these services
