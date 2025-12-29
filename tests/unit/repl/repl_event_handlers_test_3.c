/**
 * @file repl_event_handlers_test_3.c
 * @brief Unit tests for REPL event handler functions (Part 3)
 *
 * Tests for coverage gaps in persist_assistant_msg, handle_agent_request_error,
 * process_agent_curl_events, and related paths.
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
#include "../../../src/message.h"
#include "../../../src/wrapper.h"
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
static ik_db_ctx_t *fake_db;

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

static void setup(void)
{
    ctx = talloc_new(NULL);

    /* Create fake database context */
    fake_db = talloc_zero(ctx, ik_db_ctx_t);

    /* Create shared context */
    shared = talloc_zero(ctx, ik_shared_ctx_t);
    shared->term = talloc_zero(shared, ik_term_ctx_t);
    shared->term->tty_fd = 0;
    shared->db_ctx = fake_db;
    shared->session_id = 123;
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

/* ========== Database persistence tests ========== */

START_TEST(test_persist_with_thinking_level_low)
{
    agent->thinking_level = 1;  /* LOW */
    agent->response_model = talloc_strdup(agent, "test-model");
    agent->assistant_response = talloc_strdup(agent, "Test response");

    ik_repl_handle_agent_request_success(repl, agent);

    /* Response should be freed */
    ck_assert_ptr_null(agent->assistant_response);
}

END_TEST

START_TEST(test_persist_with_thinking_level_med)
{
    agent->thinking_level = 2;  /* MED */
    agent->response_model = talloc_strdup(agent, "test-model");
    agent->assistant_response = talloc_strdup(agent, "Test response");

    ik_repl_handle_agent_request_success(repl, agent);

    ck_assert_ptr_null(agent->assistant_response);
}

END_TEST

START_TEST(test_persist_with_thinking_level_high)
{
    agent->thinking_level = 3;  /* HIGH */
    agent->response_model = talloc_strdup(agent, "test-model");
    agent->assistant_response = talloc_strdup(agent, "Test response");

    ik_repl_handle_agent_request_success(repl, agent);

    ck_assert_ptr_null(agent->assistant_response);
}

END_TEST

START_TEST(test_persist_with_thinking_level_unknown)
{
    agent->thinking_level = 99;  /* Unknown value - will use default "unknown" */
    agent->response_model = talloc_strdup(agent, "test-model");
    agent->assistant_response = talloc_strdup(agent, "Test response");

    ik_repl_handle_agent_request_success(repl, agent);

    ck_assert_ptr_null(agent->assistant_response);
}

END_TEST

START_TEST(test_persist_with_provider_info)
{
    agent->provider = talloc_strdup(agent, "anthropic");
    agent->response_model = talloc_strdup(agent, "claude-3-opus");
    agent->response_finish_reason = talloc_strdup(agent, "end_turn");
    agent->thinking_level = 0;
    agent->assistant_response = talloc_strdup(agent, "Test response");

    ik_repl_handle_agent_request_success(repl, agent);

    ck_assert_ptr_null(agent->assistant_response);
}

END_TEST

START_TEST(test_persist_with_usage_tokens)
{
    agent->response_input_tokens = 100;
    agent->response_output_tokens = 50;
    agent->response_thinking_tokens = 25;
    agent->response_model = talloc_strdup(agent, "test-model");
    agent->assistant_response = talloc_strdup(agent, "Test response");

    ik_repl_handle_agent_request_success(repl, agent);

    ck_assert_ptr_null(agent->assistant_response);
}

END_TEST

START_TEST(test_persist_no_usage_tokens)
{
    agent->response_input_tokens = 0;
    agent->response_output_tokens = 0;
    agent->response_thinking_tokens = 0;
    agent->response_model = talloc_strdup(agent, "test-model");
    agent->assistant_response = talloc_strdup(agent, "Test response");

    ik_repl_handle_agent_request_success(repl, agent);

    ck_assert_ptr_null(agent->assistant_response);
}

END_TEST

/* ========== curl events with error handling Tests ========== */

START_TEST(test_curl_events_with_http_error)
{
    /* Create mock provider instance */
    struct ik_provider *instance = talloc_zero(agent, struct ik_provider);
    instance->vt = &mock_vt;
    instance->ctx = NULL;
    agent->provider_instance = instance;
    agent->curl_still_running = 1;
    agent->state = IK_AGENT_STATE_WAITING_FOR_LLM;
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
    other_agent->state = IK_AGENT_STATE_IDLE;
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

START_TEST(test_curl_events_with_http_error_and_assistant_response)
{
    /* Create mock provider instance */
    struct ik_provider *instance = talloc_zero(agent, struct ik_provider);
    instance->vt = &mock_vt;
    instance->ctx = NULL;
    agent->provider_instance = instance;
    agent->curl_still_running = 1;
    agent->state = IK_AGENT_STATE_WAITING_FOR_LLM;
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
    other_agent->state = IK_AGENT_STATE_IDLE;
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

START_TEST(test_curl_events_with_running_curl_success)
{
    /* Create mock provider instance */
    struct ik_provider *instance = talloc_zero(agent, struct ik_provider);
    instance->vt = &mock_vt;
    instance->ctx = NULL;
    agent->provider_instance = instance;
    agent->curl_still_running = 1;
    agent->state = IK_AGENT_STATE_WAITING_FOR_LLM;
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
    other_agent->state = IK_AGENT_STATE_IDLE;
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

START_TEST(test_curl_events_not_current_agent)
{
    /* Create mock provider instance */
    struct ik_provider *instance = talloc_zero(agent, struct ik_provider);
    instance->vt = &mock_vt;
    instance->ctx = NULL;
    agent->provider_instance = instance;
    agent->curl_still_running = 1;
    agent->state = IK_AGENT_STATE_WAITING_FOR_LLM;
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
    other_agent->state = IK_AGENT_STATE_IDLE;
    pthread_mutex_init(&other_agent->tool_thread_mutex, NULL);
    repl->current = other_agent;

    /* Mock perform will set still_running to 0 */
    res_t result = ik_repl_handle_curl_events(repl, 1);
    ck_assert(is_ok(&result));

    pthread_mutex_destroy(&other_agent->tool_thread_mutex);
}

END_TEST

START_TEST(test_curl_events_is_current_agent_triggers_render)
{
    /* Create mock provider instance */
    struct ik_provider *instance = talloc_zero(agent, struct ik_provider);
    instance->vt = &mock_vt;
    instance->ctx = NULL;
    agent->provider_instance = instance;
    agent->curl_still_running = 1;
    agent->state = IK_AGENT_STATE_WAITING_FOR_LLM;
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

/* ========== Test Suite Setup ========== */

static Suite *repl_event_handlers_suite(void)
{
    Suite *s = suite_create("repl_event_handlers_3");

    TCase *tc_persist = tcase_create("persist");
    tcase_add_checked_fixture(tc_persist, setup, teardown);
    tcase_add_test(tc_persist, test_persist_with_thinking_level_low);
    tcase_add_test(tc_persist, test_persist_with_thinking_level_med);
    tcase_add_test(tc_persist, test_persist_with_thinking_level_high);
    tcase_add_test(tc_persist, test_persist_with_thinking_level_unknown);
    tcase_add_test(tc_persist, test_persist_with_provider_info);
    tcase_add_test(tc_persist, test_persist_with_usage_tokens);
    tcase_add_test(tc_persist, test_persist_no_usage_tokens);
    suite_add_tcase(s, tc_persist);

    TCase *tc_curl_error = tcase_create("curl_error");
    tcase_add_checked_fixture(tc_curl_error, setup, teardown);
    tcase_add_test(tc_curl_error, test_curl_events_with_http_error);
    tcase_add_test(tc_curl_error, test_curl_events_with_http_error_and_assistant_response);
    tcase_add_test(tc_curl_error, test_curl_events_with_running_curl_success);
    tcase_add_test(tc_curl_error, test_curl_events_not_current_agent);
    tcase_add_test(tc_curl_error, test_curl_events_is_current_agent_triggers_render);
    suite_add_tcase(s, tc_curl_error);

    return s;
}

int main(void)
{
    int number_failed;
    Suite *s = repl_event_handlers_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
