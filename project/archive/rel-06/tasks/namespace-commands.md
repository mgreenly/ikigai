# Task: Rename commands.h Functions to ik_cmd_ Prefix

## Target

Refactoring #3: Fix Public API Namespace Pollution

## Pre-read

### Skills

- default
- tdd
- style
- naming
- scm

### Source Files

- src/commands.h (8 cmd_* function declarations without ik_ prefix)
- src/commands.c (8 cmd_* function definitions)

### Test Patterns

- tests/unit/commands/commands_test.c
- tests/integration/*.c (may reference commands)

## Pre-conditions

- Working tree is clean
- `make check` passes

## Task

Rename all 8 command functions in commands.h to follow `ik_cmd_` naming convention.

### What

Rename these functions to follow `ik_MODULE_THING` naming:

| Current Name | New Name |
|--------------|----------|
| cmd_fork | ik_cmd_fork |
| cmd_kill | ik_cmd_kill |
| cmd_send | ik_cmd_send |
| cmd_check_mail | ik_cmd_check_mail |
| cmd_read_mail | ik_cmd_read_mail |
| cmd_delete_mail | ik_cmd_delete_mail |
| cmd_filter_mail | ik_cmd_filter_mail |
| cmd_agents | ik_cmd_agents |

### How

Mechanical find/replace operation:

1. Update declarations in `src/commands.h`
2. Update definitions in `src/commands.c`
3. Update command registration table (if any)
4. Update all call sites across the codebase
5. Update test file references

### Sub-agent opportunity

This is a mechanical change affecting multiple files. Consider using sub-agents to parallelize:
- One sub-agent for header/implementation updates
- One sub-agent for test updates
- One sub-agent for call site updates

### Why

From `project/naming.md`:
> "All public symbols follow: `ik_MODULE_THING`"

Current `cmd_` prefix lacks the `ik_` namespace, risking:
- Symbol collision with other libraries using `cmd_` prefix
- Inconsistency with other ikigai public APIs
- Reduced greppability for ikigai-specific code

## TDD Cycle

### Red

No new tests needed - existing tests verify behavior.

Run `make check` before changes to confirm baseline passes.

### Green

For each function:
1. Rename in header file
2. Rename in implementation file
3. Search for old name: `grep -rn "cmd_fork\|cmd_kill\|cmd_send" src/ tests/`
4. Update all call sites
5. Run `make check` to verify

### Verify

- `make check` passes
- `make lint` passes
- No references to old function names remain
- `grep -rn "\bcmd_fork\b\|\bcmd_kill\b\|\bcmd_send\b" src/` returns empty

## Post-conditions

- All 8 command functions renamed with `ik_cmd_` prefix
- All call sites updated
- All tests pass unchanged (only names changed, not behavior)
- All command functions in commands.h start with `ik_cmd_`
