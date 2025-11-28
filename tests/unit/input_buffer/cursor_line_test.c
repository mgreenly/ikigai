/**
 * @file cursor_line_test.c
 * @brief Unit tests for input_buffer line-based cursor operations (Ctrl+A, Ctrl+E)
 */

#include <check.h>
#include <signal.h>
#include <talloc.h>
#include "../../../src/input_buffer.h"
#include "../../test_utils.h"

/* Test: Cursor to line start - basic */
START_TEST(test_cursor_to_line_start_basic) {
    void *ctx = talloc_new(NULL);
    ik_input_buffer_t *input_buffer = NULL;

    ik_input_buffer_create(ctx, &input_buffer);

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
    /* Position cursor in middle of "world" - move left twice to be after 'r' */
    ik_input_buffer_cursor_left(input_buffer);
    ik_input_buffer_cursor_left(input_buffer);

    /* Cursor should be at byte 9 (after 'r' in "world") */
    size_t byte_offset = 0;
    size_t grapheme_offset = 0;
    res_t res = ik_input_buffer_get_cursor_position(input_buffer, &byte_offset, &grapheme_offset);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(byte_offset, 9);  /* "hello\nwor" = 9 bytes */

    /* Call cursor_to_line_start - should move to start of "world" (after \n) */
    res = ik_input_buffer_cursor_to_line_start(input_buffer);
    ck_assert(is_ok(&res));

    /* Verify cursor at byte 6 (start of "world", after \n) */
    res = ik_input_buffer_get_cursor_position(input_buffer, &byte_offset, &grapheme_offset);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(byte_offset, 6);  /* "hello\n" = 6 bytes */
    ck_assert_uint_eq(grapheme_offset, 6);

    talloc_free(ctx);
}
END_TEST
/* Test: Cursor to line start - first line */
START_TEST(test_cursor_to_line_start_first_line)
{
    void *ctx = talloc_new(NULL);
    ik_input_buffer_t *input_buffer = NULL;

    ik_input_buffer_create(ctx, &input_buffer);

    /* Insert "hello" (single line) */
    ik_input_buffer_insert_codepoint(input_buffer, 'h');
    ik_input_buffer_insert_codepoint(input_buffer, 'e');
    ik_input_buffer_insert_codepoint(input_buffer, 'l');
    ik_input_buffer_insert_codepoint(input_buffer, 'l');
    ik_input_buffer_insert_codepoint(input_buffer, 'o');

    /* Cursor is at end (byte 5) */
    /* Move to middle - move left twice */
    ik_input_buffer_cursor_left(input_buffer);
    ik_input_buffer_cursor_left(input_buffer);

    /* Cursor should be at byte 3 */
    size_t byte_offset = 0;
    size_t grapheme_offset = 0;
    res_t res = ik_input_buffer_get_cursor_position(input_buffer, &byte_offset, &grapheme_offset);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(byte_offset, 3);

    /* Call cursor_to_line_start - should move to byte 0 */
    res = ik_input_buffer_cursor_to_line_start(input_buffer);
    ck_assert(is_ok(&res));

    /* Verify cursor at byte 0 */
    res = ik_input_buffer_get_cursor_position(input_buffer, &byte_offset, &grapheme_offset);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(byte_offset, 0);
    ck_assert_uint_eq(grapheme_offset, 0);

    talloc_free(ctx);
}

END_TEST
/* Test: Cursor to line start - already at start */
START_TEST(test_cursor_to_line_start_already_at_start)
{
    void *ctx = talloc_new(NULL);
    ik_input_buffer_t *input_buffer = NULL;

    ik_input_buffer_create(ctx, &input_buffer);

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

    /* Move cursor to start of "world" line */
    /* Move left 5 times to get to start of "world" */
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

    /* Call cursor_to_line_start - should remain at byte 6 (no-op) */
    res = ik_input_buffer_cursor_to_line_start(input_buffer);
    ck_assert(is_ok(&res));

    /* Verify cursor still at byte 6 */
    res = ik_input_buffer_get_cursor_position(input_buffer, &byte_offset, &grapheme_offset);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(byte_offset, 6);
    ck_assert_uint_eq(grapheme_offset, 6);

    talloc_free(ctx);
}

END_TEST
/* Test: Cursor to line start - after newline */
START_TEST(test_cursor_to_line_start_after_newline)
{
    void *ctx = talloc_new(NULL);
    ik_input_buffer_t *input_buffer = NULL;

    ik_input_buffer_create(ctx, &input_buffer);

    /* Insert "line1\n\nline3" (empty line in middle) */
    ik_input_buffer_insert_codepoint(input_buffer, 'l');
    ik_input_buffer_insert_codepoint(input_buffer, 'i');
    ik_input_buffer_insert_codepoint(input_buffer, 'n');
    ik_input_buffer_insert_codepoint(input_buffer, 'e');
    ik_input_buffer_insert_codepoint(input_buffer, '1');
    ik_input_buffer_insert_newline(input_buffer);
    ik_input_buffer_insert_newline(input_buffer);  /* Empty line */
    ik_input_buffer_insert_codepoint(input_buffer, 'l');
    ik_input_buffer_insert_codepoint(input_buffer, 'i');
    ik_input_buffer_insert_codepoint(input_buffer, 'n');
    ik_input_buffer_insert_codepoint(input_buffer, 'e');
    ik_input_buffer_insert_codepoint(input_buffer, '3');

    /* Cursor is at end of "line3" (byte 13) */
    /* Move to start of line3 (byte 7) using cursor_left */
    ik_input_buffer_cursor_left(input_buffer);
    ik_input_buffer_cursor_left(input_buffer);
    ik_input_buffer_cursor_left(input_buffer);
    ik_input_buffer_cursor_left(input_buffer);
    ik_input_buffer_cursor_left(input_buffer);

    /* Cursor should be at byte 7 (start of "line3") */
    size_t byte_offset = 0;
    size_t grapheme_offset = 0;
    res_t res = ik_input_buffer_get_cursor_position(input_buffer, &byte_offset, &grapheme_offset);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(byte_offset, 7);

    /* Call cursor_to_line_start - should remain at byte 7 (already at start) */
    res = ik_input_buffer_cursor_to_line_start(input_buffer);
    ck_assert(is_ok(&res));

    /* Verify cursor still at byte 7 */
    res = ik_input_buffer_get_cursor_position(input_buffer, &byte_offset, &grapheme_offset);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(byte_offset, 7);
    ck_assert_uint_eq(grapheme_offset, 7);

    /* Now move to the empty line (byte 6) */
    ik_input_buffer_cursor_left(input_buffer);  /* Move to byte 6, which is after second \n */

    res = ik_input_buffer_get_cursor_position(input_buffer, &byte_offset, &grapheme_offset);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(byte_offset, 6);

    /* Call cursor_to_line_start on empty line - should remain at byte 6 */
    res = ik_input_buffer_cursor_to_line_start(input_buffer);
    ck_assert(is_ok(&res));

    /* Verify cursor still at byte 6 */
    res = ik_input_buffer_get_cursor_position(input_buffer, &byte_offset, &grapheme_offset);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(byte_offset, 6);
    ck_assert_uint_eq(grapheme_offset, 6);

    talloc_free(ctx);
}

END_TEST
/* Test: NULL input_buffer should assert */
START_TEST(test_cursor_to_line_start_null_input_buffer_asserts)
{
    /* input_buffer cannot be NULL - should abort */
    ik_input_buffer_cursor_to_line_start(NULL);
}

END_TEST
/* Test: Cursor to line end - basic */
START_TEST(test_cursor_to_line_end_basic)
{
    void *ctx = talloc_new(NULL);
    ik_input_buffer_t *input_buffer = NULL;

    ik_input_buffer_create(ctx, &input_buffer);

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
START_TEST(test_cursor_to_line_end_last_line)
{
    void *ctx = talloc_new(NULL);
    ik_input_buffer_t *input_buffer = NULL;

    ik_input_buffer_create(ctx, &input_buffer);

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
START_TEST(test_cursor_to_line_end_already_at_end)
{
    void *ctx = talloc_new(NULL);
    ik_input_buffer_t *input_buffer = NULL;

    ik_input_buffer_create(ctx, &input_buffer);

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
START_TEST(test_cursor_to_line_end_before_newline)
{
    void *ctx = talloc_new(NULL);
    ik_input_buffer_t *input_buffer = NULL;

    ik_input_buffer_create(ctx, &input_buffer);

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
/* Test: NULL input_buffer should assert */
START_TEST(test_cursor_to_line_end_null_input_buffer_asserts)
{
    /* input_buffer cannot be NULL - should abort */
    ik_input_buffer_cursor_to_line_end(NULL);
}

END_TEST

static Suite *input_buffer_cursor_line_suite(void)
{
    Suite *s = suite_create("Input Buffer Cursor Line Operations");
    TCase *tc_core = tcase_create("Core");
    TCase *tc_assertions = tcase_create("Assertions");
    tcase_set_timeout(tc_assertions, 30); // Longer timeout for valgrind

    /* Normal tests - cursor to line start */
    tcase_add_test(tc_core, test_cursor_to_line_start_basic);
    tcase_add_test(tc_core, test_cursor_to_line_start_first_line);
    tcase_add_test(tc_core, test_cursor_to_line_start_already_at_start);
    tcase_add_test(tc_core, test_cursor_to_line_start_after_newline);

    /* Normal tests - cursor to line end */
    tcase_add_test(tc_core, test_cursor_to_line_end_basic);
    tcase_add_test(tc_core, test_cursor_to_line_end_last_line);
    tcase_add_test(tc_core, test_cursor_to_line_end_already_at_end);
    tcase_add_test(tc_core, test_cursor_to_line_end_before_newline);

    /* Assertion tests */
    tcase_add_test_raise_signal(tc_assertions, test_cursor_to_line_start_null_input_buffer_asserts, SIGABRT);
    tcase_add_test_raise_signal(tc_assertions, test_cursor_to_line_end_null_input_buffer_asserts, SIGABRT);

    suite_add_tcase(s, tc_core);
    suite_add_tcase(s, tc_assertions);
    return s;
}

int main(void)
{
    int number_failed;
    Suite *s = input_buffer_cursor_line_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
