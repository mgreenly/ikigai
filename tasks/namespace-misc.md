# Task: Fix namespace violations in config, repl, and openai modules

## Target
Refactoring #3: Fix Public API Namespace Pollution (miscellaneous functions)

## Pre-read

### Skills
- scm
- naming

### Source patterns
- src/config.h
- src/config.c
- src/repl.h
- src/repl.c
- src/openai/client.h
- src/openai/client.c

### Test patterns
- tests/**/test_config*.c
- tests/**/test_repl*.c
- tests/**/test_openai*.c

## Pre-conditions
- Working tree is clean (`git status --porcelain` returns empty)
- `expand_tilde` exists in src/config.h
- `update_nav_context` exists in src/repl.h
- `get_message_at_index` exists in src/openai/client.h
- Previous task (namespace-commands.md) completed

## What
Rename 3 public functions in config, repl, and openai modules to follow naming conventions.

## How
Perform mechanical find/replace for each function. Use sub-agents to find all usages across the codebase before renaming.

### Renames (3 total)
| Old Name | New Name | Module |
|----------|----------|--------|
| `expand_tilde` | `ik_cfg_expand_tilde` | config |
| `update_nav_context` | `ik_repl_update_nav_context` | repl |
| `get_message_at_index` | `ik_openai_get_message_at_index` | openai |

### Steps
1. For each function name, use grep to find all usages across codebase
2. Update declaration in respective header file
3. Update definition in respective source file
4. Update all call sites found in step 1
5. Run `make check` to verify no compilation errors or test failures

## Why
All public symbols must follow the `ik_MODULE_THING` naming convention per project/naming.md:
- `expand_tilde` is in config module -> `ik_cfg_expand_tilde`
- `update_nav_context` is in repl module -> `ik_repl_update_nav_context`
- `get_message_at_index` is in openai module -> `ik_openai_get_message_at_index`

## TDD Cycle
This is a mechanical refactoring task. The existing tests serve as the safety net.

### Red
N/A - existing tests already pass

### Green
Apply the renames systematically:
1. Rename `expand_tilde` in config.h and config.c
2. Rename `update_nav_context` in repl.h and repl.c
3. Rename `get_message_at_index` in openai/client.h and related source files

### Refactor
Verify with `make check` that all tests pass.

## Post-conditions
- `make check` passes with no errors
- `grep -rw "expand_tilde" src/ tests/` returns no matches (only ik_cfg_expand_tilde)
- `grep -rw "update_nav_context" src/ tests/` returns no matches (only ik_repl_update_nav_context)
- `grep -rw "get_message_at_index" src/ tests/` returns no matches (only ik_openai_get_message_at_index)
- Working tree is clean (changes committed)

## Complexity
Low - mechanical find/replace refactoring

## Notes
- `expand_tilde` is marked as "internal helper function (exposed for testing)" in config.h
- `update_nav_context` is documented as called after agent switch, fork, and kill
- `get_message_at_index` is marked as "Internal wrapper function (exposed for testing)" in client.h
- Even internal/test helpers that are public symbols need the proper prefix
