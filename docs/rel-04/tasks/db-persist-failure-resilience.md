# Task: Database Persist Failure Resilience

## Target
User story: 02-single-glob-call

## Agent
model: sonnet

### Pre-read Skills
- .agents/skills/default.md
- .agents/skills/tdd.md
- .agents/skills/mocking.md
- .agents/skills/coverage.md
- .agents/skills/quality.md

### Pre-read Docs
- rel-04/README.md (Database Persistence section - "Independence" note)
- docs/error_handling.md
- docs/memory.md

### Pre-read Source (patterns)
- src/repl_actions.c (existing pattern: DB failure logged, memory authoritative)
- src/db/message.c (ik_db_message_insert)
- src/wrapper.h (MOCKABLE pattern for testing)

### Pre-read Tests (patterns)
- tests/unit/repl/handle_request_success_test.c (REPL testing patterns)
- tests/unit/db/message_test.c (message persistence tests)

## Pre-conditions
- `make check` passes
- Task `db-tool-persist.md` completed
- Tool message persistence works for tool_call and tool_result kinds

## Task
Verify that tool_call and tool_result are truly independent events. Database persist failures must not break the session - memory is authoritative.

Key behaviors to test:
1. tool_call persist fails → session continues, tool executes, tool_result persisted
2. tool_result persist fails → session continues, memory has complete state
3. Both persist fail → session continues, memory has complete state
4. Failures are logged (not silent) but non-fatal

## TDD Cycle

### Red
1. Create tests in `tests/unit/db/tool_persist_resilience_test.c`:
   - Mock `ik_db_message_insert` to fail on "tool_call" kind
   - Verify tool execution still happens
   - Verify tool_result persist is still attempted
   - Verify conversation memory has both messages

2. Add test for tool_result persist failure:
   - Mock `ik_db_message_insert` to fail on "tool_result" kind
   - Verify tool_call was persisted
   - Verify conversation memory has both messages
   - Verify session continues (no crash/abort)

3. Add test for both persists failing:
   - Mock `ik_db_message_insert` to always fail
   - Verify conversation memory has both messages
   - Verify tool loop continues to next API request

4. Run `make check` - expect test failures

### Green
1. Ensure tool loop does NOT abort on persist failure
2. Ensure persist failures are logged (check existing pattern in repl_actions.c)
3. Ensure each persist is independent (failure of one doesn't skip the other)
4. Run `make check` - expect pass

### Refactor
1. Verify error messages are clear and actionable
2. Consider if warning should be displayed in scrollback (user visibility)
3. Run `make check` - verify still green

## Post-conditions
- `make check` passes
- `make lint && make coverage` passes
- tool_call persist failure does not block tool execution
- tool_result persist failure does not block session
- Memory state is complete regardless of DB failures
- Persist failures are logged
- 100% test coverage for failure paths
