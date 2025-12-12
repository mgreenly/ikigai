# Autocomplete Viewport Scroll Fix

## Target
Bug Fix - autocomplete suggestions clipped off bottom of screen when content fills viewport.

## Pre-read

### Skills
- `.agents/skills/default.md`
- `.agents/skills/tdd.md`
- `.agents/skills/scm.md`

### Docs
- `docs/memory.md`

### Source Patterns
- `src/repl_viewport.c` - Viewport calculation logic
- `src/repl.h` - REPL context with `completion` field
- `src/layer_completion.c` - Completion layer with height calculation
- `src/completion.h` - Completion types

### Test Patterns
- `tests/unit/repl/` - Existing REPL tests

## Pre-conditions
- Working tree is clean
- `make check` passes

## Task

Fix viewport calculation to include autocomplete height in the document model.

**Current behavior:**
1. When scrollback is empty/small, content draws from top of screen
2. Blank space exists below the lower separator
3. Autocomplete renders in that blank space - works fine

**Bug scenario:**
1. When content fills the viewport, lower separator is at bottom of screen
2. User types `/slash` to trigger autocomplete
3. Autocomplete would render below the separator but is clipped off screen
4. Viewport doesn't adjust to show autocomplete

**Root cause:**
In `ik_repl_calculate_viewport()` (repl_viewport.c:38), document height is:
```c
size_t document_height = scrollback_rows + 1 + input_buffer_display_rows + 1;
```
This doesn't include completion layer height.

**Fix:**
1. Add completion height to document_height calculation:
   ```c
   size_t completion_rows = (repl->completion != NULL) ? repl->completion->count : 0;
   size_t document_height = scrollback_rows + 1 + input_buffer_display_rows + 1 + completion_rows;
   ```

2. The existing viewport logic will then naturally scroll to show the autocomplete since the document is taller.

3. The constraint "cursor never above viewport top" is already satisfied because:
   - When `viewport_offset=0`, we show the last `terminal_rows` of the document
   - The input buffer (where cursor lives) is always before the completion layer
   - So scrolling to show completion keeps cursor visible

**Constraint to verify:**
- Input cursor must never scroll above the top of the viewport (row 0)
- If autocomplete is taller than available space below cursor, cursor should be at row 0 with as much autocomplete shown as fits

## TDD Cycle

### Red
1. Write test that sets up REPL with:
   - Scrollback content that fills most of the viewport
   - Active completion with multiple candidates
2. Calculate viewport and verify document_height includes completion rows
3. Test should fail because completion height not included

### Green
1. In `ik_repl_calculate_viewport()`, get completion height from `repl->completion`
2. Add completion rows to document_height calculation
3. Also update the duplicate calculation in `ik_repl_render_frame()` (around line 188)
4. `make check` passes

### Refactor
1. Consider extracting document height calculation to avoid duplication between `ik_repl_calculate_viewport()` and `ik_repl_render_frame()`
2. `make check` passes

## Post-conditions
- Document height includes completion layer when active
- Autocomplete suggestions visible when triggered, regardless of scrollback size
- Input cursor never scrolls above viewport top
- `make check` passes
- `make lint` passes
