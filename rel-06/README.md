# rel-06: Agent Process Model (Phases 1-7)

Continuation of Unix/Erlang-inspired process model for ikigai agents. See [agent-process-model.md](../agent-process-model.md) for the design document.

## Prerequisites

rel-05 Phase 0 (Architecture Refactor) must be complete:
- `shared_ctx` struct (terminal, render, input, db_pool)
- `agent_ctx` struct (scrollback, llm, history, state)
- `repl_ctx.current` points to single agent

## Core Concepts

| Concept | Description |
|---------|-------------|
| **Registry** | Database table is source of truth for agent existence |
| **Identity** | UUID (base64url, 22 chars), optional name, parent-child relationships |
| **Fork** | The only creation primitive (`/fork` command + tool) |
| **History** | Delta storage with fork points (git-like) |
| **Signals** | Lifecycle control (`/kill`, `--cascade`) |
| **Mailbox** | Pull-model message passing between agents |

## Architecture

```
┌─────────────────────────────────────────┐
│           shared_ctx (singleton)        │
│  ┌─────────┐ ┌──────────┐ ┌──────────┐  │
│  │ terminal│ │  render  │ │ db_pool  │  │
│  │  input  │ │          │ │          │  │
│  └─────────┘ └──────────┘ └──────────┘  │
└─────────────────────────────────────────┘
                    │
                    │ repl_ctx.current
                    ▼
┌──────────────┐ ┌──────────────┐ ┌──────────────┐
│  agent_ctx   │ │  agent_ctx   │ │  agent_ctx   │
│  (agent 0)   │ │  (child)     │ │  (child)     │
│ ┌──────────┐ │ │ ┌──────────┐ │ │ ┌──────────┐ │
│ │scrollback│ │ │ │scrollback│ │ │ │scrollback│ │
│ │ llm_conn │ │ │ │ llm_conn │ │ │ │ llm_conn │ │
│ │ history  │ │ │ │ history  │ │ │ │ history  │ │
│ │input_buf │ │ │ │input_buf │ │ │ │input_buf │ │
│ │scroll_pos│ │ │ │scroll_pos│ │ │ │scroll_pos│ │
│ │  uuid    │ │ │ │  uuid    │ │ │ │  uuid    │ │
│ │parent_id │ │ │ │parent_id │ │ │ │parent_id │ │
│ └──────────┘ │ │ └──────────┘ │ │ └──────────┘ │
└──────────────┘ └──────────────┘ └──────────────┘
```

## Implementation Phases

### Phase 1: Registry + Identity

Database foundation for agent tracking.

- Agent registry schema (uuid, name, parent_uuid, fork_message_id, status, timestamps)
- Basic CRUD operations
- Agent 0 created and registered on startup

### Phase 2: Multiple Agents + Switching

Support multiple agents in memory with switching.

- Agent array in shared_ctx
- Switch operation (save/restore input buffer, scroll position)
- Navigation commands or hotkeys
- Separator shows current agent

### Phase 3: Fork

The creation primitive.

- `/fork` command (no prompt version)
- Creates child in registry with parent relationship
- `/fork "prompt"` variant (child receives prompt as first message)
- Sync barrier (wait for running tools before fork)
- Auto-switch to child

### Phase 4: History Inheritance

Git-like delta storage for forked agents.

- fork_message_id tracks branch point in PARENT's history
- Child stores parent_uuid reference
- Delta storage (child stores only post-fork messages)
- Replay requires walking FULL ancestor chain to a clear event
- "Walk backwards, play forwards" reconstruction algorithm

### Phase 5: Startup Replay

Reconstruct state from database on restart.

- Query registry for `status = 'running'` agents
- For each agent, walk ancestor chain backwards to find clear event
- Collect ancestor chain: [child, parent, grandparent, ..., root_or_clear_origin]
- Reverse to get chronological order
- Replay forward: for each ancestor, load messages from their origin to their fork point
- Uses existing append-based replay (no prepend needed)
- Resume where left off

#### Replay Algorithm Detail

**Message Range Structure:**

Each range entry contains enough information to query the exact subset of messages:

```c
typedef struct {
    char *agent_uuid;   // Which agent's messages to query
    int64_t start_id;   // Start AFTER this message ID (0 = from beginning)
    int64_t end_id;     // End AT this message ID (0 = no limit, i.e., leaf)
} ik_replay_range_t;
```

- `start_id` is **exclusive** (query messages AFTER this ID)
- `end_id` is **inclusive** (query messages up to and including this ID)
- `end_id = 0` means "no upper limit" (used for leaf agent)

**Query per range:**

```sql
SELECT * FROM messages
WHERE agent_uuid = $1
  AND id > $2                    -- after start_id
  AND ($3 = 0 OR id <= $3)       -- up to end_id (0 = no limit)
ORDER BY created_at
```

**Walk Algorithm (child to root):**

```
build_replay_ranges(leaf_agent):
    ranges = []
    current = leaf_agent
    end_id = 0  // leaf has no upper bound

    while current != NULL:
        // Find most recent clear for this agent (within range)
        clear_id = find_most_recent_clear(current.uuid, max_id=end_id)

        if clear_id found:
            // Clear found - start after clear, this terminates the walk
            ranges.append({current.uuid, clear_id, end_id})
            break
        else:
            // No clear - include all messages from beginning
            ranges.append({current.uuid, 0, end_id})

            if current.parent_uuid == NULL:
                break  // Root agent reached, done

            // Move to parent - parent's range ends at this agent's fork point
            end_id = current.fork_message_id
            current = lookup(current.parent_uuid)

    return reverse(ranges)  // Chronological order (root first)
```

**Exit conditions:**
1. **Clear found** - Context boundary reached, stop walking
2. **Root reached** - `parent_uuid == NULL`, no more ancestors

**Replay execution:**

```
replay(ranges):
    context = empty_context()
    for range in ranges:  // Already in chronological order
        messages = query_messages(range.agent_uuid, range.start_id, range.end_id)
        for msg in messages:
            process_event(context, msg)
    return context
```

**Example: Fork without clear**

```
root ─── m1 ─── m2 ─── m3 ─── m4 ─── m5
                        │
                        └─── m6 ─── m7 (child, forked at m3)
```

Agents table:
- root: `uuid=root, parent_uuid=NULL, fork_message_id=NULL`
- child: `uuid=child, parent_uuid=root, fork_message_id=3`

Walk from child:
1. `current=child, end_id=0`: no clear → append `{child, 0, 0}` → `end_id=3` → `current=root`
2. `current=root, end_id=3`: no clear → append `{root, 0, 3}` → `parent_uuid=NULL` → break

Ranges (before reverse): `[{child, 0, 0}, {root, 0, 3}]`
Ranges (after reverse): `[{root, 0, 3}, {child, 0, 0}]`

Queries:
1. root: `id > 0 AND id <= 3` → m1, m2, m3
2. child: `id > 0` (no limit) → m6, m7

Result: m1, m2, m3, m6, m7 ✓

**Example: Fork with clear in parent**

```
root ─── m1 ─── clear(m2) ─── m3 ─── m4
                                      │
                                      └─── m5 ─── m6 (child)
```

Agents table:
- root: `uuid=root, parent_uuid=NULL, fork_message_id=NULL`
- child: `uuid=child, parent_uuid=root, fork_message_id=4`

Walk from child:
1. `current=child, end_id=0`: no clear → append `{child, 0, 0}` → `end_id=4` → `current=root`
2. `current=root, end_id=4`: clear at m2 → append `{root, 2, 4}` → break

Ranges (after reverse): `[{root, 2, 4}, {child, 0, 0}]`

Queries:
1. root: `id > 2 AND id <= 4` → m3, m4 (skips m1 and clear)
2. child: `id > 0` → m5, m6

Result: m3, m4, m5, m6 ✓ (clear excluded m1)

### Phase 6: Lifecycle (Signals)

Agent termination.

- `/kill` (self - current agent)
- `/kill <uuid>` (specific agent)
- `/kill <uuid> --cascade` (agent + all descendants)
- Status updates in registry
- Orphan handling policy

### Phase 7: Mailbox

Inter-agent communication.

- Mail schema (sender, recipient, body, timestamp, read status)
- `/send <uuid> "message"`
- `/check-mail` (check for messages)
- `/read-mail` (read messages)
- Pull model (agents explicitly check)

### Future: Memory Documents

Shared state between agents (markdown documents in database). Defer unless needed.

## Key Design Decisions

1. **Fork only**: No `/spawn` - fork is the single creation primitive (like Unix)
2. **Registry is truth**: Database table, not parent memory, tracks agent existence
3. **Self-fork**: Only current agent can fork (process calls fork() on itself)
4. **History inheritance**: Child starts with copy of parent's conversation
5. **"Walk backwards, play forwards"**: Reconstruction algorithm walks ancestor chain backwards to find context origin (clear event), then replays forward in chronological order
6. **Clear events are context boundaries**: Ancestry traversal stops at clear events, which mark context origin
7. **No depth limits**: Practical limits only, no artificial restrictions
8. **Auto-switch**: UI switches to child after fork (configurable later)
