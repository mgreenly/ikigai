# Task: Fix Abbreviation Violations (mgr -> manager, param -> parameter)

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

- src/debug_pipe.h (ik_debug_mgr_* function declarations)
- src/debug_pipe.c (ik_debug_mgr_* function definitions)
- src/shared.h (debug_mgr field)
- src/tool.h (ik_tool_add_string_param declaration)
- src/tool.c (ik_tool_add_string_param definition)

### Test Patterns

- tests/unit/debug_pipe/debug_pipe_test.c (if exists)
- tests/unit/tool/tool_test.c

## Pre-conditions

- Working tree is clean
- `make check` passes

## Task

Fix abbreviation violations per `project/naming.md` which states these words must not be abbreviated: handler, manager, server, client, function, parameter, shutdown, payload.

### What

Rename functions and fields that use forbidden abbreviations:

**mgr -> manager:**

| Current Name | New Name |
|--------------|----------|
| ik_debug_mgr_create | ik_debug_manager_create |
| ik_debug_mgr_add_pipe | ik_debug_manager_add_pipe |
| ik_debug_mgr_add_to_fdset | ik_debug_manager_add_to_fdset |
| ik_debug_mgr_handle_ready | ik_debug_manager_handle_ready |
| debug_mgr (field in shared.h) | debug_manager |

**param -> parameter:**

| Current Name | New Name |
|--------------|----------|
| ik_tool_add_string_param | ik_tool_add_string_parameter |

### How

Mechanical find/replace operation:

**Phase 1: debug_mgr -> debug_manager**
1. Rename all `ik_debug_mgr_*` functions in header and implementation
2. Rename `debug_mgr` field in `src/shared.h` to `debug_manager`
3. Update all references across codebase
4. Run `make check`

**Phase 2: param -> parameter**
1. Rename `ik_tool_add_string_param` in header and implementation
2. Update all call sites
3. Run `make check`

### Sub-agent opportunity

The two abbreviation fixes (mgr and param) are independent. Consider using sub-agents to parallelize:
- One sub-agent for mgr -> manager renaming
- One sub-agent for param -> parameter renaming

### Why

From `project/naming.md`:
> "These words are NEVER abbreviated in identifiers: handler, manager, server, client, function, parameter, shutdown, payload"

Abbreviations reduce code clarity and violate the project's ubiquitous language principles (DDD).

## TDD Cycle

### Red

No new tests needed - existing tests verify behavior.

Run `make check` before changes to confirm baseline passes.

### Green

1. Rename all `ik_debug_mgr_*` functions:
   - Update debug_pipe.h
   - Update debug_pipe.c
   - Search: `grep -rn "ik_debug_mgr" src/ tests/`
   - Update all call sites
   - Run `make check`

2. Rename `debug_mgr` field:
   - Update shared.h struct definition
   - Search: `grep -rn "->debug_mgr\|\.debug_mgr" src/ tests/`
   - Update all field accesses
   - Run `make check`

3. Rename `ik_tool_add_string_param`:
   - Update tool.h
   - Update tool.c
   - Search: `grep -rn "ik_tool_add_string_param" src/ tests/`
   - Update all call sites
   - Run `make check`

### Verify

- `make check` passes
- `make lint` passes
- No abbreviation violations remain:
  ```bash
  grep -rn "ik_debug_mgr" src/
  grep -rn "debug_mgr" src/
  grep -rn "ik_tool_add_string_param\b" src/
  ```
  All return empty (no matches)

## Post-conditions

- All `ik_debug_mgr_*` functions renamed to `ik_debug_manager_*`
- `debug_mgr` field renamed to `debug_manager` in shared.h
- `ik_tool_add_string_param` renamed to `ik_tool_add_string_parameter`
- All call sites and field accesses updated
- All tests pass unchanged (only names changed, not behavior)
- No forbidden abbreviations (mgr, param) remain in public API
