# Scrollback Layer Viewport Bug

## Problem Summary

Mouse wheel scrolling works for the first few scroll notches, then stops updating the display even though the viewport_offset continues to increment correctly. The issue is symmetric - scrolling back "replays" the invisible scrolls before the display starts updating again.

## Root Cause

In `src/layer_scrollback.c`, the `scrollback_render()` function finds the logical line at a physical row but **ignores the row offset within wrapped lines**.

```c
// Line 55-57: Finds start_row_offset but never uses it!
size_t start_line_idx, start_row_offset;
res_t res = ik_scrollback_find_logical_line_at_physical_row(scrollback, start_row,
                                                            &start_line_idx, &start_row_offset);
```

When a logical line wraps across multiple physical rows:
- Physical row 126 might be row 2 of logical line 50
- Physical row 127 might be row 3 of logical line 50

But the render just outputs complete logical lines (lines 79-97), so both viewport positions render the same output.

## Fix Required

Modify `scrollback_render()` in `src/layer_scrollback.c` to:
1. For the first logical line, skip `start_row_offset` wrapped rows
2. For the last logical line, only render up to `end_row_offset` wrapped rows
3. Handle the physical row boundaries correctly when outputting wrapped content

## Relevant Files

- `src/layer_scrollback.c` - The bug is here, in `scrollback_render()`
- `src/scrollback.c` - Contains `ik_scrollback_find_logical_line_at_physical_row()` which correctly returns the offset
- `src/repl_actions_viewport.c` - Scroll handlers (working correctly, has debug logging)
- `src/repl_viewport.c` - Calculates first_visible_row (working correctly)

## Current State

- Scroll detector correctly emits SCROLL_UP/SCROLL_DOWN (1 per wheel notch)
- viewport_offset increments/decrements correctly
- first_visible_row is calculated correctly
- Layer cake passes correct start_row and row_count to layer render
- BUG: scrollback layer ignores start_row_offset when rendering wrapped lines

## Debug Logging

Current logging in `src/repl_actions_viewport.c` shows:
```json
{"event": "scroll_up", "old": 4, "new": 5, "max": 131, "doc_height": 179, "term_rows": 48, "first_visible": 126}
```

The values are correct - the render just doesn't reflect them for wrapped lines.

## Test Case

1. Start ikigai with a session that has long wrapped lines in scrollback
2. Scroll up slowly - first few scrolls work
3. When viewport enters a wrapped line, scrolling appears to stop
4. Continue scrolling - offset still increments (visible in logs)
5. Scroll back down - first scroll(s) don't visually change, then it starts working

## Related Context

- Mode 1007h converts mouse wheel to arrow key bursts (3 arrows per notch on Ghostty)
- Scroll detector distinguishes wheel (rapid bursts) from keyboard (single arrows with timeout)
- IK_SCROLL_RESULT_ABSORBED added to prevent viewport_offset reset on absorbed arrows
