/**
 * @file kill_to_line_end_test.c
 * @brief Unit tests for input_buffer kill_to_line_end operation (Ctrl+K)
 */

#include <check.h>
#include <signal.h>
#include <talloc.h>
#include "../../../src/input_buffer/core.h"
#include "../../test_utils.h"

/* Test: kill_to_line_end basic operation */
START_TEST(test_kill_to_line_end_basic) {
    void *ctx = talloc_new(NULL);
    ik_input_buffer_t *input_buffer = NULL;

    input_buffer = ik_input_buffer_create(ctx);

    /* Insert "hello world" */
    ik_input_buffer_insert_codepoint(input_buffer, 'h');
    ik_input_buffer_insert_codepoint(input_buffer, 'e');
    ik_input_buffer_insert_codepoint(input_buffer, 'l');
    ik_input_buffer_insert_codepoint(input_buffer, 'l');
    ik_input_buffer_insert_codepoint(input_buffer, 'o');
    ik_input_buffer_insert_codepoint(input_buffer, ' ');
    ik_input_buffer_insert_codepoint(input_buffer, 'w');
    ik_input_buffer_insert_codepoint(input_buffer, 'o');
    ik_input_buffer_insert_codepoint(input_buffer, 'r');
    ik_input_buffer_insert_codepoint(input_buffer, 'l');
    ik_input_buffer_insert_codepoint(input_buffer, 'd');

    /* Move cursor to position 6 (after "hello ") */
    for (int i = 0; i < 5; i++) {
        ik_input_buffer_cursor_left(input_buffer);
    }

    /* Get cursor position before kill */
    size_t cursor_before = 0;
    size_t grapheme_before = 0;
    res_t res = ik_input_buffer_get_cursor_position(input_buffer, &cursor_before, &grapheme_before);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(cursor_before, 6); /* After "hello " */

    /* Action: kill to line end */
    res = ik_input_buffer_kill_to_line_end(input_buffer);
    ck_assert(is_ok(&res));

    /* Assert: text is "hello ", cursor unchanged */
    size_t result_len = 0;
    const char *result_text = ik_input_buffer_get_text(input_buffer, &result_len);
    ck_assert_uint_eq(result_len, 6);
    ck_assert_mem_eq(result_text, "hello ", 6);

    size_t cursor_after = 0;
    size_t grapheme_after = 0;
    res = ik_input_buffer_get_cursor_position(input_buffer, &cursor_after, &grapheme_after);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(cursor_after, 6);

    talloc_free(ctx);
}
END_TEST
/* Test: kill_to_line_end when cursor is at newline */
START_TEST(test_kill_to_line_end_at_newline) {
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

    /* Move cursor back to just after "hello" (before \n) */
    for (int i = 0; i < 6; i++) {
        ik_input_buffer_cursor_left(input_buffer);
    }

    size_t cursor_before = 0;
    size_t grapheme_before = 0;
    res_t res = ik_input_buffer_get_cursor_position(input_buffer, &cursor_before, &grapheme_before);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(cursor_before, 5); /* After "hello" */

    /* Action: kill to line end (should not delete the newline) */
    res = ik_input_buffer_kill_to_line_end(input_buffer);
    ck_assert(is_ok(&res));

    /* Assert: text is "hello\nworld", cursor unchanged (newline not deleted) */
    size_t result_len = 0;
    const char *result_text = ik_input_buffer_get_text(input_buffer, &result_len);
    ck_assert_uint_eq(result_len, 11); /* "hello\nworld" */
    ck_assert_mem_eq(result_text, "hello\nworld", 11);

    size_t cursor_after = 0;
    size_t grapheme_after = 0;
    res = ik_input_buffer_get_cursor_position(input_buffer, &cursor_after, &grapheme_after);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(cursor_after, 5);

    talloc_free(ctx);
}

END_TEST
/* Test: kill_to_line_end when already at line end */
START_TEST(test_kill_to_line_end_already_at_end) {
    void *ctx = talloc_new(NULL);
    ik_input_buffer_t *input_buffer = NULL;

    input_buffer = ik_input_buffer_create(ctx);

    /* Insert "hello" */
    ik_input_buffer_insert_codepoint(input_buffer, 'h');
    ik_input_buffer_insert_codepoint(input_buffer, 'e');
    ik_input_buffer_insert_codepoint(input_buffer, 'l');
    ik_input_buffer_insert_codepoint(input_buffer, 'l');
    ik_input_buffer_insert_codepoint(input_buffer, 'o');

    size_t cursor_before = 0;
    size_t grapheme_before = 0;
    res_t res = ik_input_buffer_get_cursor_position(input_buffer, &cursor_before, &grapheme_before);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(cursor_before, 5); /* At end */

    /* Action: kill to line end (should be no-op) */
    res = ik_input_buffer_kill_to_line_end(input_buffer);
    ck_assert(is_ok(&res));

    /* Assert: text unchanged */
    size_t result_len = 0;
    const char *result_text = ik_input_buffer_get_text(input_buffer, &result_len);
    ck_assert_uint_eq(result_len, 5);
    ck_assert_mem_eq(result_text, "hello", 5);

    size_t cursor_after = 0;
    size_t grapheme_after = 0;
    res = ik_input_buffer_get_cursor_position(input_buffer, &cursor_after, &grapheme_after);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(cursor_after, 5);

    talloc_free(ctx);
}

END_TEST
/* Test: kill_to_line_end in multiline text */
START_TEST(test_kill_to_line_end_multiline) {
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

    /* Move cursor to middle of line2 (after "li") */
    /* Current position: after "line3" (17) */
    /* Target position: after "li" in line2 (8) */
    for (int i = 0; i < 9; i++) {
        ik_input_buffer_cursor_left(input_buffer);
    }

    size_t cursor_before = 0;
    size_t grapheme_before = 0;
    res_t res = ik_input_buffer_get_cursor_position(input_buffer, &cursor_before, &grapheme_before);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(cursor_before, 8); /* After "line1\nli" */

    /* Action: kill to line end (should only delete "ne2" from line2) */
    res = ik_input_buffer_kill_to_line_end(input_buffer);
    ck_assert(is_ok(&res));

    /* Assert: text is "line1\nli\nline3", cursor unchanged */
    size_t result_len = 0;
    const char *result_text = ik_input_buffer_get_text(input_buffer, &result_len);
    ck_assert_uint_eq(result_len, 14); /* "line1\nli\nline3" */
    ck_assert_mem_eq(result_text, "line1\nli\nline3", 14);

    size_t cursor_after = 0;
    size_t grapheme_after = 0;
    res = ik_input_buffer_get_cursor_position(input_buffer, &cursor_after, &grapheme_after);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(cursor_after, 8);

    talloc_free(ctx);
}

END_TEST

#if !defined(NDEBUG) && !defined(SKIP_SIGNAL_TESTS)
/* Test: NULL input_buffer should assert */
START_TEST(test_kill_to_line_end_null_input_buffer_asserts) {
    /* input_buffer cannot be NULL - should abort */
    ik_input_buffer_kill_to_line_end(NULL);
}

END_TEST
#endif

static Suite *input_buffer_kill_to_line_end_suite(void)
{
    Suite *s = suite_create("Input Buffer Kill To Line End");
    TCase *tc_core = tcase_create("Core");
    tcase_set_timeout(tc_core, 30);
    tcase_set_timeout(tc_core, 30);
    tcase_set_timeout(tc_core, 30);
    tcase_set_timeout(tc_core, 30);

    /* Normal tests */
    tcase_add_test(tc_core, test_kill_to_line_end_basic);
    tcase_add_test(tc_core, test_kill_to_line_end_at_newline);
    tcase_add_test(tc_core, test_kill_to_line_end_already_at_end);
    tcase_add_test(tc_core, test_kill_to_line_end_multiline);

    suite_add_tcase(s, tc_core);

#if !defined(NDEBUG) && !defined(SKIP_SIGNAL_TESTS)
    /* Assertion tests */
    TCase *tc_assertions = tcase_create("Assertions");
    tcase_set_timeout(tc_assertions, 30);
    tcase_set_timeout(tc_assertions, 30);
    tcase_set_timeout(tc_assertions, 30);
    tcase_set_timeout(tc_assertions, 30);
    tcase_set_timeout(tc_assertions, 30); // Longer timeout for valgrind
    tcase_add_test_raise_signal(tc_assertions, test_kill_to_line_end_null_input_buffer_asserts, SIGABRT);
    suite_add_tcase(s, tc_assertions);
#endif

    return s;
}

int main(void)
{
    int number_failed;
    Suite *s = input_buffer_kill_to_line_end_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
