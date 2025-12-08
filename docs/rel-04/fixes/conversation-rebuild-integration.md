# Fix: Conversation Rebuild Integration

## Problem

When ikigai restarts, the scrollback displays the replayed conversation correctly, but the **LLM conversation context is empty**. The `ik_msg_from_db()` function exists and works, but is never called during session restore.

### Current Flow (Broken)

```
restart → load from DB → render to scrollback → [missing: rebuild conversation]
                                                  ↓
                                              LLM has no history!
```

### Expected Flow

```
restart → load from DB → render to scrollback → rebuild conversation
                                                  ↓
                                              LLM has full context
```

## Root Cause

`src/repl/session_restore.c` lines 100-117 contain commented-out code for conversation rebuild:

```c
// TODO: Rebuild conversation from replay context for LLM context
// Example code (when ik_conversation_t is integrated):
//   for (size_t i = 0; i < replay_ctx->count; i++) {
//       res_t msg_res = ik_msg_from_db(repl, replay_ctx->messages[i]);
//       ...
```

The function `ik_msg_from_db()` exists in `src/msg.c` and is fully implemented, but **is never called**.

## Agent
model: sonnet

## Skills to Load

Read these `.agents/skills/` files:
- `default.md` - Project context and structure
- `tdd.md` - Test-driven development approach
- `style.md` - Code style conventions
- `coverage.md` - Coverage requirements

## Files to Explore

### Source files:
- `src/repl/session_restore.c` - Lines 100-117 (commented TODO)
- `src/msg.c` - `ik_msg_from_db()` implementation
- `src/msg.h` - Function declaration
- `src/openai/conversation.c` - `ik_openai_conversation_add_msg()` for adding messages
- `src/repl.h` - `repl->conversation` field

### Test files:
- `tests/unit/msg_from_db_test.c` - Unit tests for conversion
- `tests/integration/repl/session_restore_test.c` - Session restore tests

## Situation

### Infrastructure Already Ready

| Component | Status |
|-----------|--------|
| `ik_msg_from_db()` | Fully implemented in msg.c |
| `ik_openai_conversation_add_msg()` | Works |
| Replay context iteration | Loop already exists (lines 84-98) |

### Only Missing

The actual call to `ik_msg_from_db()` and adding to conversation.

## High-Level Goal

**Uncomment and implement the conversation rebuild loop in `session_restore.c`.**

### Required Changes

In `src/repl/session_restore.c`, replace the TODO comment (lines 100-117) with actual code:

```c
// Rebuild conversation from replay context for LLM context
for (size_t i = 0; i < replay_ctx->count; i++) {
    ik_message_t *db_msg = replay_ctx->messages[i];

    // Convert DB format to canonical format
    res_t msg_res = ik_msg_from_db(repl->conversation, db_msg);
    if (is_err(&msg_res)) {
        talloc_free(tmp);
        return msg_res;
    }

    // Add to conversation if not skipped (NULL means skip)
    ik_msg_t *msg = msg_res.ok;
    if (msg != NULL) {
        res_t add_res = ik_openai_conversation_add_msg(repl->conversation, msg);
        if (is_err(&add_res)) {
            talloc_free(tmp);
            return add_res;
        }
    }
}
```

### Message Type Mapping

`ik_msg_from_db()` already handles all message types:

| DB kind | Action |
|---------|--------|
| system, user, assistant | Copy to canonical msg |
| tool_call, tool_result | Copy with data_json |
| clear, mark, rewind | Return NULL (skip) |

## Testing Strategy

1. **Unit test**: Verify `ik_msg_from_db()` handles all message kinds (already exists)
2. **Integration test**: Restore session with messages, verify conversation count matches
3. **E2E verification**: After restart, LLM response shows awareness of history

## Dependencies

This fix should be done **after** `tool-persist-integration.md` so that tool messages are actually persisted to the database.

## Success Criteria

- `make check` passes
- `make lint && make coverage` passes with 100% coverage
- Session restore rebuilds conversation from replay context
- After restart, LLM receives full conversation history including tool messages
- `ik_openai_serialize_request()` produces correct API payload with history
