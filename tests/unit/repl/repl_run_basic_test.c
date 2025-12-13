/**
 * @file repl_run_basic_test.c
 * @brief Unit tests for REPL event loop basic functionality
 */

#include "repl_run_test_common.h"
#include "../../../src/agent.h"
#include "../../../src/shared.h"

/* Test: Simple character input followed by Ctrl+C */
START_TEST(test_repl_run_simple_char_input) {
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

    mock_input = "a\x03";
    mock_input_pos = 0;

    res = ik_repl_run(repl);
    ck_assert(is_ok(&res));

    size_t len = 0;
    const char *text = ik_input_buffer_get_text(repl->current->input_buffer, &len);
    ck_assert_uint_eq(len, 1);
    ck_assert_int_eq(text[0], 'a');

    ck_assert(repl->quit);

    talloc_free(ctx);
}
END_TEST
/* Test: Multiple character input */
START_TEST(test_repl_run_multiple_chars)
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

    mock_input = "abc\x03";
    mock_input_pos = 0;

    res = ik_repl_run(repl);
    ck_assert(is_ok(&res));

    size_t len = 0;
    const char *text = ik_input_buffer_get_text(input_buf, &len);
    ck_assert_uint_eq(len, 3);
    ck_assert_int_eq(text[0], 'a');
    ck_assert_int_eq(text[1], 'b');
    ck_assert_int_eq(text[2], 'c');

    ck_assert(repl->quit);

    talloc_free(ctx);
}

END_TEST
/* Test: Input with newline */
START_TEST(test_repl_run_with_newline)
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

    mock_input = "hi\n\x03";
    mock_input_pos = 0;

    res = ik_repl_run(repl);
    ck_assert(is_ok(&res));

    size_t len = 0;
    const char *text = ik_input_buffer_get_text(input_buf, &len);
    ck_assert_uint_eq(len, 3);
    ck_assert_int_eq(text[0], 'h');
    ck_assert_int_eq(text[1], 'i');
    ck_assert_int_eq(text[2], '\n');

    talloc_free(ctx);
}

END_TEST
/* Test: Input with backspace */
START_TEST(test_repl_run_with_backspace)
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

    mock_input = "ab\x7f\x03";
    mock_input_pos = 0;

    res = ik_repl_run(repl);
    ck_assert(is_ok(&res));

    size_t len = 0;
    const char *text = ik_input_buffer_get_text(input_buf, &len);
    ck_assert_uint_eq(len, 1);
    ck_assert_int_eq(text[0], 'a');

    talloc_free(ctx);
}

END_TEST
/* Test: Read EOF */
START_TEST(test_repl_run_read_eof)
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

    mock_input = "";
    mock_input_pos = 0;

    res = ik_repl_run(repl);
    ck_assert(is_ok(&res));

    size_t len = 0;
    (void)ik_input_buffer_get_text(input_buf, &len);
    ck_assert_uint_eq(len, 0);

    ck_assert_int_eq(repl->quit, false);

    talloc_free(ctx);
}

END_TEST
/* Test: REPL handles incomplete escape sequence at EOF */
START_TEST(test_repl_run_unknown_action)
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

    mock_input = "a\x1b";
    mock_input_pos = 0;

    res = ik_repl_run(repl);
    ck_assert(is_ok(&res));

    ck_assert_int_eq(repl->quit, false);

    size_t len = 0;
    const char *text = ik_input_buffer_get_text(input_buf, &len);
    ck_assert_uint_eq(len, 1);
    ck_assert_int_eq(text[0], 'a');

    talloc_free(ctx);
}

END_TEST
/* Test: Select timeout triggers curl event handling */
START_TEST(test_repl_run_select_timeout)
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
    repl->spinner_state.visible = false;  // Spinner not visible
    init_repl_multi_handle(repl);

    // Simulate select timeout (return 0) on first call only, then return to normal behavior
    mock_select_return_value = 0;  // Return 0 (timeout)
    mock_select_return_on_call = 0;  // Only on first call (call number 0)
    mock_select_call_count = 0;  // Reset call counter
    mock_input = "\x03";  // Ctrl+C on second iteration to exit
    mock_input_pos = 0;

    res = ik_repl_run(repl);
    ck_assert(is_ok(&res));

    // Reset mocks
    mock_select_return_value = -1;
    mock_select_return_on_call = -1;
    mock_select_call_count = 0;

    talloc_free(ctx);
}

END_TEST
/* Test: Active curl transfers trigger curl event handling */
START_TEST(test_repl_run_active_curl_transfers)
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
    repl->spinner_state.visible = false;
    init_repl_multi_handle(repl);
    repl->curl_still_running = 1;  // Simulate active curl transfer

    mock_select_return_value = -1;  // Use default behavior (returns 1)
    mock_input = "\x03";  // Ctrl+C to exit
    mock_input_pos = 0;

    res = ik_repl_run(repl);
    ck_assert(is_ok(&res));

    talloc_free(ctx);
}

END_TEST

static Suite *repl_run_basic_suite(void)
{
    Suite *s = suite_create("REPL_Run_Basic");
    TCase *tc_core = tcase_create("Core");
    tcase_set_timeout(tc_core, 30);

    tcase_add_test(tc_core, test_repl_run_simple_char_input);
    tcase_add_test(tc_core, test_repl_run_multiple_chars);
    tcase_add_test(tc_core, test_repl_run_with_newline);
    tcase_add_test(tc_core, test_repl_run_with_backspace);
    tcase_add_test(tc_core, test_repl_run_read_eof);
    tcase_add_test(tc_core, test_repl_run_unknown_action);
    tcase_add_test(tc_core, test_repl_run_select_timeout);
    tcase_add_test(tc_core, test_repl_run_active_curl_transfers);

    suite_add_tcase(s, tc_core);
    return s;
}

int main(void)
{
    int32_t number_failed;
    Suite *s = repl_run_basic_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
