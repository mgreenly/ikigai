#include "tests/test_constants.h"
#include "apps/ikigai/wrapper_pthread.h"
/**
 * @file repl_tool_completion_poll_test.c
 * @brief Unit tests for poll and submit tool completion functions
 *
 * Tests ik_repl_poll_tool_completions and ik_repl_submit_tool_loop_continuation.
 */

#include "apps/ikigai/agent.h"
#include "apps/ikigai/config.h"
#include "apps/ikigai/input_buffer/core.h"
#include "apps/ikigai/message.h"
#include "apps/ikigai/providers/provider.h"
#include "apps/ikigai/providers/request.h"
#include "apps/ikigai/render.h"
#include "apps/ikigai/repl.h"
#include "apps/ikigai/repl_event_handlers.h"
#include "apps/ikigai/repl_tool_completion.h"
#include "apps/ikigai/scrollback.h"
#include "apps/ikigai/shared.h"
#include "shared/terminal.h"
#include "apps/ikigai/tool.h"
#include "shared/wrapper.h"
#include "apps/ikigai/db/message.h"

#include <check.h>
#include <inttypes.h>
#include <pthread.h>
#include <stdatomic.h>
#include <talloc.h>
#include <unistd.h>

/* Mock db message insert */
res_t ik_db_message_insert_(void *db, int64_t session_id, const char *agent_uuid,
                            const char *kind, const char *content, const char *data_json)
{
    (void)db; (void)session_id; (void)agent_uuid;
    (void)kind; (void)content; (void)data_json;
    return OK(NULL);
}

/* Mock render frame - used to test the render branch without full render setup */
res_t ik_repl_render_frame_(void *repl)
{
    (void)repl;
    return OK(NULL);
}

/* Test control flags for mocks */
static bool mock_provider_should_fail = true;
static bool mock_request_should_fail = false;
static bool mock_stream_should_fail = false;

/* Mock provider vtable */
static res_t mock_start_stream(void *provider_ctx, const ik_request_t *request,
                               ik_stream_cb_t stream_cb, void *stream_data,
                               ik_provider_completion_cb_t completion_cb, void *completion_data)
{
    (void)provider_ctx; (void)request;
    (void)stream_cb; (void)stream_data;
    (void)completion_cb; (void)completion_data;

    if (mock_stream_should_fail) {
        return ERR(provider_ctx, PROVIDER, "Mock stream error");
    }
    return OK(NULL);
}

static ik_provider_vtable_t mock_provider_vt = {
    .fdset = NULL,
    .perform = NULL,
    .timeout = NULL,
    .info_read = NULL,
    .start_request = NULL,
    .start_stream = mock_start_stream,
    .cleanup = NULL,
    .cancel = NULL,
};

static ik_provider_t mock_provider = {
    .name = "mock",
    .ctx = NULL,
    .vt = &mock_provider_vt,
};

/* Mock agent_get_provider - needed for tool loop continuation */
res_t ik_agent_get_provider_(void *agent, void **provider_out)
{
    if (mock_provider_should_fail) {
        /* Return error with agent context to satisfy error context requirements */
        return ERR(agent, PROVIDER, "Mock provider error");
    }
    mock_provider.ctx = agent;
    *provider_out = &mock_provider;
    return OK(NULL);
}

/* Mock request_build_from_conversation - needed if get_provider succeeds */
res_t ik_request_build_from_conversation_(TALLOC_CTX *ctx, void *agent, void *registry, void **req_out)
{
    (void)agent;
    (void)registry;

    if (mock_request_should_fail) {
        /* Return error with ctx to satisfy error context requirements */
        return ERR(ctx, PARSE, "Mock request build error");
    }
    /* Create a minimal mock request */
    ik_request_t *req = talloc_zero(ctx, ik_request_t);
    *req_out = req;
    return OK(NULL);
}

/* Dummy thread function that immediately exits - used for tests that need
 * a valid thread handle for pthread_join but don't actually run the thread */
static void *dummy_thread_func(void *arg)
{
    (void)arg;
    return NULL;
}

static void *ctx;
static ik_repl_ctx_t *repl;
static ik_agent_ctx_t *agent;

static void setup(void)
{
    /* Reset mock flags */
    mock_provider_should_fail = true;
    mock_request_should_fail = false;
    mock_stream_should_fail = false;

    ctx = talloc_new(NULL);

    /* Create minimal shared context */
    ik_shared_ctx_t *shared = talloc_zero(ctx, ik_shared_ctx_t);
    shared->db_ctx = NULL;
    shared->session_id = 0;
    shared->cfg = talloc_zero(ctx, ik_config_t);
    shared->cfg->max_tool_turns = 10;
    shared->term = talloc_zero(ctx, ik_term_ctx_t);
    shared->term->screen_rows = 24;
    shared->term->screen_cols = 80;
    shared->render = NULL;  /* Don't need rendering for these tests */

    /* Create REPL context */
    repl = talloc_zero(ctx, ik_repl_ctx_t);
    repl->shared = shared;
    repl->agents = NULL;
    repl->agent_count = 0;

    /* Create agent */
    agent = talloc_zero(repl, ik_agent_ctx_t);
    agent->shared = shared;
    agent->repl = repl;
    agent->scrollback = ik_scrollback_create(agent, 80);
    atomic_store(&agent->state, IK_AGENT_STATE_EXECUTING_TOOL);
    agent->messages = NULL;
    agent->message_count = 0;
    agent->message_capacity = 0;
    agent->tool_iteration_count = 0;
    agent->response_finish_reason = NULL;
    agent->curl_still_running = 0;
    agent->pending_tool_call = NULL;
    agent->input_buffer = NULL;  /* Not needed for most tests */
    agent->provider = talloc_strdup(agent, "openai");
    agent->model = talloc_strdup(agent, "gpt-4");

    pthread_mutex_init_(&agent->tool_thread_mutex, NULL);
    agent->tool_thread_running = false;
    agent->tool_thread_complete = false;
    agent->tool_thread_ctx = NULL;
    agent->tool_thread_result = NULL;

    repl->current = agent;
}

static void teardown(void)
{
    talloc_free(ctx);
}

/* Helper: Setup agent for tool completion with thread */
static void setup_tool_completion(const char *finish_reason)
{
    agent->tool_thread_ctx = talloc_new(agent);
    agent->tool_thread_result = talloc_strdup(agent->tool_thread_ctx, "result");
    agent->pending_tool_call = ik_tool_call_create(agent, "call_1", "bash", "{}");
    agent->response_finish_reason = talloc_strdup(agent, finish_reason);
    pthread_create_(&agent->tool_thread, NULL, dummy_thread_func, NULL);
    agent->tool_thread_running = true;
    agent->tool_thread_complete = true;
}

/* Test: poll with agent in agents array */
START_TEST(test_poll_tool_completions_agents_array) {
    setup_tool_completion("stop");
    repl->agents = talloc_array(repl, ik_agent_ctx_t *, 1);
    repl->agents[0] = agent;
    repl->agent_count = 1;
    repl->current = NULL;
    pthread_mutex_lock_(&agent->tool_thread_mutex);
    atomic_store(&agent->state, IK_AGENT_STATE_EXECUTING_TOOL);
    agent->tool_thread_complete = true;
    pthread_mutex_unlock_(&agent->tool_thread_mutex);
    res_t result = ik_repl_poll_tool_completions(repl);
    ck_assert(is_ok(&result));
    ck_assert_int_eq(agent->state, IK_AGENT_STATE_IDLE);
    ck_assert_uint_eq(agent->message_count, 2);
}
END_TEST

/* Test: poll with current agent not executing */
START_TEST(test_poll_tool_completions_current_not_executing) {
    repl->agent_count = 0;
    repl->current = agent;
    pthread_mutex_lock_(&agent->tool_thread_mutex);
    atomic_store(&agent->state, IK_AGENT_STATE_IDLE);
    agent->tool_thread_complete = false;
    pthread_mutex_unlock_(&agent->tool_thread_mutex);
    size_t initial_count = agent->message_count;
    res_t result = ik_repl_poll_tool_completions(repl);
    ck_assert(is_ok(&result));
    ck_assert_int_eq(agent->state, IK_AGENT_STATE_IDLE);
    ck_assert_uint_eq(agent->message_count, initial_count);
}
END_TEST

/* Test: poll with current agent executing */
START_TEST(test_poll_tool_completions_current_executing) {
    setup_tool_completion("stop");
    repl->agent_count = 0;
    pthread_mutex_lock_(&agent->tool_thread_mutex);
    atomic_store(&agent->state, IK_AGENT_STATE_EXECUTING_TOOL);
    agent->tool_thread_complete = true;
    pthread_mutex_unlock_(&agent->tool_thread_mutex);
    repl->current = agent;
    res_t result = ik_repl_poll_tool_completions(repl);
    ck_assert(is_ok(&result));
    ck_assert_int_eq(agent->state, IK_AGENT_STATE_IDLE);
    ck_assert_uint_eq(agent->message_count, 2);
}
END_TEST

/* Test: poll with no agents */
START_TEST(test_poll_tool_completions_no_agents) {
    repl->agent_count = 0;
    repl->current = NULL;
    res_t result = ik_repl_poll_tool_completions(repl);
    ck_assert(is_ok(&result));
}
END_TEST

/* Test: poll with agent not complete */
START_TEST(test_poll_tool_completions_agent_not_complete) {
    setup_tool_completion("stop");
    repl->agents = talloc_array(repl, ik_agent_ctx_t *, 1);
    repl->agents[0] = agent;
    repl->agent_count = 1;
    repl->current = NULL;
    pthread_mutex_lock_(&agent->tool_thread_mutex);
    atomic_store(&agent->state, IK_AGENT_STATE_EXECUTING_TOOL);
    agent->tool_thread_complete = false;
    pthread_mutex_unlock_(&agent->tool_thread_mutex);
    size_t initial_count = agent->message_count;
    res_t result = ik_repl_poll_tool_completions(repl);
    ck_assert(is_ok(&result));
    ck_assert_uint_eq(agent->message_count, initial_count);
    ck_assert_int_eq(agent->state, IK_AGENT_STATE_EXECUTING_TOOL);
    pthread_join_(agent->tool_thread, NULL);
}
END_TEST

/* Test: poll with agent in wrong state */
START_TEST(test_poll_tool_completions_agent_wrong_state) {
    setup_tool_completion("stop");
    repl->agents = talloc_array(repl, ik_agent_ctx_t *, 1);
    repl->agents[0] = agent;
    repl->agent_count = 1;
    repl->current = NULL;
    pthread_mutex_lock_(&agent->tool_thread_mutex);
    atomic_store(&agent->state, IK_AGENT_STATE_IDLE);
    agent->tool_thread_complete = true;
    pthread_mutex_unlock_(&agent->tool_thread_mutex);
    size_t initial_count = agent->message_count;
    res_t result = ik_repl_poll_tool_completions(repl);
    ck_assert(is_ok(&result));
    ck_assert_uint_eq(agent->message_count, initial_count);
    ck_assert_int_eq(agent->state, IK_AGENT_STATE_IDLE);
    pthread_join_(agent->tool_thread, NULL);
}
END_TEST

/* Test: poll current executing not complete */
START_TEST(test_poll_tool_completions_current_executing_not_complete) {
    setup_tool_completion("stop");
    repl->agent_count = 0;
    repl->current = agent;
    pthread_mutex_lock_(&agent->tool_thread_mutex);
    atomic_store(&agent->state, IK_AGENT_STATE_EXECUTING_TOOL);
    agent->tool_thread_complete = false;
    pthread_mutex_unlock_(&agent->tool_thread_mutex);
    size_t initial_count = agent->message_count;
    res_t result = ik_repl_poll_tool_completions(repl);
    ck_assert(is_ok(&result));
    ck_assert_uint_eq(agent->message_count, initial_count);
    ck_assert_int_eq(agent->state, IK_AGENT_STATE_EXECUTING_TOOL);
    pthread_join_(agent->tool_thread, NULL);
}
END_TEST

/* Test: submit with request build error */
START_TEST(test_submit_tool_loop_continuation_request_error) {
    agent->tool_thread_ctx = talloc_new(agent);
    agent->tool_thread_result = talloc_strdup(agent->tool_thread_ctx, "result");
    agent->pending_tool_call = ik_tool_call_create(agent, "call_1", "bash", "{}");
    agent->response_finish_reason = talloc_strdup(agent, "tool_calls");
    atomic_store(&agent->state, IK_AGENT_STATE_WAITING_FOR_LLM);
    mock_provider_should_fail = false;
    mock_request_should_fail = true;
    mock_stream_should_fail = false;
    size_t initial_scrollback_count = agent->scrollback->count;
    ik_repl_submit_tool_loop_continuation(repl, agent);
    ck_assert_int_eq(agent->state, IK_AGENT_STATE_IDLE);
    ck_assert(agent->scrollback->count > initial_scrollback_count);
}
END_TEST

/* Test: submit with start_stream error */
START_TEST(test_submit_tool_loop_continuation_stream_error) {
    agent->tool_thread_ctx = talloc_new(agent);
    agent->tool_thread_result = talloc_strdup(agent->tool_thread_ctx, "result");
    agent->pending_tool_call = ik_tool_call_create(agent, "call_1", "bash", "{}");
    agent->response_finish_reason = talloc_strdup(agent, "tool_calls");
    atomic_store(&agent->state, IK_AGENT_STATE_WAITING_FOR_LLM);
    mock_provider_should_fail = false;
    mock_request_should_fail = false;
    mock_stream_should_fail = true;
    size_t initial_scrollback_count = agent->scrollback->count;
    ik_repl_submit_tool_loop_continuation(repl, agent);
    ck_assert_int_eq(agent->state, IK_AGENT_STATE_IDLE);
    ck_assert(agent->scrollback->count > initial_scrollback_count);
}
END_TEST

/* Test: submit with success */
START_TEST(test_submit_tool_loop_continuation_success) {
    agent->tool_thread_ctx = talloc_new(agent);
    agent->tool_thread_result = talloc_strdup(agent->tool_thread_ctx, "result");
    agent->pending_tool_call = ik_tool_call_create(agent, "call_1", "bash", "{}");
    agent->response_finish_reason = talloc_strdup(agent, "tool_calls");
    atomic_store(&agent->state, IK_AGENT_STATE_WAITING_FOR_LLM);
    agent->curl_still_running = 0;
    mock_provider_should_fail = false;
    mock_request_should_fail = false;
    mock_stream_should_fail = false;
    ik_repl_submit_tool_loop_continuation(repl, agent);
    ck_assert_int_eq(agent->curl_still_running, 1);
}
END_TEST

/**
 * Test suite
 */
static Suite *repl_tool_completion_poll_suite(void)
{
    Suite *s = suite_create("repl_tool_completion_poll");

    TCase *tc_core = tcase_create("core");
    tcase_set_timeout(tc_core, IK_TEST_TIMEOUT);
    tcase_add_checked_fixture(tc_core, setup, teardown);
    tcase_add_test(tc_core, test_poll_tool_completions_agents_array);
    tcase_add_test(tc_core, test_poll_tool_completions_current_not_executing);
    tcase_add_test(tc_core, test_poll_tool_completions_current_executing);
    tcase_add_test(tc_core, test_poll_tool_completions_no_agents);
    tcase_add_test(tc_core, test_poll_tool_completions_agent_not_complete);
    tcase_add_test(tc_core, test_poll_tool_completions_agent_wrong_state);
    tcase_add_test(tc_core, test_poll_tool_completions_current_executing_not_complete);
    tcase_add_test(tc_core, test_submit_tool_loop_continuation_request_error);
    tcase_add_test(tc_core, test_submit_tool_loop_continuation_stream_error);
    tcase_add_test(tc_core, test_submit_tool_loop_continuation_success);
    suite_add_tcase(s, tc_core);

    return s;
}

int main(void)
{
    Suite *s = repl_tool_completion_poll_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/apps/ikigai/repl/repl_tool_completion_poll_test.xml");
    srunner_run_all(sr, CK_VERBOSE);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);
    return (number_failed == 0) ? 0 : 1;
}
