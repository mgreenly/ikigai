# rel-05: Agent Process Model

Unix/Erlang-inspired process model for ikigai agents. See [agent-process-model.md](../agent-process-model.md) for the design document.

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
                    │ current
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

**shared_ctx** (one per terminal):
- Terminal I/O (stdin/stdout)
- Render loop
- Input event processing
- Database connection pool
- Pointer to current agent

**agent_ctx** (one per agent):
- Scrollback buffer
- LLM connection/streaming state
- Conversation history
- Saved input buffer (preserved on switch)
- Saved scroll position
- UUID, parent relationship

## Implementation Phases

### Phase 0: Architecture Refactor

Restructure code for multi-agent support. No new features, no user-visible changes.

**Goal**: Single agent still works, but code is organized so adding agents is straightforward.

- Define `shared_ctx` struct (terminal, render, input)
- Define `agent_ctx` struct (scrollback, llm, history, state)
- Extract current monolithic code into these two structures
- `shared_ctx.current` points to single agent
- All existing functionality preserved

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

- fork_message_id tracks branch point
- Child references parent's history up to fork point
- Delta storage (child stores only post-fork messages)
- Replay algorithm for history reconstruction

### Phase 5: Startup Replay

Reconstruct state from database on restart.

- Query registry for `status = 'running'` agents
- Replay history from fork points
- Reconstruct parent-child relationships
- Resume where left off

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

## Reference

Previous rel-05 work preserved in `docs/rel-05.bak/` for reference:
- User stories with interaction transcripts
- Task files with TDD structure
- Useful for edge cases and UI patterns

## Key Design Decisions

1. **Fork only**: No `/spawn` - fork is the single creation primitive (like Unix)
2. **Registry is truth**: Database table, not parent memory, tracks agent existence
3. **Self-fork**: Only current agent can fork (process calls fork() on itself)
4. **History inheritance**: Child starts with copy of parent's conversation
5. **No depth limits**: Practical limits only, no artificial restrictions
6. **Auto-switch**: UI switches to child after fork (configurable later)
