# Task: Tool Dispatcher

## Target
User story: 02-single-glob-call

## Agent
model: haiku

### Pre-read Skills
- .agents/skills/default.md
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
- Task `tool-argument-parser.md` completed (provides `ik_tool_arg_get_string()`)

## Task
Implement `ik_tool_dispatch()` function that routes tool calls by name to the appropriate execution function. The dispatcher is responsible for:
1. Parsing the JSON `arguments` string
2. Extracting parameters specific to each tool using `ik_tool_arg_get_string()`
3. Validating required parameters are present
4. Calling each tool with typed parameters (not raw JSON)

For rel-04, supported tools are: "glob", "file_read", "grep", "file_write", "bash". Unknown tools return a JSON error in the standard format: `{"error": "Unknown tool: <name>"}`. Invalid JSON or missing required parameters return appropriate error messages.

## TDD Cycle

### Red
1. Add tests to `tests/unit/tool/test_tool.c` or create `tests/unit/tool/test_dispatcher.c`:
   - `ik_tool_dispatch()` with tool name "glob" and valid JSON parses arguments and calls `ik_tool_exec_glob()` with typed parameters
   - Result from tool execution is returned unchanged
   - Invalid JSON arguments returns error JSON: `{"error": "Invalid JSON arguments"}`
   - Missing required parameter returns error JSON: `{"error": "Missing required parameter: pattern"}` (for glob)
   - Missing required parameter returns error JSON: `{"error": "Missing required parameter: path"}` (for file_read)
   - Tool name "unknown_tool" returns error JSON: `{"error": "Unknown tool: unknown_tool"}`
   - Tool name with NULL or empty string returns appropriate error
   - Verify error format matches spec: single "error" field in JSON object
2. Add declaration to `src/tool.h`:
   - `ik_tool_dispatch(void *parent, const char *tool_name, const char *arguments)` returning `res_t`
3. Add stub in `src/tool.c`: always return `OK("{\"error\": \"Not implemented\"}")`
4. Run `make check` - expect assertion failure (tests expect correct JSON parsing and tool execution)

### Green
1. Replace stub in `src/tool.c` with implementation:
   - Check tool_name is not NULL or empty
   - Parse arguments JSON string using yyjson
   - If parse fails, return error JSON: `{"error": "Invalid JSON arguments"}`
   - Use `strcmp()` to match tool_name
   - For "glob":
     - Extract "pattern" (required) using `ik_tool_arg_get_string()` from tool-argument-parser task
     - Extract "path" (optional, may be NULL)
     - Validate "pattern" is present, return error if missing: `{"error": "Missing required parameter: pattern"}`
     - Call `ik_tool_exec_glob(parent, pattern, path)` with typed parameters
     - Return result
   - For "file_read":
     - Extract "path" (required) using `ik_tool_arg_get_string()`
     - Validate "path" is present, return error if missing
     - Call `ik_tool_exec_file_read(parent, path)` and return result
   - For "grep":
     - Extract "pattern" (required), "glob" (optional), "path" (optional)
     - Validate "pattern" is present
     - Call `ik_tool_exec_grep(parent, pattern, glob, path)` and return result
   - For "file_write":
     - Extract "path" (required) and "content" (required)
     - Validate both are present
     - Call `ik_tool_exec_file_write(parent, path, content)` and return result
   - For "bash":
     - Extract "command" (required)
     - Validate "command" is present
     - Call `ik_tool_exec_bash(parent, command)` and return result
   - For unknown tools, build error JSON: `{"error": "Unknown tool: <name>"}`
   - Use yyjson_mut for JSON building (error responses)
2. Run `make check` - expect pass

### Refactor
1. Consider organizing into helper functions per tool if dispatcher grows large:
   - `dispatch_glob()` - handles glob-specific parsing and execution
   - `dispatch_file_read()` - handles file_read-specific parsing and execution
   - `dispatch_grep()` - handles grep-specific parsing and execution
   - `dispatch_file_write()` - handles file_write-specific parsing and execution
   - `dispatch_bash()` - handles bash-specific parsing and execution
2. Consider future extensibility (switch statement or function pointer table for tool routing)
3. Ensure error messages are consistent with docs/error_handling.md
4. Verify all JSON uses talloc-based allocation (parent context)
5. Run `make check` - verify still green

## Post-conditions
- `make check` passes
- `make lint && make coverage` passes
- `ik_tool_dispatch()` parses JSON arguments and routes all tools with typed parameters:
  - "glob" to `ik_tool_exec_glob(parent, pattern, path)`
  - "file_read" to `ik_tool_exec_file_read(parent, path)`
  - "grep" to `ik_tool_exec_grep(parent, pattern, glob, path)`
  - "file_write" to `ik_tool_exec_file_write(parent, path, content)`
  - "bash" to `ik_tool_exec_bash(parent, command)`
- Invalid JSON returns `{"error": "Invalid JSON arguments"}` format
- Missing required parameters return `{"error": "Missing required parameter: <name>"}` format
- Unknown tools return `{"error": "Unknown tool: <name>"}` format
- 100% test coverage for new code
