# Task: Tool Result Limit Metadata

## Target
User story: 11-tool-loop-limit-reached

## Agent
model: sonnet

### Pre-read Skills
- .agents/skills/default.md
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
- src/openai/client.h (ik_openai_msg_t with tool_call_id, tool message creation)
- src/openai/client.c (JSON serialization with yyjson pattern)
- src/tool.h (tool execution functions)
- src/tool.c (JSON result building pattern)

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
1. Add test to tool test suite (e.g., `tests/unit/tool/tool_limit_test.c`):
   - Create a tool result JSON string (e.g., `{"output": "file.c", "count": 1}`)
   - Call function to add limit metadata: `ik_tool_result_add_limit_metadata(parent, result_json, max_tool_turns)`
   - Parse returned JSON and verify it contains:
     - Original `"output"` and `"count"` fields preserved
     - `"limit_reached": true`
     - `"limit_message": "Tool call limit reached (3). Stopping tool loop."`
2. Add declaration to `src/tool.h`:
   - `char *ik_tool_result_add_limit_metadata(void *parent, const char *result_json, int max_tool_turns)`
3. Add stub in `src/tool.c`: `return talloc_strdup(parent, result_json);` (returns unchanged)
4. Run `make check` - expect assertion failure (tests expect limit metadata fields)

### Green
1. Replace stub in `src/tool.c` with implementation:
   - Parse input JSON with yyjson
   - Create mutable copy of the JSON object
   - Add `"limit_reached": true` field
   - Add `"limit_message"` field with formatted message
   - Serialize back to JSON string
   - Return new JSON string (talloc'd to parent)
3. Run `make check` - expect pass

### Refactor
1. Ensure proper yyjson memory cleanup
2. Handle edge cases (malformed input JSON, NULL inputs)
3. Verify naming consistency with project conventions
4. Run `make check` - verify still green

## Post-conditions
- `make check` passes
- `make lint && make coverage` passes
- `ik_tool_result_add_limit_metadata()` function adds limit fields to JSON result
- Original JSON fields are preserved when metadata is added
- Normal tool results (without limit) work unchanged
- 100% test coverage for new code
