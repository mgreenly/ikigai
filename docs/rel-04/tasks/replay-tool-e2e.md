# Task: Session Replay E2E Verification

## Target
User story: 12-session-replay-with-tools

## Agent
model: sonnet

### Pre-read Skills
- .agents/skills/default.md
- .agents/skills/tdd.md
- .agents/skills/database.md
- .agents/skills/coverage.md

### Pre-read Docs
- rel-04/user-stories/12-session-replay-with-tools.md
- rel-04/README.md (section "Message Transformation (Replay to API)")

### Pre-read Source (patterns)
- src/db/replay.c (replay algorithm)
- src/openai/client.c (request serialization)
- tests/integration/db/ (integration test patterns)

### Pre-read Tests (patterns)
- tests/integration/db/replay_basic_test.c
- tests/integration/db/session_lifecycle_test.c

## Pre-conditions
- `make check` passes
- All Story 02-11 tasks completed
- Tool persistence and replay work (tasks 16-17)
- Tool execution loop works end-to-end

## Task
Create an end-to-end integration test that verifies the complete session replay flow with tool messages. This test simulates:
1. A conversation with tool calls persisted to database
2. Application "restart" (new replay context)
3. Session restoration with tool messages
4. Verification that restored context produces correct API request format

This is a verification test - the functionality is already implemented. This test ensures all the pieces work together correctly.

## TDD Cycle

### Red
1. Create `tests/integration/db/replay_tool_e2e_test.c`:
   - Insert a complete tool conversation into database:
     - user message: "Find C files"
     - tool_call: glob with pattern "*.c"
     - tool_result: list of files
     - assistant: summary response
   - Create new replay context and load session
   - Verify messages are in correct order
   - Verify tool messages have correct structure for API serialization
   - Verify the serialized request matches expected OpenAI format
2. Run `make check` - should pass (functionality exists), but this confirms it

### Green
1. If any issues found, fix them in the appropriate source files
2. The test should pass with existing implementation
3. Run `make check` - expect pass

### Refactor
1. Run `make lint && make coverage`
2. Verify the test covers the full user story 12 walkthrough:
   - Session restore with tool messages
   - Correct message ordering
   - Correct API format after serialization
3. Run `make check` - verify still green

## Post-conditions
- `make check` passes
- `make lint && make coverage` passes
- E2E test verifies complete tool replay flow
- 100% test coverage for new test code
- Story 12 is complete: session replay with tools verified end-to-end
