# Task: Bash External Tool

**UNATTENDED EXECUTION:** This task executes automatically without human oversight. All context is provided.

**Model:** sonnet/thinking
**Depends on:** None

## Context

**Working directory:** Project root (where `Makefile` lives)
**All paths are relative to project root**, not to this task file.

This task creates the first external tool (bash). External tools are standalone executables that follow a JSON protocol: `--schema` returns the tool's schema, stdin receives JSON arguments, stdout returns JSON results.

## Pre-Read

**Skills:**
- `/load errors` - Result types, PANIC usage
- `/load style` - Code style conventions
- `/load naming` - ik_MODULE_THING naming pattern

**Plan:**
- `cdd/plan/tool-specifications.md` - Section "Tool: bash" for complete schema and behavior
- `cdd/plan/tool-discovery-execution.md` - Section "Build System" for Makefile patterns

**Source (examples):**
- `src/json_allocator.c` - How to use talloc-based yyjson allocator
- `src/panic.c` - PANIC macro usage
- `src/error.c` - res_t type (for reference, tools use simple JSON errors)

## Libraries

Use only:
- `yyjson` (vendored at `src/vendor/yyjson/`) - JSON parsing and building
- `talloc` (system library) - Memory management
- POSIX `popen()` - Command execution

Do not introduce new dependencies.

## Preconditions

- [ ] Working copy is clean (verify with `jj diff --summary`)

## Objective

Create a standalone external tool executable for bash command execution. The tool:
1. Responds to `--schema` flag with JSON schema
2. Reads JSON arguments from stdin
3. Executes the command via `popen()`
4. Returns JSON result with output and exit code

## Directory Structure

Create:
```
src/tools/bash/
  main.c
libexec/ikigai/
  bash (built executable)
```

## Schema (`--schema` output)

```json
{
  "name": "bash",
  "description": "Execute a shell command and return output",
  "parameters": {
    "type": "object",
    "properties": {
      "command": {
        "type": "string",
        "description": "Shell command to execute"
      }
    },
    "required": ["command"]
  }
}
```

## Execution Behavior

**Input (stdin):**
```json
{"command": "ls -la /tmp"}
```

**Output (stdout):**
```json
{
  "output": "total 8\ndrwxrwxrwt  10 root root  220 Dec 24 10:00 .",
  "exit_code": 0
}
```

**Implementation details:**
- Use `popen(command, "r")` to execute command
- Capture stdout (stderr merged by shell)
- Strip single trailing newline from output (if present)
- Dynamic buffer allocation (start 4KB, grow as needed)
- Exit code via `WEXITSTATUS(pclose(fp))`
- Failed `popen()` treated as exit code 127

**Tool never returns error envelope.** All outcomes (success, command failure, shell errors) use success envelope with appropriate exit codes.

## Makefile Changes

Add to Makefile:

```makefile
# Shared sources for all tools
TOOL_COMMON_SRCS = src/error.c src/panic.c src/json_allocator.c src/vendor/yyjson/yyjson.c

.PHONY: tool-bash

tool-bash: libexec/ikigai/bash

libexec/ikigai/bash: src/tools/bash/main.c $(TOOL_COMMON_SRCS) | libexec/ikigai
	$(CC) $(CFLAGS) -o $@ src/tools/bash/main.c $(TOOL_COMMON_SRCS) -ltalloc

libexec/ikigai:
	mkdir -p $@
```

## Test Specification

**Reference:** `cdd/plan/test-specification.md` → "Phase 1: External Tools" → "tool-bash.md"

**Testing approach:** External tools are standalone executables tested via shell commands (no unit test file).

**Manual verification commands (run in order.json stop):**
1. `libexec/ikigai/bash --schema` - Returns valid JSON schema
2. `echo '{"command":"echo hello"}' | libexec/ikigai/bash` - Returns `{"output":"hello","exit_code":0}`
3. `echo '{"command":"false"}' | libexec/ikigai/bash` - Returns non-zero exit_code
4. `echo '{}' | libexec/ikigai/bash` - Returns error JSON (missing command)

**Behaviors to verify:**
- Output buffer grows dynamically for large output
- Trailing newline stripped from output
- Exit code extracted via `WEXITSTATUS(pclose())`
- popen() failure returns exit_code 127

**If helper functions extracted:** Create `tests/unit/tools/bash_helpers_test.c` following patterns in test-specification.md

## Implementation Notes

1. **main() structure:**
   - Check for `--schema` flag, print schema and exit
   - Otherwise, read all of stdin into buffer
   - Parse JSON, extract "command" field
   - Execute via popen, collect output
   - Build result JSON, print to stdout

2. **Memory management:**
   - Use talloc for all allocations
   - Create root context, free on exit
   - Use `ik_make_talloc_allocator()` for yyjson

3. **Error handling:**
   - Missing "command" field → return JSON error (not crash)
   - Invalid JSON → return JSON error
   - popen failure → return exit_code 127

## Completion

After completing work (whether success, partial, or failed), commit all changes:

```bash
jj commit -m "$(cat <<'EOF'
task(tool-bash.md): [success|partial|failed] - [brief description]

[Optional: Details about what was accomplished, failures, or remaining work]
EOF
)"
```

Report status to orchestration:
- Success: `/task-done tool-bash.md`
- Partial/Failed: `/task-fail tool-bash.md`

## Postconditions

- [ ] `make tool-bash` builds without warnings
- [ ] `libexec/ikigai/bash --schema` returns valid JSON
- [ ] `echo '{"command":"echo hello"}' | libexec/ikigai/bash` returns correct JSON
- [ ] All changes committed using commit message template
- [ ] Working copy is clean (no uncommitted changes)
