# Task: Tool Dispatcher

## Target
User story: 02-single-glob-call

## Agent
model: haiku

### Pre-read Skills
- .agents/skills/tdd.md
- .agents/skills/coverage.md
- .agents/skills/quality.md
- .agents/skills/naming.md

### Pre-read Docs
- docs/memory.md
- docs/return_values.md
- docs/error_handling.md
- rel-04/README.md (tool execution model and error format)

### Pre-read Source (patterns)
- src/tool.h (existing tool module and ik_tool_exec_glob)
- src/error.h (res_t, OK/ERR macros)
- src/config.c (JSON building with yyjson_mut)

### Pre-read Tests (patterns)
- tests/unit/tool/ (glob unit tests created by task glob-execute.md)

## Pre-conditions
- `make check` passes
- `ik_tool_call_t` struct exists
- `ik_tool_exec_glob()` exists and is tested
- Task `glob-execute.md` completed

## Task
Implement `ik_tool_dispatch()` function that routes tool calls by name to the appropriate execution function. For rel-04, only "glob" tool is supported. Unknown tools return a JSON error in the standard format: `{"error": "Unknown tool: <name>"}`.

## TDD Cycle

### Red
1. Add tests to `tests/unit/tool/test_tool.c` or create `tests/unit/tool/test_dispatcher.c`:
   - `ik_tool_dispatch()` with tool name "glob" calls `ik_tool_exec_glob()`
   - Result from glob execution is returned unchanged
   - Tool name "unknown_tool" returns error JSON: `{"error": "Unknown tool: unknown_tool"}`
   - Tool name with NULL or empty string returns appropriate error
   - Verify error format matches spec: single "error" field in JSON object
2. Add declaration to `src/tool.h`:
   - `ik_tool_dispatch(void *parent, const char *tool_name, const char *arguments)` returning `res_t`
3. Add stub in `src/tool.c`: always return `OK("{\"error\": \"Not implemented\"}")`
4. Run `make check` - expect assertion failure (tests expect glob execution or correct error format)

### Green
1. Replace stub in `src/tool.c` with implementation:
   - Check tool_name is not NULL or empty
   - Use `strcmp()` to match tool name
   - If "glob", call `ik_tool_exec_glob(parent, arguments, NULL)` and return result
   - For unknown tools, build error JSON: `{"error": "Unknown tool: <name>"}`
   - Use yyjson_mut for JSON building
3. Run `make check` - expect pass

### Refactor
1. Consider future extensibility (switch statement or function pointer table)
2. Ensure error messages are consistent with docs/error_handling.md
3. Verify all JSON uses talloc-based allocation (parent context)
4. Run `make check` - verify still green

## Post-conditions
- `make check` passes
- `make lint && make coverage` passes
- `ik_tool_dispatch()` routes "glob" to `ik_tool_exec_glob()`
- Unknown tools return `{"error": "Unknown tool: <name>"}` format
- 100% test coverage for new code
