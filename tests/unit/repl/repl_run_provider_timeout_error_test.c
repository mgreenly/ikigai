/**
 * @file repl_run_provider_timeout_error_test.c
 * @brief Unit test for REPL run when provider timeout returns error
 */

#include "repl_run_common.h"
#include "../../../src/agent.h"
#include "../../../src/shared.h"
#include "../../../src/providers/provider.h"
#include <pthread.h>

/* Mock provider timeout that returns an error */
static res_t mock_provider_timeout_fails(void *provider_ctx, long *timeout)
{
    (void)provider_ctx;
    (void)timeout;
    void *tmp = talloc_new(NULL);
    res_t result = ERR(tmp, IO, "Mock provider timeout error");
    return result;
}

static res_t mock_provider_fdset(void *provider_ctx, fd_set *read_fds, fd_set *write_fds,
                                  fd_set *exc_fds, int *max_fd)
{
    (void)provider_ctx;
    (void)read_fds;
    (void)write_fds;
    (void)exc_fds;
    *max_fd = 5;
    return OK(NULL);
}

static res_t mock_provider_perform(void *provider_ctx, int *still_running)
{
    (void)provider_ctx;
    *still_running = 0;
    return OK(NULL);
}

static void mock_provider_info_read(void *provider_ctx, ik_logger_t *logger)
{
    (void)provider_ctx;
    (void)logger;
}

static ik_provider_vtable_t mock_vt_timeout_error = {
    .fdset = mock_provider_fdset,
    .timeout = mock_provider_timeout_fails,
    .perform = mock_provider_perform,
    .info_read = mock_provider_info_read,
    .cleanup = NULL
};

/* Test: provider timeout returns error in repl_run (should propagate error) */
START_TEST(test_repl_run_provider_timeout_error)
{
    void *ctx = talloc_new(NULL);

    ik_input_buffer_t *input_buf = ik_input_buffer_create(ctx);
    ik_input_parser_t *parser = ik_input_parser_create(ctx);

    ik_term_ctx_t *term = talloc_zero(ctx, ik_term_ctx_t);
    ck_assert_ptr_nonnull(term);
    term->tty_fd = 0;
    term->screen_rows = 24;
    term->screen_cols = 80;

    ik_render_ctx_t *render = NULL;
    res_t res = ik_render_create(ctx, 24, 80, 1, &render);
    ck_assert(is_ok(&res));

    ik_scrollback_t *scrollback = ik_scrollback_create(ctx, 80);

    ik_repl_ctx_t *repl = talloc_zero(ctx, ik_repl_ctx_t);
    ik_shared_ctx_t *shared = talloc_zero(repl, ik_shared_ctx_t);
    repl->shared = shared;
    repl->input_parser = parser;
    shared->term = term;
    shared->render = render;

    // Create agent context with provider that fails timeout
    ik_agent_ctx_t *agent = talloc_zero(repl, ik_agent_ctx_t);
    agent->shared = shared;
    agent->repl = repl;
    agent->input_buffer = input_buf;
    agent->scrollback = scrollback;
    agent->viewport_offset = 0;
    agent->state = IK_AGENT_STATE_IDLE;
    agent->curl_still_running = 0;
    pthread_mutex_init(&agent->tool_thread_mutex, NULL);

    // Set up provider instance that returns error from timeout
    struct ik_provider *instance = talloc_zero(agent, struct ik_provider);
    instance->vt = &mock_vt_timeout_error;
    instance->ctx = NULL;
    agent->provider_instance = instance;

    // Add agent to repl
    repl->agent_count = 1;
    repl->agents = talloc_array(repl, ik_agent_ctx_t *, 1);
    repl->agents[0] = agent;
    repl->current = agent;
    repl->quit = false;

    // Run REPL - should hit error from provider timeout and return error
    res = ik_repl_run(repl);
    ck_assert(is_err(&res));
    talloc_free(res.err);

    pthread_mutex_destroy(&agent->tool_thread_mutex);
    talloc_free(ctx);
}
END_TEST

static Suite *repl_run_provider_timeout_error_suite(void)
{
    Suite *s = suite_create("REPL_Run_Provider_Timeout_Error");
    TCase *tc_core = tcase_create("Core");
    tcase_set_timeout(tc_core, 30);
    tcase_set_timeout(tc_core, 30);
    tcase_set_timeout(tc_core, 30);
    tcase_set_timeout(tc_core, 30);
    tcase_set_timeout(tc_core, 30);

    tcase_add_test(tc_core, test_repl_run_provider_timeout_error);

    suite_add_tcase(s, tc_core);
    return s;
}

int main(void)
{
    int32_t number_failed;
    Suite *s = repl_run_provider_timeout_error_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
