# Architecture

**Status:** Early design discussion, not finalized.

## Overview

PostgreSQL is the coordination layer. All agent state, queues, and inboxes live in the database. Threads are independent - they don't share memory for coordination, they query the database.

## Threading Model

```
┌─────────────────────────────────────────────┐
│              Main Thread                     │
├─────────────────────────────────────────────┤
│  Config loading                              │
│  Terminal setup / raw mode                   │
│  Input handling (keystrokes)                 │
│  Screen rendering                            │
│  Agent manager (spawn/destroy threads)       │
│  DB connection (UI queries)                  │
└─────────────────────────────────────────────┘
         │
         │ spawn/manage
         ▼
┌─────────────────────────────────────────────┐
│           Agent Thread (N)                   │
├─────────────────────────────────────────────┤
│  DB connection (own handle)                  │
│  LLM client (own curl handle)                │
│  Scrollback buffer                           │
│  Event loop: LLM ↔ tools ↔ DB               │
└─────────────────────────────────────────────┘
```

### What's Shared (Singleton)

- Config (read-only after init)
- Terminal state
- Input handling
- Rendering (reads agent scrollback)
- Agent manager

### What's Per-Agent (Thread)

- Database connection
- LLM client
- Scrollback buffer
- Working directory (if worktree)

### Thread Safety

Agents don't share memory for coordination. PostgreSQL handles:
- Queue state
- Inbox delivery
- Message persistence
- Concurrency (MVCC, transactions)

Only shared memory is scrollback buffer (agent writes, main thread reads for rendering). Needs mutex.

## Database Schema

### Existing Tables

```sql
sessions (
    id BIGSERIAL PRIMARY KEY,
    started_at TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    ended_at TIMESTAMPTZ,
    title TEXT
)

messages (
    id BIGSERIAL PRIMARY KEY,
    session_id BIGINT NOT NULL REFERENCES sessions(id),
    agent_id BIGINT REFERENCES agents(id),  -- NEW: which agent
    kind TEXT NOT NULL,
    content TEXT,
    data JSONB,
    created_at TIMESTAMPTZ NOT NULL DEFAULT NOW()
)
```

### New Tables

```sql
agents (
    id BIGSERIAL PRIMARY KEY,
    session_id BIGINT NOT NULL REFERENCES sessions(id),
    name TEXT NOT NULL,
    type TEXT NOT NULL,        -- 'main', 'helper', 'sub'
    parent_id BIGINT REFERENCES agents(id),  -- NULL for main
    status TEXT NOT NULL,      -- 'active', 'finished', 'killed'
    worktree_path TEXT,
    created_at TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    finished_at TIMESTAMPTZ,

    UNIQUE(session_id, name)
)

queue (
    id BIGSERIAL PRIMARY KEY,
    agent_id BIGINT NOT NULL REFERENCES agents(id),  -- owner's queue
    name TEXT,
    prompt TEXT NOT NULL,
    model TEXT,
    timeout_seconds INT,
    status TEXT NOT NULL,      -- 'queued', 'running', 'finished'
    runner_agent_id BIGINT REFERENCES agents(id),  -- sub-agent executing this
    created_at TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    started_at TIMESTAMPTZ,
    finished_at TIMESTAMPTZ
)

inbox (
    id BIGSERIAL PRIMARY KEY,
    to_agent_id BIGINT NOT NULL REFERENCES agents(id),
    from_agent_id BIGINT NOT NULL REFERENCES agents(id),
    content TEXT NOT NULL,
    is_final_result BOOLEAN NOT NULL DEFAULT FALSE,
    created_at TIMESTAMPTZ NOT NULL DEFAULT NOW()
)
```

### Indexes

```sql
CREATE INDEX idx_agents_session ON agents(session_id);
CREATE INDEX idx_agents_parent ON agents(parent_id);
CREATE INDEX idx_messages_agent ON messages(agent_id, created_at);
CREATE INDEX idx_queue_agent_status ON queue(agent_id, status);
CREATE INDEX idx_inbox_to_agent ON inbox(to_agent_id, created_at);
```

## Database Operations

### Queue Operations

```sql
-- /push
INSERT INTO queue (agent_id, name, prompt, model, timeout_seconds, status)
VALUES ($1, $2, $3, $4, $5, 'queued')
RETURNING id;

-- /run (start all)
UPDATE queue
SET status = 'running', started_at = NOW()
WHERE agent_id = $1 AND status = 'queued'
RETURNING *;

-- /run N (start up to N)
UPDATE queue
SET status = 'running', started_at = NOW()
WHERE id IN (
    SELECT id FROM queue
    WHERE agent_id = $1 AND status = 'queued'
    ORDER BY created_at
    LIMIT $2
)
RETURNING *;

-- /queue (show status)
SELECT * FROM queue
WHERE agent_id = $1
ORDER BY created_at;
```

### Inbox Operations

```sql
-- /send
INSERT INTO inbox (to_agent_id, from_agent_id, content, is_final_result)
VALUES ($1, $2, $3, $4);

-- Also notify for blocking receive
NOTIFY inbox_agent_$1;

-- /check (non-blocking, FIFO)
DELETE FROM inbox
WHERE id = (
    SELECT id FROM inbox
    WHERE to_agent_id = $1
    ORDER BY created_at
    LIMIT 1
)
RETURNING *;

-- /check from=X (non-blocking, specific sender)
DELETE FROM inbox
WHERE id = (
    SELECT id FROM inbox
    WHERE to_agent_id = $1 AND from_agent_id = $2
    ORDER BY created_at
    LIMIT 1
)
RETURNING *;

-- /receive (blocking) - use LISTEN/NOTIFY
LISTEN inbox_agent_$1;
-- Poll with PQconsumeInput + PQnotifies
-- When notified, run /check query
```

### Agent Lifecycle

```sql
-- Create main agent (on session start)
INSERT INTO agents (session_id, name, type, status)
VALUES ($1, 'main', 'main', 'active')
RETURNING id;

-- Create helper agent (/agent new)
INSERT INTO agents (session_id, name, type, parent_id, status, worktree_path)
VALUES ($1, $2, 'helper', $3, 'active', $4)
RETURNING id;

-- Create sub-agent (when /run starts a task)
INSERT INTO agents (session_id, name, type, parent_id, status)
VALUES ($1, $2, 'sub', $3, 'active')
RETURNING id;

-- Link task to runner
UPDATE queue SET runner_agent_id = $1 WHERE id = $2;

-- Finish agent
UPDATE agents SET status = 'finished', finished_at = NOW()
WHERE id = $1;

-- Kill agent
UPDATE agents SET status = 'killed', finished_at = NOW()
WHERE id = $1;
```

### Replay (Agent-Aware)

```sql
-- Replay specific agent's conversation
SELECT * FROM messages
WHERE session_id = $1 AND agent_id = $2
ORDER BY created_at;
```

## LISTEN/NOTIFY for Blocking

PostgreSQL's LISTEN/NOTIFY enables `/receive` without busy polling:

```c
// Setup listener
PQexec(conn, "LISTEN inbox_agent_123");

// In event loop
while (waiting) {
    PQconsumeInput(conn);
    PGnotify *notify = PQnotifies(conn);
    if (notify) {
        // Message arrived, run /check query
        PQfreemem(notify);
        break;
    }
    // Brief sleep or select() on socket
}
```

When another agent sends:
```sql
INSERT INTO inbox (...) VALUES (...);
SELECT pg_notify('inbox_agent_123', '');
```

## Event Logging

The `messages` table stores conversation events for replay:
- clear, system, user, assistant, mark, rewind

Operational tables (queue, inbox, agents) handle mutable state.

**Not logged initially:**
- Agent created/finished
- Task started/finished
- Messages sent/received

Add logging when needed for debugging or audit.

## Related

- [agents.md](agents.md) - Agent types and UI model
- [agent-queues.md](agent-queues.md) - Queue and inbox commands
