# Task: Fix Scrollback Partial Line Rendering for Embedded Newlines

## Target

Bug Fix: Scrollback rendering skips content when viewport starts mid-line with embedded newlines

## Pre-read Skills
- .agents/skills/default.md
- .agents/skills/tdd.md
- .agents/skills/scm.md
- .agents/skills/naming.md
- .agents/skills/style.md
- .agents/skills/errors.md

## Pre-read Docs
- docs/architecture.md (scrollback, viewport, layer system)

## Pre-read Source (patterns)
- src/layer_scrollback.c (the buggy rendering code)
- src/scrollback.c (ik_scrollback_get_byte_offset_at_display_col, segment_widths)
- src/scrollback.h (ik_line_layout_t with newline_count and segment_widths)

## Pre-conditions
- Working tree is clean (`git status --porcelain` returns empty)
- `make check` passes
- Bug present: blank lines appear below lower separator when scrollback contains embedded newlines

## Problem Analysis

### Symptom
When scrollback content contains embedded `\n` characters (multi-line LLM responses, code blocks), blank lines appear below the lower separator and cursor is positioned incorrectly. The number of blank lines equals the number of embedded newlines in the scrollback content.

### Debug Info
Separator debug shows values like:
- `off=0 row=13 h=12 doc=25 sb=22`

The math is correct (doc=25, h=12, so row=13 is valid for showing bottom of document). But rendered output has 3 blank lines, indicating only 9 rows rendered instead of 12.

### Root Cause

In `src/layer_scrollback.c` lines 94-100:

```c
// For first line: skip start_row_offset worth of display columns
if (i == start_line_idx && start_row_offset > 0) {
    size_t skip_cols = start_row_offset * width;
    res = ik_scrollback_get_byte_offset_at_display_col(scrollback, i, skip_cols, &render_start);
    ...
}
```

This calculates `skip_cols = start_row_offset * width`, assuming every physical row is exactly `width` display columns. But when a row ends with `\n`, it's shorter than `width` columns.

Then `ik_scrollback_get_byte_offset_at_display_col()` (src/scrollback.c:449) skips newlines without counting them:

```c
// Skip newlines (they don't contribute to display width)
if (cp == '\n') {
    pos += (size_t)bytes;
    continue;
}
```

**Example:** Text "Hello\nWorld" at width=80:
- Row 0: "Hello" (5 cols) + newline
- Row 1: "World" (5 cols)
- `physical_lines = 2`

To skip row 0, caller asks for `skip_cols = 1 * 80 = 80`. But the function walks:
- 'H','e','l','l','o' → col=5
- '\n' → **skipped, col stays 5**
- 'W','o','r','l','d' → col=10

Never reaches col=80, returns end of string. Entire content skipped.

### Impact
- N embedded newlines = N rows of content "lost"
- Blank lines appear at bottom of viewport
- Cursor positioned incorrectly (offset by N rows)
- Bug only manifests when viewport starts partway into a logical line with embedded newlines

## Task

Fix `layer_scrollback.c` to correctly calculate byte offsets when skipping physical rows that contain embedded newlines.

**Solution:** Use `layouts[i].segment_widths[]` (added by previous fix) to calculate the actual byte offset. Instead of `skip_cols = start_row_offset * width`, walk through segments and their wrapped rows to find the correct starting byte position.

## TDD Cycle

### Red

1. Create test `tests/unit/layer/layer_scrollback_partial_newline_test.c`:
   - Create scrollback with width=80
   - Append line with embedded newlines: "Line1\nLine2\nLine3" (3 segments)
   - Create layer and output buffer
   - Call `scrollback_render()` with `start_row=1, row_count=2` (skip first segment)
   - Verify output contains "Line2\r\nLine3\r\n" (not empty or partial)

2. Add test for wrapped segment + newline:
   - Append line: "A" * 100 + "\nShort" (first segment wraps to 2 rows at width=80)
   - Render with `start_row=1, row_count=2` (skip first wrapped row)
   - Verify output contains remainder of first segment plus "Short"

3. Add test for skipping multiple newline segments:
   - Append line: "A\nB\nC\nD"
   - Render with `start_row=2, row_count=2` (skip A and B)
   - Verify output contains "C\r\nD\r\n"

4. Run `make check` - expect FAIL (current code produces wrong output)

### Green

1. In `src/layer_scrollback.c`, replace the skip_cols calculation with segment-aware logic:

   ```c
   // For first line: find byte offset for start_row_offset using segment widths
   if (i == start_line_idx && start_row_offset > 0) {
       // Use segment_widths to find correct byte offset
       size_t rows_to_skip = start_row_offset;
       size_t segment_count = scrollback->layouts[i].newline_count + 1;
       size_t *seg_widths = scrollback->layouts[i].segment_widths;
       size_t cols_to_skip = 0;

       for (size_t seg = 0; seg < segment_count && rows_to_skip > 0; seg++) {
           size_t seg_rows = (seg_widths[seg] == 0) ? 1
               : (seg_widths[seg] + width - 1) / width;

           if (rows_to_skip >= seg_rows) {
               // Skip entire segment
               cols_to_skip += seg_widths[seg];
               cols_to_skip++;  // +1 for the newline character itself
               rows_to_skip -= seg_rows;
           } else {
               // Skip partial segment (wrapped rows)
               cols_to_skip += rows_to_skip * width;
               rows_to_skip = 0;
           }
       }

       res = ik_scrollback_get_byte_offset_at_display_col(scrollback, i, cols_to_skip, &render_start);
       ...
   }
   ```

2. Apply similar fix for `render_end` calculation (lines 103-114) if stopping mid-line.

3. Run `make check` - expect PASS

### Verify

1. Run `make check` - all tests pass
2. Run `make lint` - complexity checks pass
3. Manual test:
   - Run `bin/ikigai`
   - Have conversation with multi-line responses (code blocks, lists)
   - Resize terminal to various small heights (10, 12, 15 rows)
   - Verify no blank lines appear below lower separator
   - Verify cursor positioned correctly on input line
   - Scroll up/down and verify content renders correctly

## Post-conditions

- `make check` passes
- No blank lines below lower separator regardless of scrollback content
- Cursor always positioned correctly
- Partial line rendering works with embedded newlines
- No regression in existing scrollback/layer tests
- Working tree is clean (all changes committed)

## Related

- Previous fix: `scrollback-newline-counting-bug.md` - added `segment_widths` to layout
- This fix uses that data to correct the rendering path
