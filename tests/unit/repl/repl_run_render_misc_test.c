#include "agent.h"
/**
 * @file repl_run_render_misc_test.c
 * @brief Unit tests for REPL render errors and miscellaneous tests
 */

#include "repl_run_common.h"
#include "../../../src/agent.h"
#include "../../../src/shared.h"
#include "../../../src/repl_actions.h"

/* Test: Initial render error */
START_TEST(test_repl_run_initial_render_error) {
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

    mock_write_should_fail = true;

    res = ik_repl_run(repl);
    ck_assert(is_err(&res));

    mock_write_should_fail = false;

    talloc_free(ctx);
}

END_TEST
/* Test: Render error during event loop */
START_TEST(test_repl_run_render_error_in_loop) {
    void *ctx = talloc_new(NULL);

    res_t res;
    mock_write_should_fail = false;
    mock_write_fail_after = 1;
    mock_write_count = 0;

    ik_input_buffer_t *input_buf = NULL;
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

    mock_input = "a";
    mock_input_pos = 0;

    res = ik_repl_run(repl);
    ck_assert(is_err(&res));

    mock_write_fail_after = -1;
    mock_write_count = 0;

    talloc_free(ctx);
}

END_TEST
/* Test: Render error during spinner timeout */
START_TEST(test_repl_run_spinner_render_error) {
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

    // Set spinner visible (simulates WAITING_FOR_LLM state)
    repl->current->spinner_state.visible = true;
    repl->current->spinner_state.frame_index = 0;

    // Mock select to return 0 (timeout) on first call, then set quit flag
    mock_select_return_value = 0;
    mock_select_call_count = 0;
    mock_select_return_on_call = 0;  // Return mock value on first call

    // Set up mock input to trigger quit after the spinner render
    mock_input = "\x04";  // Ctrl-D (EOF) - will be read on second iteration
    mock_input_pos = 0;

    // Make write fail on the second write (first is initial render, second is spinner timeout render)
    mock_write_should_fail = false;
    mock_write_fail_after = 1;  // Fail after 1 successful write
    mock_write_count = 0;

    res = ik_repl_run(repl);

    // Should propagate error from ik_repl_render_frame during spinner timeout
    ck_assert(is_err(&res));

    // Reset mocks
    mock_select_return_value = -999;
    mock_select_call_count = 0;
    mock_select_return_on_call = -1;
    mock_write_should_fail = false;
    mock_write_fail_after = -1;
    mock_write_count = 0;
    mock_input = NULL;
    mock_input_pos = 0;

    talloc_free(ctx);
}

END_TEST
/* Test: Spinner advance and render on timeout (success case) */
START_TEST(test_repl_run_spinner_timeout_success) {
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

    // Set spinner visible (simulates WAITING_FOR_LLM state)
    repl->current->spinner_state.visible = true;
    repl->current->spinner_state.frame_index = 0;

    // Mock select to return 0 (timeout) on first call, then 1 (ready) on second call
    mock_select_return_value = 0;
    mock_select_call_count = 0;
    mock_select_return_on_call = 0;  // Return mock value only on first call

    // Set up mock input to trigger quit after the spinner timeout
    mock_input = "\x04";  // Ctrl-D (EOF) - will be read on second iteration
    mock_input_pos = 0;

    // Let writes succeed
    mock_write_should_fail = false;
    mock_write_fail_after = -1;
    mock_write_count = 0;

    res = ik_repl_run(repl);

    // Should complete successfully (EOF from Ctrl-D)
    ck_assert(is_ok(&res));

    // Verify spinner was advanced (frame index should have incremented from 0)
    ck_assert_uint_ne((unsigned int)repl->current->spinner_state.frame_index, 0);

    // Reset mocks
    mock_select_return_value = -999;
    mock_select_call_count = 0;
    mock_select_return_on_call = -1;
    mock_input = NULL;
    mock_input_pos = 0;

    talloc_free(ctx);
}

END_TEST
/* Test: process_action returns error for invalid Unicode codepoint (unit test) */
START_TEST(test_repl_process_action_invalid_codepoint) {
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

    // Create action with invalid Unicode codepoint (> 0x10FFFF)
    ik_input_action_t action;
    action.type = IK_INPUT_CHAR;
    action.codepoint = 0x110000;  // Invalid - exceeds max Unicode codepoint

    // Process action should fail with INVALID_ARG error
    res = ik_repl_process_action(repl, &action);
    ck_assert(is_err(&res));
    ck_assert_int_eq(res.err->code, ERR_INVALID_ARG);

    talloc_free(ctx);
}

END_TEST
/* Test: ik_repl_handle_terminal_input processes character successfully (integration test) */
START_TEST(test_handle_terminal_input_success) {
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

    // Mock input with a simple ASCII character
    mock_input = "a";
    mock_input_pos = 0;

    bool should_exit = false;
    res = ik_repl_handle_terminal_input(repl, 0, &should_exit);
    ck_assert(is_ok(&res));
    ck_assert(!should_exit);

    // Verify character was added to input buffer
    size_t text_len;
    const char *text = ik_input_buffer_get_text(input_buf, &text_len);
    ck_assert_uint_eq((unsigned int)text_len, 1);
    ck_assert_int_eq(text[0], 'a');

    talloc_free(ctx);
}

END_TEST

static Suite *repl_run_render_misc_suite(void)
{
    Suite *s = suite_create("REPL_Run_Render_Misc");
    TCase *tc_core = tcase_create("Core");
    tcase_set_timeout(tc_core, IK_TEST_TIMEOUT);
    tcase_set_timeout(tc_core, IK_TEST_TIMEOUT);
    tcase_set_timeout(tc_core, IK_TEST_TIMEOUT);
    tcase_set_timeout(tc_core, IK_TEST_TIMEOUT);
    tcase_set_timeout(tc_core, IK_TEST_TIMEOUT);

    tcase_add_test(tc_core, test_repl_run_initial_render_error);
    tcase_add_test(tc_core, test_repl_run_render_error_in_loop);
    tcase_add_test(tc_core, test_repl_run_spinner_render_error);
    tcase_add_test(tc_core, test_repl_run_spinner_timeout_success);
    tcase_add_test(tc_core, test_repl_process_action_invalid_codepoint);
    tcase_add_test(tc_core, test_handle_terminal_input_success);

    suite_add_tcase(s, tc_core);
    return s;
}

int main(void)
{
    int32_t number_failed;
    Suite *s = repl_run_render_misc_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
