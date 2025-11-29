# Task: Rebuild Conversation from Replay Context

## Target
User story: 02-single-glob-call

## Agent
model: sonnet

### Pre-read Skills
- .agents/skills/tdd.md
- .agents/skills/testability.md
- .agents/skills/coverage.md
- .agents/skills/mocking.md

### Pre-read Docs
- rel-04/README.md (section "Message Transformation (Replay to API)")
- docs/memory.md (talloc ownership patterns)
- docs/error_handling.md (res_t patterns)

### Pre-read Source (patterns)
- src/openai/client.h (ik_openai_msg_t structure, ik_openai_conversation_t)
- src/openai/client.c (ik_openai_msg_create, ik_openai_serialize_request)
- src/repl/session_restore.c (current restore logic - populates scrollback but not conversation)
- src/db/replay.h (ik_message_t, ik_replay_context_t)

### Pre-read Tests (patterns)
- tests/unit/openai/client_structures_test.c (message creation patterns)
- tests/integration/repl/session_restore_test.c (if exists, session restore patterns)

## Pre-conditions
- `make check` passes
- Task `db-tool-replay.md` completed (replay context includes tool messages)
- Task `assistant-tool-calls-msg.md` completed (ik_openai_msg_t has tool_calls, tool_call_count)
- Task `tool-result-msg.md` completed (ik_openai_msg_t has tool_call_id)
- ik_openai_serialize_request() handles all message variants

## Task
Add logic to rebuild `repl->conversation` from `ik_replay_context_t` during session restore.

**Current gap**: `session_restore.c` loads replay context and populates scrollback for display, but does NOT rebuild the OpenAI conversation. After restart, the LLM has no context.

**Solution**: Transform each `ik_message_t` (DB format) into `ik_openai_msg_t` (API format) and add to `repl->conversation`.

### Transformation Rules

| DB kind | OpenAI role | Fields |
|---------|-------------|--------|
| `"system"` | `"system"` | content |
| `"user"` | `"user"` | content |
| `"assistant"` | `"assistant"` | content |
| `"tool_call"` | `"assistant"` | tool_calls array (parse from data_json) |
| `"tool_result"` | `"tool"` | tool_call_id + content (parse from data_json) |

Skip non-conversation kinds: `clear`, `mark`, `rewind`.

### New Function

Create `ik_openai_msg_from_db()` in `src/openai/client.c`:
- Input: `ik_message_t *` (from replay context)
- Output: `res_t` containing `ik_openai_msg_t *` (or NULL for skipped kinds)
- Parses `data_json` for tool_call and tool_result kinds
- Uses existing factory functions for message creation

## TDD Cycle

### Red
1. Create tests in `tests/unit/openai/test_msg_from_db.c`:
   - `test_msg_from_db_user`: kind="user" → role="user", content preserved
   - `test_msg_from_db_system`: kind="system" → role="system", content preserved
   - `test_msg_from_db_assistant`: kind="assistant" → role="assistant", content preserved
   - `test_msg_from_db_tool_call`: kind="tool_call" → role="assistant" with tool_calls array
   - `test_msg_from_db_tool_result`: kind="tool_result" → role="tool" with tool_call_id
   - `test_msg_from_db_skip_clear`: kind="clear" → returns OK(NULL)
   - `test_msg_from_db_skip_mark`: kind="mark" → returns OK(NULL)
2. Create tests for session restore conversation rebuild:
   - After restore with user/assistant messages, conversation has correct count
   - After restore with tool messages, conversation includes tool_calls and tool results
3. Add `ik_openai_msg_from_db()` declaration to `src/openai/client.h`
4. Add stub in `src/openai/client.c`: `return OK(NULL);` (always skips all messages)
5. Run `make check` - expect assertion failure (tests expect valid messages for user/assistant/tool kinds)

### Green
1. Replace stub in `src/openai/client.c` with implementation:
   - Handle system/user/assistant: call `ik_openai_msg_create()`
   - Handle tool_call: parse data_json, call `ik_openai_msg_create_with_tool_calls()`
   - Handle tool_result: parse data_json, call `ik_openai_tool_msg_create()`
   - Handle clear/mark/rewind: return OK(NULL)
3. Update `ik_repl_restore_session()` in `src/repl/session_restore.c`:
   - After populating scrollback, iterate replay_ctx->messages
   - For each message, call `ik_openai_msg_from_db()`
   - If result is non-NULL, add to `repl->conversation`
4. Run `make check` - expect pass

### Refactor
1. Verify memory ownership: messages owned by conversation, freed on conversation clear
2. Consider error handling for malformed data_json
3. Ensure transformation matches what serialization expects
4. Run `make check` - verify still green

## Post-conditions
- `make check` passes
- `make lint && make coverage` passes
- `ik_openai_msg_from_db()` transforms all message kinds correctly
- `ik_repl_restore_session()` rebuilds `repl->conversation` from replay context
- Session restore now provides full context to LLM, not just scrollback display
- 100% test coverage for new code
