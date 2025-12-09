# Task: Remove Scroll Debug Logging

## Target

Cleanup: Remove temporary debug logging added during mouse scroll bug investigation.

## Pre-read Skills
- .agents/skills/default.md
- .agents/skills/style.md

## Pre-read Source
- src/scroll_accumulator.c (debug logging)
- src/repl_actions.c (debug logging)
- src/repl_actions_viewport.c (debug logging)
- src/repl_viewport.c (debug logging)

## Pre-conditions
- Working tree is clean (`git status --porcelain` returns empty)
- `make check` passes
- Mouse scroll bug is confirmed fixed

## Task

Remove all debug logging added during the mouse scroll investigation. The bug is now confirmed fixed and the logging is no longer needed.

## Files to Clean

Based on scratch.md documentation, debug logging was added to:

1. **src/scroll_accumulator.c** - scroll_acc_result logging
2. **src/repl_actions.c** - input_parsed logging
3. **src/repl_actions_viewport.c** - scroll_up logging
4. **src/repl_viewport.c** - render_viewport logging

Look for patterns like:
- `ik_log_debug_json(doc)` with events like "input_parsed", "scroll_acc_result", "scroll_up", "render_viewport"
- `yyjson_mut_doc *doc = ik_log_create()`
- Static tracking variables like `last_logged_first`

## TDD Cycle

### Red

This is a cleanup task - no new tests needed. Verify `make check` passes before starting.

### Green

1. Search for debug logging patterns in each file
2. Remove the logging code blocks
3. Remove any static variables used only for logging deduplication
4. Remove unused `#include "logger.h"` if no longer needed

### Verify

1. `make check` - all tests pass
2. `make lint` - passes
3. `grep` for the event names to ensure all removed:
   ```bash
   grep -r "input_parsed\|scroll_acc_result\|scroll_up\|render_viewport" src/
   ```
   Should return no matches in the scroll-related files.

## Post-conditions
- Working tree is clean (all changes committed)

- `make check` passes
- No debug logging related to scroll investigation in codebase
- Code is clean and production-ready
