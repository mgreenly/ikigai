/**
 * @file column_preservation_test.c
 * @brief Unit tests for column preservation during multi-line cursor navigation
 */

#include <check.h>
#include <talloc.h>
#include "../../../src/input_buffer/core.h"
#include "../../test_utils.h"

/* Test: Column preservation when moving up and down through lines of different lengths */
START_TEST(test_cursor_up_down_column_preservation) {
    void *ctx = talloc_new(NULL);
    ik_input_buffer_t *input_buffer = NULL;

    input_buffer = ik_input_buffer_create(ctx);

    /* Create three lines with different lengths:
     * Line 1: "short" (5 chars)
     * Line 2: "this is a much longer line" (27 chars)
     * Line 3: "tiny" (4 chars)
     */
    const char *line1 = "short";
    const char *line2 = "this is a much longer line";
    const char *line3 = "tiny";

    /* Insert line 1 */
    for (const char *p = line1; *p; p++) {
        ik_input_buffer_insert_codepoint(input_buffer, (uint32_t)*p);
    }
    ik_input_buffer_insert_newline(input_buffer);

    /* Insert line 2 */
    for (const char *p = line2; *p; p++) {
        ik_input_buffer_insert_codepoint(input_buffer, (uint32_t)*p);
    }
    ik_input_buffer_insert_newline(input_buffer);

    /* Insert line 3 */
    for (const char *p = line3; *p; p++) {
        ik_input_buffer_insert_codepoint(input_buffer, (uint32_t)*p);
    }

    /* Now we have: "short\nthis is a much longer line\ntiny"
     * Total: 5 + 1 + 27 + 1 + 4 = 38 bytes
     * Position the cursor at column 10 of line 2 (the long line)
     * Line 2 starts at byte 6 (after "short\n")
     * Column 10 means byte 6 + 10 = 16
     */
    input_buffer->cursor_byte_offset = 16;
    size_t text_len;
    const char *text = ik_input_buffer_get_text(input_buffer, &text_len);
    ik_input_buffer_cursor_set_position(input_buffer->cursor, text, text_len, 16);

    /* Verify we're at column 10 of line 2 */
    size_t byte_offset = 0;
    size_t grapheme_offset = 0;
    res_t res = ik_input_buffer_get_cursor_position(input_buffer, &byte_offset, &grapheme_offset);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(byte_offset, 16);  /* byte 16 */
    ck_assert_uint_eq(grapheme_offset, 16);  /* grapheme 16 */

    /* Move UP - should clamp to column 5 (end of "short") since line 1 is only 5 chars */
    res = ik_input_buffer_cursor_up(input_buffer);
    ck_assert(is_ok(&res));

    res = ik_input_buffer_get_cursor_position(input_buffer, &byte_offset, &grapheme_offset);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(byte_offset, 5);  /* Clamped to end of "short" */
    ck_assert_uint_eq(grapheme_offset, 5);

    /* Move DOWN - should return to column 10, NOT stay at column 5!
     * This is the bug: currently it returns to column 5 instead of column 10
     * Expected: byte 16 (column 10 of line 2)
     */
    res = ik_input_buffer_cursor_down(input_buffer);
    ck_assert(is_ok(&res));

    res = ik_input_buffer_get_cursor_position(input_buffer, &byte_offset, &grapheme_offset);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(byte_offset, 16);  /* Should return to original column 10 */
    ck_assert_uint_eq(grapheme_offset, 16);

    talloc_free(ctx);
}
END_TEST
/* Test: Column preservation resets on horizontal movement */
START_TEST(test_column_preservation_resets_on_horizontal_move) {
    void *ctx = talloc_new(NULL);
    ik_input_buffer_t *input_buffer = NULL;

    input_buffer = ik_input_buffer_create(ctx);

    /* Create two lines: "short" and "this is a longer line" */
    const char *line1 = "short";
    const char *line2 = "this is a longer line";

    for (const char *p = line1; *p; p++) {
        ik_input_buffer_insert_codepoint(input_buffer, (uint32_t)*p);
    }
    ik_input_buffer_insert_newline(input_buffer);
    for (const char *p = line2; *p; p++) {
        ik_input_buffer_insert_codepoint(input_buffer, (uint32_t)*p);
    }

    /* Position at column 10 of line 2 (byte 6 + 10 = 16) */
    input_buffer->cursor_byte_offset = 16;
    size_t text_len;
    const char *text = ik_input_buffer_get_text(input_buffer, &text_len);
    ik_input_buffer_cursor_set_position(input_buffer->cursor, text, text_len, 16);

    /* Move up (clamps to end of "short" at byte 5) */
    res_t res = ik_input_buffer_cursor_up(input_buffer);
    ck_assert(is_ok(&res));

    /* Move left - this should reset the target column */
    res = ik_input_buffer_cursor_left(input_buffer);
    ck_assert(is_ok(&res));

    size_t byte_offset = 0;
    size_t grapheme_offset = 0;
    res = ik_input_buffer_get_cursor_position(input_buffer, &byte_offset, &grapheme_offset);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(byte_offset, 4);  /* Now at column 4 of line 1 */

    /* Move down - should go to column 4 of line 2, NOT column 10
     * Because horizontal movement reset the target column
     * Expected: byte 6 + 4 = 10
     */
    res = ik_input_buffer_cursor_down(input_buffer);
    ck_assert(is_ok(&res));

    res = ik_input_buffer_get_cursor_position(input_buffer, &byte_offset, &grapheme_offset);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(byte_offset, 10);  /* Column 4 of line 2 */
    ck_assert_uint_eq(grapheme_offset, 10);

    talloc_free(ctx);
}

END_TEST
/* Test: Multiple consecutive vertical movements preserve column */
START_TEST(test_multiple_vertical_movements) {
    void *ctx = talloc_new(NULL);
    ik_input_buffer_t *input_buffer = NULL;

    input_buffer = ik_input_buffer_create(ctx);

    /* Create four lines with varying lengths:
     * "short"
     * "this is very long"
     * "mid"
     * "another very long line"
     */
    const char *line1 = "short";
    const char *line2 = "this is very long";
    const char *line3 = "mid";
    const char *line4 = "another very long line";

    for (const char *p = line1; *p; p++) {
        ik_input_buffer_insert_codepoint(input_buffer, (uint32_t)*p);
    }
    ik_input_buffer_insert_newline(input_buffer);
    for (const char *p = line2; *p; p++) {
        ik_input_buffer_insert_codepoint(input_buffer, (uint32_t)*p);
    }
    ik_input_buffer_insert_newline(input_buffer);
    for (const char *p = line3; *p; p++) {
        ik_input_buffer_insert_codepoint(input_buffer, (uint32_t)*p);
    }
    ik_input_buffer_insert_newline(input_buffer);
    for (const char *p = line4; *p; p++) {
        ik_input_buffer_insert_codepoint(input_buffer, (uint32_t)*p);
    }

    /* Position at column 10 of line 4
     * Line 1: "short\n" = 6 bytes (0-5)
     * Line 2: "this is very long\n" = 18 bytes (6-23)
     * Line 3: "mid\n" = 4 bytes (24-27)
     * Line 4: "another very long line" = 22 bytes (28-49)
     * Column 10 of line 4 = byte 28 + 10 = 38
     */
    input_buffer->cursor_byte_offset = 38;
    size_t text_len;
    const char *text = ik_input_buffer_get_text(input_buffer, &text_len);
    ik_input_buffer_cursor_set_position(input_buffer->cursor, text, text_len, 38);

    size_t byte_offset = 0;
    size_t grapheme_offset = 0;

    /* Move up to line 3 - clamps to column 3 (end of "mid") */
    res_t res = ik_input_buffer_cursor_up(input_buffer);
    ck_assert(is_ok(&res));
    res = ik_input_buffer_get_cursor_position(input_buffer, &byte_offset, &grapheme_offset);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(byte_offset, 27);  /* Clamped to end of "mid" (24 + 3) */

    /* Move up again to line 2 - should go to column 10, NOT column 3 */
    res = ik_input_buffer_cursor_up(input_buffer);
    ck_assert(is_ok(&res));
    res = ik_input_buffer_get_cursor_position(input_buffer, &byte_offset, &grapheme_offset);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(byte_offset, 16);  /* Column 10 of line 2 (6 + 10) */

    /* Move down to line 3 again - should clamp to column 3 */
    res = ik_input_buffer_cursor_down(input_buffer);
    ck_assert(is_ok(&res));
    res = ik_input_buffer_get_cursor_position(input_buffer, &byte_offset, &grapheme_offset);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(byte_offset, 27);  /* Clamped to end of "mid" */

    /* Move down to line 4 - should return to column 10 */
    res = ik_input_buffer_cursor_down(input_buffer);
    ck_assert(is_ok(&res));
    res = ik_input_buffer_get_cursor_position(input_buffer, &byte_offset, &grapheme_offset);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(byte_offset, 38);  /* Back to original column 10 (28 + 10) */

    talloc_free(ctx);
}

END_TEST

/* Suite creation */
static Suite *column_preservation_suite(void)
{
    Suite *s = suite_create("Column Preservation");
    TCase *tc_core = tcase_create("Core");
    tcase_set_timeout(tc_core, IK_TEST_TIMEOUT);
    tcase_set_timeout(tc_core, IK_TEST_TIMEOUT);
    tcase_set_timeout(tc_core, IK_TEST_TIMEOUT);
    tcase_set_timeout(tc_core, IK_TEST_TIMEOUT);
    tcase_set_timeout(tc_core, IK_TEST_TIMEOUT);

    tcase_add_test(tc_core, test_cursor_up_down_column_preservation);
    tcase_add_test(tc_core, test_column_preservation_resets_on_horizontal_move);
    tcase_add_test(tc_core, test_multiple_vertical_movements);

    suite_add_tcase(s, tc_core);
    return s;
}

int main(void)
{
    Suite *s = column_preservation_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
