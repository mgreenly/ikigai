#include "tests/test_constants.h"
/**
 * @file repl_event_handlers_test_3_curl.c
 * @brief Unit tests for REPL curl event handling functions
 *
 * Tests for coverage gaps in process_agent_curl_events, handle_agent_request_error,
 * and related curl error handling paths.
 */

#include "apps/ikigai/repl_event_handlers.h"
#include "apps/ikigai/agent.h"
#include "apps/ikigai/repl.h"
#include "apps/ikigai/shared.h"
#include "apps/ikigai/scrollback.h"
#include "apps/ikigai/config.h"
#include "shared/terminal.h"
#include "apps/ikigai/input.h"
#include "apps/ikigai/db/connection.h"
#include "apps/ikigai/db/message.h"
#include "apps/ikigai/providers/provider.h"
#include "apps/ikigai/providers/common/http_multi.h"
#include "apps/ikigai/scroll_detector.h"
#include "apps/ikigai/input_buffer/core.h"
#include "apps/ikigai/render.h"
#include "apps/ikigai/tool.h"
#include "apps/ikigai/message.h"
#include "shared/wrapper.h"
#include "apps/ikigai/repl_tool_completion.h"
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
static ik_db_ctx_t *fake_db;

/* Global context for mock error creation */
static TALLOC_CTX *error_ctx = NULL;

/* Mock database insert that succeeds */
res_t ik_db_message_insert_(void *db,
                            int64_t session_id,
                            const char *agent_uuid,
                            const char *kind,
                            const char *content,
                            const char *data_json)
{
    (void)db;
    (void)session_id;
    (void)agent_uuid;
    (void)kind;
    (void)content;
    (void)data_json;
    return OK(NULL);
}

/* Mock render frame to avoid needing full render infrastructure */
res_t ik_repl_render_frame_(void *repl_ctx)
{
    (void)repl_ctx;
    return OK(NULL);
}

/* Mock provider vtable */
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

static res_t mock_perform_error(void *provider_ctx, int *still_running)
{
    (void)provider_ctx;
    (void)still_running;
    /* Return error to test error handling path */
    /* Use global error_ctx which is set up in test setup */
    return ERR(error_ctx, PROVIDER, "mock perform error");
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

static ik_provider_vtable_t mock_vt_error = {
    .fdset = mock_fdset,
    .timeout = mock_timeout,
    .perform = mock_perform_error,
    .info_read = mock_info_read,
    .cleanup = NULL
};

static void setup(void)
{
    ctx = talloc_new(NULL);
    error_ctx = ctx;  /* Set global error context for mocks */

    /* Create fake database context */
    fake_db = talloc_zero(ctx, ik_db_ctx_t);

    /* Create shared context */
    shared = talloc_zero(ctx, ik_shared_ctx_t);
    shared->term = talloc_zero(shared, ik_term_ctx_t);
    shared->term->tty_fd = 1;
    shared->term->screen_rows = 24;
    shared->term->screen_cols = 80;
    shared->db_ctx = fake_db;
    shared->session_id = 123;
    shared->logger = NULL;

    /* Create render context for tests that trigger render */
    ik_render_ctx_t *render = NULL;
    res_t res = ik_render_create(ctx, 24, 80, 1, &render);
    (void)res;  // Ignore result in test setup
    shared->render = render;

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
    agent->input_buffer = ik_input_buffer_create(agent);
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
    agent->messages = NULL;
    agent->message_count = 0;
    agent->message_capacity = 0;

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

/* ========== curl events with error handling Tests ========== */

START_TEST(test_curl_events_with_http_error) {
    /* Create mock provider instance */
    struct ik_provider *instance = talloc_zero(agent, struct ik_provider);
    instance->vt = &mock_vt;
    instance->ctx = NULL;
    agent->provider_instance = instance;
    agent->curl_still_running = 1;
    atomic_store(&agent->state, IK_AGENT_STATE_WAITING_FOR_LLM);
    agent->http_error_message = talloc_strdup(agent, "Connection failed");

    /* Add agent to repl - but set different agent as current to avoid render */
    repl->agent_count = 1;
    repl->agents = talloc_array(repl, ik_agent_ctx_t *, 1);
    repl->agents[0] = agent;

    /* Create different current agent to avoid render path */
    ik_agent_ctx_t *other_agent = talloc_zero(repl, ik_agent_ctx_t);
    other_agent->shared = shared;
    other_agent->scrollback = ik_scrollback_create(other_agent, 80);
    other_agent->curl_still_running = 0;
    other_agent->provider_instance = NULL;
    atomic_store(&other_agent->state, IK_AGENT_STATE_IDLE);
    pthread_mutex_init(&other_agent->tool_thread_mutex, NULL);
    repl->current = other_agent;

    /* Mock perform will set still_running to 0 */
    res_t result = ik_repl_handle_curl_events(repl, 1);
    ck_assert(is_ok(&result));

    /* Error message should be freed and displayed in scrollback */
    ck_assert_ptr_null(agent->http_error_message);

    pthread_mutex_destroy(&other_agent->tool_thread_mutex);
}

END_TEST

START_TEST(test_curl_events_with_http_error_and_assistant_response) {
    /* Create mock provider instance */
    struct ik_provider *instance = talloc_zero(agent, struct ik_provider);
    instance->vt = &mock_vt;
    instance->ctx = NULL;
    agent->provider_instance = instance;
    agent->curl_still_running = 1;
    atomic_store(&agent->state, IK_AGENT_STATE_WAITING_FOR_LLM);
    agent->http_error_message = talloc_strdup(agent, "Connection failed");
    agent->assistant_response = talloc_strdup(agent, "Partial response");

    /* Add agent to repl - but set different agent as current to avoid render */
    repl->agent_count = 1;
    repl->agents = talloc_array(repl, ik_agent_ctx_t *, 1);
    repl->agents[0] = agent;

    /* Create different current agent to avoid render path */
    ik_agent_ctx_t *other_agent = talloc_zero(repl, ik_agent_ctx_t);
    other_agent->shared = shared;
    other_agent->scrollback = ik_scrollback_create(other_agent, 80);
    other_agent->curl_still_running = 0;
    other_agent->provider_instance = NULL;
    atomic_store(&other_agent->state, IK_AGENT_STATE_IDLE);
    pthread_mutex_init(&other_agent->tool_thread_mutex, NULL);
    repl->current = other_agent;

    /* Mock perform will set still_running to 0 */
    res_t result = ik_repl_handle_curl_events(repl, 1);
    ck_assert(is_ok(&result));

    /* Both error and assistant response should be freed */
    ck_assert_ptr_null(agent->http_error_message);
    ck_assert_ptr_null(agent->assistant_response);

    pthread_mutex_destroy(&other_agent->tool_thread_mutex);
}

END_TEST

START_TEST(test_curl_events_with_running_curl_success) {
    /* Create mock provider instance */
    struct ik_provider *instance = talloc_zero(agent, struct ik_provider);
    instance->vt = &mock_vt;
    instance->ctx = NULL;
    agent->provider_instance = instance;
    agent->curl_still_running = 1;
    atomic_store(&agent->state, IK_AGENT_STATE_WAITING_FOR_LLM);
    agent->assistant_response = talloc_strdup(agent, "Response text");

    /* Add agent to repl - but set different agent as current to avoid render */
    repl->agent_count = 1;
    repl->agents = talloc_array(repl, ik_agent_ctx_t *, 1);
    repl->agents[0] = agent;

    /* Create different current agent to avoid render path */
    ik_agent_ctx_t *other_agent = talloc_zero(repl, ik_agent_ctx_t);
    other_agent->shared = shared;
    other_agent->scrollback = ik_scrollback_create(other_agent, 80);
    other_agent->curl_still_running = 0;
    other_agent->provider_instance = NULL;
    atomic_store(&other_agent->state, IK_AGENT_STATE_IDLE);
    pthread_mutex_init(&other_agent->tool_thread_mutex, NULL);
    repl->current = other_agent;

    /* Mock perform will set still_running to 0 */
    res_t result = ik_repl_handle_curl_events(repl, 1);
    ck_assert(is_ok(&result));

    /* Response should be freed */
    ck_assert_ptr_null(agent->assistant_response);

    pthread_mutex_destroy(&other_agent->tool_thread_mutex);
}

END_TEST

START_TEST(test_curl_events_not_current_agent) {
    /* Create mock provider instance */
    struct ik_provider *instance = talloc_zero(agent, struct ik_provider);
    instance->vt = &mock_vt;
    instance->ctx = NULL;
    agent->provider_instance = instance;
    agent->curl_still_running = 1;
    atomic_store(&agent->state, IK_AGENT_STATE_WAITING_FOR_LLM);
    agent->assistant_response = talloc_strdup(agent, "Response text");

    /* Add agent to repl */
    repl->agent_count = 1;
    repl->agents = talloc_array(repl, ik_agent_ctx_t *, 1);
    repl->agents[0] = agent;

    /* Create different current agent */
    ik_agent_ctx_t *other_agent = talloc_zero(repl, ik_agent_ctx_t);
    other_agent->shared = shared;
    other_agent->scrollback = ik_scrollback_create(other_agent, 80);
    other_agent->curl_still_running = 0;
    other_agent->provider_instance = NULL;
    atomic_store(&other_agent->state, IK_AGENT_STATE_IDLE);
    pthread_mutex_init(&other_agent->tool_thread_mutex, NULL);
    repl->current = other_agent;

    /* Mock perform will set still_running to 0 */
    res_t result = ik_repl_handle_curl_events(repl, 1);
    ck_assert(is_ok(&result));

    pthread_mutex_destroy(&other_agent->tool_thread_mutex);
}

END_TEST

START_TEST(test_curl_events_is_current_agent_triggers_render) {
    /* Create mock provider instance */
    struct ik_provider *instance = talloc_zero(agent, struct ik_provider);
    instance->vt = &mock_vt;
    instance->ctx = NULL;
    agent->provider_instance = instance;
    agent->curl_still_running = 1;
    atomic_store(&agent->state, IK_AGENT_STATE_WAITING_FOR_LLM);
    agent->assistant_response = talloc_strdup(agent, "Response text");

    /* Add agent to repl and set as current */
    repl->agent_count = 1;
    repl->agents = talloc_array(repl, ik_agent_ctx_t *, 1);
    repl->agents[0] = agent;
    repl->current = agent;  /* Agent is current - will trigger render */

    /* Mock perform will set still_running to 0, triggering render path */
    res_t result = ik_repl_handle_curl_events(repl, 1);
    ck_assert(is_ok(&result));

    /* Response should be freed */
    ck_assert_ptr_null(agent->assistant_response);
}

END_TEST

START_TEST(test_curl_events_with_null_current) {
    /* No agents in array, current is NULL */
    repl->agent_count = 0;
    repl->agents = NULL;
    repl->current = NULL;

    /* Should handle gracefully - just returns OK */
    res_t result = ik_repl_handle_curl_events(repl, 1);
    ck_assert(is_ok(&result));
}

END_TEST

START_TEST(test_curl_events_state_not_waiting_for_llm) {
    /* Create mock provider instance */
    struct ik_provider *instance = talloc_zero(agent, struct ik_provider);
    instance->vt = &mock_vt;
    instance->ctx = NULL;
    agent->provider_instance = instance;
    agent->curl_still_running = 1;
    atomic_store(&agent->state, IK_AGENT_STATE_IDLE);  /* Not WAITING_FOR_LLM */

    /* Add agent to repl */
    repl->agent_count = 1;
    repl->agents = talloc_array(repl, ik_agent_ctx_t *, 1);
    repl->agents[0] = agent;
    repl->current = agent;

    /* Mock perform will set still_running to 0, but state check will skip transition */
    res_t result = ik_repl_handle_curl_events(repl, 1);
    ck_assert(is_ok(&result));
}

END_TEST

START_TEST(test_curl_events_no_provider_instance) {
    /* No provider instance - should just skip processing */
    agent->provider_instance = NULL;
    agent->curl_still_running = 0;
    atomic_store(&agent->state, IK_AGENT_STATE_IDLE);

    /* Add agent to repl */
    repl->agent_count = 1;
    repl->agents = talloc_array(repl, ik_agent_ctx_t *, 1);
    repl->agents[0] = agent;
    repl->current = agent;

    /* Should handle gracefully */
    res_t result = ik_repl_handle_curl_events(repl, 1);
    ck_assert(is_ok(&result));
}

END_TEST

START_TEST(test_curl_events_perform_error) {
    /* Create mock provider instance with error vtable */
    struct ik_provider *instance = talloc_zero(agent, struct ik_provider);
    instance->vt = &mock_vt_error;
    instance->ctx = NULL;
    agent->provider_instance = instance;
    agent->curl_still_running = 1;
    atomic_store(&agent->state, IK_AGENT_STATE_WAITING_FOR_LLM);

    /* Add agent to repl */
    repl->agent_count = 1;
    repl->agents = talloc_array(repl, ik_agent_ctx_t *, 1);
    repl->agents[0] = agent;
    repl->current = agent;

    /* perform() will return an error */
    res_t result = ik_repl_handle_curl_events(repl, 1);
    /* Should propagate the error */
    ck_assert(is_err(&result));
}

END_TEST

/* ========== Test Suite Setup ========== */

static Suite *repl_event_handlers_curl_suite(void)
{
    Suite *s = suite_create("repl_event_handlers_curl");

    TCase *tc_curl_error = tcase_create("curl_error");
    tcase_set_timeout(tc_curl_error, IK_TEST_TIMEOUT);
    tcase_add_checked_fixture(tc_curl_error, setup, teardown);
    tcase_add_test(tc_curl_error, test_curl_events_with_http_error);
    tcase_add_test(tc_curl_error, test_curl_events_with_http_error_and_assistant_response);
    tcase_add_test(tc_curl_error, test_curl_events_with_running_curl_success);
    tcase_add_test(tc_curl_error, test_curl_events_not_current_agent);
    tcase_add_test(tc_curl_error, test_curl_events_is_current_agent_triggers_render);
    tcase_add_test(tc_curl_error, test_curl_events_with_null_current);
    tcase_add_test(tc_curl_error, test_curl_events_state_not_waiting_for_llm);
    tcase_add_test(tc_curl_error, test_curl_events_no_provider_instance);
    tcase_add_test(tc_curl_error, test_curl_events_perform_error);
    suite_add_tcase(s, tc_curl_error);

    return s;
}

int main(void)
{
    int number_failed;
    Suite *s = repl_event_handlers_curl_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/apps/ikigai/repl/repl_event_handlers_3_curl_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
