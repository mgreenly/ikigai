# Task: Persist Tool Messages to Database

## Target
User story: 02-single-glob-call

## Agent
model: sonnet

### Pre-read Skills
- .agents/skills/default.md
- .agents/skills/tdd.md
- .agents/skills/database.md
- .agents/skills/coverage.md
- .agents/skills/quality.md

### Pre-read Docs
- docs/v1-database-design.md
- docs/decisions/json-yaml-projection.md
- rel-04/user-stories/02-single-glob-call.md (step 12: "All messages persisted to database")

### Pre-read Source (patterns)
- src/db/message.c (existing VALID_KINDS array pattern to extend)
- src/db/message.h (ik_db_message_insert API to preserve)
- src/db/connection.h (database context types)

### Pre-read Tests (patterns)
- tests/unit/db/message_test.c (unit test structure for message kinds)
- tests/unit/db/message_kind_validation_test.c (validates kind strings)
- tests/integration/db/message_insert_test.c (integration tests showing JSONB data_json handling)

## Pre-conditions
- `make check` passes
- Tool display in scrollback works
- Task `scrollback-tool-display.md` completed

## Task
Extend message persistence to handle tool-related events. Currently supported kinds:
- clear, system, user, assistant, mark, rewind

Need to add:
- `tool_call` - Assistant's request to execute a tool
- `tool_result` - Result of tool execution

Use the `data_json` field to store structured tool data (id, name, arguments, result).

## TDD Cycle

### Red
1. Extend tests in `tests/unit/db/message_test.c`:
   - Insert message with kind "tool_call" succeeds
   - Insert message with kind "tool_result" succeeds
   - data_json field stores tool call details
   - Invalid kinds still rejected
2. Run `make check` - expect test failure (kinds not valid)

### Green
1. Update `src/db/message.c`:
   - Add "tool_call" to the VALID_KINDS array (after "assistant")
   - Add "tool_result" to the VALID_KINDS array (after "tool_call")
2. Integrate persistence into conversation flow:
   - After parsing tool calls, persist with kind "tool_call"
   - After executing tool, persist with kind "tool_result"
   - Store tool details in data_json field using schema from README.md
3. Run `make check` - expect pass

### Refactor
1. Consider if tool_call content should be the formatted display string or raw JSON
2. Run `make check` - verify still green

## Post-conditions
- `make check` passes
- `make lint && make coverage` passes
- Tool events persist to database with appropriate kinds
- data_json contains structured tool information
- 100% test coverage for new code
- Story 02 is complete: single glob call works end-to-end with persistence
- Note: db-tool-replay.md follows immediately to complete the tool replay feature
