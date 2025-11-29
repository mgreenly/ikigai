# Task: Persist Tool Messages to Database

## Target
User story: 02-single-glob-call

## Agent
model: sonnet

## Pre-conditions
- `make check` passes
- Tool display in scrollback works
- Task `scrollback-tool-display.md` completed

## Context
Read before starting:
- src/db/message.h (message persistence API)
- src/db/message.c (implementation)
- docs/decisions/ (any ADRs on message schema)
- rel-04/user-stories/02-single-glob-call.md (step 12: "All messages persisted to database")

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
   - Add "tool_call" and "tool_result" to valid kinds
2. Integrate persistence into conversation flow:
   - After parsing tool calls, persist with kind "tool_call"
   - After executing tool, persist with kind "tool_result"
   - Store tool details in data_json field
3. Run `make check` - expect pass

### Refactor
1. Consider if tool_call content should be the formatted display string or raw JSON
2. Ensure replay can reconstruct tool state (for session restore)
3. Run `make check` - verify still green

## Post-conditions
- `make check` passes
- `make lint && make coverage` passes
- Tool events persist to database with appropriate kinds
- data_json contains structured tool information
- 100% test coverage for new code
- Story 02 is complete: single glob call works end-to-end with persistence
