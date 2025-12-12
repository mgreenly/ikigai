# Task: Fix Scrollback Line Counting for Embedded Newlines

## Target

Bug Fix: Cursor position and input rendering incorrect due to scrollback physical line miscounting

## Pre-read Skills
- .agents/skills/default.md
- .agents/skills/tdd.md
- .agents/skills/naming.md
- .agents/skills/style.md
- .agents/skills/errors.md

## Pre-read Docs
- docs/architecture.md (scrollback, viewport calculation)

## Pre-read Source (patterns)
- src/scrollback.c (line counting logic)
- src/scrollback.h (ik_line_layout_t structure)
- src/repl_viewport.c (viewport calculation using scrollback counts)

## Pre-conditions
- Working tree is clean (`git status --porcelain` returns empty)
- `make check` passes
- Bug present: cursor appears below lower separator depending on scrollback content

## Problem Analysis

### Symptom
When launching ikigai, the cursor position and input rendering are wrong depending on scrollback content. Debug info shows impossible values like `row=13 h=12` (row should be within height bounds).

### Root Cause
`ik_scrollback_ensure_layout()` recalculates physical lines using only `display_width`:

```c
physical_lines = (display_width + terminal_width - 1) / terminal_width;
```

This ignores embedded newlines. A line like "Line1\nLine2\nLine3" has `display_width` summed across all segments, but needs 3+ rows minimum (one per newline-delimited segment).

In contrast, `ik_scrollback_append_line()` correctly handles newlines by iterating through text and counting rows per segment.

### Impact
- Always happens, severity varies by content
- More newlines in scrollback = larger counting error
- Cursor positioned incorrectly (below lower separator)
- Viewport calculations use wrong document height

## Task

Fix `ik_scrollback_ensure_layout()` to correctly count physical lines when text contains embedded newlines.

**Solution options:**

1. **Store segment widths** - Add `newline_count` and `segment_widths[]` to `ik_line_layout_t`, populated during `append_line`, used during `ensure_layout`

2. **Rescan text** - Have `ensure_layout` rescan text for newlines (loses O(1) claim but simpler)

3. **Store newline positions** - Store byte offsets of newlines, calculate segment widths on demand

## TDD Cycle

### Red

1. Create test `tests/unit/scrollback/scrollback_newline_reflow_test.c`:
   - Create scrollback with width=80
   - Append line with embedded newlines: "Line1\nLine2\nLine3"
   - Verify `physical_lines` == 3 (one per segment)
   - Call `ensure_layout(scrollback, 40)` (narrower width)
   - Verify `physical_lines` still >= 3 (segments still need at least 1 row each)
   - If any segment is longer than 40 cols, expect more rows

2. Add test for trailing newline:
   - Append "content\n" (ends with newline)
   - Verify trailing empty line is counted
   - Resize and verify count preserved

3. Run `make check` - expect FAIL (ensure_layout undercounts)

### Green

1. Modify `ik_line_layout_t` in `src/scrollback.h`:
   ```c
   typedef struct {
       size_t display_width;
       size_t physical_lines;
       size_t newline_count;
       size_t *segment_widths;  // Array of widths between newlines
   } ik_line_layout_t;
   ```

2. Update `ik_scrollback_append_line()` in `src/scrollback.c`:
   - Count newlines in first pass
   - Allocate `segment_widths` array (newline_count + 1 segments)
   - Store each segment's display width as you scan
   - Store `newline_count` in layout

3. Update `ik_scrollback_ensure_layout()`:
   ```c
   for (size_t i = 0; i < scrollback->count; i++) {
       size_t segment_count = layouts[i].newline_count + 1;
       size_t *seg_widths = layouts[i].segment_widths;
       size_t physical_lines = 0;

       for (size_t seg = 0; seg < segment_count; seg++) {
           if (seg_widths[seg] == 0) {
               physical_lines += 1;  // Empty segment = 1 row
           } else {
               physical_lines += (seg_widths[seg] + width - 1) / width;
           }
       }
       layouts[i].physical_lines = physical_lines;
   }
   ```

4. Run `make check` - expect PASS

### Verify

1. Run `make check` - all tests pass
2. Run `make lint` - complexity checks pass
3. Manual test:
   - Run `bin/ikigai`
   - Resize terminal to various heights (12, 13, 20, etc.)
   - Verify cursor always appears on input line between separators
   - Verify debug info shows `row < h` always
   - Test with different scrollback content (multi-line responses, code blocks)

## Post-conditions

- `make check` passes
- Cursor always renders correctly regardless of scrollback content
- `ensure_layout` correctly counts physical lines with embedded newlines
- No regression in existing scrollback tests
- Working tree is clean (all changes committed)
