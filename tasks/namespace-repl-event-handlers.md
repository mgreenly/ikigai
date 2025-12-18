# Task: Fix namespace violations in repl_event_handlers module

## Target
Refactoring #3: Fix Public API Namespace Pollution (repl_event_handlers functions)

## Pre-read

### Skills
- scm
- naming

### Source patterns
- src/repl_event_handlers.h
- src/repl_event_handlers.c

### Test patterns
- tests/**/test_repl*.c

## Pre-conditions
- Working tree is clean (`git status --porcelain` returns empty)
- All 10 unprefixed functions exist in src/repl_event_handlers.h

## What
Rename 10 public functions in repl_event_handlers module to follow `ik_repl_*` naming convention.

## How
Perform mechanical find/replace for each function. Use sub-agents to find all usages across the codebase before renaming.

### Renames (10 total)
| Old Name | New Name |
|----------|----------|
| `calculate_select_timeout_ms` | `ik_repl_calculate_select_timeout_ms` |
| `setup_fd_sets` | `ik_repl_setup_fd_sets` |
| `handle_terminal_input` | `ik_repl_handle_terminal_input` |
| `handle_curl_events` | `ik_repl_handle_curl_events` |
| `handle_agent_request_success` | `ik_repl_handle_agent_request_success` |
| `handle_tool_completion` | `ik_repl_handle_tool_completion` |
| `handle_agent_tool_completion` | `ik_repl_handle_agent_tool_completion` |
| `calculate_curl_min_timeout` | `ik_repl_calculate_curl_min_timeout` |
| `handle_select_timeout` | `ik_repl_handle_select_timeout` |
| `poll_tool_completions` | `ik_repl_poll_tool_completions` |

### Steps
1. For each function name, use grep to find all usages across codebase
2. Update declaration in src/repl_event_handlers.h
3. Update definition in src/repl_event_handlers.c
4. Update all call sites found in step 1
5. Run `make check` to verify no compilation errors or test failures

## Why
All public symbols must follow the `ik_MODULE_THING` naming convention per project/naming.md. These functions are part of the repl module and must use the `ik_repl_` prefix.

## TDD Cycle
This is a mechanical refactoring task. The existing tests serve as the safety net.

### Red
N/A - existing tests already pass

### Green
Apply the renames systematically:
1. Rename all 10 functions in header file
2. Rename all 10 functions in source file
3. Update all call sites

### Refactor
Verify with `make check` that all tests pass.

## Post-conditions
- `make check` passes with no errors
- `grep -r "calculate_select_timeout_ms\|setup_fd_sets\|handle_terminal_input\|handle_curl_events\|handle_agent_request_success\|handle_tool_completion\|handle_agent_tool_completion\|calculate_curl_min_timeout\|handle_select_timeout\|poll_tool_completions" src/ tests/` returns no matches (old names gone)
- All renamed functions use `ik_repl_` prefix
- Working tree is clean (changes committed)

## Complexity
Low - mechanical find/replace refactoring

## Notes
- Search the entire codebase including tests for usages before renaming
- These functions are declared in repl_event_handlers.h but may be called from repl.c and other files
- Comments referencing these functions should also be updated
