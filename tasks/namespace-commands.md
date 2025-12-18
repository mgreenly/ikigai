# Task: Fix namespace violations in commands module

## Target
Refactoring #3: Fix Public API Namespace Pollution (commands functions)

## Pre-read

### Skills
- scm
- naming

### Source patterns
- src/commands.h
- src/commands.c

### Test patterns
- tests/**/test_command*.c

## Pre-conditions
- Working tree is clean (`git status --porcelain` returns empty)
- All 8 unprefixed command functions exist in src/commands.h
- Previous task (namespace-repl-event-handlers.md) completed

## What
Rename 8 public command handler functions to follow `ik_cmd_*` naming convention.

## How
Perform mechanical find/replace for each function. Use sub-agents to find all usages across the codebase before renaming.

### Renames (8 total)
| Old Name | New Name |
|----------|----------|
| `cmd_fork` | `ik_cmd_fork` |
| `cmd_kill` | `ik_cmd_kill` |
| `cmd_send` | `ik_cmd_send` |
| `cmd_check_mail` | `ik_cmd_check_mail` |
| `cmd_read_mail` | `ik_cmd_read_mail` |
| `cmd_delete_mail` | `ik_cmd_delete_mail` |
| `cmd_filter_mail` | `ik_cmd_filter_mail` |
| `cmd_agents` | `ik_cmd_agents` |

### Steps
1. For each function name, use grep to find all usages across codebase
2. Update declaration in src/commands.h
3. Update definition in src/commands.c
4. Update all call sites found in step 1 (likely in command registry array)
5. Run `make check` to verify no compilation errors or test failures

## Why
All public symbols must follow the `ik_MODULE_THING` naming convention per project/naming.md. These are command handlers that use the `cmd` module prefix and must use `ik_cmd_`.

## TDD Cycle
This is a mechanical refactoring task. The existing tests serve as the safety net.

### Red
N/A - existing tests already pass

### Green
Apply the renames systematically:
1. Rename all 8 functions in header file
2. Rename all 8 functions in source file
3. Update command registry (likely an array of function pointers)

### Refactor
Verify with `make check` that all tests pass.

## Post-conditions
- `make check` passes with no errors
- `grep -rw "cmd_fork\|cmd_kill\|cmd_send\|cmd_check_mail\|cmd_read_mail\|cmd_delete_mail\|cmd_filter_mail\|cmd_agents" src/ tests/` returns no matches (old names gone)
- All renamed functions use `ik_cmd_` prefix
- Working tree is clean (changes committed)

## Complexity
Low - mechanical find/replace refactoring

## Notes
- The command handlers are likely registered in a static array in commands.c
- Search for function pointer usages, not just direct calls
- These are handler functions, so they're probably referenced in a registry rather than called directly
