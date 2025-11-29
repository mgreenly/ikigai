# Task: Execute Bash Tool

## Target
User story: 07-bash-command

## Agent
model: sonnet

## Pre-conditions
- `make check` passes
- `ik_tool_call_t` struct exists
- `ik_tool_build_bash_schema()` exists
- Tasks `file-write-execute.md` and `conversation-tool-loop.md` completed

## Context
Read before starting:
- docs/memory.md (talloc patterns)
- docs/return_values.md (Result types)
- src/tool.h (existing tool module)
- rel-04/user-stories/07-bash-command.md (see Tool Result A for expected format)
- rel-04/tasks/glob-execute.md (similar execution pattern)
- man 3 popen (executing shell commands)

## Task
Implement bash tool execution. Given a command string, execute it via shell and return stdout and exit code as JSON matching the expected format:

```json
{"output": "main", "exit_code": 0}
```

## TDD Cycle

### Red
1. Add tests to `tests/unit/tool/test_tool.c` or create `tests/unit/tool/test_bash_execute.c`:
   - `ik_tool_exec_bash()` with command "echo test" returns output and exit code 0
   - Result is valid JSON with "output" and "exit_code" fields
   - Failed command returns non-zero exit code
   - Command with no output returns `{"output": "", "exit_code": 0}`
   - Invalid/unsafe command patterns are handled appropriately
2. Run `make check` - expect compile failure

### Green
1. Add to `src/tool.h`:
   - Declare `ik_tool_exec_bash(void *parent, const char *arguments)` returning `res_t`
2. Implement in `src/tool.c`:
   - Parse arguments JSON to extract command string
   - Use popen() to execute command and capture stdout
   - Capture exit code with pclose()
   - Build result JSON with yyjson
   - Handle execution errors (command not found, permission denied, etc.)
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
