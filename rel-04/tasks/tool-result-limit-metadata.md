# Task: Tool Result Limit Metadata

## Target
User story: 11-tool-loop-limit-reached

## Agent
model: sonnet

### Pre-read Skills
- .agents/skills/tdd.md
- .agents/skills/naming.md
- .agents/skills/coverage.md
- .agents/skills/testability.md
- .agents/skills/style.md
- .agents/skills/quality.md

### Pre-read Docs
- docs/architecture.md
- docs/memory.md
- docs/error_handling.md
- rel-04/user-stories/11-tool-loop-limit-reached.md
- rel-04/tasks/tool-result-msg.md

### Pre-read Source (patterns)
- src/openai/client.h (extend ik_openai_msg_t for tool_call_id)
- src/openai/client.c (JSON serialization with yyjson pattern)
- src/tools/tool.h (tool result structures)
- src/openai/message.h (message construction)

### Pre-read Tests (patterns)
- tests/unit/openai/client_structures_test.c (testing patterns for message structures)
- tests/unit/openai/client_http_mock_test.c (mocking and result handling)

## Pre-conditions
- `make check` passes
- Task `tool-loop-counter.md` completed
- Conversation loop detects when limit is reached
- Tool result messages work (Story 02: tool-result-msg.md completed)

## Task
When the tool iteration limit is reached, add metadata to the tool result message. The result should include:
- `limit_reached: true` (boolean flag)
- `limit_message: "Tool call limit reached (3). Stopping tool loop."` (explanation string)

This metadata is added to the JSON content of the tool result message so the model understands why the loop is stopping.

## TDD Cycle

### Red
1. Add test to tool result message test suite:
   - Create tool result with normal output
   - Call function to add limit metadata (e.g., `ik_tool_result_set_limit_reached(result, 3)`)
   - Serialize to JSON
   - Verify JSON contains:
     - Original output/count fields
     - `"limit_reached": true`
     - `"limit_message": "Tool call limit reached (3). Stopping tool loop."`
2. Run `make check` - expect compile failure (function doesn't exist)

### Green
1. Locate tool result structure (e.g., `ik_tool_result_t` in src/tools/tool.h)
2. Add fields for limit tracking:
   - `bool limit_reached`
   - `char *limit_message` (or use fixed buffer if appropriate)
3. Implement function to set limit metadata:
   - `ik_tool_result_set_limit_reached(result, max_iterations)`
   - Sets `limit_reached = true`
   - Generates message: "Tool call limit reached (%d). Stopping tool loop."
4. Update JSON serialization to include these fields when present
5. Run `make check` - expect pass

### Refactor
1. Ensure proper memory management for limit_message
2. Consider if limit fields should be optional (only serialized if limit_reached is true)
3. Check if existing result cleanup functions need updates
4. Verify naming consistency with project conventions
5. Run `make check` - verify still green

## Post-conditions
- `make check` passes
- `make lint && make coverage` passes
- Tool results can include limit_reached and limit_message fields
- JSON serialization includes metadata when limit is reached
- Normal tool results (without limit) work unchanged
- 100% test coverage for new code
