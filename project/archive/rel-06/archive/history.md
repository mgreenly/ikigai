# Session History Replay - Investigation and Decisions

Date: 2025-12-17

## Problem Statement

Session history is not restored on app restart or fork. When the user types "what is 1 + 1", quits, and restarts, the conversation history is lost. Similarly, when forking an agent, the child agent's scrollback is empty despite expecting to inherit the parent's history.

## Root Cause Analysis

### The `agent_uuid` Mismatch

Migration `003-messages-agent-uuid.sql` added the `agent_uuid` column to the messages table with the intent that "new inserts should include agent_uuid". However, the code continues to pass `NULL` for `agent_uuid` in all core message inserts.

**Affected call sites (all passing `NULL` for `agent_uuid`):**

| File | Line | Message Kind |
|------|------|--------------|
| `repl_actions_llm.c` | 103-104 | user |
| `repl_event_handlers.c` | 156-157 | assistant |
| `repl_tool.c` | 115-118 | tool_call, tool_result |
| `repl_tool.c` | 255-258 | tool_call, tool_result (async) |
| `commands.c` | 203-204 | clear |
| `commands.c` | 219-221 | system |
| `commands_mark.c` | 96-97 | mark |
| `commands_mark.c` | 156-157 | rewind |
| `commands_agent.c` | 95-96 | user (fork prompt) |

**Note:** `agent_restore.c` fresh install code (lines 144-147, 165-168) correctly passes `repl->current->uuid` - no fix needed there.

**Note:** `commands_agent.c` has some call sites that already correctly pass `agent_uuid`:
- Line 209: `cmd_kill_cascade()` passes `repl->current->uuid` for `agent_killed`
- Line 290: `cmd_kill()` self-kill passes `parent->uuid` for `agent_killed`
- Line 394: `cmd_kill()` target-kill passes `repl->current->uuid` for `agent_killed`

Only line 95-96 (fork prompt user message) needs fixing.

### SQL Query Mismatch

The agent-based replay query in `agent_replay.c:195-198`:
```sql
WHERE agent_uuid = $1 AND id > $2 AND ($3 = 0 OR id <= $3)
ORDER BY created_at
```

In SQL, `NULL = 'any-value'` is always false. Messages inserted with `agent_uuid = NULL` are never returned by this query.

### Two Replay Mechanisms

There are currently two replay mechanisms:

1. **Old: `ik_db_messages_load()`** in `src/db/replay.c`
   - Queries by `session_id` only
   - Flat model, no agent hierarchy support
   - Query: `WHERE session_id = $1 ORDER BY created_at`
   - **Has NO production callers** - only used by tests

2. **New: `ik_agent_replay_history()`** in `src/db/agent_replay.c`
   - Queries by `agent_uuid` with ancestry traversal
   - Supports fork/hierarchy via `ik_agent_build_replay_ranges()`
   - Walks backwards from leaf agent through parent chain
   - **Used by production code** (`ik_repl_restore_agents()`)

The restoration code (`ik_repl_restore_agents()`) uses the new agent-based replay, but messages are inserted with `NULL` for `agent_uuid`, so nothing is found.

**Note:** The restoration code in `agent_restore.c` already correctly separates conversation vs scrollback population - the problem is purely that messages lack `agent_uuid` when inserted.

### Missing Persistence: Slash Command Output

Slash commands (`/agents`, `/help`, `/model`, etc.) render output directly to scrollback but do NOT persist to the database. This output is lost on restart even if the `agent_uuid` fix is applied.

### Missing Renderer: `agent_killed` Events

The `agent_killed` kind is in `VALID_KINDS` (db/message.c) and correctly persisted by `/kill` commands, but `event_render.c` has no handler for it. During replay, `agent_killed` events will return `ERR("Unknown event kind")`. This must be fixed alongside adding handlers for new kinds.

## Decisions

### D1: Fix `agent_uuid` in All Message Inserts

All `ik_db_message_insert()` calls must pass `repl->current->uuid` instead of `NULL`.

### D2: Remove Old Replay Mechanism

After the fix, remove `ik_db_messages_load()` from `src/db/replay.c`. The agent-based replay is the only mechanism needed for the multi-agent model.

**Note:** The old mechanism has no production callers - removal only requires migrating tests to use the agent-based replay or keeping the function for test convenience.

### D3: Persist Slash Command Output

Add a new message kind for slash command output (e.g., `command` or `output`). This ensures command output survives restart.

### D4: Replay Populates Both Scrollback and Conversation

On restart or fork, replay must populate:

1. **Scrollback Buffer** - ALL event types:
   - `user`, `assistant`, `system` - conversation messages
   - `tool_call`, `tool_result` - tool interactions
   - `clear`, `mark`, `rewind` - session control
   - `command` (new) - slash command output
   - `fork` (new) - fork events

2. **Message History (for LLM)** - Only conversation-relevant types:
   - `system`, `user`, `assistant`, `tool_call`, `tool_result`
   - Filtered via `ik_msg_is_conversation_kind()`

**Note:** This is already implemented correctly in `agent_restore.c` (lines 82-109 for conversation, lines 98-109 for scrollback). The issue is that the replay finds no messages because they were inserted with NULL agent_uuid.

### D5: Fork Event Representation

Fork events are recorded in BOTH parent and child histories with different representations:

**Parent's history:**
```
kind: "fork"
content: "Forked child RQtxuv..."
data: {"child_uuid": "RQtxuv...", "fork_message_id": 123, "role": "parent"}
```

**Child's history:**
```
kind: "fork"
content: "Forked from wlT4G-..."
data: {"parent_uuid": "wlT4G-...", "fork_message_id": 123, "role": "child"}
```

Properties:
- Same `kind: "fork"` for both
- Different `content` for display in scrollback
- `data.role` distinguishes parent vs child
- Each knows the relationship via UUID reference
- Fork events are NOT sent to LLM (meta-event, not conversation)

## Implementation Tasks

**Minimal viable fix:** Phase 1, step 1 alone will restore session history on restart. The other phases add polish (fork events, slash command persistence).

### Phase 1: Core Fix

1. Update all `ik_db_message_insert()` calls to pass `repl->current->uuid`
2. Update `ik_db_message_is_valid_kind()` to accept new kinds (`command`, `fork`)
   - Current valid kinds: `clear, system, user, assistant, tool_call, tool_result, mark, rewind, agent_killed`
   - Add: `command`, `fork`
3. Add `agent_killed` handler to `ik_event_render()` (currently missing - causes ERR on replay)
4. Add fork event persistence in `cmd_fork()`:
   - Insert parent-side fork event before creating child
   - Insert child-side fork event after child creation

### Phase 2: Slash Command Persistence

1. Add `command` message kind (already added to VALID_KINDS in Phase 1)
2. Persist slash command output in `ik_cmd_dispatch()` or individual command handlers
3. Update `ik_event_render()` to handle `command` and `fork` kinds
   - Currently only handles: user, assistant, system, tool_call, tool_result, mark, rewind, clear
   - Missing handlers: `agent_killed` (added in Phase 1), `command`, `fork`
   - Unknown kinds return `ERR()` - must add handlers for new kinds

### Phase 3: Cleanup

1. Migrate tests from `ik_db_messages_load()` to agent-based replay
   - 15+ test files currently use the old mechanism
   - Option A: Migrate all tests to use `ik_agent_replay_history()`
   - Option B: Keep `ik_db_messages_load()` for test convenience (simpler, no production impact)
2. If Option A: Remove `ik_db_messages_load()` from `src/db/replay.c`
3. Remove any other references to the old replay mechanism

### Phase 4: Fork Improvements

1. Ensure `cmd_fork()` properly uses `ik_agent_replay_history()` to populate child's scrollback
2. Or: copy parent's scrollback directly at fork time (simpler, matches visual expectation)

## Files to Modify

- `src/repl_actions_llm.c` - user message insert
- `src/repl_event_handlers.c` - assistant message insert
- `src/repl_tool.c` - tool_call/tool_result inserts (2 locations)
- `src/commands.c` - clear/system inserts, slash command dispatch
- `src/commands_mark.c` - mark/rewind inserts
- `src/commands_agent.c` - fork prompt insert, add fork event persistence
- `src/db/message.c` - add new valid kinds (`command`, `fork`)
- `src/db/replay.c` - remove old mechanism (test migration only, see Phase 3 options)
- `src/event_render.c` - add handlers for `agent_killed`, `command`, `fork`

**No changes needed for agent_uuid:**
- `src/repl/agent_restore.c` - already passes `repl->current->uuid` correctly
- `src/commands_agent.c` lines 209, 290, 394 - `agent_killed` inserts already pass correct uuid

## Open Questions

1. Should the child's scrollback be populated via DB replay or by copying parent's scrollback buffer directly?
   - DB replay: Consistent with restart behavior
   - Copy: Simpler, includes ephemeral content not in DB

2. What prefix/styling should `command` kind use in scrollback rendering?

3. Should `fork` events include timestamps or other metadata?
