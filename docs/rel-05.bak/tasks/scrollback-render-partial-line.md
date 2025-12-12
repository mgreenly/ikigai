# Task: Fix Scrollback Render to Handle Partial Lines

## Target

Bug Fix: Mouse scroll within wrapped lines now updates the display correctly.

## Pre-read Skills
- .agents/skills/default.md
- .agents/skills/tdd.md
- .agents/skills/naming.md
- .agents/skills/style.md
- .agents/skills/errors.md
- .agents/skills/scm.md

## Pre-read Docs
- docs/memory.md (talloc patterns)

## Pre-read Source
- src/layer_scrollback.c (THE BUG IS HERE - `scrollback_render()` ignores row offsets)
- src/scrollback.c (`ik_scrollback_get_byte_offset_at_display_col()` - from previous task)
- src/scrollback.h (API reference)
- tests/unit/layer/scrollback_layer_test.c (existing layer tests)

## Pre-conditions
- Working tree is clean (`git status --porcelain` returns empty)
- `make check` passes
- `ik_scrollback_get_byte_offset_at_display_col()` exists (from previous task)

## Background

### The Bug

In `src/layer_scrollback.c`, `scrollback_render()` correctly computes `start_row_offset` and `end_row_offset` but **never uses them**:

```c
// Lines 55-57: Correctly finds row offset within logical line
size_t start_line_idx, start_row_offset;
res_t res = ik_scrollback_find_logical_line_at_physical_row(scrollback, start_row,
                                                            &start_line_idx, &start_row_offset);

// Lines 79-97: BUG! Renders COMPLETE logical lines, ignoring offsets
for (size_t i = start_line_idx; i <= end_line_idx; i++) {
    // ...
    for (size_t j = 0; j < line_len; j++) {  // <-- Starts at 0, not at row offset!
        // ...
    }
    ik_output_buffer_append(output, "\r\n", 2);  // <-- Always adds newline!
}
```

**Result:** When viewport starts mid-wrapped-line, it still renders the full logical line. Scrolling by one physical row has no visual effect until you scroll past the entire wrapped line.

### The Fix

1. For **first line** (`i == start_line_idx`): Skip `start_row_offset * width` display columns
2. For **last line** (`i == end_line_idx`): Only render up to `(end_row_offset + 1) * width` display columns
3. Only add `\r\n` when we render to the **end** of a logical line (not when we stop mid-line)

### Column Calculation

```c
// First line: skip these many display columns
size_t skip_cols = start_row_offset * (size_t)terminal_width;

// Last line: stop at this display column
size_t stop_cols = (end_row_offset + 1) * (size_t)terminal_width;
```

Convert display columns to byte offsets using `ik_scrollback_get_byte_offset_at_display_col()`.

## TDD Cycle

### Red

Add tests to `tests/unit/layer/scrollback_layer_test.c`:

```c
// Test: Render starting from middle of wrapped line
START_TEST(test_scrollback_layer_render_partial_start)
{
    TALLOC_CTX *ctx = talloc_new(NULL);

    // Create scrollback with width 10
    ik_scrollback_t *scrollback = ik_scrollback_create(ctx, 10);

    // Line 0: "Short" (1 row)
    ik_scrollback_append_line(scrollback, "Short", 5);

    // Line 1: "AAAAAAAAAA" + "BBBBBBBBBB" (20 chars = 2 rows at width 10)
    ik_scrollback_append_line(scrollback, "AAAAAAAAAABBBBBBBBBB", 20);

    ik_layer_t *layer = ik_scrollback_layer_create(ctx, "scrollback", scrollback);
    ik_output_buffer_t *output = ik_output_buffer_create(ctx, 256);

    // Render starting at physical row 2 (second row of line 1: "BBBBBBBBBB")
    layer->render(layer, output, 10, 2, 1);

    // Should render "BBBBBBBBBB" + \r\n (because it's end of logical line)
    ck_assert_uint_eq(output->size, 12);  // 10 chars + \r\n
    ck_assert(memcmp(output->data, "BBBBBBBBBB\r\n", 12) == 0);

    talloc_free(ctx);
}
END_TEST

// Test: Render ending in middle of wrapped line (no trailing \r\n)
START_TEST(test_scrollback_layer_render_partial_end)
{
    TALLOC_CTX *ctx = talloc_new(NULL);

    ik_scrollback_t *scrollback = ik_scrollback_create(ctx, 10);

    // Line 0: 30 chars = 3 rows at width 10
    ik_scrollback_append_line(scrollback, "AAAAAAAAAABBBBBBBBBBCCCCCCCCCC", 30);

    ik_layer_t *layer = ik_scrollback_layer_create(ctx, "scrollback", scrollback);
    ik_output_buffer_t *output = ik_output_buffer_create(ctx, 256);

    // Render only first 2 rows (ending mid-line, before "CCCCCCCCCC")
    layer->render(layer, output, 10, 0, 2);

    // Should render "AAAAAAAAAA" + "BBBBBBBBBB" with NO trailing \r\n
    // (we stopped mid-logical-line)
    ck_assert_uint_eq(output->size, 20);  // 20 chars, no \r\n
    ck_assert(memcmp(output->data, "AAAAAAAAAABBBBBBBBBB", 20) == 0);

    talloc_free(ctx);
}
END_TEST

// Test: Render middle portion of wrapped line (skip start, stop before end)
START_TEST(test_scrollback_layer_render_partial_middle)
{
    TALLOC_CTX *ctx = talloc_new(NULL);

    ik_scrollback_t *scrollback = ik_scrollback_create(ctx, 10);

    // Line 0: 40 chars = 4 rows at width 10
    ik_scrollback_append_line(scrollback, "AAAAAAAAAABBBBBBBBBBCCCCCCCCCCDDDDDDDDDD", 40);

    ik_layer_t *layer = ik_scrollback_layer_create(ctx, "scrollback", scrollback);
    ik_output_buffer_t *output = ik_output_buffer_create(ctx, 256);

    // Render rows 1-2 (skip AAAA, stop before DDDD)
    layer->render(layer, output, 10, 1, 2);

    // Should render "BBBBBBBBBB" + "CCCCCCCCCC" with NO trailing \r\n
    ck_assert_uint_eq(output->size, 20);
    ck_assert(memcmp(output->data, "BBBBBBBBBBCCCCCCCCCC", 20) == 0);

    talloc_free(ctx);
}
END_TEST

// Test: Render with UTF-8 content (multi-byte chars)
START_TEST(test_scrollback_layer_render_partial_utf8)
{
    TALLOC_CTX *ctx = talloc_new(NULL);

    ik_scrollback_t *scrollback = ik_scrollback_create(ctx, 5);

    // "cafe monde" with accented chars
    // "cafe " (6 bytes, 5 cols) + "monde" (5 bytes, 5 cols) = 11 bytes, 10 cols = 2 rows
    ik_scrollback_append_line(scrollback, "caf\xc3\xa9 monde", 11);

    ik_layer_t *layer = ik_scrollback_layer_create(ctx, "scrollback", scrollback);
    ik_output_buffer_t *output = ik_output_buffer_create(ctx, 256);

    // Render second row only ("monde")
    layer->render(layer, output, 5, 1, 1);

    // Should render "monde\r\n" (end of logical line)
    ck_assert_uint_eq(output->size, 7);  // 5 + \r\n
    ck_assert(memcmp(output->data, "monde\r\n", 7) == 0);

    talloc_free(ctx);
}
END_TEST

// Test: Render with ANSI escape sequences
START_TEST(test_scrollback_layer_render_partial_ansi)
{
    TALLOC_CTX *ctx = talloc_new(NULL);

    ik_scrollback_t *scrollback = ik_scrollback_create(ctx, 10);

    // "\x1b[31m" + "AAAAAAAAAA" + "\x1b[0m" + "BBBBBBBBBB"
    // ANSI: 5 bytes, text: 10, ANSI: 4, text: 10 = 29 bytes, 20 display cols = 2 rows
    ik_scrollback_append_line(scrollback, "\x1b[31mAAAAAAAAAA\x1b[0mBBBBBBBBBB", 29);

    ik_layer_t *layer = ik_scrollback_layer_create(ctx, "scrollback", scrollback);
    ik_output_buffer_t *output = ik_output_buffer_create(ctx, 256);

    // Render second row ("BBBBBBBBBB")
    layer->render(layer, output, 10, 1, 1);

    // Should render the reset ANSI + "BBBBBBBBBB\r\n"
    // Note: We start at byte offset after first 10 display cols
    // The ANSI reset (\x1b[0m) comes before BBBB, so it should be included
    ck_assert(output->size > 0);
    ck_assert(strstr((char *)output->data, "BBBBBBBBBB") != NULL);

    talloc_free(ctx);
}
END_TEST

// Test: Single-row line doesn't break when rendered partially
START_TEST(test_scrollback_layer_render_single_row_line)
{
    TALLOC_CTX *ctx = talloc_new(NULL);

    ik_scrollback_t *scrollback = ik_scrollback_create(ctx, 80);
    ik_scrollback_append_line(scrollback, "Short line", 10);

    ik_layer_t *layer = ik_scrollback_layer_create(ctx, "scrollback", scrollback);
    ik_output_buffer_t *output = ik_output_buffer_create(ctx, 256);

    // Render the single row
    layer->render(layer, output, 80, 0, 1);

    // Should render "Short line\r\n"
    ck_assert_uint_eq(output->size, 12);
    ck_assert(memcmp(output->data, "Short line\r\n", 12) == 0);

    talloc_free(ctx);
}
END_TEST

// Test: Multiple lines with first partial, last partial
START_TEST(test_scrollback_layer_render_multiple_lines_partial)
{
    TALLOC_CTX *ctx = talloc_new(NULL);

    ik_scrollback_t *scrollback = ik_scrollback_create(ctx, 10);

    // Line 0: 20 chars = 2 rows
    ik_scrollback_append_line(scrollback, "AAAAAAAAAABBBBBBBBBB", 20);
    // Line 1: 10 chars = 1 row
    ik_scrollback_append_line(scrollback, "CCCCCCCCCC", 10);
    // Line 2: 20 chars = 2 rows
    ik_scrollback_append_line(scrollback, "DDDDDDDDDDEEEEEEEEEE", 20);

    // Physical rows: 0=AAAA, 1=BBBB, 2=CCCC, 3=DDDD, 4=EEEE

    ik_layer_t *layer = ik_scrollback_layer_create(ctx, "scrollback", scrollback);
    ik_output_buffer_t *output = ik_output_buffer_create(ctx, 256);

    // Render rows 1-3 (BBBB + CCCC + DDDD)
    layer->render(layer, output, 10, 1, 3);

    // BBBB\r\n (end of line 0) + CCCC\r\n (complete line 1) + DDDD (partial line 2)
    // = 10+2 + 10+2 + 10 = 34 bytes
    const char *expected = "BBBBBBBBBB\r\nCCCCCCCCCC\r\nDDDDDDDDDD";
    ck_assert_uint_eq(output->size, 34);
    ck_assert(memcmp(output->data, expected, 34) == 0);

    talloc_free(ctx);
}
END_TEST
```

Run `make check` - expect FAIL (render still outputs complete lines).

### Green

Modify `scrollback_render()` in `src/layer_scrollback.c`:

```c
static void scrollback_render(const ik_layer_t *layer,
                              ik_output_buffer_t *output,
                              size_t width,
                              size_t start_row,
                              size_t row_count)
{
    assert(layer != NULL);       // LCOV_EXCL_BR_LINE
    assert(layer->data != NULL); // LCOV_EXCL_BR_LINE
    assert(output != NULL);      // LCOV_EXCL_BR_LINE

    ik_scrollback_layer_data_t *data = (ik_scrollback_layer_data_t *)layer->data;
    ik_scrollback_t *scrollback = data->scrollback;

    // Ensure layout is up to date
    ik_scrollback_ensure_layout(scrollback, (int32_t)width);

    // Handle empty scrollback
    size_t total_lines = ik_scrollback_get_line_count(scrollback);
    if (total_lines == 0 || row_count == 0) {
        return;
    }

    // Find start logical line and row offset within it
    size_t start_line_idx, start_row_offset;
    res_t res = ik_scrollback_find_logical_line_at_physical_row(scrollback, start_row,
                                                                &start_line_idx, &start_row_offset);
    if (!is_ok(&res)) {
        return;
    }

    // Find end logical line and row offset
    size_t end_physical_row = start_row + row_count;
    size_t end_line_idx, end_row_offset;
    assert(end_physical_row > 0);  // LCOV_EXCL_BR_LINE
    res = ik_scrollback_find_logical_line_at_physical_row(scrollback, end_physical_row - 1,
                                                          &end_line_idx, &end_row_offset);
    if (!is_ok(&res)) {
        end_line_idx = total_lines - 1;
        // For end_row_offset, use last row of last line
        end_row_offset = scrollback->layouts[end_line_idx].physical_lines - 1;
    }

    // Render logical lines from start_line_idx to end_line_idx
    assert(end_line_idx < total_lines);  // LCOV_EXCL_BR_LINE
    for (size_t i = start_line_idx; i <= end_line_idx; i++) {
        const char *line_text = NULL;
        size_t line_len = 0;
        res = ik_scrollback_get_line_text(scrollback, i, &line_text, &line_len);
        (void)res;

        // Calculate byte range to render for this line
        size_t render_start = 0;
        size_t render_end = line_len;
        bool is_line_end = true;  // Are we rendering to end of logical line?

        // For first line: skip start_row_offset worth of display columns
        if (i == start_line_idx && start_row_offset > 0) {
            size_t skip_cols = start_row_offset * width;
            res = ik_scrollback_get_byte_offset_at_display_col(scrollback, i, skip_cols, &render_start);
            if (is_err(&res)) {
                render_start = 0;
            }
        }

        // For last line: only render up to (end_row_offset + 1) * width display columns
        if (i == end_line_idx) {
            size_t line_physical_rows = scrollback->layouts[i].physical_lines;
            // Are we stopping before end of this logical line?
            if (end_row_offset + 1 < line_physical_rows) {
                size_t stop_cols = (end_row_offset + 1) * width;
                res = ik_scrollback_get_byte_offset_at_display_col(scrollback, i, stop_cols, &render_end);
                if (is_err(&res)) {
                    render_end = line_len;
                }
                is_line_end = false;  // We're stopping mid-line
            }
        }

        // Copy line text from render_start to render_end, converting \n to \r\n
        for (size_t j = render_start; j < render_end; j++) {
            if (line_text[j] == '\n') {
                ik_output_buffer_append(output, "\r\n", 2);
            } else {
                ik_output_buffer_append(output, &line_text[j], 1);
            }
        }

        // Add \r\n only if we rendered to end of logical line
        if (is_line_end) {
            ik_output_buffer_append(output, "\r\n", 2);
        }
    }
}
```

Run `make check` - expect PASS.

### Verify

1. `make check` - all tests pass
2. `make lint` - complexity checks pass
3. Manual test:
   - Run `bin/ikigai`
   - Generate long wrapped content (paste a long paragraph)
   - Mouse wheel scroll
   - Verify smooth per-row scrolling (display updates with each scroll notch)
   - Verify no visual glitches at wrap boundaries
   - Verify UTF-8 text displays correctly when scrolling mid-line

## Post-conditions
- Working tree is clean (all changes committed)
- `make check` passes
- Mouse scroll within wrapped lines produces visual change
- Per-physical-row scrolling works smoothly
- UTF-8 and wide characters render correctly at row boundaries
- ANSI escapes preserved in partial line output
- `\r\n` only added at end of logical lines
- Tests verify partial line rendering in all cases

## Notes

### Edge Cases Handled

1. **First line partial**: Skip `start_row_offset * width` display columns
2. **Last line partial**: Stop at `(end_row_offset + 1) * width` display columns
3. **Middle lines**: Render completely (not first and not last in viewport)
4. **Single-row lines**: `start_row_offset = end_row_offset = 0`, render fully
5. **UTF-8/ANSI**: Byte offsets computed correctly via display column API

### Why No Caching?

The byte offset lookup is O(n) where n is line length, but:
- Only computed for first and last visible lines (not every line)
- Lines are typically short (< 1KB)
- Avoids complexity of cache invalidation
- Scrolling is already fast enough in practice
