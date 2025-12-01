# Task: Execute Bash Tool

## Target
User story: 07-bash-command

## Agent
model: sonnet

### Pre-read Skills
- .agents/skills/default.md
- .agents/skills/tdd.md
- .agents/skills/quality.md
- .agents/skills/testability.md
- .agents/skills/coverage.md

### Pre-read Docs
- docs/memory.md
- docs/return_values.md
- docs/error_handling.md
- docs/error_testing.md
- rel-04/user-stories/07-bash-command.md (user story - see Tool Result A for expected format)
- rel-04/tasks/glob-execute.md (related task - similar execution pattern)
- rel-04/docs/tool-result-format.md
- rel-04/docs/tool_use.md

### Pre-read Source (patterns)
- src/config.c (JSON building with yyjson and talloc allocator)
- src/error.h (res_t pattern and error creation)
- src/wrapper.c (system call patterns and wrappers)
- src/tool.h (existing tool module)

### Pre-read Tests (patterns)
- tests/integration/config_integration_test.c (test setup, talloc contexts, file operations)
- tests/integration/db/error_handling_test.c (error result handling patterns)

## Pre-conditions
- `make check` passes
- `ik_tool_call_t` struct exists
- `ik_tool_build_bash_schema()` exists
- Task `file-write-execute.md` completed
- Task `tool-loop-continuation.md` completed (conversation loop works)

## Task
Implement bash tool execution. Given a command string, execute it via shell and return stdout and exit code as JSON matching the expected format:

```json
{"success": true, "data": {"output": "main", "exit_code": 0}}
```

Note: For bash execution, the exit_code is included in the data field. A non-zero exit code does NOT indicate a tool error - the tool successfully executed the command. Tool errors (like being unable to execute popen) should use the error envelope: `{"success": false, "error": "message"}`

## TDD Cycle

### Red
1. Add tests to `tests/unit/tool/test_tool.c` or create `tests/unit/tool/test_bash_execute.c`:
   - `ik_tool_exec_bash()` with command "echo test" returns output and exit code 0
   - Result is valid JSON with envelope format: `{"success": true, "data": {"output": "...", "exit_code": 0}}`
   - Failed command returns non-zero exit code: `{"success": true, "data": {"output": "...", "exit_code": N}}`
   - Command with no output returns `{"success": true, "data": {"output": "", "exit_code": 0}}`
   - Execution errors (popen fails) return error envelope: `{"success": false, "error": "message"}`
2. Add declaration to `src/tool.h`:
   - `ik_tool_exec_bash(void *parent, const char *command)` returning `res_t`
   - Note: The dispatcher handles JSON parsing to extract the command string
3. Add stub in `src/tool.c`: `return OK("{\"success\": true, \"data\": {\"output\": \"\", \"exit_code\": -1}}");`
4. Run `make check` - expect assertion failure (tests expect command execution with exit_code 0)

### Green
1. Replace stub in `src/tool.c` with implementation:
   - Use popen() to execute command and capture stdout
   - Capture exit code with pclose()
   - Build result JSON with yyjson in envelope format: `{"success": true, "data": {"output": "...", "exit_code": N}}`
   - Handle execution errors (popen fails) and return error envelope: `{"success": false, "error": "message"}`
   - Note: Non-zero exit codes from the command itself should still be wrapped in the success envelope with the exit_code field
3. Run `make check` - expect pass

### Refactor
1. Ensure popen resources are properly closed
2. Consider output size limits (prevent memory exhaustion from infinite output)
3. Handle stderr appropriately (redirect to stdout or separate field)
4. Run `make check` - verify still green

## Post-conditions
- `make check` passes
- `make lint && make coverage` passes
- `ik_tool_exec_bash()` executes shell commands and returns JSON result
- Handles edge cases (command failure, empty output, errors)
- 100% test coverage for new code
