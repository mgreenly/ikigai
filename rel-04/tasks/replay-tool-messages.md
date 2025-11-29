# Task: Replay Tool Messages in Session Restore

## Target
User story: 12-session-replay-with-tools

## Agent
model: sonnet

### Pre-read Skills
- .agents/skills/tdd.md
- .agents/skills/database.md
- .agents/skills/coverage.md
- .agents/skills/testability.md

### Pre-read Docs
- docs/v1-database-design.md
- docs/architecture.md
- rel-04/user-stories/12-session-replay-with-tools.md

### Pre-read Source (patterns)
- src/db/replay.c (process_event function shows message kind handling pattern)
- src/db/message.c (VALID_KINDS array shows kind validation pattern)
- src/db/replay.h (message structure and replay context types)

### Pre-read Tests (patterns)
- tests/unit/db/replay_coverage_test.c (test pattern for replay with database setup/teardown)
- tests/unit/db/message_kind_validation_test.c (kind validation test pattern)
- tests/unit/db/replay_test.c (existing replay tests)

## Pre-conditions
- `make check` passes
- Task `db-tool-persist.md` completed
- `tool_call` and `tool_result` are valid message kinds in database
- Session replay works for system/user/assistant messages

## Task
Extend the replay algorithm to handle `tool_call` and `tool_result` message kinds during session restore. These should be appended to the context array just like `system`, `user`, and `assistant` messages.

Currently the `process_event` function in replay.c only handles:
- clear, system, user, assistant (append to context)
- mark, rewind (special checkpoint handling)

Need to add `tool_call` and `tool_result` to the list of message kinds that get appended to context.

## TDD Cycle

### Red
1. Extend tests in `tests/unit/db/replay_test.c`:
   - Add test: replay session with tool_call message
   - Add test: replay session with tool_result message
   - Add test: replay full tool conversation (user → assistant tool_call → tool_result → assistant response)
   - Verify messages are in correct order in context
2. Run `make check` - expect test failure (kinds not handled in replay)

### Green
1. Update `src/db/replay.c`:
   - Modify `process_event` function to handle `tool_call` and `tool_result`
   - These should append to context like system/user/assistant (no special handling needed)
   - Add these kinds to the existing if condition on line 152-156
2. Run `make check` - expect pass

### Refactor
1. Consider if tool messages need any special display formatting
2. Verify data_json field is preserved through replay
3. Ensure session restore displays "[Session restored: N messages]" with correct count
4. Run `make check` - verify still green

## Post-conditions
- `make check` passes
- `make lint && make coverage` passes
- Replay handles `tool_call` and `tool_result` message kinds
- Tool messages appear in context array in correct chronological order
- 100% test coverage for new code
- Story 12 is complete: session replay works with tool conversations
