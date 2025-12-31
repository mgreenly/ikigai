/**
 * @file repl_event_handlers_state_transition_test.c
 * @brief Unit tests for state transition coverage in REPL event handlers
 *
 * Tests for coverage gaps where agent state changes during request processing.
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
#include "../../../src/input_buffer/core.h"
#include "../../../src/render.h"
#include "../../../src/tool.h"
#include "../../../src/message.h"
#include "../../../src/wrapper.h"
#include "../../../src/repl_tool_completion.h"
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

/* Track if state transition mock was called */
static bool start_tool_execution_called = false;

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

/* Mock agent add message */
res_t ik_agent_add_message_(void *agent_ctx, void *msg)
{
    (void)agent_ctx;
    (void)msg;
    return OK(NULL);
}

/* Mock agent start tool execution - this changes the state */
void ik_agent_start_tool_execution_(void *agent_ctx)
{
    ik_agent_ctx_t *agent_local = (ik_agent_ctx_t *)agent_ctx;
    start_tool_execution_called = true;
    /* Change state from WAITING_FOR_LLM to EXECUTING_TOOL */
    agent_local->state = IK_AGENT_STATE_EXECUTING_TOOL;
}

/* Mock agent should continue tool loop */
int ik_agent_should_continue_tool_loop_(const void *agent_ctx)
{
    (void)agent_ctx;
    return 0;
}

/* Mock repl submit tool loop continuation */
void ik_repl_submit_tool_loop_continuation_(void *repl_ctx, void *agent_ctx)
{
    (void)repl_ctx;
    (void)agent_ctx;
}

/* Mock agent transition to idle */
void ik_agent_transition_to_idle_(void *agent_ctx)
{
    ik_agent_ctx_t *agent_local = (ik_agent_ctx_t *)agent_ctx;
    agent_local->state = IK_AGENT_STATE_IDLE;
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
    error_ctx = ctx;
    start_tool_execution_called = false;

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
    (void)res;
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
}

static void teardown(void)
{
    pthread_mutex_destroy(&agent->tool_thread_mutex);
    talloc_free(ctx);
    ctx = NULL;
    repl = NULL;
    shared = NULL;
    agent = NULL;
    fake_db = NULL;
    error_ctx = NULL;
}

/**
 * Test: State changes from WAITING_FOR_LLM to EXECUTING_TOOL after successful request
 * Covers: Line 257 branch where current_state != WAITING_FOR_LLM after handling request
 */
START_TEST(test_state_changes_to_executing_tool)
{
    /* Create a dummy tool call structure */
    ik_tool_call_t *dummy_tool_call = talloc_zero(agent, ik_tool_call_t);
    dummy_tool_call->id = talloc_strdup(dummy_tool_call, "call_123");
    dummy_tool_call->name = talloc_strdup(dummy_tool_call, "test_tool");
    dummy_tool_call->arguments = talloc_strdup(dummy_tool_call, "{}");

    /* Create mock provider instance */
    struct ik_provider *instance = talloc_zero(agent, struct ik_provider);
    instance->vt = &mock_vt;
    instance->ctx = NULL;
    agent->provider_instance = instance;
    agent->curl_still_running = 1;
    agent->state = IK_AGENT_STATE_WAITING_FOR_LLM;
    agent->assistant_response = talloc_strdup(agent, "Response text");
    agent->pending_tool_call = dummy_tool_call;  /* This will trigger tool execution */

    /* Add agent to repl and set as current */
    repl->agent_count = 1;
    repl->agents = talloc_array(repl, ik_agent_ctx_t *, 1);
    repl->agents[0] = agent;
    repl->current = agent;

    /* Mock perform will set still_running to 0 */
    res_t result = ik_repl_handle_curl_events(repl, 1);
    ck_assert(is_ok(&result));

    /* Verify that start_tool_execution was called */
    ck_assert(start_tool_execution_called);

    /* Verify state changed to EXECUTING_TOOL */
    ck_assert_int_eq(agent->state, IK_AGENT_STATE_EXECUTING_TOOL);

    /* Response should be freed */
    ck_assert_ptr_null(agent->assistant_response);
}

END_TEST

/**
 * Test: Current agent is in the agents array (normal case)
 * Covers: Line 284 branch where current IS in array
 */
START_TEST(test_current_agent_in_array)
{
    /* Create mock provider instance */
    struct ik_provider *instance = talloc_zero(agent, struct ik_provider);
    instance->vt = &mock_vt;
    instance->ctx = NULL;
    agent->provider_instance = instance;
    agent->curl_still_running = 1;
    agent->state = IK_AGENT_STATE_WAITING_FOR_LLM;
    agent->assistant_response = talloc_strdup(agent, "Response text");

    /* Add agent to repl array AND set as current */
    repl->agent_count = 1;
    repl->agents = talloc_array(repl, ik_agent_ctx_t *, 1);
    repl->agents[0] = agent;
    repl->current = agent;  /* Current IS in array */

    /* Process events - should only process once (not duplicate) */
    res_t result = ik_repl_handle_curl_events(repl, 1);
    ck_assert(is_ok(&result));

    /* Response should be freed */
    ck_assert_ptr_null(agent->assistant_response);
}

END_TEST

/* ========== Test Suite Setup ========== */

static Suite *repl_event_handlers_state_transition_suite(void)
{
    Suite *s = suite_create("repl_event_handlers_state_transition");

    TCase *tc_state = tcase_create("state_transitions");
    tcase_set_timeout(tc_state, 30);
    tcase_add_checked_fixture(tc_state, setup, teardown);
    tcase_add_test(tc_state, test_state_changes_to_executing_tool);
    tcase_add_test(tc_state, test_current_agent_in_array);
    suite_add_tcase(s, tc_state);

    return s;
}

int main(void)
{
    int number_failed;
    Suite *s = repl_event_handlers_state_transition_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
