#include "../../test_constants.h"
/**
 * @file repl_event_handlers_test_3_tool.c
 * @brief Unit tests for REPL event handler tool execution paths
 *
 * Tests for coverage gaps in ik_repl_handle_agent_request_success,
 * specifically the pending_tool_call and tool_loop_continuation branches.
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
#include "../../../src/scroll_detector.h"
#include "../../../src/input_buffer/core.h"
#include "../../../src/render.h"
#include "../../../src/tool.h"
#include "../../../src/message.h"
#include "../../../src/wrapper.h"
#include <check.h>
#include <talloc.h>
#include <unistd.h>
#include <pthread.h>
#include <stdatomic.h>

static void *ctx;
static ik_repl_ctx_t *repl;
static ik_shared_ctx_t *shared;
static ik_agent_ctx_t *agent;
static ik_db_ctx_t *fake_db;

/* Mock functions for tool execution paths */
static bool mock_start_tool_called = false;
static bool mock_should_continue_called = false;
static bool mock_should_continue_return = false;
static bool mock_submit_continuation_called = false;

void ik_agent_start_tool_execution_(void *agent_ptr)
{
    (void)agent_ptr;
    mock_start_tool_called = true;
}

int ik_agent_should_continue_tool_loop_(const void *agent_ptr)
{
    (void)agent_ptr;
    mock_should_continue_called = true;
    return mock_should_continue_return ? 1 : 0;
}

void ik_repl_submit_tool_loop_continuation_(void *repl_ptr, void *agent_ptr)
{
    (void)repl_ptr;
    (void)agent_ptr;
    mock_submit_continuation_called = true;
}

res_t ik_agent_add_message_(void *agent_ptr, void *msg_ptr)
{
    (void)agent_ptr;
    (void)msg_ptr;
    return OK(NULL);
}

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

static void setup(void)
{
    ctx = talloc_new(NULL);

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

    /* Create render context */
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
    atomic_store(&agent->state, IK_AGENT_STATE_IDLE);
    agent->tool_iteration_count = 0;
    pthread_mutex_init(&agent->tool_thread_mutex, NULL);

    /* Agent metadata */
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

START_TEST(test_success_with_pending_tool_call) {
    /* Reset mock state */
    mock_start_tool_called = false;
    mock_should_continue_called = false;
    mock_submit_continuation_called = false;

    /* Set up agent with assistant response and pending tool call */
    agent->assistant_response = talloc_strdup(agent, "Response text");

    /* Create pending tool call */
    ik_tool_call_t *tool_call = talloc_zero(agent, ik_tool_call_t);
    tool_call->id = talloc_strdup(tool_call, "call_123");
    tool_call->name = talloc_strdup(tool_call, "test_tool");
    tool_call->arguments = talloc_strdup(tool_call, "{}");
    agent->pending_tool_call = tool_call;

    /* Set as current for persist_assistant_msg */
    repl->current = agent;

    /* Call the success handler directly */
    ik_repl_handle_agent_request_success(repl, agent);

    /* Verify tool execution was started (via mock) */
    ck_assert(mock_start_tool_called);
    /* Should not continue to tool loop check when tool is pending */
    ck_assert(!mock_should_continue_called);
    ck_assert(!mock_submit_continuation_called);
}

END_TEST

START_TEST(test_success_with_tool_loop_continuation) {
    /* Reset mock state */
    mock_start_tool_called = false;
    mock_should_continue_called = false;
    mock_submit_continuation_called = false;
    mock_should_continue_return = true;  /* Tool loop should continue */

    /* Set up agent with assistant response but no pending tool */
    agent->assistant_response = talloc_strdup(agent, "Response text");
    agent->pending_tool_call = NULL;  /* No pending tool */
    agent->tool_iteration_count = 0;

    /* Set as current for persist_assistant_msg */
    repl->current = agent;

    /* Call the success handler directly */
    ik_repl_handle_agent_request_success(repl, agent);

    /* Verify tool loop continuation was called */
    ck_assert(!mock_start_tool_called);  /* No pending tool */
    ck_assert(mock_should_continue_called);
    ck_assert(mock_submit_continuation_called);
    /* Iteration count should be incremented */
    ck_assert_int_eq(agent->tool_iteration_count, 1);
}

END_TEST

START_TEST(test_success_without_tool_continuation) {
    /* Reset mock state */
    mock_start_tool_called = false;
    mock_should_continue_called = false;
    mock_submit_continuation_called = false;
    mock_should_continue_return = false;  /* Tool loop should NOT continue */

    /* Set up agent with assistant response but no pending tool */
    agent->assistant_response = talloc_strdup(agent, "Response text");
    agent->pending_tool_call = NULL;  /* No pending tool */
    agent->tool_iteration_count = 5;

    /* Set as current for persist_assistant_msg */
    repl->current = agent;

    /* Call the success handler directly */
    ik_repl_handle_agent_request_success(repl, agent);

    /* Verify tool loop check was done but continuation not submitted */
    ck_assert(!mock_start_tool_called);
    ck_assert(mock_should_continue_called);
    ck_assert(!mock_submit_continuation_called);
    /* Iteration count should NOT be incremented */
    ck_assert_int_eq(agent->tool_iteration_count, 5);
}

END_TEST

static Suite *repl_event_handlers_tool_suite(void)
{
    Suite *s = suite_create("repl_event_handlers_tool");

    TCase *tc_tool = tcase_create("tool_execution");
    tcase_set_timeout(tc_tool, IK_TEST_TIMEOUT);
    tcase_add_checked_fixture(tc_tool, setup, teardown);
    tcase_add_test(tc_tool, test_success_with_pending_tool_call);
    tcase_add_test(tc_tool, test_success_with_tool_loop_continuation);
    tcase_add_test(tc_tool, test_success_without_tool_continuation);
    suite_add_tcase(s, tc_tool);

    return s;
}

int main(void)
{
    int number_failed;
    Suite *s = repl_event_handlers_tool_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/repl/repl_event_handlers_3_tool_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
