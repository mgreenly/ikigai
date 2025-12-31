/**
 * @file repl_event_handlers_1_fd_sets_test.c
 * @brief Unit tests for REPL event handler fd_set functions
 *
 * Tests fd_set setup functions.
 */

#include "repl_event_handlers.h"
#include "../../../src/agent.h"
#include "../../../src/repl.h"
#include "../../../src/shared.h"
#include "../../../src/scrollback.h"
#include "../../../src/config.h"
#include "../../../src/terminal.h"
#include "../../../src/input.h"
#include "../../../src/db/connection.h"
#include "../../../src/db/message.h"
#include "../../../src/providers/provider.h"
#include "../../../src/providers/common/http_multi.h"
#include "../../../src/scroll_detector.h"
#include "../../../src/tool.h"
#include <check.h>
#include <talloc.h>
#include <curl/curl.h>
#include <unistd.h>
#include <sys/select.h>
#include <errno.h>
#include <pthread.h>

static void *ctx;
static ik_repl_ctx_t *repl;
static ik_shared_ctx_t *shared;
static ik_agent_ctx_t *agent;

/* Mock provider vtable for testing */
static res_t mock_fdset(void *provider_ctx, fd_set *read_fds, fd_set *write_fds,
                        fd_set *exc_fds, int *max_fd)
{
    (void)provider_ctx;
    (void)read_fds;
    (void)write_fds;
    (void)exc_fds;
    *max_fd = 10;
    return OK(NULL);
}

static res_t mock_timeout(void *provider_ctx, long *timeout)
{
    (void)provider_ctx;
    *timeout = 500;
    return OK(NULL);
}

static res_t mock_perform(void *provider_ctx, int *still_running)
{
    (void)provider_ctx;
    *still_running = 0;
    return OK(NULL);
}

static void mock_info_read(void *provider_ctx, ik_logger_t *logger)
{
    (void)provider_ctx;
    (void)logger;
}

static ik_provider_vtable_t mock_vt = {
    .fdset = mock_fdset,
    .timeout = mock_timeout,
    .perform = mock_perform,
    .info_read = mock_info_read,
    .cleanup = NULL
};

/* Additional mocks for error testing */
static res_t mock_fdset_fails(void *provider_ctx, fd_set *read_fds, fd_set *write_fds,
                              fd_set *exc_fds, int *max_fd)
{
    (void)provider_ctx;
    (void)read_fds;
    (void)write_fds;
    (void)exc_fds;
    (void)max_fd;
    /* Need a context for ERR macro - create a temporary one */
    void *tmp = talloc_new(NULL);
    res_t result = ERR(tmp, IO, "Mock fdset error");
    /* The error is owned by tmp, which we return - caller must free */
    return result;
}

static ik_provider_vtable_t mock_vt_fails = {
    .fdset = mock_fdset_fails,
    .timeout = mock_timeout,
    .perform = mock_perform,
    .info_read = mock_info_read,
    .cleanup = NULL
};

static res_t mock_fdset_high_fd(void *provider_ctx, fd_set *read_fds, fd_set *write_fds,
                                fd_set *exc_fds, int *max_fd)
{
    (void)provider_ctx;
    (void)read_fds;
    (void)write_fds;
    (void)exc_fds;
    *max_fd = 100;
    return OK(NULL);
}

static ik_provider_vtable_t mock_vt_high = {
    .fdset = mock_fdset_high_fd,
    .timeout = mock_timeout,
    .perform = mock_perform,
    .info_read = mock_info_read,
    .cleanup = NULL
};

static void setup(void)
{
    ctx = talloc_new(NULL);

    /* Create shared context */
    shared = talloc_zero(ctx, ik_shared_ctx_t);
    shared->term = talloc_zero(shared, ik_term_ctx_t);
    shared->term->tty_fd = 0;
    shared->db_ctx = NULL;
    shared->session_id = 0;
    shared->logger = NULL;

    /* Create REPL context */
    repl = talloc_zero(ctx, ik_repl_ctx_t);
    repl->shared = shared;
    repl->agent_count = 0;
    repl->agents = NULL;
    repl->input_parser = NULL;
    repl->scroll_det = NULL;

    /* Create agent context */
    agent = talloc_zero(repl, ik_agent_ctx_t);
    agent->shared = shared;
    agent->scrollback = ik_scrollback_create(agent, 80);
    agent->curl_still_running = 0;
    agent->http_error_message = NULL;
    agent->assistant_response = NULL;
    agent->pending_tool_call = NULL;
    agent->provider_instance = NULL;
    agent->state = IK_AGENT_STATE_IDLE;
    agent->tool_iteration_count = 0;
    pthread_mutex_init(&agent->tool_thread_mutex, NULL);

    /* Agent metadata for database tests */
    agent->uuid = talloc_strdup(agent, "test-uuid");
    agent->provider = NULL;
    agent->response_model = NULL;
    agent->response_finish_reason = NULL;
    agent->response_input_tokens = 0;
    agent->response_output_tokens = 0;
    agent->response_thinking_tokens = 0;
    agent->thinking_level = 0;

    /* Spinner state */
    agent->spinner_state.visible = false;
    agent->spinner_state.frame_index = 0;

    repl->current = agent;
}

static void teardown(void)
{
    pthread_mutex_destroy(&agent->tool_thread_mutex);
    talloc_free(ctx);
}

/* ========== ik_repl_setup_fd_sets Tests ========== */

START_TEST(test_setup_fd_sets_no_agents)
{
    fd_set read_fds, write_fds, exc_fds;
    int max_fd = -1;

    res_t result = ik_repl_setup_fd_sets(repl, &read_fds, &write_fds, &exc_fds, &max_fd);
    ck_assert(is_ok(&result));
    ck_assert_int_eq(max_fd, 0);
    ck_assert(FD_ISSET(0, &read_fds));
}

END_TEST

START_TEST(test_setup_fd_sets_with_provider_instance)
{
    /* Create mock provider instance */
    struct ik_provider *instance = talloc_zero(agent, struct ik_provider);
    instance->vt = &mock_vt;
    instance->ctx = NULL;
    agent->provider_instance = instance;

    /* Add agent to repl */
    repl->agent_count = 1;
    repl->agents = talloc_array(repl, ik_agent_ctx_t *, 1);
    repl->agents[0] = agent;

    fd_set read_fds, write_fds, exc_fds;
    int max_fd = -1;

    res_t result = ik_repl_setup_fd_sets(repl, &read_fds, &write_fds, &exc_fds, &max_fd);
    ck_assert(is_ok(&result));
    ck_assert_int_eq(max_fd, 10);  /* Mock returns 10 */
}

END_TEST

START_TEST(test_setup_fd_sets_provider_returns_error)
{
    /* Create mock provider instance */
    struct ik_provider *instance = talloc_zero(agent, struct ik_provider);
    instance->vt = &mock_vt_fails;
    instance->ctx = NULL;
    agent->provider_instance = instance;

    /* Add agent to repl */
    repl->agent_count = 1;
    repl->agents = talloc_array(repl, ik_agent_ctx_t *, 1);
    repl->agents[0] = agent;

    fd_set read_fds, write_fds, exc_fds;
    int max_fd = -1;

    res_t result = ik_repl_setup_fd_sets(repl, &read_fds, &write_fds, &exc_fds, &max_fd);
    ck_assert(is_err(&result));
    talloc_free(result.err);
}

END_TEST

START_TEST(test_setup_fd_sets_updates_max_fd)
{
    /* Create mock provider instance */
    struct ik_provider *instance = talloc_zero(agent, struct ik_provider);
    instance->vt = &mock_vt_high;
    instance->ctx = NULL;
    agent->provider_instance = instance;

    /* Add agent to repl */
    repl->agent_count = 1;
    repl->agents = talloc_array(repl, ik_agent_ctx_t *, 1);
    repl->agents[0] = agent;

    fd_set read_fds, write_fds, exc_fds;
    int max_fd = -1;

    res_t result = ik_repl_setup_fd_sets(repl, &read_fds, &write_fds, &exc_fds, &max_fd);
    ck_assert(is_ok(&result));
    ck_assert_int_eq(max_fd, 100);  /* Should be updated to higher value */
}

END_TEST

START_TEST(test_setup_fd_sets_agent_fd_not_higher)
{
    /* Create mock provider instance that returns a lower fd than terminal */
    struct ik_provider *instance = talloc_zero(agent, struct ik_provider);
    instance->vt = &mock_vt;  /* Returns max_fd=10 */
    instance->ctx = NULL;
    agent->provider_instance = instance;

    /* Add agent to repl */
    repl->agent_count = 1;
    repl->agents = talloc_array(repl, ik_agent_ctx_t *, 1);
    repl->agents[0] = agent;

    /* Set terminal fd higher than what provider returns */
    shared->term->tty_fd = 50;

    fd_set read_fds, write_fds, exc_fds;
    int max_fd = -1;

    res_t result = ik_repl_setup_fd_sets(repl, &read_fds, &write_fds, &exc_fds, &max_fd);
    ck_assert(is_ok(&result));
    ck_assert_int_eq(max_fd, 50);  /* Should remain at terminal_fd since it's higher */
}

END_TEST

/* ========== Test Suite Setup ========== */

static Suite *repl_event_handlers_fd_sets_suite(void)
{
    Suite *s = suite_create("repl_event_handlers_1_fd_sets");

    TCase *tc_fd_sets = tcase_create("fd_sets");
    tcase_set_timeout(tc_fd_sets, 30);
    tcase_add_checked_fixture(tc_fd_sets, setup, teardown);
    tcase_add_test(tc_fd_sets, test_setup_fd_sets_no_agents);
    tcase_add_test(tc_fd_sets, test_setup_fd_sets_with_provider_instance);
    tcase_add_test(tc_fd_sets, test_setup_fd_sets_provider_returns_error);
    tcase_add_test(tc_fd_sets, test_setup_fd_sets_updates_max_fd);
    tcase_add_test(tc_fd_sets, test_setup_fd_sets_agent_fd_not_higher);
    suite_add_tcase(s, tc_fd_sets);

    return s;
}

int main(void)
{
    int number_failed;
    Suite *s = repl_event_handlers_fd_sets_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
