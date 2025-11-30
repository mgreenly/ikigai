# Task: E2E Test for tool_choice None Mode

## Target
User story: 13-tool-choice-none

## Agent
model: sonnet

### Pre-read Skills
- .agents/skills/tdd.md
- .agents/skills/testability.md
- .agents/skills/mocking.md
- .agents/skills/coverage.md
- .agents/skills/quality.md
- .agents/skills/default.md

### Pre-read Docs
- docs/error_handling.md
- docs/architecture.md
- rel-04/user-stories/13-tool-choice-none.md
- rel-04/tasks/tool-loop-limit-e2e.md

### Pre-read Source (patterns)
- src/openai/client.c (request creation and serialization patterns)
- src/openai/client_multi_request.c (how requests are assembled and sent with curl)
- src/wrapper.h (MOCKABLE pattern for mocking external dependencies)

### Pre-read Tests (patterns)
- tests/unit/openai/client_multi_test_common.h (comprehensive mocking infrastructure for curl and callbacks)
- tests/unit/openai/client_multi_add_request_test.c (testing patterns for request creation with mocked dependencies)
- tests/integration/openai_mock_verification_test.c (integration test setup and stream callback patterns)
- tests/integration/ (existing e2e test structure)

## Pre-conditions
- `make check` passes
- Task `tool-choice-auto-e2e.md` completed
- Auto mode e2e test passes

## Task
Create an end-to-end test that verifies `tool_choice: "none"` works correctly:
1. User asks for file search
2. Request explicitly sets `tool_choice: "none"`
3. Model responds with text only (cannot call tools)
4. No tool execution occurs

Note: This requires a way to set tool_choice for a request. This might be a command-line flag, config setting, or test-only API.

**Note**: This is a verification test. The functionality is implemented in earlier tasks. If previous tasks are complete, the test should pass. If it fails, identify and fix gaps in the implementation.

## TDD Cycle

### Red
1. Create tests/integration/test_tool_choice_none.c:
   - Configure request to use tool_choice none
   - Mock OpenAI API to return text-only response (no tool_calls)
   - Send user message "Find all C files in src/"
   - Verify outgoing request has `"tool_choice": "none"`
   - Verify NO tool execution occurs
   - Verify assistant responds with text only
2. Run `make check` - expect failure if no way to configure tool_choice

### Green
1. If no configuration mechanism exists, add minimal one:
   - Option A: Test-only function to set tool_choice
   - Option B: Config file or env var for tool_choice
   - Option C: Command-line flag (if REPL supports it)
2. Implement test to verify none mode behavior
3. Run `make check` - expect pass

### Refactor
1. Consider if configuration mechanism is suitable for user-facing features
2. Document how to use tool_choice none mode
3. Verify test doesn't interfere with other tests
4. Run `make check` - verify still green

## Post-conditions
- `make check` passes
- `make lint && make coverage` passes
- E2E test verifies tool_choice none mode
- Test covers full conversation flow from user story 13-tool-choice-none
- Some mechanism exists to configure tool_choice (even if test-only)
- 100% coverage of none mode path
