#include "agent.h"
/**
 * @file repl_run_curl_error_test.c
 * @brief Unit tests for REPL curl error handling
 */

#include "repl_run_test_common.h"
#include "../../../src/agent.h"
#include "../../../src/shared.h"

/* Test: curl_multi_fdset() error (should propagate error and exit) */
START_TEST(test_repl_run_curl_multi_fdset_error) {
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
    repl->current = talloc_zero(repl, ik_agent_ctx_t);
    ck_assert_ptr_nonnull(repl);
    repl->input_parser = parser;
    ik_shared_ctx_t *shared = talloc_zero(repl, ik_shared_ctx_t);
    repl->shared = shared;
    shared->term = term;
    shared->render = render;

    // Create agent context for display state
    ik_agent_ctx_t *agent = talloc_zero(repl, ik_agent_ctx_t);
    repl->current = agent;
    repl->current->input_buffer = input_buf;
    repl->current->scrollback = scrollback;
    repl->current->viewport_offset = 0;
    repl->quit = false;
    init_repl_multi_handle(repl);

    // Make curl_multi_fdset_ fail
    mock_curl_multi_fdset_should_fail = true;

    res = ik_repl_run(repl);
    ck_assert(is_err(&res));  // Should propagate error from curl_multi_fdset

    // Reset mock
    mock_curl_multi_fdset_should_fail = false;

    talloc_free(ctx);
}

END_TEST
/* Test: curl_multi_perform() error (should propagate error and exit) */
START_TEST(test_repl_run_curl_multi_perform_error)
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
    repl->current = talloc_zero(repl, ik_agent_ctx_t);
    ik_shared_ctx_t *shared = talloc_zero(repl, ik_shared_ctx_t);
    repl->shared = shared;
    ck_assert_ptr_nonnull(repl);
    repl->input_parser = parser;
    shared->term = term;
    shared->render = render;

    // Create agent context for display state
    ik_agent_ctx_t *agent = talloc_zero(repl, ik_agent_ctx_t);
    repl->current = agent;
    repl->current->input_buffer = input_buf;
    repl->current->scrollback = scrollback;
    repl->current->viewport_offset = 0;
    repl->quit = false;
    init_repl_multi_handle(repl);

    // Simulate active curl request by setting curl_still_running > 0
    repl->current->curl_still_running = 1;

    // Make curl_multi_perform_ fail
    mock_curl_multi_perform_should_fail = true;

    // Call handle_curl_events_ directly to test error handling
    res = ik_repl_handle_curl_events(repl, 1);
    ck_assert(is_err(&res));  // Should propagate error from curl_multi_perform

    // Reset mock
    mock_curl_multi_perform_should_fail = false;

    talloc_free(ctx);
}

END_TEST
/* Test: curl_multi_timeout error handling */
START_TEST(test_repl_run_curl_multi_timeout_error)
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
    repl->current = talloc_zero(repl, ik_agent_ctx_t);
    ik_shared_ctx_t *shared = talloc_zero(repl, ik_shared_ctx_t);
    repl->shared = shared;
    ck_assert_ptr_nonnull(repl);
    repl->input_parser = parser;
    shared->term = term;
    shared->render = render;

    // Create agent context for display state
    ik_agent_ctx_t *agent = talloc_zero(repl, ik_agent_ctx_t);
    repl->current = agent;
    repl->current->input_buffer = input_buf;
    repl->current->scrollback = scrollback;
    repl->current->viewport_offset = 0;
    repl->quit = false;
    init_repl_multi_handle(repl);

    // Make curl_multi_timeout_ fail
    mock_curl_multi_timeout_should_fail = true;

    // Run the REPL - should fail with IO error from curl_multi_timeout
    res = ik_repl_run(repl);
    ck_assert(is_err(&res));

    // Reset mock
    mock_curl_multi_timeout_should_fail = false;

    talloc_free(ctx);
}

END_TEST

static Suite *repl_run_curl_error_suite(void)
{
    Suite *s = suite_create("REPL_Run_Curl_Error");
    TCase *tc_core = tcase_create("Core");
    tcase_set_timeout(tc_core, 30);

    tcase_add_test(tc_core, test_repl_run_curl_multi_fdset_error);
    tcase_add_test(tc_core, test_repl_run_curl_multi_perform_error);
    tcase_add_test(tc_core, test_repl_run_curl_multi_timeout_error);

    suite_add_tcase(s, tc_core);
    return s;
}

int main(void)
{
    int32_t number_failed;
    Suite *s = repl_run_curl_error_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
