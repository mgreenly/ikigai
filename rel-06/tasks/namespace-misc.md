# Task: Rename Miscellaneous Functions to Use ik_ Prefix

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

- src/config.h (expand_tilde declaration)
- src/config.c (expand_tilde definition)
- src/repl.h (update_nav_context declaration)
- src/repl.c (update_nav_context definition)
- src/openai/client.h (get_message_at_index declaration)
- src/openai/client.c or client_msg.c (get_message_at_index definition)

### Test Patterns

- tests/unit/config/config_test.c
- tests/unit/repl/repl_test.c
- tests/unit/openai/client_test.c

## Pre-conditions

- Working tree is clean
- `make check` passes

## Task

Rename 3 miscellaneous public functions that violate the `ik_MODULE_THING` naming convention.

### What

Rename these functions to follow `ik_MODULE_THING` naming:

| Current Name | New Name | Module |
|--------------|----------|--------|
| expand_tilde | ik_cfg_expand_tilde | config |
| update_nav_context | ik_repl_update_nav_context | repl |
| get_message_at_index | ik_openai_get_message_at_index | openai |

### How

Mechanical find/replace operation for each function:

**expand_tilde:**
1. Rename in `src/config.h`
2. Rename in `src/config.c`
3. Update all call sites

**update_nav_context:**
1. Rename in `src/repl.h`
2. Rename in `src/repl.c`
3. Update all call sites

**get_message_at_index:**
1. Rename in `src/openai/client.h`
2. Rename in implementation file
3. Update all call sites

### Sub-agent opportunity

Each of these 3 renames is independent. Sub-agents could handle one function each, then merge results.

### Why

From `project/naming.md`:
> "All public symbols follow: `ik_MODULE_THING`"

These scattered violations:
- Break naming consistency across the codebase
- Make it harder to identify which module owns a function
- Risk symbol collision with external libraries

## TDD Cycle

### Red

No new tests needed - existing tests verify behavior.

Run `make check` before changes to confirm baseline passes.

### Green

1. Rename `expand_tilde` to `ik_cfg_expand_tilde`
   - Update config.h, config.c, and call sites
   - Run `make check`

2. Rename `update_nav_context` to `ik_repl_update_nav_context`
   - Update repl.h, repl.c, and call sites
   - Run `make check`

3. Rename `get_message_at_index` to `ik_openai_get_message_at_index`
   - Update client.h, implementation, and call sites
   - Run `make check`

### Verify

- `make check` passes
- `make lint` passes
- No references to old function names remain:
  ```bash
  grep -rn "\bexpand_tilde\b" src/
  grep -rn "\bupdate_nav_context\b" src/
  grep -rn "\bget_message_at_index\b" src/
  ```
  All return empty (no matches)

## Post-conditions

- `expand_tilde` renamed to `ik_cfg_expand_tilde`
- `update_nav_context` renamed to `ik_repl_update_nav_context`
- `get_message_at_index` renamed to `ik_openai_get_message_at_index`
- All call sites updated
- All tests pass unchanged (only names changed, not behavior)
