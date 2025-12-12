# Task: Fix Scrollback Rendering to Respect Physical Row Offset

## Target

Bug Fix: Mouse scroll within wrapped lines has no visual effect because `scrollback_render` ignores `start_row_offset`.

## Pre-read Skills
- .agents/skills/default.md
- .agents/skills/tdd.md
- .agents/skills/naming.md
- .agents/skills/style.md
- .agents/skills/errors.md

## Pre-read Docs
- docs/memory.md (talloc patterns)

## Pre-read Source
- src/layer_scrollback.c (the bug location - lines 56-97)
- src/scrollback.h (layout structures, API)
- src/scrollback.c (implementation patterns)
- src/utf8.h (UTF-8 width calculation)

## Pre-conditions
- Working tree is clean (`git status --porcelain` returns empty)
- `make check` passes

## Background

### The Bug

In `layer_scrollback.c`, `scrollback_render`:

```c
// Line 56-57: Gets physical row offset within logical line
res = ik_scrollback_find_logical_line_at_physical_row(scrollback, start_row,
                                                      &start_line_idx, &start_row_offset);

// Line 79-97: Renders full logical lines - IGNORES start_row_offset!
for (size_t i = start_line_idx; i <= end_line_idx; i++) {
    // ... renders entire logical line
}
```

When a logical line wraps across multiple physical rows, scrolling changes `first_visible_row` but the rendering always shows the full logical line. So scrolling within a wrapped line has no visual effect.

**Example:**
- Logical line 10 wraps into physical rows 115-119 (5 rows)
- Scroll to first_visible_row=118 → still renders full logical line 10
- Scroll to first_visible_row=117 → still renders full logical line 10
- Visual doesn't change until you scroll past the entire wrapped line

### The Fix

For the first logical line, skip rendering the first `start_row_offset` physical rows worth of characters.

This requires:
1. A way to find the byte offset at a given physical row boundary within a wrapped line
2. Modify `scrollback_render` to start rendering from that byte offset for the first line

### Layout Model

`ik_line_layout_t` stores:
- `display_width` - total display columns (UTF-8 aware)
- `physical_lines` - how many terminal rows (`ceil(display_width / terminal_width)`)

Physical row N within a logical line starts at approximately `N * terminal_width` display columns into the text.

## TDD Cycle

### Red

Add tests in `tests/unit/scrollback/scrollback_test.c`:

```c
// Test 1: Get byte offset at row 0 (should be 0)
START_TEST(test_get_byte_offset_at_row_zero)
{
    ik_scrollback_t *sb = ik_scrollback_create(ctx, 10);  // 10 cols wide
    // "Hello World!" = 12 chars, wraps to 2 rows at width 10
    ik_scrollback_append_line(sb, "Hello World!", 12);

    size_t byte_offset = 0;
    res_t res = ik_scrollback_get_byte_offset_at_row(sb, 0, 0, &byte_offset);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(byte_offset, 0);
}
END_TEST

// Test 2: Get byte offset at row 1 of wrapped line
START_TEST(test_get_byte_offset_at_row_one)
{
    ik_scrollback_t *sb = ik_scrollback_create(ctx, 10);
    // "Hello World!" wraps: "Hello Worl" (row 0), "d!" (row 1)
    ik_scrollback_append_line(sb, "Hello World!", 12);

    size_t byte_offset = 0;
    res_t res = ik_scrollback_get_byte_offset_at_row(sb, 0, 1, &byte_offset);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(byte_offset, 10);  // Start at "d!"
}
END_TEST

// Test 3: UTF-8 handling (multi-byte chars)
START_TEST(test_get_byte_offset_at_row_utf8)
{
    ik_scrollback_t *sb = ik_scrollback_create(ctx, 5);
    // "héllo" = 6 bytes (é is 2 bytes), 5 display cols, fits in 1 row
    // "héllo wörld" = 13 bytes, 11 display cols, wraps to 3 rows at width 5
    ik_scrollback_append_line(sb, "héllo wörld", 13);

    size_t byte_offset = 0;
    res_t res = ik_scrollback_get_byte_offset_at_row(sb, 0, 1, &byte_offset);
    ck_assert(is_ok(&res));
    // Row 0: "héllo" (6 bytes, 5 cols)
    // Row 1 starts at byte 6
    ck_assert_uint_eq(byte_offset, 6);
}
END_TEST

// Test 4: Row beyond line's physical rows returns error
START_TEST(test_get_byte_offset_at_row_out_of_range)
{
    ik_scrollback_t *sb = ik_scrollback_create(ctx, 10);
    ik_scrollback_append_line(sb, "Short", 5);  // 1 row

    size_t byte_offset = 0;
    res_t res = ik_scrollback_get_byte_offset_at_row(sb, 0, 1, &byte_offset);
    ck_assert(is_err(&res));
}
END_TEST

// Test 5: Wide characters (CJK) - each takes 2 columns
START_TEST(test_get_byte_offset_at_row_wide_chars)
{
    ik_scrollback_t *sb = ik_scrollback_create(ctx, 6);
    // "日本語" = 9 bytes, 6 display cols (each char is 3 bytes, 2 cols)
    // At width 6, fits in 1 row
    // "日本語x" = 10 bytes, 7 cols, wraps to 2 rows
    ik_scrollback_append_line(sb, "日本語x", 10);

    size_t byte_offset = 0;
    res_t res = ik_scrollback_get_byte_offset_at_row(sb, 0, 1, &byte_offset);
    ck_assert(is_ok(&res));
    // Row 0: "日本語" (9 bytes, 6 cols)
    // Row 1: "x" starts at byte 9
    ck_assert_uint_eq(byte_offset, 9);
}
END_TEST
```

Add integration test for the render fix:

```c
// Test that scrollback render respects start_row_offset
START_TEST(test_scrollback_layer_render_with_row_offset)
{
    // Setup: Create scrollback with a line that wraps
    ik_scrollback_t *sb = ik_scrollback_create(ctx, 10);
    ik_scrollback_append_line(sb, "AAAAAAAAAA", 10);  // Line 0: 1 row
    ik_scrollback_append_line(sb, "BBBBBBBBBBCCCCCCCCCC", 20);  // Line 1: 2 rows

    // Create layer
    ik_layer_t *layer = ik_scrollback_layer_create(ctx, "scrollback", sb);

    // Render starting at physical row 1 (middle of line 1)
    ik_output_buffer_t *output = ik_output_buffer_create(ctx, 256);
    layer->render(layer, output, 10, 1, 2);  // start_row=1, row_count=2

    // Should render "CCCCCCCCCC" (row 1 of line 1), not "BBBBBBBBBB..."
    // Note: render adds \r\n after each line
    ck_assert_str_eq(output->data, "CCCCCCCCCC\r\n");
}
END_TEST
```

Run `make check` - expect FAIL (new function doesn't exist, render doesn't use offset).

### Green

1. Add new function to `src/scrollback.h`:

```c
/**
 * @brief Get byte offset at a given physical row within a logical line
 *
 * For a wrapped line, finds the byte offset where the specified physical
 * row begins. Used for partial line rendering when scrolling.
 *
 * @param scrollback Scrollback buffer
 * @param line_index Logical line index (0-based)
 * @param row_offset Physical row within line (0 = first row)
 * @param byte_offset_out Pointer to receive byte offset
 * @return RES_OK on success, RES_ERR if row_offset >= line's physical_lines
 *
 * Assertions:
 * - scrollback must not be NULL
 * - line_index must be < line count
 * - byte_offset_out must not be NULL
 */
res_t ik_scrollback_get_byte_offset_at_row(ik_scrollback_t *scrollback,
                                           size_t line_index,
                                           size_t row_offset,
                                           size_t *byte_offset_out);
```

2. Implement in `src/scrollback.c`:

```c
res_t ik_scrollback_get_byte_offset_at_row(ik_scrollback_t *scrollback,
                                           size_t line_index,
                                           size_t row_offset,
                                           size_t *byte_offset_out)
{
    assert(scrollback != NULL);       // LCOV_EXCL_BR_LINE
    assert(line_index < scrollback->count);  // LCOV_EXCL_BR_LINE
    assert(byte_offset_out != NULL);  // LCOV_EXCL_BR_LINE

    // Row 0 always starts at byte 0
    if (row_offset == 0) {
        *byte_offset_out = 0;
        return OK(NULL);
    }

    // Check row_offset is valid
    if (row_offset >= scrollback->layouts[line_index].physical_lines) {
        return ERR(scrollback, INVALID, "row_offset exceeds line's physical rows");
    }

    // Get line text
    const char *text = scrollback->text_buffer + scrollback->text_offsets[line_index];
    size_t text_len = scrollback->text_lengths[line_index];
    int32_t terminal_width = scrollback->cached_width;

    // Target column = row_offset * terminal_width
    size_t target_col = row_offset * (size_t)terminal_width;

    // Walk UTF-8 text counting display columns until we reach target
    size_t byte_pos = 0;
    size_t col = 0;

    while (byte_pos < text_len && col < target_col) {
        // Get width of current character
        int32_t char_width = ik_utf8_display_width(text + byte_pos, text_len - byte_pos);
        if (char_width < 0) {
            // Invalid UTF-8 - treat as 1 byte, 1 col
            char_width = 1;
            byte_pos++;
        } else {
            // Advance by character's byte length
            size_t char_bytes = ik_utf8_char_length((uint8_t)text[byte_pos]);
            byte_pos += char_bytes;
        }
        col += (size_t)char_width;
    }

    *byte_offset_out = byte_pos;
    return OK(NULL);
}
```

3. Modify `scrollback_render` in `src/layer_scrollback.c`:

```c
static void scrollback_render(const ik_layer_t *layer,
                              ik_output_buffer_t *output,
                              size_t width,
                              size_t start_row,
                              size_t row_count)
{
    // ... existing setup code ...

    // Find which logical lines correspond to the requested physical rows
    size_t start_line_idx, start_row_offset;
    res_t res = ik_scrollback_find_logical_line_at_physical_row(scrollback, start_row,
                                                                &start_line_idx, &start_row_offset);
    if (!is_ok(&res)) {
        return;
    }

    // ... find end_line_idx code ...

    // Render logical lines from start_line_idx to end_line_idx (inclusive)
    for (size_t i = start_line_idx; i <= end_line_idx; i++) {
        const char *line_text = NULL;
        size_t line_len = 0;
        res = ik_scrollback_get_line_text(scrollback, i, &line_text, &line_len);
        (void)res;

        // For first line, skip bytes for start_row_offset
        size_t render_start = 0;
        if (i == start_line_idx && start_row_offset > 0) {
            res = ik_scrollback_get_byte_offset_at_row(scrollback, i, start_row_offset, &render_start);
            if (is_err(&res)) {
                render_start = 0;  // Fallback to full line
            }
        }

        // Copy line text from render_start, converting \n to \r\n
        for (size_t j = render_start; j < line_len; j++) {
            if (line_text[j] == '\n') {
                ik_output_buffer_append(output, "\r\n", 2);
            } else {
                ik_output_buffer_append(output, &line_text[j], 1);
            }
        }

        // Add \r\n at end of each line
        ik_output_buffer_append(output, "\r\n", 2);
    }
}
```

4. Run `make check` - expect PASS.

### Verify

1. `make check` - all tests pass
2. `make lint` - complexity checks pass
3. Manual test:
   - Run `bin/ikigai`
   - Generate long wrapped content (paste a long paragraph or code block)
   - Mouse wheel scroll
   - Verify smooth per-row scrolling (visual changes with each scroll notch)
   - Verify no visual glitches at wrap boundaries

## Post-conditions
- Working tree is clean (all changes committed)

- `make check` passes
- Mouse scroll within wrapped lines produces visual change
- Per-physical-row scrolling works smoothly
- UTF-8 and wide characters handle correctly
- New function `ik_scrollback_get_byte_offset_at_row` has tests
- Integration test verifies layer rendering with row offset

## Notes

### Why Not Cache Row Byte Offsets?

We could cache byte offsets for each row boundary in `ik_line_layout_t`, but:
- Only needed for first visible line during render
- Lines are typically short (< 1KB)
- UTF-8 walk is O(n) but n is small
- Avoids memory overhead for every line

### Edge Cases

- Empty lines: `row_offset` can only be 0, byte_offset = 0
- Single-row lines: `row_offset` can only be 0
- Lines ending exactly at row boundary: handled naturally by column counting
