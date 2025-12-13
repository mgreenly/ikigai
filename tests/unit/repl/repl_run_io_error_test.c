/**
 * @file repl_run_io_error_test.c
 * @brief Unit tests for REPL I/O error handling (read/select errors)
 */

#include "repl_run_test_common.h"
#include "../../../src/agent.h"
#include "../../../src/shared.h"
#include "../../../src/repl_actions.h"
#include <errno.h>

/* Test: read() returns -1 with EINTR (should continue loop) */
START_TEST(test_repl_run_read_error_eintr) {
    void *ctx = talloc_new(NULL);

    ik_input_buffer_t *input_buf = NULL;
    res_t res;
    input_buf = ik_input_buffer_create(ctx);

    ik_input_parser_t *parser = ik_input_parser_create(ctx);

    ik_term_ctx_t *term = talloc_zero(ctx, ik_term_ctx_t);
    ck_assert_ptr_nonnull(term);
    term->tty_fd = 0;
    term->screen_rows = 24;
    term->screen_cols = 80;

    ik_render_ctx_t *render = NULL;
    res = ik_render_create(ctx, 24, 80, 1, &render);
    ck_assert(is_ok(&res));

    ik_scrollback_t *scrollback = ik_scrollback_create(ctx, 80);

    ik_repl_ctx_t *repl = talloc_zero(ctx, ik_repl_ctx_t);
    ck_assert_ptr_nonnull(repl);
    repl->input_buffer = input_buf;
    repl->input_parser = parser;
    ik_shared_ctx_t *shared = talloc_zero(repl, ik_shared_ctx_t);
    repl->shared = shared;
    shared->term = term;
    shared->render = render;

    // Create agent context for display state
    ik_agent_ctx_t *agent = talloc_zero(repl, ik_agent_ctx_t);
    repl->current = agent;
    repl->current->scrollback = scrollback;
    repl->current->viewport_offset = 0;
    repl->quit = false;
    init_repl_multi_handle(repl);

    // First call to read fails with EINTR, second call returns EOF
    mock_read_fail_count = 0;  // Fail once (count starts at 0, decrements to -1)
    mock_read_errno = EINTR;
    mock_input = "";  // Will return EOF after failed read
    mock_input_pos = 0;

    res = ik_repl_run(repl);
    ck_assert(is_ok(&res));  // Should handle EINTR gracefully and continue

    // Reset mock
    mock_read_fail_count = -1;

    talloc_free(ctx);
}

END_TEST
/* Test: read() returns -1 with non-EINTR error (should exit) */
START_TEST(test_repl_run_read_error_other)
{
    void *ctx = talloc_new(NULL);

    ik_input_buffer_t *input_buf = NULL;
    res_t res;
    input_buf = ik_input_buffer_create(ctx);

    ik_input_parser_t *parser = ik_input_parser_create(ctx);

    ik_term_ctx_t *term = talloc_zero(ctx, ik_term_ctx_t);
    ck_assert_ptr_nonnull(term);
    term->tty_fd = 0;
    term->screen_rows = 24;
    term->screen_cols = 80;

    ik_render_ctx_t *render = NULL;
    res = ik_render_create(ctx, 24, 80, 1, &render);
    ck_assert(is_ok(&res));

    ik_scrollback_t *scrollback = ik_scrollback_create(ctx, 80);

    ik_repl_ctx_t *repl = talloc_zero(ctx, ik_repl_ctx_t);
    ik_shared_ctx_t *shared = talloc_zero(repl, ik_shared_ctx_t);
    repl->shared = shared;
    ck_assert_ptr_nonnull(repl);
    repl->input_buffer = input_buf;
    repl->input_parser = parser;
    shared->term = term;
    shared->render = render;

    // Create agent context for display state
    ik_agent_ctx_t *agent = talloc_zero(repl, ik_agent_ctx_t);
    repl->current = agent;
    repl->current->scrollback = scrollback;
    repl->current->viewport_offset = 0;
    repl->quit = false;
    init_repl_multi_handle(repl);

    // Read fails with EIO (non-EINTR error)
    mock_read_fail_count = 0;  // Fail once
    mock_read_errno = EIO;
    mock_input = "";
    mock_input_pos = 0;

    res = ik_repl_run(repl);
    ck_assert(is_ok(&res));  // Should exit gracefully

    // Reset mock
    mock_read_fail_count = -1;

    talloc_free(ctx);
}

END_TEST
/* Test: select() returns -1 with EINTR (should check resize and continue) */
START_TEST(test_repl_run_select_error_eintr)
{
    void *ctx = talloc_new(NULL);

    ik_input_buffer_t *input_buf = NULL;
    res_t res;
    input_buf = ik_input_buffer_create(ctx);

    ik_input_parser_t *parser = ik_input_parser_create(ctx);

    ik_term_ctx_t *term = talloc_zero(ctx, ik_term_ctx_t);
    ck_assert_ptr_nonnull(term);
    term->tty_fd = 0;
    term->screen_rows = 24;
    term->screen_cols = 80;

    ik_render_ctx_t *render = NULL;
    res = ik_render_create(ctx, 24, 80, 1, &render);
    ck_assert(is_ok(&res));

    ik_scrollback_t *scrollback = ik_scrollback_create(ctx, 80);

    ik_repl_ctx_t *repl = talloc_zero(ctx, ik_repl_ctx_t);
    ik_shared_ctx_t *shared = talloc_zero(repl, ik_shared_ctx_t);
    repl->shared = shared;
    ck_assert_ptr_nonnull(repl);
    repl->input_buffer = input_buf;
    repl->input_parser = parser;
    shared->term = term;
    shared->render = render;

    // Create agent context for display state
    ik_agent_ctx_t *agent = talloc_zero(repl, ik_agent_ctx_t);
    repl->current = agent;
    repl->current->scrollback = scrollback;
    repl->current->viewport_offset = 0;
    repl->quit = false;
    init_repl_multi_handle(repl);

    // First select call fails with EINTR, next call succeeds with EOF
    mock_select_return_value = -1;
    mock_select_errno = EINTR;
    mock_select_call_count = 0;
    mock_select_return_on_call = 0;  // Only first call fails
    mock_input = "";  // EOF on read

    res = ik_repl_run(repl);
    ck_assert(is_ok(&res));  // Should handle EINTR and continue

    // Reset mock
    mock_select_return_value = -999;
    mock_select_errno = -1;
    mock_select_call_count = 0;
    mock_select_return_on_call = -1;

    talloc_free(ctx);
}

END_TEST
/* Test: select() returns -1 with non-EINTR error (should break loop) */
START_TEST(test_repl_run_select_error_other)
{
    void *ctx = talloc_new(NULL);

    ik_input_buffer_t *input_buf = NULL;
    res_t res;
    input_buf = ik_input_buffer_create(ctx);

    ik_input_parser_t *parser = ik_input_parser_create(ctx);

    ik_term_ctx_t *term = talloc_zero(ctx, ik_term_ctx_t);
    ck_assert_ptr_nonnull(term);
    term->tty_fd = 0;
    term->screen_rows = 24;
    term->screen_cols = 80;

    ik_render_ctx_t *render = NULL;
    res = ik_render_create(ctx, 24, 80, 1, &render);
    ck_assert(is_ok(&res));

    ik_scrollback_t *scrollback = ik_scrollback_create(ctx, 80);

    ik_repl_ctx_t *repl = talloc_zero(ctx, ik_repl_ctx_t);
    ik_shared_ctx_t *shared = talloc_zero(repl, ik_shared_ctx_t);
    repl->shared = shared;
    ck_assert_ptr_nonnull(repl);
    repl->input_buffer = input_buf;
    repl->input_parser = parser;
    shared->term = term;
    shared->render = render;

    // Create agent context for display state
    ik_agent_ctx_t *agent = talloc_zero(repl, ik_agent_ctx_t);
    repl->current = agent;
    repl->current->scrollback = scrollback;
    repl->current->viewport_offset = 0;
    repl->quit = false;
    init_repl_multi_handle(repl);

    // Select fails with EBADF (non-EINTR error)
    mock_select_return_value = -1;
    mock_select_errno = EBADF;
    mock_select_call_count = 0;
    mock_select_return_on_call = -1;  // Always fail

    res = ik_repl_run(repl);
    ck_assert(is_ok(&res));  // Should break loop and exit gracefully

    // Reset mock
    mock_select_return_value = -999;
    mock_select_errno = -1;
    mock_select_call_count = 0;

    talloc_free(ctx);
}

END_TEST

static Suite *repl_run_io_error_suite(void)
{
    Suite *s = suite_create("REPL_Run_IO_Error");
    TCase *tc_core = tcase_create("Core");
    tcase_set_timeout(tc_core, 30);

    tcase_add_test(tc_core, test_repl_run_read_error_eintr);
    tcase_add_test(tc_core, test_repl_run_read_error_other);
    tcase_add_test(tc_core, test_repl_run_select_error_eintr);
    tcase_add_test(tc_core, test_repl_run_select_error_other);

    suite_add_tcase(s, tc_core);
    return s;
}

int main(void)
{
    int32_t number_failed;
    Suite *s = repl_run_io_error_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
