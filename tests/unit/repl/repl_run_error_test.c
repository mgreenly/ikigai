/**
 * @file repl_run_error_test.c
 * @brief Unit tests for REPL event loop error handling
 */

#include "repl_run_test_common.h"

/* Test: Initial render error */
START_TEST(test_repl_run_initial_render_error) {
    void *ctx = talloc_new(NULL);

    ik_input_buffer_t *input_buf = NULL;
    res_t res = ik_input_buffer_create(ctx, &input_buf);
    ck_assert(is_ok(&res));

    ik_input_parser_t *parser = NULL;
    res = ik_input_parser_create(ctx, &parser);
    ck_assert(is_ok(&res));

    ik_term_ctx_t *term = talloc_zero(ctx, ik_term_ctx_t);
    ck_assert_ptr_nonnull(term);
    term->tty_fd = 0;
    term->screen_rows = 24;
    term->screen_cols = 80;

    ik_render_ctx_t *render = NULL;
    res = ik_render_create(ctx, 24, 80, 1, &render);
    ck_assert(is_ok(&res));

    ik_scrollback_t *scrollback = NULL;
    res = ik_scrollback_create(ctx, 80, &scrollback);
    ck_assert(is_ok(&res));

    ik_repl_ctx_t *repl = talloc_zero(ctx, ik_repl_ctx_t);
    ck_assert_ptr_nonnull(repl);
    repl->input_buffer = input_buf;
    repl->input_parser = parser;
    repl->term = term;
    repl->render = render;
    repl->scrollback = scrollback;
    repl->viewport_offset = 0;
    repl->quit = false;

    mock_write_should_fail = true;

    res = ik_repl_run(repl);
    ck_assert(is_err(&res));

    mock_write_should_fail = false;

    talloc_free(ctx);
}
END_TEST
/* Test: Render error during event loop */
START_TEST(test_repl_run_render_error_in_loop)
{
    void *ctx = talloc_new(NULL);

    mock_write_should_fail = false;
    mock_write_fail_after = 1;
    mock_write_count = 0;

    ik_input_buffer_t *input_buf = NULL;
    res_t res = ik_input_buffer_create(ctx, &input_buf);
    ck_assert(is_ok(&res));

    ik_input_parser_t *parser = NULL;
    res = ik_input_parser_create(ctx, &parser);
    ck_assert(is_ok(&res));

    ik_term_ctx_t *term = talloc_zero(ctx, ik_term_ctx_t);
    ck_assert_ptr_nonnull(term);
    term->tty_fd = 0;
    term->screen_rows = 24;
    term->screen_cols = 80;

    ik_render_ctx_t *render = NULL;
    res = ik_render_create(ctx, 24, 80, 1, &render);
    ck_assert(is_ok(&res));

    ik_scrollback_t *scrollback = NULL;
    res = ik_scrollback_create(ctx, 80, &scrollback);
    ck_assert(is_ok(&res));

    ik_repl_ctx_t *repl = talloc_zero(ctx, ik_repl_ctx_t);
    ck_assert_ptr_nonnull(repl);
    repl->input_buffer = input_buf;
    repl->input_parser = parser;
    repl->term = term;
    repl->render = render;
    repl->scrollback = scrollback;
    repl->viewport_offset = 0;
    repl->quit = false;

    mock_input = "a";
    mock_input_pos = 0;

    res = ik_repl_run(repl);
    ck_assert(is_err(&res));

    mock_write_fail_after = -1;
    mock_write_count = 0;

    talloc_free(ctx);
}

END_TEST

static Suite *repl_run_error_suite(void)
{
    Suite *s = suite_create("REPL_Run_Error");
    TCase *tc_core = tcase_create("Core");
    tcase_set_timeout(tc_core, 30);

    tcase_add_test(tc_core, test_repl_run_initial_render_error);
    tcase_add_test(tc_core, test_repl_run_render_error_in_loop);

    suite_add_tcase(s, tc_core);
    return s;
}

int main(void)
{
    int32_t number_failed;
    Suite *s = repl_run_error_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
