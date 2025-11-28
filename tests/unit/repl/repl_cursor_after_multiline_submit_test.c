/**
 * @file repl_cursor_after_multiline_submit_test.c
 * @brief Test for cursor positioning after multiline input submission
 *
 * Tests that after submitting a multiline input, the viewport calculation
 * correctly accounts for the number of lines in the scrollback.
 */

#include <check.h>
#include <talloc.h>
#include <string.h>
#include "../../../src/repl.h"
#include "../../../src/error.h"
#include "../../test_utils.h"

// Helper to create minimal REPL context for testing
static ik_repl_ctx_t *create_test_repl(void *ctx)
{
    ik_term_ctx_t *term = talloc_zero(ctx, ik_term_ctx_t);
    term->screen_rows = 24;
    term->screen_cols = 80;
    term->tty_fd = -1;

    ik_render_ctx_t *render = NULL;
    res_t res = ik_render_create(ctx, term->screen_rows, term->screen_cols, term->tty_fd, &render);
    ck_assert(is_ok(&res));

    ik_input_buffer_t *input_buf = NULL;
    res = ik_input_buffer_create(ctx, &input_buf);
    ck_assert(is_ok(&res));

    ik_scrollback_t *scrollback = NULL;
    res = ik_scrollback_create(ctx, term->screen_cols, &scrollback);
    ck_assert(is_ok(&res));

    ik_repl_ctx_t *repl = talloc_zero(ctx, ik_repl_ctx_t);
    repl->term = term;
    repl->render = render;
    repl->input_buffer = input_buf;
    repl->scrollback = scrollback;
    repl->viewport_offset = 0;
    return repl;
}

START_TEST(test_scrollback_lines_after_single_line_submit) {
    void *ctx = talloc_new(NULL);
    ik_repl_ctx_t *repl = create_test_repl(ctx);

    res_t res = ik_input_buffer_insert_codepoint(repl->input_buffer, 'A');
    ck_assert(is_ok(&res));
    res = ik_repl_submit_line(repl);
    ck_assert(is_ok(&res));

    ik_input_buffer_ensure_layout(repl->input_buffer, 80);
    ik_scrollback_ensure_layout(repl->scrollback, 80);
    ck_assert_uint_eq(ik_scrollback_get_line_count(repl->scrollback), 1);
    ck_assert_uint_eq((size_t)ik_scrollback_get_total_physical_lines(repl->scrollback), 1);
    ck_assert_uint_eq(repl->input_buffer->physical_lines, 0);

    talloc_free(ctx);
}
END_TEST START_TEST(test_scrollback_lines_after_multiline_submit)
{
    void *ctx = talloc_new(NULL);
    ik_repl_ctx_t *repl = create_test_repl(ctx);

    res_t res = ik_input_buffer_insert_codepoint(repl->input_buffer, 'A');
    ck_assert(is_ok(&res));
    res = ik_input_buffer_insert_codepoint(repl->input_buffer, '\n');
    ck_assert(is_ok(&res));
    res = ik_input_buffer_insert_codepoint(repl->input_buffer, 'B');
    ck_assert(is_ok(&res));
    res = ik_repl_submit_line(repl);
    ck_assert(is_ok(&res));

    ik_input_buffer_ensure_layout(repl->input_buffer, 80);
    ik_scrollback_ensure_layout(repl->scrollback, 80);
    ck_assert_uint_eq(ik_scrollback_get_line_count(repl->scrollback), 1);
    ck_assert_uint_eq((size_t)ik_scrollback_get_total_physical_lines(repl->scrollback), 2);
    ck_assert_uint_eq(repl->input_buffer->physical_lines, 0);

    talloc_free(ctx);
}

END_TEST START_TEST(test_scrollback_lines_with_leading_newline)
{
    void *ctx = talloc_new(NULL);
    ik_repl_ctx_t *repl = create_test_repl(ctx);

    res_t res = ik_input_buffer_insert_codepoint(repl->input_buffer, '\n');
    ck_assert(is_ok(&res));
    res = ik_input_buffer_insert_codepoint(repl->input_buffer, 'A');
    ck_assert(is_ok(&res));
    res = ik_repl_submit_line(repl);
    ck_assert(is_ok(&res));

    ik_input_buffer_ensure_layout(repl->input_buffer, 80);
    ik_scrollback_ensure_layout(repl->scrollback, 80);
    ck_assert_uint_eq(ik_scrollback_get_line_count(repl->scrollback), 1);
    ck_assert_uint_eq((size_t)ik_scrollback_get_total_physical_lines(repl->scrollback), 2);

    talloc_free(ctx);
}

END_TEST START_TEST(test_scrollback_lines_with_trailing_newline)
{
    void *ctx = talloc_new(NULL);
    ik_repl_ctx_t *repl = create_test_repl(ctx);

    res_t res = ik_input_buffer_insert_codepoint(repl->input_buffer, 'A');
    ck_assert(is_ok(&res));
    res = ik_input_buffer_insert_codepoint(repl->input_buffer, '\n');
    ck_assert(is_ok(&res));
    res = ik_repl_submit_line(repl);
    ck_assert(is_ok(&res));

    ik_input_buffer_ensure_layout(repl->input_buffer, 80);
    ik_scrollback_ensure_layout(repl->scrollback, 80);
    ck_assert_uint_eq(ik_scrollback_get_line_count(repl->scrollback), 1);
    ck_assert_uint_eq((size_t)ik_scrollback_get_total_physical_lines(repl->scrollback), 2);

    talloc_free(ctx);
}

END_TEST START_TEST(test_scrollback_lines_after_three_line_submit)
{
    void *ctx = talloc_new(NULL);
    ik_repl_ctx_t *repl = create_test_repl(ctx);

    res_t res = ik_input_buffer_insert_codepoint(repl->input_buffer, 'A');
    ck_assert(is_ok(&res));
    res = ik_input_buffer_insert_codepoint(repl->input_buffer, '\n');
    ck_assert(is_ok(&res));
    res = ik_input_buffer_insert_codepoint(repl->input_buffer, 'B');
    ck_assert(is_ok(&res));
    res = ik_input_buffer_insert_codepoint(repl->input_buffer, '\n');
    ck_assert(is_ok(&res));
    res = ik_input_buffer_insert_codepoint(repl->input_buffer, 'C');
    ck_assert(is_ok(&res));
    res = ik_repl_submit_line(repl);
    ck_assert(is_ok(&res));

    ik_input_buffer_ensure_layout(repl->input_buffer, 80);
    ik_scrollback_ensure_layout(repl->scrollback, 80);
    ck_assert_uint_eq(ik_scrollback_get_line_count(repl->scrollback), 1);
    ck_assert_uint_eq((size_t)ik_scrollback_get_total_physical_lines(repl->scrollback), 3);
    ck_assert_uint_eq(repl->input_buffer->physical_lines, 0);

    talloc_free(ctx);
}

END_TEST START_TEST(test_scrollback_lines_a_b_trailing_newline)
{
    void *ctx = talloc_new(NULL);
    ik_repl_ctx_t *repl = create_test_repl(ctx);

    res_t res = ik_input_buffer_insert_codepoint(repl->input_buffer, 'A');
    ck_assert(is_ok(&res));
    res = ik_input_buffer_insert_codepoint(repl->input_buffer, '\n');
    ck_assert(is_ok(&res));
    res = ik_input_buffer_insert_codepoint(repl->input_buffer, 'B');
    ck_assert(is_ok(&res));
    res = ik_input_buffer_insert_codepoint(repl->input_buffer, '\n');
    ck_assert(is_ok(&res));
    res = ik_repl_submit_line(repl);
    ck_assert(is_ok(&res));

    ik_input_buffer_ensure_layout(repl->input_buffer, 80);
    ik_scrollback_ensure_layout(repl->scrollback, 80);
    ck_assert_uint_eq(ik_scrollback_get_line_count(repl->scrollback), 1);
    ck_assert_uint_eq((size_t)ik_scrollback_get_total_physical_lines(repl->scrollback), 3);

    talloc_free(ctx);
}

END_TEST START_TEST(test_scrollback_lines_single_newline)
{
    void *ctx = talloc_new(NULL);
    ik_repl_ctx_t *repl = create_test_repl(ctx);

    res_t res = ik_input_buffer_insert_codepoint(repl->input_buffer, '\n');
    ck_assert(is_ok(&res));
    res = ik_repl_submit_line(repl);
    ck_assert(is_ok(&res));

    ik_input_buffer_ensure_layout(repl->input_buffer, 80);
    ik_scrollback_ensure_layout(repl->scrollback, 80);
    ck_assert_uint_eq(ik_scrollback_get_line_count(repl->scrollback), 1);
    ck_assert_uint_eq((size_t)ik_scrollback_get_total_physical_lines(repl->scrollback), 1);

    talloc_free(ctx);
}

END_TEST START_TEST(test_scrollback_lines_double_newline)
{
    void *ctx = talloc_new(NULL);
    ik_repl_ctx_t *repl = create_test_repl(ctx);

    res_t res = ik_input_buffer_insert_codepoint(repl->input_buffer, '\n');
    ck_assert(is_ok(&res));
    res = ik_input_buffer_insert_codepoint(repl->input_buffer, '\n');
    ck_assert(is_ok(&res));
    res = ik_repl_submit_line(repl);
    ck_assert(is_ok(&res));

    ik_input_buffer_ensure_layout(repl->input_buffer, 80);
    ik_scrollback_ensure_layout(repl->scrollback, 80);
    ck_assert_uint_eq(ik_scrollback_get_line_count(repl->scrollback), 1);
    ck_assert_uint_eq((size_t)ik_scrollback_get_total_physical_lines(repl->scrollback), 2);

    talloc_free(ctx);
}

END_TEST START_TEST(test_scrollback_lines_content_double_newline)
{
    void *ctx = talloc_new(NULL);
    ik_repl_ctx_t *repl = create_test_repl(ctx);

    res_t res = ik_input_buffer_insert_codepoint(repl->input_buffer, 'A');
    ck_assert(is_ok(&res));
    res = ik_input_buffer_insert_codepoint(repl->input_buffer, '\n');
    ck_assert(is_ok(&res));
    res = ik_input_buffer_insert_codepoint(repl->input_buffer, '\n');
    ck_assert(is_ok(&res));
    res = ik_repl_submit_line(repl);
    ck_assert(is_ok(&res));

    ik_input_buffer_ensure_layout(repl->input_buffer, 80);
    ik_scrollback_ensure_layout(repl->scrollback, 80);
    ck_assert_uint_eq(ik_scrollback_get_line_count(repl->scrollback), 1);
    ck_assert_uint_eq((size_t)ik_scrollback_get_total_physical_lines(repl->scrollback), 3);

    talloc_free(ctx);
}

END_TEST

// Test suite
static Suite *cursor_after_multiline_submit_suite(void)
{
    Suite *s = suite_create("REPL_CursorAfterMultilineSubmit");
    TCase *tc_core = tcase_create("Core");
    tcase_set_timeout(tc_core, 30);

    tcase_add_test(tc_core, test_scrollback_lines_after_single_line_submit);
    tcase_add_test(tc_core, test_scrollback_lines_after_multiline_submit);
    tcase_add_test(tc_core, test_scrollback_lines_with_leading_newline);
    tcase_add_test(tc_core, test_scrollback_lines_with_trailing_newline);
    tcase_add_test(tc_core, test_scrollback_lines_after_three_line_submit);
    tcase_add_test(tc_core, test_scrollback_lines_a_b_trailing_newline);
    tcase_add_test(tc_core, test_scrollback_lines_single_newline);
    tcase_add_test(tc_core, test_scrollback_lines_double_newline);
    tcase_add_test(tc_core, test_scrollback_lines_content_double_newline);

    suite_add_tcase(s, tc_core);
    return s;
}

int main(void)
{
    Suite *s = cursor_after_multiline_submit_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
