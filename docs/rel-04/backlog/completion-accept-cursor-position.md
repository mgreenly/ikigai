# Completion Accept Cursor Position

## Problem

After pressing Tab to accept a completion, the cursor moves to column 0 instead of the end of the completed text.

**Before Tab:**
```
/mod*
     ^ cursor at position 4
```

**After Tab (actual):**
```
*model
^ cursor at position 0
```

**After Tab (expected):**
```
/model*
       ^ cursor at position 6
```

## Root Cause

`ik_input_buffer_set_text()` in `src/input_buffer/core.c` unconditionally resets the cursor to position 0 when replacing text:

```c
input_buffer->cursor_byte_offset = 0;
input_buffer->target_column = 0;
input_buffer->cursor->byte_offset = 0;
input_buffer->cursor->grapheme_offset = 0;
```

This is correct for some use cases (history browsing, Escape revert) but wrong for completion acceptance where the cursor should end up at the end of the new text.

## Why This Matters

- Users expect to continue typing after accepting a completion
- Cursor at position 0 means they'd have to navigate to the end manually
- Standard shell completion behavior positions cursor at end

## Proposed Fix

Two options:

**Option A:** Add parameter to `ik_input_buffer_set_text()` for cursor positioning:
- `cursor_at_end` boolean parameter
- When true, position cursor at end of new text
- When false, reset to position 0 (current behavior)

**Option B:** Add separate function `ik_input_buffer_set_text_cursor_end()`:
- Wraps `set_text()` then moves cursor to end
- Keeps existing function unchanged

**Option C:** Position cursor in `update_input_with_completion_selection()`:
- After calling `set_text()`, explicitly call cursor-to-end function
- Uses existing `ik_input_buffer_end()` or similar

Option C is least invasive if a cursor-to-end function already exists.

## Scope

- `src/input_buffer/core.c` - `ik_input_buffer_set_text()` or new function
- `src/repl_actions_completion.c` - `update_input_with_completion_selection()`
- Update tests for cursor position after completion

## Related

- Tab handling: `src/repl_actions_completion.c` (`ik_repl_handle_tab_action()`)
- Input buffer: `src/input_buffer/core.c`
