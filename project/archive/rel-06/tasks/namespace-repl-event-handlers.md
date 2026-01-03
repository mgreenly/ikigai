# Task: Rename repl_event_handlers.h Functions to ik_repl_ Prefix

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

- src/repl_event_handlers.h (10 function declarations without ik_repl_ prefix)
- src/repl_event_handlers.c (10 function definitions)

### Test Patterns

- tests/unit/repl_event_handlers/repl_event_handlers_test.c

## Pre-conditions

- Working tree is clean
- `make check` passes

## Task

Rename all 10 public functions in repl_event_handlers to follow `ik_repl_` naming convention.

### What

Rename these functions to follow `ik_MODULE_THING` naming:

| Current Name | New Name |
|--------------|----------|
| calculate_select_timeout_ms | ik_repl_calculate_select_timeout_ms |
| setup_fd_sets | ik_repl_setup_fd_sets |
| handle_terminal_input | ik_repl_handle_terminal_input |
| handle_curl_events | ik_repl_handle_curl_events |
| handle_agent_request_success | ik_repl_handle_agent_request_success |
| handle_tool_completion | ik_repl_handle_tool_completion |
| handle_agent_tool_completion | ik_repl_handle_agent_tool_completion |
| calculate_curl_min_timeout | ik_repl_calculate_curl_min_timeout |
| handle_select_timeout | ik_repl_handle_select_timeout |
| poll_tool_completions | ik_repl_poll_tool_completions |

### How

Mechanical find/replace operation:

1. Update declarations in `src/repl_event_handlers.h`
2. Update definitions in `src/repl_event_handlers.c`
3. Update all call sites across the codebase
4. Update test file references

### Sub-agent opportunity

This is a wide-ranging mechanical change affecting many files. Consider using sub-agents to parallelize:
- One sub-agent per function rename, OR
- One sub-agent for header/implementation, another for call sites

### Why

From `project/naming.md`:
> "All public symbols follow: `ik_MODULE_THING`"

Current names lack namespace prefix, risking:
- Symbol collision with other libraries
- Unclear module ownership
- Reduced greppability for module-specific code

## TDD Cycle

### Red

No new tests needed - existing tests verify behavior.

Run `make check` before changes to confirm baseline passes.

### Green

For each function:
1. Rename in header file
2. Rename in implementation file
3. Search for old name: `grep -rn "old_name" src/ tests/`
4. Update all call sites
5. Run `make check` to verify

### Verify

- `make check` passes
- `make lint` passes
- No references to old function names remain
- `grep -rn "^calculate_select_timeout_ms\|^setup_fd_sets\|^handle_terminal_input" src/` returns empty

## Post-conditions

- All 10 functions renamed with `ik_repl_` prefix
- All call sites updated
- All tests pass unchanged (only names changed, not behavior)
- No unprefixed function names in repl_event_handlers.h
