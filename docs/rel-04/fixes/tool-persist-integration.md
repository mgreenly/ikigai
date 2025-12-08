# Fix: Tool Message Persistence Integration

## Problem

Tool messages (tool_call and tool_result) are displayed in scrollback during live sessions but are **never persisted to the database**. When ikigai restarts and replays from the database, tool messages don't appear because there's nothing to replay.

### Current Flow (Broken)

```
tool execution → add to conversation → render to scrollback → [missing: persist to DB]
```

### Expected Flow

```
tool execution → add to conversation → render to scrollback → persist to DB
```

## Root Cause

`src/repl_tool.c` has two tool execution paths:
1. **Sync path**: `ik_repl_execute_pending_tool()` (lines 59-111)
2. **Async path**: `ik_repl_complete_tool_execution()` (lines 184-257)

Both paths:
- Add messages to conversation via `ik_openai_msg_create_tool_call()` and `ik_openai_msg_create_tool_result()`
- Render to scrollback via `ik_event_render()`
- **Do NOT persist to database** - missing `ik_db_message_insert()` calls

## Agent
model: sonnet

## Skills to Load

Read these `.agents/skills/` files:
- `default.md` - Project context and structure
- `tdd.md` - Test-driven development approach
- `style.md` - Code style conventions
- `database.md` - Database patterns
- `coverage.md` - Coverage requirements

## Files to Explore

### Source files:
- `src/repl_tool.c` - Tool execution (lines 101-105 sync, 231-235 async)
- `src/repl_actions.c:159-170` - User message persistence pattern to follow
- `src/repl_event_handlers.c:138` - Assistant message persistence pattern
- `src/openai/client_msg.c` - Message creation with data_json
- `src/db/message.c` - VALID_KINDS already includes tool_call, tool_result

### Test files:
- `tests/unit/repl/repl_tool_test.c` - Existing tool execution tests
- `tests/integration/db/message_insert_test.c` - DB insert patterns

## Situation

### Infrastructure Already Ready

| Component | Status |
|-----------|--------|
| `VALID_KINDS` in message.c | Includes "tool_call", "tool_result" |
| `replay.c` process_event | Handles tool_call, tool_result |
| `event_render.c` | Renders tool kinds |
| Message creation | `data_json` already populated |

### Only Missing

Database INSERT calls in `repl_tool.c`.

## High-Level Goal

**Add `ik_db_message_insert()` calls for tool_call and tool_result messages in both execution paths.**

### Required Changes

In `src/repl_tool.c`, after the scrollback render calls:

**Sync path (`ik_repl_execute_pending_tool`)** - after line 105:
```c
// 5. Persist to database
if (repl->db_ctx != NULL && repl->current_session_id > 0) {
    ik_db_message_insert(repl->db_ctx, repl->current_session_id,
                         "tool_call", formatted_call, tc_msg->data_json);
    ik_db_message_insert(repl->db_ctx, repl->current_session_id,
                         "tool_result", formatted_result, result_msg->data_json);
}
```

**Async path (`ik_repl_complete_tool_execution`)** - after line 235:
```c
// 4. Persist to database
if (repl->db_ctx != NULL && repl->current_session_id > 0) {
    ik_db_message_insert(repl->db_ctx, repl->current_session_id,
                         "tool_call", formatted_call, tc_msg->data_json);
    ik_db_message_insert(repl->db_ctx, repl->current_session_id,
                         "tool_result", formatted_result, result_msg->data_json);
}
```

### data_json Format

The message creation functions already build proper JSON:

**tool_call** (from `ik_openai_msg_create_tool_call`):
```json
{"id":"call_xxx","type":"function","function":{"name":"glob","arguments":"{...}"}}
```

**tool_result** (from `ik_openai_msg_create_tool_result`):
```json
{"tool_call_id":"call_xxx"}
```

## Testing Strategy

1. **Unit test**: Mock `ik_db_message_insert_()` and verify calls with correct parameters
2. **Integration test**: Insert tool messages, reload session, verify they appear
3. **E2E verification**: Existing `replay_tool_e2e_test.c` should now pass with real data

## Success Criteria

- `make check` passes
- `make lint && make coverage` passes with 100% coverage
- Tool messages persist to database during tool execution
- Session replay shows tool messages after restart
- data_json contains structured tool information for API serialization
