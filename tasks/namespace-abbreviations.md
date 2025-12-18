# Task: Fix abbreviation violations in debug_pipe and tool modules

## Target
Refactoring #3: Fix Public API Namespace Pollution (abbreviation violations)

## Pre-read

### Skills
- scm
- naming

### Source patterns
- src/debug_pipe.h
- src/debug_pipe.c
- src/tool.h
- src/tool.c

### Test patterns
- tests/**/test_debug*.c
- tests/**/test_tool*.c

## Pre-conditions
- Working tree is clean (`git status --porcelain` returns empty)
- `ik_debug_mgr_*` functions exist in src/debug_pipe.h
- `ik_tool_add_string_param` exists in src/tool.h
- Previous task (namespace-misc.md) completed

## What
Fix abbreviation violations per naming.md: "manager" and "parameter" must NOT be abbreviated.

## How
Perform mechanical find/replace for each symbol. Use sub-agents to find all usages across the codebase before renaming.

### Renames

#### debug_pipe module (4 functions)
| Old Name | New Name |
|----------|----------|
| `ik_debug_mgr_create` | `ik_debug_manager_create` |
| `ik_debug_mgr_add_pipe` | `ik_debug_manager_add_pipe` |
| `ik_debug_mgr_add_to_fdset` | `ik_debug_manager_add_to_fdset` |
| `ik_debug_mgr_handle_ready` | `ik_debug_manager_handle_ready` |

#### tool module (1 function)
| Old Name | New Name |
|----------|----------|
| `ik_tool_add_string_param` | `ik_tool_add_string_parameter` |

### Steps
1. For each function name, use grep to find all usages across codebase
2. Update declarations in header files
3. Update definitions in source files
4. Update all call sites found in step 1
5. Update any comments or documentation referencing these names
6. Run `make check` to verify no compilation errors or test failures

## Why
Per naming.md, these words must NOT be abbreviated:
- `manager` - must be spelled out completely
- `parameter` - must be spelled out completely

The abbreviations `mgr` and `param` violate the naming convention.

## TDD Cycle
This is a mechanical refactoring task. The existing tests serve as the safety net.

### Red
N/A - existing tests already pass

### Green
Apply the renames systematically:
1. Rename all `ik_debug_mgr_*` functions to `ik_debug_manager_*`
2. Rename `ik_tool_add_string_param` to `ik_tool_add_string_parameter`

### Refactor
Verify with `make check` that all tests pass.

## Post-conditions
- `make check` passes with no errors
- `grep -r "ik_debug_mgr_" src/ tests/` returns no matches
- `grep -r "ik_tool_add_string_param[^e]" src/ tests/` returns no matches (param but not parameter)
- All renamed functions use full words (manager, parameter)
- Working tree is clean (changes committed)

## Complexity
Low - mechanical find/replace refactoring

## Notes
- The debug_pipe module has additional usages in repl.c and shared.c per grep search
- The type `ik_debug_pipe_manager_t` should also be checked for consistency (it uses full word already)
- Search for both function names and any struct field references or variable names that might use the abbreviated forms
- Comments in debug_pipe.h reference `repl->debug_mgr` which should be updated
