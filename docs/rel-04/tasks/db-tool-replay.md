# Task: Replay Tool Messages With Transformation

## Target
User story: 02-single-glob-call

## Agent
model: sonnet

### Pre-read Skills
- .agents/skills/default.md
- .agents/skills/tdd.md
- .agents/skills/database.md
- .agents/skills/coverage.md
- .agents/skills/testability.md

### Pre-read Docs
- rel-04/README.md (section "Message Transformation (Replay to API)")
- docs/v1-database-design.md
- rel-04/user-stories/02-single-glob-call.md

### Pre-read Source (patterns)
- src/db/replay.c (process_event function, append_message helper)
- src/db/replay.h (ik_message_t structure)
- src/db/message.c (VALID_KINDS array - now includes tool_call, tool_result)

### Pre-read Tests (patterns)
- tests/unit/db/replay_core_test.c (existing replay tests)
- tests/unit/db/replay_coverage_test.c (test patterns with database setup)

## Pre-conditions
- `make check` passes
- Task `db-tool-persist.md` completed
- `tool_call` and `tool_result` are valid message kinds in database
- Session replay works for system/user/assistant messages

## Task
Extend the replay algorithm to handle `tool_call` and `tool_result` message kinds. This requires **transformation** from database storage format to the internal representation needed for API serialization.

**Critical insight**: Unlike system/user/assistant which can be appended as-is, tool messages require transformation because:
- Database stores: `kind: "tool_call"` with `data_json` containing tool details
- API expects: `role: "assistant"` with `tool_calls` array, OR `role: "tool"` with `tool_call_id`

The replay layer doesn't serialize to JSON (that's the client layer), but it must store messages in a format that the serialization layer can use.

### Transformation Rules

**For `tool_call` messages:**
- Database: `{kind: "tool_call", data_json: {id, type, function: {name, arguments}}}`
- Internal: Message with `kind: "tool_call"`, preserving `data_json` for serialization
- The serialization layer will later convert to `{role: "assistant", tool_calls: [...]}`

**For `tool_result` messages:**
- Database: `{kind: "tool_result", data_json: {tool_call_id, name, output, success}}`
- Internal: Message with `kind: "tool_result"`, preserving `data_json` for serialization
- The serialization layer will later convert to `{role: "tool", tool_call_id: "...", content: "..."}`

## TDD Cycle

### Red
1. Add tests in `tests/unit/db/replay_test.c`:
   - `test_replay_tool_call_message`: Replay session with tool_call, verify in context
   - `test_replay_tool_result_message`: Replay session with tool_result, verify in context
   - `test_replay_full_tool_conversation`: User → tool_call → tool_result → assistant sequence
   - `test_replay_tool_message_preserves_data_json`: Verify data_json is preserved for serialization
   - `test_replay_multiple_tool_calls`: Session with multiple tool call/result pairs
2. Run `make check` - expect test failures (tool kinds not handled in process_event)

### Green
1. Update `src/db/replay.c` `process_event()` function:
   - Add condition for `tool_call` and `tool_result` kinds
   - Call `append_message()` to add to context (same as system/user/assistant)
   - The key is preserving the `data_json` field which contains structured tool data
2. Verify the existing `append_message()` already preserves `data_json` (it does - line 120-126)
3. Run `make check` - expect pass

### Refactor
1. Run `make lint && make coverage` - verify 100% coverage
2. Verify tool messages appear in context array in correct chronological order
3. Verify data_json field is preserved through replay (essential for later serialization)
4. Consider edge cases:
   - Empty data_json (malformed tool message)
   - tool_result without preceding tool_call
5. Run `make check` - verify still green

## Post-conditions
- `make check` passes
- `make lint && make coverage` passes
- Replay handles `tool_call` and `tool_result` message kinds
- Tool messages appear in context with preserved data_json
- 100% test coverage for new code
- Replay works correctly immediately after persistence - zero technical debt
