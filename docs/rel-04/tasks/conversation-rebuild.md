# Task: Rebuild Conversation from Replay Context

## Target
User story: 02-single-glob-call

## Agent
model: sonnet

### Pre-read Skills
- .agents/skills/default.md
- .agents/skills/tdd.md
- .agents/skills/testability.md
- .agents/skills/coverage.md
- .agents/skills/mocking.md

### Pre-read Docs
- rel-04/README.md (section "Canonical Message Format")
- docs/memory.md (talloc ownership patterns)
- docs/error_handling.md (res_t patterns)

### Pre-read Source (patterns)
- src/msg.h (ik_msg_t structure - canonical message format)
- src/msg.c (ik_msg_create functions)
- src/conversation.h (ik_conversation_t structure)
- src/repl/session_restore.c (current restore logic - populates scrollback but not conversation)
- src/db/replay.h (ik_message_t, ik_replay_context_t)

### Pre-read Tests (patterns)
- tests/unit/msg_test.c (canonical message creation patterns)
- tests/integration/repl/session_restore_test.c (if exists, session restore patterns)

## Pre-conditions
- `make check` passes
- Task `db-tool-replay.md` completed (replay context includes tool messages)
- Canonical message format (ik_msg_t) is implemented with kind discriminator
- ik_conversation_t uses canonical messages (ik_msg_t)
- ik_openai_serialize_request() transforms canonical messages to OpenAI wire format

## Task
Add logic to rebuild `repl->conversation` from `ik_replay_context_t` during session restore.

**Current gap**: `session_restore.c` loads replay context and populates scrollback for display, but does NOT rebuild the conversation. After restart, the LLM has no context.

**Solution**: Load each `ik_message_t` (DB format) into `ik_msg_t` (canonical in-memory format) and add to `repl->conversation`. Since the DB format and canonical format are nearly identical (both use the `kind` discriminator), this is primarily a copy/validation operation, not a transformation.

### Loading Rules

The DB `kind` field maps directly to the canonical `kind` field. Both formats use the same discriminator values:

| DB kind | Canonical kind | Fields copied |
|---------|----------------|---------------|
| `"system"` | `"system"` | content (data_json is NULL) |
| `"user"` | `"user"` | content (data_json is NULL) |
| `"assistant"` | `"assistant"` | content (data_json is NULL) |
| `"tool_call"` | `"tool_call"` | content + data_json |
| `"tool_result"` | `"tool_result"` | content + data_json |

Skip non-conversation kinds: `clear`, `mark`, `rewind`.

**Note**: The `data_json` field is preserved as-is. Transformation to OpenAI wire format (e.g., parsing `data_json` into `tool_calls` arrays) happens later in the serializer (`ik_openai_serialize_request()`), not during conversation rebuild.

### New Function

Create `ik_msg_from_db()` in `src/msg.c`:
- Input: `ik_message_t *` (from replay context)
- Output: `res_t` containing `ik_msg_t *` (or NULL for skipped kinds)
- Copies `kind`, `content`, and `data_json` fields
- Validates that `kind` is a recognized conversation message type
- Returns NULL for non-conversation kinds (clear, mark, rewind)

## TDD Cycle

### Red
1. Create tests in `tests/unit/msg_from_db_test.c`:
   - `test_msg_from_db_user`: kind="user" → canonical kind="user", content preserved
   - `test_msg_from_db_system`: kind="system" → canonical kind="system", content preserved
   - `test_msg_from_db_assistant`: kind="assistant" → canonical kind="assistant", content preserved
   - `test_msg_from_db_tool_call`: kind="tool_call" → canonical kind="tool_call", content + data_json preserved
   - `test_msg_from_db_tool_result`: kind="tool_result" → canonical kind="tool_result", content + data_json preserved
   - `test_msg_from_db_skip_clear`: kind="clear" → returns OK(NULL)
   - `test_msg_from_db_skip_mark`: kind="mark" → returns OK(NULL)
2. Create tests for session restore conversation rebuild:
   - After restore with user/assistant messages, conversation has correct count
   - After restore with tool messages, conversation includes correct kind and data_json
3. Add `ik_msg_from_db()` declaration to `src/msg.h`
4. Add stub in `src/msg.c`: `return OK(NULL);` (always skips all messages)
5. Run `make check` - expect assertion failure (tests expect valid messages for user/assistant/tool kinds)

### Green
1. Replace stub in `src/msg.c` with implementation:
   - Handle system/user/assistant: copy kind and content, set data_json to NULL
   - Handle tool_call: copy kind, content, and data_json
   - Handle tool_result: copy kind, content, and data_json
   - Handle clear/mark/rewind: return OK(NULL)
   - Validate kind field is recognized
2. Update `ik_repl_restore_session()` in `src/repl/session_restore.c`:
   - After populating scrollback, iterate replay_ctx->messages
   - For each message, call `ik_msg_from_db()`
   - If result is non-NULL, add to `repl->conversation`
3. Run `make check` - expect pass

### Refactor
1. Verify memory ownership: messages owned by conversation, freed on conversation clear
2. Ensure canonical format matches what serializer expects (kind values, data_json structure)
3. Consider adding validation for data_json structure on tool messages
4. Run `make check` - verify still green

## Post-conditions
- `make check` passes
- `make lint && make coverage` passes
- `ik_msg_from_db()` loads all conversation message kinds correctly (preserving kind, content, data_json)
- `ik_repl_restore_session()` rebuilds `repl->conversation` from replay context
- Session restore now provides full context to LLM, not just scrollback display
- Canonical messages in conversation can be serialized to OpenAI wire format by `ik_openai_serialize_request()`
- 100% test coverage for new code
