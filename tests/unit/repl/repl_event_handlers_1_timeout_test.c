#include "../../test_constants.h"
/**
 * @file repl_event_handlers_1_timeout_test.c
 * @brief Unit tests for REPL event handler timeout functions
 *
 * Tests timeout calculation functions.
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
#include <stdatomic.h>

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

static res_t mock_timeout_500(void *provider_ctx, long *timeout)
{
    (void)provider_ctx;
    *timeout = 500;
    return OK(NULL);
}

static res_t mock_timeout_200(void *provider_ctx, long *timeout)
{
    (void)provider_ctx;
    *timeout = 200;
    return OK(NULL);
}

static ik_provider_vtable_t mock_vt_500 = {
    .fdset = mock_fdset,
    .timeout = mock_timeout_500,
    .perform = mock_perform,
    .info_read = mock_info_read,
    .cleanup = NULL
};

static ik_provider_vtable_t mock_vt_200 = {
    .fdset = mock_fdset,
    .timeout = mock_timeout_200,
    .perform = mock_perform,
    .info_read = mock_info_read,
    .cleanup = NULL
};

static res_t mock_timeout_negative(void *provider_ctx, long *timeout)
{
    (void)provider_ctx;
    *timeout = -1;  /* No timeout available */
    return OK(NULL);
}

static ik_provider_vtable_t mock_vt_timeout_negative = {
    .fdset = mock_fdset,
    .timeout = mock_timeout_negative,
    .perform = mock_perform,
    .info_read = mock_info_read,
    .cleanup = NULL
};

static res_t mock_timeout_fails(void *provider_ctx, long *timeout)
{
    (void)provider_ctx;
    (void)timeout;
    /* Need a context for ERR macro - create a temporary one */
    void *tmp = talloc_new(NULL);
    res_t result = ERR(tmp, IO, "Mock timeout error");
    /* The error is owned by tmp, which we return - caller must free */
    return result;
}

static ik_provider_vtable_t mock_vt_timeout_fails = {
    .fdset = mock_fdset,
    .timeout = mock_timeout_fails,
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
    atomic_store(&agent->state, IK_AGENT_STATE_IDLE);
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

/* ========== ik_repl_calculate_curl_min_timeout Tests ========== */

START_TEST(test_curl_min_timeout_no_agents) {
    long timeout = -1;

    res_t result = ik_repl_calculate_curl_min_timeout(repl, &timeout);
    ck_assert(is_ok(&result));
    ck_assert_int_eq(timeout, -1);
}

END_TEST

START_TEST(test_curl_min_timeout_with_provider) {
    /* Create mock provider instance */
    struct ik_provider *instance = talloc_zero(agent, struct ik_provider);
    instance->vt = &mock_vt;
    instance->ctx = NULL;
    agent->provider_instance = instance;

    /* Add agent to repl */
    repl->agent_count = 1;
    repl->agents = talloc_array(repl, ik_agent_ctx_t *, 1);
    repl->agents[0] = agent;

    long timeout = -1;

    res_t result = ik_repl_calculate_curl_min_timeout(repl, &timeout);
    ck_assert(is_ok(&result));
    ck_assert_int_eq(timeout, 500);  /* Mock returns 500 */
}

END_TEST

START_TEST(test_curl_min_timeout_chooses_minimum) {
    /* Create first agent with 500ms timeout */
    struct ik_provider *instance1 = talloc_zero(agent, struct ik_provider);
    instance1->vt = &mock_vt_500;
    instance1->ctx = NULL;
    agent->provider_instance = instance1;

    /* Create second agent with 200ms timeout */
    ik_agent_ctx_t *agent2 = talloc_zero(repl, ik_agent_ctx_t);
    agent2->shared = shared;
    agent2->scrollback = ik_scrollback_create(agent2, 80);
    pthread_mutex_init(&agent2->tool_thread_mutex, NULL);
    struct ik_provider *instance2 = talloc_zero(agent2, struct ik_provider);
    instance2->vt = &mock_vt_200;
    instance2->ctx = NULL;
    agent2->provider_instance = instance2;

    /* Add both agents to repl */
    repl->agent_count = 2;
    repl->agents = talloc_array(repl, ik_agent_ctx_t *, 2);
    repl->agents[0] = agent;
    repl->agents[1] = agent2;

    long timeout = -1;
    res_t result = ik_repl_calculate_curl_min_timeout(repl, &timeout);
    ck_assert(is_ok(&result));
    ck_assert_int_eq(timeout, 200);  /* Should pick minimum */

    pthread_mutex_destroy(&agent2->tool_thread_mutex);
}

END_TEST

START_TEST(test_curl_min_timeout_provider_error) {
    /* Create mock provider instance that returns error from timeout */
    struct ik_provider *instance = talloc_zero(agent, struct ik_provider);
    instance->vt = &mock_vt_timeout_fails;
    instance->ctx = NULL;
    agent->provider_instance = instance;

    /* Add agent to repl */
    repl->agent_count = 1;
    repl->agents = talloc_array(repl, ik_agent_ctx_t *, 1);
    repl->agents[0] = agent;

    long timeout = -1;

    res_t result = ik_repl_calculate_curl_min_timeout(repl, &timeout);
    ck_assert(is_err(&result));
    talloc_free(result.err);
}

END_TEST

START_TEST(test_curl_min_timeout_negative_timeout) {
    /* Create mock provider instance that returns -1 for timeout */
    struct ik_provider *instance = talloc_zero(agent, struct ik_provider);
    instance->vt = &mock_vt_timeout_negative;
    instance->ctx = NULL;
    agent->provider_instance = instance;

    /* Add agent to repl */
    repl->agent_count = 1;
    repl->agents = talloc_array(repl, ik_agent_ctx_t *, 1);
    repl->agents[0] = agent;

    long timeout = 999;  /* Set to non-negative to verify it stays -1 */

    res_t result = ik_repl_calculate_curl_min_timeout(repl, &timeout);
    ck_assert(is_ok(&result));
    ck_assert_int_eq(timeout, -1);  /* Should remain -1 since provider returned -1 */
}

END_TEST

/* ========== ik_repl_calculate_select_timeout_ms Tests ========== */

START_TEST(test_select_timeout_default) {
    long timeout = ik_repl_calculate_select_timeout_ms(repl, -1);
    ck_assert_int_eq(timeout, 1000);  /* Default when no timeouts active */
}

END_TEST

START_TEST(test_select_timeout_with_spinner) {
    agent->spinner_state.visible = true;

    long timeout = ik_repl_calculate_select_timeout_ms(repl, -1);
    ck_assert_int_eq(timeout, 80);  /* Spinner timeout */
}

END_TEST

START_TEST(test_select_timeout_with_executing_tool) {
    /* Add agent to repl */
    repl->agent_count = 1;
    repl->agents = talloc_array(repl, ik_agent_ctx_t *, 1);
    repl->agents[0] = agent;

    /* Set agent to executing tool state */
    pthread_mutex_lock(&agent->tool_thread_mutex);
    atomic_store(&agent->state, IK_AGENT_STATE_EXECUTING_TOOL);
    pthread_mutex_unlock(&agent->tool_thread_mutex);

    long timeout = ik_repl_calculate_select_timeout_ms(repl, -1);
    ck_assert_int_eq(timeout, 50);  /* Tool poll timeout */
}

END_TEST

START_TEST(test_select_timeout_with_scroll_detector) {
    /* Create scroll detector */
    repl->scroll_det = ik_scroll_detector_create(repl);
    ck_assert_ptr_nonnull(repl->scroll_det);

    long timeout = ik_repl_calculate_select_timeout_ms(repl, -1);
    /* Timeout will depend on scroll detector state, just verify it's calculated */
    ck_assert(timeout > 0 || timeout == -1);
}

END_TEST

START_TEST(test_select_timeout_prefers_minimum) {
    /* Set multiple timeouts and verify minimum is returned */
    agent->spinner_state.visible = true;  /* 80ms */

    long timeout = ik_repl_calculate_select_timeout_ms(repl, 100);  /* curl: 100ms */
    ck_assert_int_eq(timeout, 80);  /* Should pick spinner (minimum) */

    timeout = ik_repl_calculate_select_timeout_ms(repl, 50);  /* curl: 50ms */
    ck_assert_int_eq(timeout, 50);  /* Should pick curl (minimum) */
}

END_TEST

/* ========== Test Suite Setup ========== */

static Suite *repl_event_handlers_timeout_suite(void)
{
    Suite *s = suite_create("repl_event_handlers_1_timeout");

    TCase *tc_timeout = tcase_create("timeout");
    tcase_set_timeout(tc_timeout, IK_TEST_TIMEOUT);
    tcase_add_checked_fixture(tc_timeout, setup, teardown);
    tcase_add_test(tc_timeout, test_curl_min_timeout_no_agents);
    tcase_add_test(tc_timeout, test_curl_min_timeout_with_provider);
    tcase_add_test(tc_timeout, test_curl_min_timeout_chooses_minimum);
    tcase_add_test(tc_timeout, test_curl_min_timeout_provider_error);
    tcase_add_test(tc_timeout, test_curl_min_timeout_negative_timeout);
    tcase_add_test(tc_timeout, test_select_timeout_default);
    tcase_add_test(tc_timeout, test_select_timeout_with_spinner);
    tcase_add_test(tc_timeout, test_select_timeout_with_executing_tool);
    tcase_add_test(tc_timeout, test_select_timeout_with_scroll_detector);
    tcase_add_test(tc_timeout, test_select_timeout_prefers_minimum);
    suite_add_tcase(s, tc_timeout);

    return s;
}

int main(void)
{
    int number_failed;
    Suite *s = repl_event_handlers_timeout_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/repl/repl_event_handlers_1_timeout_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
