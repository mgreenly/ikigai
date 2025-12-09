# Mouse Scroll Bug Investigation - Current State

## Problem Summary

Mouse wheel scrolling works partially but stops having visual effect when scrolling within a wrapped line.

## Root Cause Chain

### 1. Scroll Detection (FIXED)
- Terminal mode 1007 converts mouse wheel to arrow escape sequences
- Mouse wheel sends 2 rapid ARROW_UP/DOWN events per notch (~0-1ms apart)
- `scroll_accumulator.c` uses deferred detection to distinguish wheel from keyboard:
  - First arrow → buffered (NONE)
  - Second arrow within 10ms → SCROLL_UP/DOWN emitted
  - Both arrows consumed, pending cleared

### 2. Event Routing (FIXED)
- `repl_actions.c` intercepts arrow events, routes through scroll_accumulator
- SCROLL_UP/DOWN → calls scroll handler → increments viewport_offset
- Timeout handler in `repl.c` calls arrow handlers directly (not through scroll_acc)

### 3. Viewport Calculation (WORKING)
- `repl_viewport.c` calculates `first_visible_row` based on viewport_offset
- Log shows first_visible correctly decrements: 120 → 119 → 118 → 117 → 116 → 115

### 4. Scrollback Rendering (BUG HERE)
**File:** `src/layer_scrollback.c`, function `scrollback_render`

```c
// Line 56-57: Gets physical row offset within logical line
res = ik_scrollback_find_logical_line_at_physical_row(scrollback, start_row,
                                                      &start_line_idx, &start_row_offset);

// Line 79-97: Renders full logical lines - IGNORES start_row_offset!
for (size_t i = start_line_idx; i <= end_line_idx; i++) {
    // ... renders entire logical line
}
```

**The bug:** When a logical line wraps across multiple physical rows, scrolling changes `first_visible_row` but the rendering always shows the full logical line. So scrolling within a wrapped line has no visual effect.

**Example:**
- Logical line 10 wraps into physical rows 115-119 (5 rows)
- Scroll to first_visible=118 → still renders full logical line 10
- Scroll to first_visible=117 → still renders full logical line 10
- Scroll to first_visible=116 → still renders full logical line 10
- Visual doesn't change until you scroll past the entire wrapped line

## Fix Options

### Option 1: Per-logical-line scrolling (simpler)
- Accept that scroll granularity is per-logical-line
- Modify viewport_offset increments to jump by full logical lines
- Simpler but less smooth scrolling

### Option 2: Per-physical-row scrolling (proper)
- Modify `scrollback_render` to skip `start_row_offset` physical rows from first line
- Need to track character positions for wrapped lines
- More complex but smoother scrolling

## Files Modified (with debug logging)

- `src/scroll_accumulator.c` - Fixed: don't buffer after SCROLL
- `src/repl_actions.c` - Fixed: removed viewport_offset=0 on NONE; added logging
- `src/repl.c` - Fixed: call arrow handlers directly from timeout
- `src/repl_actions_viewport.c` - Added scroll_up logging
- `src/repl_viewport.c` - Added render_viewport logging

## Debug Logging Added

```json
{"event":"input_parsed","type":"ARROW_UP"}
{"event":"scroll_acc_result","result":"SCROLL_UP","now_ms":...}
{"event":"scroll_up","max_offset":120,"old":4,"new":5}
{"event":"render_viewport","viewport_offset":5,"max_offset":120,"first_visible":115}
```

## Next Step

Decide on fix approach for `layer_scrollback.c` rendering.
