# Agent Process Model

This document explores a Unix/Erlang-inspired process model for ikigai agents.

## Core Philosophy

Agents are processes. The mental model draws from:
- **Unix**: fork(), signals, process table, orphan reparenting
- **Erlang**: Lightweight processes, mailboxes, message passing

## Agent Identity

- **UUID**: Internal unique identifier, base64url encoded (22 chars, never reused)
- **Name**: Optional human-friendly name
- **Relationships**: Parent knows child UUIDs, child knows parent UUID

## Agent Registry

The agent registry (database table) is the **source of truth** for agent existence, not parent memory.

```sql
CREATE TABLE agents (
    uuid          TEXT PRIMARY KEY,
    name          TEXT,              -- optional human-friendly name
    parent_uuid   TEXT REFERENCES agents(uuid),
    fork_message_id TEXT,            -- where we branched from parent's history
    status        TEXT NOT NULL,     -- 'running', 'dead'
    created_at    INTEGER NOT NULL,
    ended_at      INTEGER
);
```

Key properties:
- Never deletes records (history of all agents)
- Tracks parent-child relationships
- Tracks status for replay on startup
- fork_message_id enables delta-based history storage

## Fork: The Creation Primitive

`/fork` is the only way to create agents. Available as both command and tool.

**With prompt (task delegation):**
```
/fork "analyze the database schema"

Parent: continues, receives child UUID
Child:  inherits parent's history, receives prompt as first new message
```

**Without prompt (divergence point):**
```
/fork

Parent: continues, receives child UUID, waits for input
Child:  inherits parent's history, waits for input
```

### Fork Semantics

- **History inheritance**: Child starts with full copy of parent's conversation history (copy-on-write semantics)
- **Sync barrier**: Fork waits for all running tools to complete before executing
- **Self-fork only**: Only the current agent can fork (like Unix - a process calls fork() on itself)
- **Unlimited depth**: Any agent can fork, no artificial depth limits
- **Auto-switch**: UI switches to child after fork (configurable)

### History Storage (Git-like)

```
root --- msg1 --- msg2 --- msg3 --- msg4 --- msg5 (parent continues)
                            |
                            +--- msg4' --- msg5' (child after fork)
```

Options:
1. **Full copy**: Each agent stores complete history (simple, wasteful)
2. **Delta storage**: Child stores fork point + only post-fork messages (efficient)

Delta storage replay:
```
replay(agent):
    if agent.parent:
        replay(parent) up to fork_point
    apply agent's own messages after fork
```

## Signals: Lifecycle Management

Signals provide lifecycle control without switching to each agent.

```
/kill                    # kill self (current agent)
/kill <uuid>             # kill specific agent
/kill <uuid> --cascade   # kill agent + all descendants
```

Future signals (if needed):
- TERM: Graceful shutdown (finish current work, then die)
- KILL: Immediate termination
- STOP/CONT: Pause and resume execution

Signals are system-level with limited vocabulary, distinct from application-level mailbox messages.

## Mailbox: Inter-Agent Communication

Erlang-style message passing between agents.

```
/send <uuid> "message"   # send to agent's mailbox
/check-mail              # check for messages (pull model)
/read-mail               # read messages
/filter-mail --from <uuid>  # filter by sender
```

Properties:
- **Pull model**: Agents must explicitly check for mail
- **UUID knowledge**: Parent knows children, children know parent
- **UUID sharing**: Agents can share UUIDs via messages or shared docs

Future consideration: Push notifications (inject mail arrival into agent context).

## Shared State: StoredAssets

Markdown documents in the database that agents can create, read, and delete.

- Accessed through tooling
- Agents share document paths via mailbox messages
- Separate from mailbox (persistent vs transient)

## Orphan Handling

When parent is cleared or killed, what happens to children?

Since the registry (not parent memory) is source of truth:
- Children continue to exist
- Status could become 'orphaned' or reparented to root
- Design decision: TBD based on practical needs

## Startup Replay

On ikigai startup:
1. Query agent registry for all `status = 'running'` agents
2. For each agent, replay history from fork point
3. Reconstruct parent-child relationships from registry
4. Agents resume where they left off

## Design Principles

1. **Registry is truth**: Agent existence tracked in database, not parent memory
2. **Single primitive**: Fork is the only creation mechanism
3. **Start simple**: Add features (complex signals, push notifications, permissions) as needed
4. **Unix/Erlang hybrid**: Process model from Unix, message passing from Erlang

## Summary Table

| Aspect | Decision |
|--------|----------|
| Identity | UUID (base64url, 22 chars), optional names |
| Creation | `/fork` only (command + tool) |
| History at fork | Inherited (copy-on-write) |
| Fork with prompt | Child gets prompt as first message |
| Fork without prompt | Both wait for input |
| Tool state at fork | Sync barrier - wait for completion |
| Who can fork | Only current agent (self) |
| Depth limit | None (practical limits apply) |
| Parent return | Child's UUID |
| UI after fork | Auto-switch to child |
| Communication | Mailbox (pull model) |
| Lifecycle control | Signals (KILL, --cascade) |
| Shared state | Memory documents |
| Source of truth | Agent registry table |
