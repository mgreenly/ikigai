/**
 * @file cursor_up_down_test.c
 * @brief Unit tests for input_buffer vertical cursor movement operations
 */

#include <check.h>
#include <signal.h>
#include <talloc.h>
#include "../../../src/input_buffer/core.h"
#include "../../test_utils.h"

/* Test: Cursor up - basic */
START_TEST(test_cursor_up_basic) {
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

    /* Cursor should be at end: byte 17 (after "line1\nline2\nline3"), grapheme 17 */
    size_t byte_offset = 0;
    size_t grapheme_offset = 0;
    res_t res = ik_input_buffer_get_cursor_position(input_buffer, &byte_offset, &grapheme_offset);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(byte_offset, 17);
    ck_assert_uint_eq(grapheme_offset, 17);

    /* Move cursor to start of line2 (byte 6, after first newline) */
    input_buffer->cursor_byte_offset = 6;
    size_t text_len;
    const char *text = ik_input_buffer_get_text(input_buffer, &text_len);
    ik_input_buffer_cursor_set_position(input_buffer->cursor, text, text_len, 6);

    /* Move up - should go to start of line1 (byte 0) */
    res = ik_input_buffer_cursor_up(input_buffer);
    ck_assert(is_ok(&res));

    /* Verify cursor at byte 0, grapheme 0 */
    res = ik_input_buffer_get_cursor_position(input_buffer, &byte_offset, &grapheme_offset);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(byte_offset, 0);
    ck_assert_uint_eq(grapheme_offset, 0);

    talloc_free(ctx);
}

END_TEST
/* Test: Cursor up from first line - no-op */
START_TEST(test_cursor_up_from_first_line) {
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

    /* Move to position 2 (middle of first line) */
    input_buffer->cursor_byte_offset = 2;
    size_t text_len;
    const char *text = ik_input_buffer_get_text(input_buffer, &text_len);
    ik_input_buffer_cursor_set_position(input_buffer->cursor, text, text_len, 2);

    /* Move up - should be no-op (already on first line) */
    res_t res = ik_input_buffer_cursor_up(input_buffer);
    ck_assert(is_ok(&res));

    /* Verify cursor still at byte 2, grapheme 2 */
    size_t byte_offset = 0;
    size_t grapheme_offset = 0;
    res = ik_input_buffer_get_cursor_position(input_buffer, &byte_offset, &grapheme_offset);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(byte_offset, 2);
    ck_assert_uint_eq(grapheme_offset, 2);

    talloc_free(ctx);
}

END_TEST
/* Test: Cursor up with column preservation */
START_TEST(test_cursor_up_column_preservation) {
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

    /* Move to position 9 (column 3 of second line: after 'h') */
    input_buffer->cursor_byte_offset = 9;
    size_t text_len;
    const char *text = ik_input_buffer_get_text(input_buffer, &text_len);
    ik_input_buffer_cursor_set_position(input_buffer->cursor, text, text_len, 9);

    /* Move up - should go to column 3 of first line (after 'c') */
    res_t res = ik_input_buffer_cursor_up(input_buffer);
    ck_assert(is_ok(&res));

    /* Verify cursor at byte 3, grapheme 3 */
    size_t byte_offset = 0;
    size_t grapheme_offset = 0;
    res = ik_input_buffer_get_cursor_position(input_buffer, &byte_offset, &grapheme_offset);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(byte_offset, 3);
    ck_assert_uint_eq(grapheme_offset, 3);

    talloc_free(ctx);
}

END_TEST
/* Test: Cursor up to shorter line */
START_TEST(test_cursor_up_shorter_line) {
    void *ctx = talloc_new(NULL);
    ik_input_buffer_t *input_buffer = NULL;

    input_buffer = ik_input_buffer_create(ctx);

    /* Insert "ab\nabcdef" (first line shorter) */
    ik_input_buffer_insert_codepoint(input_buffer, 'a');
    ik_input_buffer_insert_codepoint(input_buffer, 'b');
    ik_input_buffer_insert_newline(input_buffer);
    ik_input_buffer_insert_codepoint(input_buffer, 'a');
    ik_input_buffer_insert_codepoint(input_buffer, 'b');
    ik_input_buffer_insert_codepoint(input_buffer, 'c');
    ik_input_buffer_insert_codepoint(input_buffer, 'd');
    ik_input_buffer_insert_codepoint(input_buffer, 'e');
    ik_input_buffer_insert_codepoint(input_buffer, 'f');

    /* Move to position 7 (column 4 of second line: after 'd') */
    input_buffer->cursor_byte_offset = 7;
    size_t text_len;
    const char *text = ik_input_buffer_get_text(input_buffer, &text_len);
    ik_input_buffer_cursor_set_position(input_buffer->cursor, text, text_len, 7);

    /* Move up - should go to end of first line (byte 2, after 'b') */
    res_t res = ik_input_buffer_cursor_up(input_buffer);
    ck_assert(is_ok(&res));

    /* Verify cursor at byte 2, grapheme 2 (end of first line) */
    size_t byte_offset = 0;
    size_t grapheme_offset = 0;
    res = ik_input_buffer_get_cursor_position(input_buffer, &byte_offset, &grapheme_offset);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(byte_offset, 2);
    ck_assert_uint_eq(grapheme_offset, 2);

    talloc_free(ctx);
}

END_TEST
/* Test: Cursor up with empty line */
START_TEST(test_cursor_up_empty_line) {
    void *ctx = talloc_new(NULL);
    ik_input_buffer_t *input_buffer = NULL;

    input_buffer = ik_input_buffer_create(ctx);

    /* Insert "\nabc" (first line empty) */
    ik_input_buffer_insert_newline(input_buffer);
    ik_input_buffer_insert_codepoint(input_buffer, 'a');
    ik_input_buffer_insert_codepoint(input_buffer, 'b');
    ik_input_buffer_insert_codepoint(input_buffer, 'c');

    /* Move to position 2 (column 1 of second line: after 'a') */
    input_buffer->cursor_byte_offset = 2;
    size_t text_len;
    const char *text = ik_input_buffer_get_text(input_buffer, &text_len);
    ik_input_buffer_cursor_set_position(input_buffer->cursor, text, text_len, 2);

    /* Move up - should go to start of first line (byte 0) */
    res_t res = ik_input_buffer_cursor_up(input_buffer);
    ck_assert(is_ok(&res));

    /* Verify cursor at byte 0, grapheme 0 */
    size_t byte_offset = 0;
    size_t grapheme_offset = 0;
    res = ik_input_buffer_get_cursor_position(input_buffer, &byte_offset, &grapheme_offset);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(byte_offset, 0);
    ck_assert_uint_eq(grapheme_offset, 0);

    talloc_free(ctx);
}

END_TEST
/* Test: Cursor up with UTF-8 */
START_TEST(test_cursor_up_utf8) {
    void *ctx = talloc_new(NULL);
    ik_input_buffer_t *input_buffer = NULL;

    input_buffer = ik_input_buffer_create(ctx);

    /* Insert "aé中🎉\ndefg" (2-byte, 3-byte, 4-byte UTF-8) */
    ik_input_buffer_insert_codepoint(input_buffer, 'a');          // 1 byte
    ik_input_buffer_insert_codepoint(input_buffer, 0x00E9);       // é (2 bytes)
    ik_input_buffer_insert_codepoint(input_buffer, 0x4E2D);       // 中 (3 bytes)
    ik_input_buffer_insert_codepoint(input_buffer, 0x1F389);      // 🎉 (4 bytes)
    ik_input_buffer_insert_newline(input_buffer);
    ik_input_buffer_insert_codepoint(input_buffer, 'd');
    ik_input_buffer_insert_codepoint(input_buffer, 'e');
    ik_input_buffer_insert_codepoint(input_buffer, 'f');
    ik_input_buffer_insert_codepoint(input_buffer, 'g');

    /* Move to position 15 (column 4 of second line: after 'g') */
    // Text is: a(1) + é(2) + 中(3) + 🎉(4) + \n(1) + d(1) + e(1) + f(1) + g(1) = byte 15
    input_buffer->cursor_byte_offset = 15;
    size_t text_len;
    const char *text = ik_input_buffer_get_text(input_buffer, &text_len);
    ik_input_buffer_cursor_set_position(input_buffer->cursor, text, text_len, 15);

    /* Move up - should go to column 4 of first line (after 🎉, byte 10) */
    // Line 1: a(1) + é(2) + 中(3) + 🎉(4) = byte 10, grapheme 4
    res_t res = ik_input_buffer_cursor_up(input_buffer);
    ck_assert(is_ok(&res));

    /* Verify cursor at byte 10 (a + é + 中 + 🎉 = 1 + 2 + 3 + 4), grapheme 4 */
    size_t byte_offset = 0;
    size_t grapheme_offset = 0;
    res = ik_input_buffer_get_cursor_position(input_buffer, &byte_offset, &grapheme_offset);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(byte_offset, 10);
    ck_assert_uint_eq(grapheme_offset, 4);

    talloc_free(ctx);
}

#if !defined(NDEBUG) && !defined(SKIP_SIGNAL_TESTS)
/* Test: Cursor up - NULL input_buffer asserts */
START_TEST(test_cursor_up_null_input_buffer_asserts) {
    ik_input_buffer_cursor_up(NULL);
}

END_TEST
#endif

static Suite *input_buffer_cursor_up_suite(void)
{
    Suite *s = suite_create("Input Buffer Cursor Up");
    TCase *tc_core = tcase_create("Core");
    tcase_set_timeout(tc_core, IK_TEST_TIMEOUT);
    tcase_set_timeout(tc_core, IK_TEST_TIMEOUT);
    tcase_set_timeout(tc_core, IK_TEST_TIMEOUT);
    tcase_set_timeout(tc_core, IK_TEST_TIMEOUT);

    /* Normal tests */
    tcase_add_test(tc_core, test_cursor_up_basic);
    tcase_add_test(tc_core, test_cursor_up_from_first_line);
    tcase_add_test(tc_core, test_cursor_up_column_preservation);
    tcase_add_test(tc_core, test_cursor_up_shorter_line);
    tcase_add_test(tc_core, test_cursor_up_empty_line);
    tcase_add_test(tc_core, test_cursor_up_utf8);

    suite_add_tcase(s, tc_core);

#if !defined(NDEBUG) && !defined(SKIP_SIGNAL_TESTS)
    /* Assertion tests */
    TCase *tc_assertions = tcase_create("Assertions");
    tcase_set_timeout(tc_assertions, IK_TEST_TIMEOUT);
    tcase_set_timeout(tc_assertions, IK_TEST_TIMEOUT);
    tcase_set_timeout(tc_assertions, IK_TEST_TIMEOUT);
    tcase_set_timeout(tc_assertions, IK_TEST_TIMEOUT);
    tcase_set_timeout(tc_assertions, IK_TEST_TIMEOUT); // Longer timeout for valgrind
    tcase_add_test_raise_signal(tc_assertions, test_cursor_up_null_input_buffer_asserts, SIGABRT);
    suite_add_tcase(s, tc_assertions);
#endif

    return s;
}

int main(void)
{
    int number_failed;
    Suite *s = input_buffer_cursor_up_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
