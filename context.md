# Viewport Rendering Bug - Debug Context

## Problem Summary

Terminal rendering breaks at specific terminal heights. The bug manifests differently depending on terminal row count:

- **h=27,28**: Renders correctly - scrollback, upper separator, input line with cursor, lower separator
- **h=29**: Extra blank line appears between separators, cursor on lower separator (wrong)
- **h=30+**: More blank lines, cursor displaced further down (1 extra row per terminal line above 28)

## Key Observation

The magic number is **28**. Something changes at exactly h=29 that causes off-by-one row displacement that accumulates.

## Debug Info Added

Upper separator now shows: `off=X row=Y h=Z doc=W sb=S`
- `off`: viewport_offset (scroll position, 0 = bottom)
- `row`: first_visible_row in document
- `h`: terminal height (rows)
- `doc`: total document height
- `sb`: scrollback physical rows (doc - 3)

From user testing:
- doc=167 constant
- At h=27: row=140
- At h=28: row=139
- At h=29: row=138
- The viewport calculations appear CORRECT, bug is in RENDERING

## Architecture

### Layer System

Layers render in order (src/repl_init.c:109-126):
1. Scrollback (src/layer_scrollback.c)
2. Spinner (usually invisible)
3. Upper separator (src/layer_separator.c)
4. Input buffer (src/layer_input.c)
5. Lower separator
6. Completion

Each layer:
- `get_height()` returns physical rows it occupies
- `render()` outputs content to buffer, receives `start_row` and `row_count`

### Layer Cake (src/layer.c:140-185)

Iterates layers, calculates visible portion, calls render with clipped row range.

### Viewport Calculation (src/repl_viewport.c)

```c
document_height = scrollback_rows + 1 + input_buffer_display_rows + 1;
// +1 for upper separator, +1 for lower separator

if (document_height <= terminal_rows) {
    first_visible_row = 0;  // Everything fits
} else {
    // Scrolling needed
    last_visible_row = document_height - 1 - offset;
    first_visible_row = last_visible_row + 1 - terminal_rows;
}
```

### Trailing \r\n Removal (src/repl_viewport.c:216-220)

```c
// Bug fix: When rendered content fills the terminal, the trailing \r\n
// causes the terminal to scroll up by 1 row.
if (document_height >= terminal_rows && output->size >= 2) {
    if (output->data[output->size - 2] == '\r' && output->data[output->size - 1] == '\n') {
        output->size -= 2;
    }
}
```

## Recent Changes (Potentially Related)

Commits f153255 and d095957 added partial line rendering to scrollback:
- `ik_scrollback_get_byte_offset_at_display_col()` - convert display column to byte offset
- Modified `scrollback_render()` to use `start_row_offset` and `end_row_offset`
- Only adds `\r\n` when `is_line_end = true` (rendered to end of logical line)

These changes are in src/layer_scrollback.c lines 88-128.

## Hypotheses to Investigate

### 1. Scrollback Partial Line \r\n Logic

When scrollback renders the last visible line:
- If it's a complete logical line, adds `\r\n`
- If stopping mid-line, no `\r\n`

The `is_line_end` calculation (lines 102-114) might be wrong in certain viewport positions:
```c
if (i == end_line_idx) {
    size_t line_physical_rows = scrollback->layouts[i].physical_lines;
    if (end_row_offset + 1 < line_physical_rows) {
        is_line_end = false;
    }
}
```

### 2. Empty Input Layer Output

When input is empty, it outputs `\r\n` (line 78-81 in layer_input.c). This should create 1 row. But:
- Removing it causes separators to be adjacent (no cursor space)
- Keeping it causes extra blank line at h>=29

### 3. Layer Height vs Output Mismatch

Each layer reports a height, but if render outputs different number of rows than claimed height, subsequent cursor positioning breaks.

### 4. Trailing \r\n Removal Interaction

The removal logic only removes ONE trailing `\r\n`. If multiple layers each add `\r\n` at the end, only the last one is removed. This could cause issues when combined with partial line rendering.

### 5. Wrapped Line at Specific Position

Maybe at h=29, a wrapped scrollback line falls at a boundary that triggers wrong `is_line_end` calculation.

## Key Files

| File | What |
|------|------|
| src/layer_scrollback.c | Scrollback render with partial line logic (THE LIKELY BUG LOCATION) |
| src/layer_input.c | Input layer, empty input handling |
| src/layer_separator.c | Separator with debug info display |
| src/layer.c | Layer cake orchestration |
| src/repl_viewport.c | Viewport calculation, cursor positioning, trailing \r\n removal |
| src/scrollback.c | `ik_scrollback_get_byte_offset_at_display_col()` |

## Tests

- tests/unit/layer/scrollback_layer_test.c - Has partial line tests that PASS
- tests/unit/repl/repl_partial_line_viewport_test.c - Viewport tests

## How to Debug

1. Run `bin/ikigai` with scrollback content
2. Resize terminal to different heights (27, 28, 29, 30, 31)
3. Observe separator debug info and cursor position
4. The bug: cursor should be on blank line between separators, but at h>=29 it's displaced

## Possible Fixes to Try

1. **Add debug to layer_cake_render** to log start_row/row_count passed to each layer
2. **Check if scrollback last line is wrapped** at the h=28->29 transition
3. **Verify scrollback output byte count** matches expected row count
4. **Trace the complete output buffer** at h=28 vs h=29 to find the difference
5. **Check if document_height calculation is off** when scrollback has certain wrapped lines

## Current State

- Debug info added to separator (uncommitted)
- Input layer back to outputting `\r\n` for empty (the blank line bug is active)
- Need to find why h>=29 behaves differently from h<=28

## Commands

```bash
make                    # Build
./bin/ikigai           # Run (needs scrollback content)
make check             # Run all tests
```
