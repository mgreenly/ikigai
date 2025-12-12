# Task: Add Byte Offset API for Partial Line Rendering

## Target

Bug Fix: Support for scrollback viewport rendering that starts/ends mid-wrapped-line.

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
- src/scrollback.c (especially `calculate_display_width_()` for UTF-8/ANSI iteration pattern)
- src/scrollback.h (API, `ik_line_layout_t` structure)
- src/ansi.h (`ik_ansi_skip_csi()` function)
- tests/unit/scrollback/scrollback_query_test.c (test patterns)

## Pre-conditions
- Working tree is clean (`git status --porcelain` returns empty)
- `make check` passes

## Background

### The Problem

When scrolling within a wrapped line, `layer_scrollback.c` needs to start rendering from the middle of a logical line. It gets `start_row_offset` (e.g., "start at physical row 2 of this logical line") but has no way to convert that to a byte offset in the text.

### Why Byte Offset is Non-trivial

The text contains:
1. **UTF-8 multi-byte characters** - "e" is 2 bytes but 1 display column
2. **Wide characters** - CJK chars are 3 bytes but 2 display columns
3. **ANSI escape sequences** - `\x1b[38;5;242m` is 11 bytes but 0 display columns

To find the byte offset at display column N, we must iterate through the text tracking display width, skipping ANSI escapes.

### Solution

Add `ik_scrollback_get_byte_offset_at_display_col()` that returns the byte offset where a given display column starts. The caller can then calculate `target_col = row_offset * terminal_width` and use this API.

## TDD Cycle

### Red

Create `tests/unit/scrollback/scrollback_byte_offset_test.c`:

```c
#include <check.h>
#include <talloc.h>
#include <string.h>
#include "../../../src/scrollback.h"

// Test 1: Column 0 returns byte offset 0
START_TEST(test_byte_offset_at_col_zero)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_scrollback_t *sb = ik_scrollback_create(ctx, 80);
    ik_scrollback_append_line(sb, "Hello World", 11);

    size_t byte_offset = 0;
    res_t res = ik_scrollback_get_byte_offset_at_display_col(sb, 0, 0, &byte_offset);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(byte_offset, 0);

    talloc_free(ctx);
}
END_TEST

// Test 2: Column 5 of ASCII text returns byte 5
START_TEST(test_byte_offset_ascii)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_scrollback_t *sb = ik_scrollback_create(ctx, 80);
    ik_scrollback_append_line(sb, "Hello World", 11);

    size_t byte_offset = 0;
    res_t res = ik_scrollback_get_byte_offset_at_display_col(sb, 0, 5, &byte_offset);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(byte_offset, 5);  // " World" starts at byte 5

    talloc_free(ctx);
}
END_TEST

// Test 3: UTF-8 multi-byte characters (e is 2 bytes, 1 column)
START_TEST(test_byte_offset_utf8_multibyte)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_scrollback_t *sb = ik_scrollback_create(ctx, 80);
    // "cafe" = 6 bytes: c(1) a(1) f(1) e(2) = 5 display cols
    ik_scrollback_append_line(sb, "caf\xc3\xa9", 5);

    size_t byte_offset = 0;
    // Column 4 is after "cafe" (4 display cols), but byte offset is 5 (after e)
    res_t res = ik_scrollback_get_byte_offset_at_display_col(sb, 0, 4, &byte_offset);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(byte_offset, 5);

    talloc_free(ctx);
}
END_TEST

// Test 4: Wide characters (CJK: 3 bytes, 2 columns each)
START_TEST(test_byte_offset_wide_chars)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_scrollback_t *sb = ik_scrollback_create(ctx, 80);
    // (nihon) = 9 bytes, 6 display cols (each char is 3 bytes, 2 cols)
    ik_scrollback_append_line(sb, "\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e", 9);

    size_t byte_offset = 0;
    // Column 2 should be at byte 3 (after first CJK char)
    res_t res = ik_scrollback_get_byte_offset_at_display_col(sb, 0, 2, &byte_offset);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(byte_offset, 3);

    // Column 4 should be at byte 6 (after second CJK char)
    res = ik_scrollback_get_byte_offset_at_display_col(sb, 0, 4, &byte_offset);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(byte_offset, 6);

    talloc_free(ctx);
}
END_TEST

// Test 5: ANSI escape sequences are skipped (0 display width)
START_TEST(test_byte_offset_with_ansi)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_scrollback_t *sb = ik_scrollback_create(ctx, 80);
    // "\x1b[31mHello\x1b[0m" = red "Hello" reset
    // ANSI: 5 bytes, Hello: 5 bytes, ANSI: 4 bytes = 14 total bytes, 5 display cols
    ik_scrollback_append_line(sb, "\x1b[31mHello\x1b[0m", 14);

    size_t byte_offset = 0;
    // Column 0 should skip ANSI and point to 'H' at byte 5
    res_t res = ik_scrollback_get_byte_offset_at_display_col(sb, 0, 0, &byte_offset);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(byte_offset, 0);  // Start of text (ANSI is part of output)

    // Column 3 ("llo") - after "Hel" (3 cols), byte 8 (skip 5 ANSI + 3 chars)
    res = ik_scrollback_get_byte_offset_at_display_col(sb, 0, 3, &byte_offset);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(byte_offset, 8);

    talloc_free(ctx);
}
END_TEST

// Test 6: Column beyond text length returns end of text
START_TEST(test_byte_offset_beyond_text)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_scrollback_t *sb = ik_scrollback_create(ctx, 80);
    ik_scrollback_append_line(sb, "Short", 5);

    size_t byte_offset = 0;
    // Column 100 is way beyond "Short" (5 cols)
    res_t res = ik_scrollback_get_byte_offset_at_display_col(sb, 0, 100, &byte_offset);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(byte_offset, 5);  // End of text

    talloc_free(ctx);
}
END_TEST

// Test 7: Empty line returns 0
START_TEST(test_byte_offset_empty_line)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_scrollback_t *sb = ik_scrollback_create(ctx, 80);
    ik_scrollback_append_line(sb, "", 0);

    size_t byte_offset = 0;
    res_t res = ik_scrollback_get_byte_offset_at_display_col(sb, 0, 0, &byte_offset);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(byte_offset, 0);

    talloc_free(ctx);
}
END_TEST

// Test 8: Invalid line index returns error
START_TEST(test_byte_offset_invalid_line)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_scrollback_t *sb = ik_scrollback_create(ctx, 80);
    ik_scrollback_append_line(sb, "Test", 4);

    size_t byte_offset = 0;
    res_t res = ik_scrollback_get_byte_offset_at_display_col(sb, 5, 0, &byte_offset);
    ck_assert(is_err(&res));

    talloc_free(ctx);
}
END_TEST

// Test 9: Mixed content (ASCII + ANSI + UTF-8)
START_TEST(test_byte_offset_mixed_content)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_scrollback_t *sb = ik_scrollback_create(ctx, 80);
    // "Hi \x1b[1mWorld\x1b[0m" = "Hi " + ANSI(4) + "World" + ANSI(4)
    // Bytes: 3 + 4 + 5 + 4 = 16, Display: 8 cols
    ik_scrollback_append_line(sb, "Hi \x1b[1mWorld\x1b[0m", 16);

    size_t byte_offset = 0;
    // Column 3 = after "Hi ", should be at byte 7 (3 + 4 ANSI)
    res_t res = ik_scrollback_get_byte_offset_at_display_col(sb, 0, 3, &byte_offset);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(byte_offset, 7);

    talloc_free(ctx);
}
END_TEST
```

Add test to Makefile and run `make check` - expect FAIL (function doesn't exist).

### Green

1. Add function declaration to `src/scrollback.h`:

```c
/**
 * @brief Get byte offset at a given display column within a line
 *
 * Iterates through the line text, tracking display width while skipping
 * ANSI escape sequences, to find the byte offset where the specified
 * display column begins. Used for partial line rendering when scrolling.
 *
 * @param scrollback Scrollback buffer
 * @param line_index Logical line index (0-based)
 * @param display_col Target display column (0-based)
 * @param byte_offset_out Pointer to receive byte offset
 * @return RES_OK on success, RES_ERR if line_index is out of range
 *
 * Notes:
 * - If display_col is beyond the line's display width, returns end of line
 * - ANSI escape sequences are skipped (they have 0 display width)
 * - UTF-8 multi-byte characters are handled correctly
 * - Wide characters (CJK) are counted as 2 display columns
 *
 * Assertions:
 * - scrollback must not be NULL
 * - byte_offset_out must not be NULL
 */
res_t ik_scrollback_get_byte_offset_at_display_col(ik_scrollback_t *scrollback,
                                                    size_t line_index,
                                                    size_t display_col,
                                                    size_t *byte_offset_out);
```

2. Implement in `src/scrollback.c` (follow `calculate_display_width_()` pattern):

```c
res_t ik_scrollback_get_byte_offset_at_display_col(ik_scrollback_t *scrollback,
                                                    size_t line_index,
                                                    size_t display_col,
                                                    size_t *byte_offset_out)
{
    assert(scrollback != NULL);       // LCOV_EXCL_BR_LINE
    assert(byte_offset_out != NULL);  // LCOV_EXCL_BR_LINE

    // Validate line index
    if (line_index >= scrollback->count) {
        return ERR(scrollback, OUT_OF_RANGE,
                   "Line index %zu out of range (count=%zu)",
                   line_index, scrollback->count);
    }

    // Column 0 always starts at byte 0
    if (display_col == 0) {
        *byte_offset_out = 0;
        return OK(NULL);
    }

    // Get line text
    const char *text = scrollback->text_buffer + scrollback->text_offsets[line_index];
    size_t length = scrollback->text_lengths[line_index];

    // Walk through text, tracking display columns
    size_t pos = 0;
    size_t col = 0;

    while (pos < length && col < display_col) {
        // Skip ANSI escape sequences (0 display width)
        size_t skip = ik_ansi_skip_csi(text, length, pos);
        if (skip > 0) {
            pos += skip;
            continue;
        }

        // Decode UTF-8 codepoint
        utf8proc_int32_t cp;
        utf8proc_ssize_t bytes = utf8proc_iterate(
            (const utf8proc_uint8_t *)(text + pos),
            (utf8proc_ssize_t)(length - pos),
            &cp);

        if (bytes <= 0) {
            // Invalid UTF-8 - treat as 1 byte, 1 column
            col++;
            pos++;
            continue;
        }

        // Skip newlines (they don't contribute to display width)
        if (cp == '\n') {
            pos += (size_t)bytes;
            continue;
        }

        // Get character display width
        int32_t width = utf8proc_charwidth(cp);
        if (width > 0) {
            col += (size_t)width;
        }

        pos += (size_t)bytes;
    }

    *byte_offset_out = pos;
    return OK(NULL);
}
```

3. Add to Makefile (test compilation).

4. Run `make check` - expect PASS.

### Verify

1. `make check` - all tests pass
2. `make lint` - no complexity warnings
3. `make coverage` - new function has coverage

## Post-conditions
- Working tree is clean (all changes committed)
- `make check` passes
- New function `ik_scrollback_get_byte_offset_at_display_col()` exists with tests
- Function handles UTF-8, wide chars, and ANSI escapes correctly
- Edge cases tested: empty lines, column beyond text, invalid line index

## Notes

This is the first of two tasks. This task adds the API; the next task integrates it into `layer_scrollback.c` to fix the rendering bug.

The function uses the same iteration pattern as `calculate_display_width_()` but stops at a target column instead of summing total width.
