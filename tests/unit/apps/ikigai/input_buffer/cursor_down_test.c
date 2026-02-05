/**
 * @file cursor_down_test.c
 * @brief Unit tests for input_buffer cursor down cursor movement operations
 */

#include <check.h>
#include <signal.h>
#include <talloc.h>
#include "apps/ikigai/input_buffer/core.h"
#include "tests/helpers/test_utils_helper.h"

/* Test: Cursor down - basic */
START_TEST(test_cursor_down_basic) {
    void *ctx = talloc_new(NULL);
    ik_input_buffer_t *input_buffer = NULL;

    input_buffer = ik_input_buffer_create(ctx);

    /* Insert "line1\nline2\nline3" */
    ik_input_buffer_insert_codepoint(input_buffer, 'l');
    ik_input_buffer_insert_codepoint(input_buffer, 'i');
    ik_input_buffer_insert_codepoint(input_buffer, 'n');
    ik_input_buffer_insert_codepoint(input_buffer, 'e');
    ik_input_buffer_insert_codepoint(input_buffer, '1');
    ik_input_buffer_insert_newline(input_buffer);
    ik_input_buffer_insert_codepoint(input_buffer, 'l');
    ik_input_buffer_insert_codepoint(input_buffer, 'i');
    ik_input_buffer_insert_codepoint(input_buffer, 'n');
    ik_input_buffer_insert_codepoint(input_buffer, 'e');
    ik_input_buffer_insert_codepoint(input_buffer, '2');
    ik_input_buffer_insert_newline(input_buffer);
    ik_input_buffer_insert_codepoint(input_buffer, 'l');
    ik_input_buffer_insert_codepoint(input_buffer, 'i');
    ik_input_buffer_insert_codepoint(input_buffer, 'n');
    ik_input_buffer_insert_codepoint(input_buffer, 'e');
    ik_input_buffer_insert_codepoint(input_buffer, '3');

    /* Move cursor to start of line1 (byte 0) */
    input_buffer->cursor_byte_offset = 0;
    size_t text_len;
    const char *text = ik_input_buffer_get_text(input_buffer, &text_len);
    ik_input_buffer_cursor_set_position(input_buffer->cursor, text, text_len, 0);

    /* Move down - should go to start of line2 (byte 6) */
    res_t res = ik_input_buffer_cursor_down(input_buffer);
    ck_assert(is_ok(&res));

    /* Verify cursor at byte 6, grapheme 6 */
    size_t byte_offset = 0;
    size_t grapheme_offset = 0;
    res = ik_input_buffer_get_cursor_position(input_buffer, &byte_offset, &grapheme_offset);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(byte_offset, 6);
    ck_assert_uint_eq(grapheme_offset, 6);

    talloc_free(ctx);
}

END_TEST
/* Test: Cursor down from last line - no-op */
START_TEST(test_cursor_down_from_last_line) {
    void *ctx = talloc_new(NULL);
    ik_input_buffer_t *input_buffer = NULL;

    input_buffer = ik_input_buffer_create(ctx);

    /* Insert "hello\nworld" */
    ik_input_buffer_insert_codepoint(input_buffer, 'h');
    ik_input_buffer_insert_codepoint(input_buffer, 'e');
    ik_input_buffer_insert_codepoint(input_buffer, 'l');
    ik_input_buffer_insert_codepoint(input_buffer, 'l');
    ik_input_buffer_insert_codepoint(input_buffer, 'o');
    ik_input_buffer_insert_newline(input_buffer);
    ik_input_buffer_insert_codepoint(input_buffer, 'w');
    ik_input_buffer_insert_codepoint(input_buffer, 'o');
    ik_input_buffer_insert_codepoint(input_buffer, 'r');
    ik_input_buffer_insert_codepoint(input_buffer, 'l');
    ik_input_buffer_insert_codepoint(input_buffer, 'd');

    /* Already at end (byte 11), on last line */
    /* Move down - should be no-op */
    res_t res = ik_input_buffer_cursor_down(input_buffer);
    ck_assert(is_ok(&res));

    /* Verify cursor still at byte 11, grapheme 11 */
    size_t byte_offset = 0;
    size_t grapheme_offset = 0;
    res = ik_input_buffer_get_cursor_position(input_buffer, &byte_offset, &grapheme_offset);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(byte_offset, 11);
    ck_assert_uint_eq(grapheme_offset, 11);

    talloc_free(ctx);
}

END_TEST
/* Test: Cursor down with column preservation */
START_TEST(test_cursor_down_column_preservation) {
    void *ctx = talloc_new(NULL);
    ik_input_buffer_t *input_buffer = NULL;

    input_buffer = ik_input_buffer_create(ctx);

    /* Insert "abcde\nfghij" */
    ik_input_buffer_insert_codepoint(input_buffer, 'a');
    ik_input_buffer_insert_codepoint(input_buffer, 'b');
    ik_input_buffer_insert_codepoint(input_buffer, 'c');
    ik_input_buffer_insert_codepoint(input_buffer, 'd');
    ik_input_buffer_insert_codepoint(input_buffer, 'e');
    ik_input_buffer_insert_newline(input_buffer);
    ik_input_buffer_insert_codepoint(input_buffer, 'f');
    ik_input_buffer_insert_codepoint(input_buffer, 'g');
    ik_input_buffer_insert_codepoint(input_buffer, 'h');
    ik_input_buffer_insert_codepoint(input_buffer, 'i');
    ik_input_buffer_insert_codepoint(input_buffer, 'j');

    /* Move to position 3 (column 3 of first line: after 'c') */
    input_buffer->cursor_byte_offset = 3;
    size_t text_len;
    const char *text = ik_input_buffer_get_text(input_buffer, &text_len);
    ik_input_buffer_cursor_set_position(input_buffer->cursor, text, text_len, 3);

    /* Move down - should go to column 3 of second line (after 'h', byte 9) */
    res_t res = ik_input_buffer_cursor_down(input_buffer);
    ck_assert(is_ok(&res));

    /* Verify cursor at byte 9, grapheme 9 */
    size_t byte_offset = 0;
    size_t grapheme_offset = 0;
    res = ik_input_buffer_get_cursor_position(input_buffer, &byte_offset, &grapheme_offset);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(byte_offset, 9);
    ck_assert_uint_eq(grapheme_offset, 9);

    talloc_free(ctx);
}

END_TEST
/* Test: Cursor down to shorter line */
START_TEST(test_cursor_down_shorter_line) {
    void *ctx = talloc_new(NULL);
    ik_input_buffer_t *input_buffer = NULL;

    input_buffer = ik_input_buffer_create(ctx);

    /* Insert "abcdef\nab" (second line shorter) */
    ik_input_buffer_insert_codepoint(input_buffer, 'a');
    ik_input_buffer_insert_codepoint(input_buffer, 'b');
    ik_input_buffer_insert_codepoint(input_buffer, 'c');
    ik_input_buffer_insert_codepoint(input_buffer, 'd');
    ik_input_buffer_insert_codepoint(input_buffer, 'e');
    ik_input_buffer_insert_codepoint(input_buffer, 'f');
    ik_input_buffer_insert_newline(input_buffer);
    ik_input_buffer_insert_codepoint(input_buffer, 'a');
    ik_input_buffer_insert_codepoint(input_buffer, 'b');

    /* Move to position 4 (column 4 of first line: after 'd') */
    input_buffer->cursor_byte_offset = 4;
    size_t text_len;
    const char *text = ik_input_buffer_get_text(input_buffer, &text_len);
    ik_input_buffer_cursor_set_position(input_buffer->cursor, text, text_len, 4);

    /* Move down - should go to end of second line (byte 9, after 'b') */
    res_t res = ik_input_buffer_cursor_down(input_buffer);
    ck_assert(is_ok(&res));

    /* Verify cursor at byte 9, grapheme 9 (end of second line) */
    size_t byte_offset = 0;
    size_t grapheme_offset = 0;
    res = ik_input_buffer_get_cursor_position(input_buffer, &byte_offset, &grapheme_offset);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(byte_offset, 9);
    ck_assert_uint_eq(grapheme_offset, 9);

    talloc_free(ctx);
}

END_TEST
/* Test: Cursor down with empty line */
START_TEST(test_cursor_down_empty_line) {
    void *ctx = talloc_new(NULL);
    ik_input_buffer_t *input_buffer = NULL;

    input_buffer = ik_input_buffer_create(ctx);

    /* Insert "abc\n" (second line empty) */
    ik_input_buffer_insert_codepoint(input_buffer, 'a');
    ik_input_buffer_insert_codepoint(input_buffer, 'b');
    ik_input_buffer_insert_codepoint(input_buffer, 'c');
    ik_input_buffer_insert_newline(input_buffer);

    /* Move to position 1 (column 1 of first line: after 'a') */
    input_buffer->cursor_byte_offset = 1;
    size_t text_len;
    const char *text = ik_input_buffer_get_text(input_buffer, &text_len);
    ik_input_buffer_cursor_set_position(input_buffer->cursor, text, text_len, 1);

    /* Move down - should go to start of second line (byte 4, after newline) */
    res_t res = ik_input_buffer_cursor_down(input_buffer);
    ck_assert(is_ok(&res));

    /* Verify cursor at byte 4, grapheme 4 */
    size_t byte_offset = 0;
    size_t grapheme_offset = 0;
    res = ik_input_buffer_get_cursor_position(input_buffer, &byte_offset, &grapheme_offset);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(byte_offset, 4);
    ck_assert_uint_eq(grapheme_offset, 4);

    talloc_free(ctx);
}

END_TEST
/* Test: Cursor down with UTF-8 */
START_TEST(test_cursor_down_utf8) {
    void *ctx = talloc_new(NULL);
    ik_input_buffer_t *input_buffer = NULL;

    input_buffer = ik_input_buffer_create(ctx);

    /* Insert "abc\naé中🎉" (2-byte, 3-byte, 4-byte UTF-8 in second line) */
    ik_input_buffer_insert_codepoint(input_buffer, 'a');
    ik_input_buffer_insert_codepoint(input_buffer, 'b');
    ik_input_buffer_insert_codepoint(input_buffer, 'c');
    ik_input_buffer_insert_newline(input_buffer);
    ik_input_buffer_insert_codepoint(input_buffer, 'a');          // 1 byte
    ik_input_buffer_insert_codepoint(input_buffer, 0x00E9);       // é (2 bytes)
    ik_input_buffer_insert_codepoint(input_buffer, 0x4E2D);       // 中 (3 bytes)
    ik_input_buffer_insert_codepoint(input_buffer, 0x1F389);      // 🎉 (4 bytes)

    /* Move to position 2 (column 2 of first line: after 'b') */
    input_buffer->cursor_byte_offset = 2;
    size_t text_len;
    const char *text = ik_input_buffer_get_text(input_buffer, &text_len);
    ik_input_buffer_cursor_set_position(input_buffer->cursor, text, text_len, 2);

    /* Move down - should go to column 2 of second line (after é, byte 7) */
    // Line 2 starts at byte 4: a(1) + é(2) = byte 7, grapheme 6 (a,b,c,\n,a,é)
    res_t res = ik_input_buffer_cursor_down(input_buffer);
    ck_assert(is_ok(&res));

    /* Verify cursor at byte 7 (after a + é), grapheme 6 */
    size_t byte_offset = 0;
    size_t grapheme_offset = 0;
    res = ik_input_buffer_get_cursor_position(input_buffer, &byte_offset, &grapheme_offset);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(byte_offset, 7);
    ck_assert_uint_eq(grapheme_offset, 6);

    talloc_free(ctx);
}

END_TEST

#if !defined(NDEBUG) && !defined(SKIP_SIGNAL_TESTS)
/* Test: Cursor down - NULL input_buffer asserts */
START_TEST(test_cursor_down_null_input_buffer_asserts) {
    ik_input_buffer_cursor_down(NULL);
}

END_TEST
#endif

static Suite *input_buffer_cursor_down_suite(void)
{
    Suite *s = suite_create("Input Buffer Cursor Down");
    TCase *tc_core = tcase_create("Core");
    tcase_set_timeout(tc_core, IK_TEST_TIMEOUT);
    tcase_set_timeout(tc_core, IK_TEST_TIMEOUT);
    tcase_set_timeout(tc_core, IK_TEST_TIMEOUT);
    tcase_set_timeout(tc_core, IK_TEST_TIMEOUT);

    /* Normal tests */
    tcase_add_test(tc_core, test_cursor_down_basic);
    tcase_add_test(tc_core, test_cursor_down_from_last_line);
    tcase_add_test(tc_core, test_cursor_down_column_preservation);
    tcase_add_test(tc_core, test_cursor_down_shorter_line);
    tcase_add_test(tc_core, test_cursor_down_empty_line);
    tcase_add_test(tc_core, test_cursor_down_utf8);

    suite_add_tcase(s, tc_core);

#if !defined(NDEBUG) && !defined(SKIP_SIGNAL_TESTS)
    /* Assertion tests */
    TCase *tc_assertions = tcase_create("Assertions");
    tcase_set_timeout(tc_assertions, IK_TEST_TIMEOUT);
    tcase_set_timeout(tc_assertions, IK_TEST_TIMEOUT);
    tcase_set_timeout(tc_assertions, IK_TEST_TIMEOUT);
    tcase_set_timeout(tc_assertions, IK_TEST_TIMEOUT);
    tcase_set_timeout(tc_assertions, IK_TEST_TIMEOUT); // Longer timeout for valgrind
    tcase_add_test_raise_signal(tc_assertions, test_cursor_down_null_input_buffer_asserts, SIGABRT);
    suite_add_tcase(s, tc_assertions);
#endif

    return s;
}

int main(void)
{
    int number_failed;
    Suite *s = input_buffer_cursor_down_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/apps/ikigai/input_buffer/cursor_down_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
