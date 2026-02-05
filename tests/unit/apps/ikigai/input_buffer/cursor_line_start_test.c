/**
 * @file cursor_line_start_test.c
 * @brief Unit tests for input_buffer cursor to line start (Ctrl+A)
 */

#include <check.h>
#include <signal.h>
#include <talloc.h>
#include "apps/ikigai/input_buffer/core.h"
#include "tests/helpers/test_utils_helper.h"

/* Test: Cursor to line start - basic */
START_TEST(test_cursor_to_line_start_basic) {
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
START_TEST(test_cursor_to_line_start_first_line) {
    void *ctx = talloc_new(NULL);
    ik_input_buffer_t *input_buffer = NULL;

    input_buffer = ik_input_buffer_create(ctx);

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
START_TEST(test_cursor_to_line_start_already_at_start) {
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
START_TEST(test_cursor_to_line_start_after_newline) {
    void *ctx = talloc_new(NULL);
    ik_input_buffer_t *input_buffer = NULL;

    input_buffer = ik_input_buffer_create(ctx);

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

#if !defined(NDEBUG) && !defined(SKIP_SIGNAL_TESTS)
/* Test: NULL input_buffer should assert */
START_TEST(test_cursor_to_line_start_null_input_buffer_asserts) {
    /* input_buffer cannot be NULL - should abort */
    ik_input_buffer_cursor_to_line_start(NULL);
}

END_TEST
#endif

static Suite *input_buffer_cursor_line_start_suite(void)
{
    Suite *s = suite_create("Input Buffer Cursor to Line Start");
    TCase *tc_core = tcase_create("Core");
    tcase_set_timeout(tc_core, IK_TEST_TIMEOUT);
    tcase_set_timeout(tc_core, IK_TEST_TIMEOUT);
    tcase_set_timeout(tc_core, IK_TEST_TIMEOUT);
    tcase_set_timeout(tc_core, IK_TEST_TIMEOUT);

    /* Normal tests - cursor to line start */
    tcase_add_test(tc_core, test_cursor_to_line_start_basic);
    tcase_add_test(tc_core, test_cursor_to_line_start_first_line);
    tcase_add_test(tc_core, test_cursor_to_line_start_already_at_start);
    tcase_add_test(tc_core, test_cursor_to_line_start_after_newline);

    suite_add_tcase(s, tc_core);

#if !defined(NDEBUG) && !defined(SKIP_SIGNAL_TESTS)
    /* Assertion tests */
    TCase *tc_assertions = tcase_create("Assertions");
    tcase_set_timeout(tc_assertions, IK_TEST_TIMEOUT);
    tcase_set_timeout(tc_assertions, IK_TEST_TIMEOUT);
    tcase_set_timeout(tc_assertions, IK_TEST_TIMEOUT);
    tcase_set_timeout(tc_assertions, IK_TEST_TIMEOUT);
    tcase_set_timeout(tc_assertions, IK_TEST_TIMEOUT); // Longer timeout for valgrind
    tcase_add_test_raise_signal(tc_assertions, test_cursor_to_line_start_null_input_buffer_asserts, SIGABRT);
    suite_add_tcase(s, tc_assertions);
#endif

    return s;
}

int main(void)
{
    int number_failed;
    Suite *s = input_buffer_cursor_line_start_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/apps/ikigai/input_buffer/cursor_line_start_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
