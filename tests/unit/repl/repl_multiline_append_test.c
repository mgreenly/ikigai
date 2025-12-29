/**
 * @file repl_multiline_append_test.c
 * @brief Unit tests for append_multiline_to_scrollback edge cases
 */

#include <check.h>
#include <talloc.h>
#include "../../../src/repl_actions.h"
#include "../../../src/scrollback.h"

/* Test: Empty output (line 27-28 coverage) */
START_TEST(test_append_empty_output) {
    void *ctx = talloc_new(NULL);

    /* Create scrollback */
    ik_scrollback_t *scrollback = ik_scrollback_create(ctx, 80);

    /* Append empty string */
    ik_repl_append_multiline_to_scrollback(scrollback, "", 0);

    /* Verify no lines were added */
    size_t line_count = ik_scrollback_get_line_count(scrollback);
    ck_assert_uint_eq(line_count, 0);

    talloc_free(ctx);
}

END_TEST
/* Test: Output ending with newline (line 37 branch 2 coverage) */
START_TEST(test_append_output_ending_with_newline)
{
    void *ctx = talloc_new(NULL);

    /* Create scrollback */
    ik_scrollback_t *scrollback = ik_scrollback_create(ctx, 80);

    /* Append string ending with newline */
    const char *text = "Line 1\n";
    ik_repl_append_multiline_to_scrollback(scrollback, text, strlen(text));

    /* Verify one line was added (no empty line after trailing newline) */
    size_t line_count = ik_scrollback_get_line_count(scrollback);
    ck_assert_uint_eq(line_count, 1);

    /* Verify the line content */
    const char *line_text = NULL;
    size_t line_len = 0;
    res_t res = ik_scrollback_get_line_text(scrollback, 0, &line_text, &line_len);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(line_len, 6);
    ck_assert_mem_eq(line_text, "Line 1", 6);

    talloc_free(ctx);
}

END_TEST
/* Test: Multiple lines ending with newline */
START_TEST(test_append_multiple_lines_ending_with_newline)
{
    void *ctx = talloc_new(NULL);

    /* Create scrollback */
    ik_scrollback_t *scrollback = ik_scrollback_create(ctx, 80);

    /* Append multiple lines ending with newline */
    const char *text = "Line 1\nLine 2\nLine 3\n";
    ik_repl_append_multiline_to_scrollback(scrollback, text, strlen(text));

    /* Verify three lines were added (no empty line after trailing newline) */
    size_t line_count = ik_scrollback_get_line_count(scrollback);
    ck_assert_uint_eq(line_count, 3);

    talloc_free(ctx);
}

END_TEST
/* Test: Just a newline character */
START_TEST(test_append_just_newline)
{
    void *ctx = talloc_new(NULL);

    /* Create scrollback */
    ik_scrollback_t *scrollback = ik_scrollback_create(ctx, 80);

    /* Append just a newline */
    ik_repl_append_multiline_to_scrollback(scrollback, "\n", 1);

    /* Verify one empty line was added */
    size_t line_count = ik_scrollback_get_line_count(scrollback);
    ck_assert_uint_eq(line_count, 1);

    /* Verify the line is empty */
    const char *line_text = NULL;
    size_t line_len = 0;
    res_t res = ik_scrollback_get_line_text(scrollback, 0, &line_text, &line_len);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(line_len, 0);

    talloc_free(ctx);
}

END_TEST

/* Test Suite */
static Suite *repl_multiline_append_suite(void)
{
    Suite *s = suite_create("REPL Multiline Append");

    TCase *tc_edge_cases = tcase_create("Edge Cases");
    tcase_set_timeout(tc_edge_cases, 30);
    tcase_set_timeout(tc_edge_cases, 30);
    tcase_set_timeout(tc_edge_cases, 30);
    tcase_set_timeout(tc_edge_cases, 30);
    tcase_set_timeout(tc_edge_cases, 30);
    tcase_add_test(tc_edge_cases, test_append_empty_output);
    tcase_add_test(tc_edge_cases, test_append_output_ending_with_newline);
    tcase_add_test(tc_edge_cases, test_append_multiple_lines_ending_with_newline);
    tcase_add_test(tc_edge_cases, test_append_just_newline);
    suite_add_tcase(s, tc_edge_cases);

    return s;
}

int main(void)
{
    int number_failed;
    Suite *s = repl_multiline_append_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
