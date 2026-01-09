/**
 * @file cursor_line_end_test.c
 * @brief Unit tests for input_buffer cursor to line end (Ctrl+E)
 */

#include <check.h>
#include <signal.h>
#include <talloc.h>
#include "../../../src/input_buffer/core.h"
#include "../../test_utils.h"

/* Test: Cursor to line end - basic */
START_TEST(test_cursor_to_line_end_basic) {
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

    /* Cursor is at end of "world" (byte 11, after 'd') */
    /* Move cursor to start of "world" - move left 5 times */
    ik_input_buffer_cursor_left(input_buffer);
    ik_input_buffer_cursor_left(input_buffer);
    ik_input_buffer_cursor_left(input_buffer);
    ik_input_buffer_cursor_left(input_buffer);
    ik_input_buffer_cursor_left(input_buffer);

    /* Cursor should be at byte 6 (start of "world") */
    size_t byte_offset = 0;
    size_t grapheme_offset = 0;
    res_t res = ik_input_buffer_get_cursor_position(input_buffer, &byte_offset, &grapheme_offset);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(byte_offset, 6);

    /* Call cursor_to_line_end - should move to end of "world" (byte 11, after 'd') */
    res = ik_input_buffer_cursor_to_line_end(input_buffer);
    ck_assert(is_ok(&res));

    /* Verify cursor at byte 11 (end of "world") */
    res = ik_input_buffer_get_cursor_position(input_buffer, &byte_offset, &grapheme_offset);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(byte_offset, 11);  /* "hello\nworld" = 11 bytes */
    ck_assert_uint_eq(grapheme_offset, 11);

    talloc_free(ctx);
}

END_TEST
/* Test: Cursor to line end - last line */
START_TEST(test_cursor_to_line_end_last_line) {
    void *ctx = talloc_new(NULL);
    ik_input_buffer_t *input_buffer = NULL;

    input_buffer = ik_input_buffer_create(ctx);

    /* Insert "hello" (single line) */
    ik_input_buffer_insert_codepoint(input_buffer, 'h');
    ik_input_buffer_insert_codepoint(input_buffer, 'e');
    ik_input_buffer_insert_codepoint(input_buffer, 'l');
    ik_input_buffer_insert_codepoint(input_buffer, 'l');
    ik_input_buffer_insert_codepoint(input_buffer, 'o');

    /* Move to middle - move left twice */
    ik_input_buffer_cursor_left(input_buffer);
    ik_input_buffer_cursor_left(input_buffer);

    /* Cursor should be at byte 3 */
    size_t byte_offset = 0;
    size_t grapheme_offset = 0;
    res_t res = ik_input_buffer_get_cursor_position(input_buffer, &byte_offset, &grapheme_offset);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(byte_offset, 3);

    /* Call cursor_to_line_end - should move to byte 5 (end of text) */
    res = ik_input_buffer_cursor_to_line_end(input_buffer);
    ck_assert(is_ok(&res));

    /* Verify cursor at byte 5 */
    res = ik_input_buffer_get_cursor_position(input_buffer, &byte_offset, &grapheme_offset);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(byte_offset, 5);
    ck_assert_uint_eq(grapheme_offset, 5);

    talloc_free(ctx);
}

END_TEST
/* Test: Cursor to line end - already at end */
START_TEST(test_cursor_to_line_end_already_at_end) {
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

    /* Cursor is already at end of "world" (byte 11) */
    size_t byte_offset = 0;
    size_t grapheme_offset = 0;
    res_t res = ik_input_buffer_get_cursor_position(input_buffer, &byte_offset, &grapheme_offset);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(byte_offset, 11);

    /* Call cursor_to_line_end - should remain at byte 11 (no-op) */
    res = ik_input_buffer_cursor_to_line_end(input_buffer);
    ck_assert(is_ok(&res));

    /* Verify cursor still at byte 11 */
    res = ik_input_buffer_get_cursor_position(input_buffer, &byte_offset, &grapheme_offset);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(byte_offset, 11);
    ck_assert_uint_eq(grapheme_offset, 11);

    talloc_free(ctx);
}

END_TEST
/* Test: Cursor to line end - before newline */
START_TEST(test_cursor_to_line_end_before_newline) {
    void *ctx = talloc_new(NULL);
    ik_input_buffer_t *input_buffer = NULL;

    input_buffer = ik_input_buffer_create(ctx);

    /* Insert "hello\nworld\ntest" */
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
    ik_input_buffer_insert_newline(input_buffer);
    ik_input_buffer_insert_codepoint(input_buffer, 't');
    ik_input_buffer_insert_codepoint(input_buffer, 'e');
    ik_input_buffer_insert_codepoint(input_buffer, 's');
    ik_input_buffer_insert_codepoint(input_buffer, 't');

    /* Cursor is at end of "test" (byte 16) */
    /* Move to first line ("hello") - move left many times */
    for (int i = 0; i < 10; i++) {
        ik_input_buffer_cursor_left(input_buffer);
    }

    /* Cursor should be at byte 6 (start of "world") */
    /* Move left 5 more times to get to start of "hello" */
    for (int i = 0; i < 6; i++) {
        ik_input_buffer_cursor_left(input_buffer);
    }

    /* Cursor should be at byte 0 (start of "hello") */
    size_t byte_offset = 0;
    size_t grapheme_offset = 0;
    res_t res = ik_input_buffer_get_cursor_position(input_buffer, &byte_offset, &grapheme_offset);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(byte_offset, 0);

    /* Call cursor_to_line_end - should move to byte 5 (before \n) */
    res = ik_input_buffer_cursor_to_line_end(input_buffer);
    ck_assert(is_ok(&res));

    /* Verify cursor at byte 5 (end of "hello", before \n) */
    res = ik_input_buffer_get_cursor_position(input_buffer, &byte_offset, &grapheme_offset);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(byte_offset, 5);  /* "hello" = 5 bytes, before \n */
    ck_assert_uint_eq(grapheme_offset, 5);

    talloc_free(ctx);
}

END_TEST

#if !defined(NDEBUG) && !defined(SKIP_SIGNAL_TESTS)
/* Test: NULL input_buffer should assert */
START_TEST(test_cursor_to_line_end_null_input_buffer_asserts) {
    /* input_buffer cannot be NULL - should abort */
    ik_input_buffer_cursor_to_line_end(NULL);
}

END_TEST
#endif

static Suite *input_buffer_cursor_line_end_suite(void)
{
    Suite *s = suite_create("Input Buffer Cursor to Line End");
    TCase *tc_core = tcase_create("Core");
    tcase_set_timeout(tc_core, 30);
    tcase_set_timeout(tc_core, 30);
    tcase_set_timeout(tc_core, 30);
    tcase_set_timeout(tc_core, 30);

    /* Normal tests - cursor to line end */
    tcase_add_test(tc_core, test_cursor_to_line_end_basic);
    tcase_add_test(tc_core, test_cursor_to_line_end_last_line);
    tcase_add_test(tc_core, test_cursor_to_line_end_already_at_end);
    tcase_add_test(tc_core, test_cursor_to_line_end_before_newline);

    suite_add_tcase(s, tc_core);

#if !defined(NDEBUG) && !defined(SKIP_SIGNAL_TESTS)
    /* Assertion tests */
    TCase *tc_assertions = tcase_create("Assertions");
    tcase_set_timeout(tc_assertions, 30);
    tcase_set_timeout(tc_assertions, 30);
    tcase_set_timeout(tc_assertions, 30);
    tcase_set_timeout(tc_assertions, 30);
    tcase_set_timeout(tc_assertions, 30); // Longer timeout for valgrind
    tcase_add_test_raise_signal(tc_assertions, test_cursor_to_line_end_null_input_buffer_asserts, SIGABRT);
    suite_add_tcase(s, tc_assertions);
#endif

    return s;
}

int main(void)
{
    int number_failed;
    Suite *s = input_buffer_cursor_line_end_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
