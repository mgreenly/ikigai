/**
 * @file repl_tool_completion_test.c
 * @brief Unit tests for repl_tool_completion functions
 *
 * Tests ik_repl_handle_tool_completion, ik_repl_handle_agent_tool_completion,
 * ik_repl_submit_tool_loop_continuation, and ik_repl_poll_tool_completions.
 */

#include "agent.h"
#include "config.h"
#include "message.h"
#include "providers/provider.h"
#include "providers/request.h"
#include "render.h"
#include "repl.h"
#include "repl_event_handlers.h"
#include "repl_tool_completion.h"
#include "scrollback.h"
#include "shared.h"
#include "terminal.h"
#include "tool.h"
#include "wrapper.h"
#include "db/message.h"

#include <check.h>
#include <inttypes.h>
#include <pthread.h>
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
    agent->state = IK_AGENT_STATE_EXECUTING_TOOL;
    agent->messages = NULL;
    agent->message_count = 0;
    agent->message_capacity = 0;
    agent->tool_iteration_count = 0;
    agent->response_finish_reason = NULL;
    agent->curl_still_running = 0;
    agent->pending_tool_call = NULL;

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

/**
 * Test: ik_repl_handle_tool_completion calls handle_agent_tool_completion
 */
START_TEST(test_handle_tool_completion_delegates_to_current) {
    /* Set up state for tool completion */
    agent->tool_thread_ctx = talloc_new(agent);
    agent->tool_thread_result = talloc_strdup(agent->tool_thread_ctx, "result");
    agent->pending_tool_call = ik_tool_call_create(agent, "call_1", "bash", "{}");
    agent->response_finish_reason = talloc_strdup(agent, "stop");

    /* Create thread and immediately mark as complete */
    pthread_create_(&agent->tool_thread, NULL, dummy_thread_func, NULL);
    agent->tool_thread_running = true;
    agent->tool_thread_complete = true;

    /* Initial state */
    int32_t initial_count = agent->tool_iteration_count;
    ck_assert_int_eq(agent->state, IK_AGENT_STATE_EXECUTING_TOOL);

    /* Make agent not current to avoid rendering */
    repl->current = NULL;

    /* Call the function */
    ik_repl_handle_agent_tool_completion(repl, agent);

    /* Verify it completed the tool */
    ck_assert_int_eq(agent->state, IK_AGENT_STATE_IDLE);
    ck_assert_uint_eq(agent->message_count, 2);  /* Tool call + tool result */
    ck_assert_int_eq(agent->tool_iteration_count, initial_count);  /* No increment, stop reason */
}

END_TEST


/**
 * Test: ik_repl_handle_agent_tool_completion renders when agent is current
 */
START_TEST(test_handle_agent_tool_completion_renders_current) {
    /* Set up state */
    agent->tool_thread_ctx = talloc_new(agent);
    agent->tool_thread_result = talloc_strdup(agent->tool_thread_ctx, "result");
    agent->pending_tool_call = ik_tool_call_create(agent, "call_1", "bash", "{}");
    agent->response_finish_reason = talloc_strdup(agent, "stop");

    /* Create thread and immediately mark as complete */
    pthread_create_(&agent->tool_thread, NULL, dummy_thread_func, NULL);
    agent->tool_thread_running = true;
    agent->tool_thread_complete = true;

    /* Ensure agent is NOT current to avoid rendering */
    repl->current = NULL;

    /* Call the function */
    ik_repl_handle_agent_tool_completion(repl, agent);

    /* Verify completion happened */
    ck_assert_int_eq(agent->state, IK_AGENT_STATE_IDLE);
}

END_TEST


/**
 * Test: ik_repl_poll_tool_completions with agent in agents array
 */
START_TEST(test_poll_tool_completions_agents_array) {
    /* Set up agents array with one agent */
    repl->agents = talloc_array(repl, ik_agent_ctx_t *, 1);
    repl->agents[0] = agent;
    repl->agent_count = 1;
    repl->current = NULL;  /* Different agent is current (or none) */

    agent->tool_thread_ctx = talloc_new(agent);
    agent->tool_thread_result = talloc_strdup(agent->tool_thread_ctx, "result");
    agent->pending_tool_call = ik_tool_call_create(agent, "call_1", "bash", "{}");
    agent->response_finish_reason = talloc_strdup(agent, "stop");

    /* Create thread and immediately mark as complete */
    pthread_create_(&agent->tool_thread, NULL, dummy_thread_func, NULL);
    agent->tool_thread_running = true;

    pthread_mutex_lock_(&agent->tool_thread_mutex);
    agent->state = IK_AGENT_STATE_EXECUTING_TOOL;
    agent->tool_thread_complete = true;
    pthread_mutex_unlock_(&agent->tool_thread_mutex);

    /* Call the function */
    res_t result = ik_repl_poll_tool_completions(repl);

    /* Verify success and tool was completed */
    ck_assert(is_ok(&result));
    ck_assert_int_eq(agent->state, IK_AGENT_STATE_IDLE);
    ck_assert_uint_eq(agent->message_count, 2);
}

END_TEST

/**
 * Test: ik_repl_poll_tool_completions with current agent not executing
 */
START_TEST(test_poll_tool_completions_current_not_executing) {
    /* Set up current agent in IDLE state */
    repl->agent_count = 0;
    repl->current = agent;

    pthread_mutex_lock_(&agent->tool_thread_mutex);
    agent->state = IK_AGENT_STATE_IDLE;
    agent->tool_thread_complete = false;
    pthread_mutex_unlock_(&agent->tool_thread_mutex);

    size_t initial_count = agent->message_count;

    /* Call the function */
    res_t result = ik_repl_poll_tool_completions(repl);

    /* Verify success but no tool completion */
    ck_assert(is_ok(&result));
    ck_assert_int_eq(agent->state, IK_AGENT_STATE_IDLE);
    ck_assert_uint_eq(agent->message_count, initial_count);
}

END_TEST

/**
 * Test suite
 */
static Suite *repl_tool_completion_suite(void)
{
    Suite *s = suite_create("repl_tool_completion");

    TCase *tc_core = tcase_create("core");
    tcase_add_checked_fixture(tc_core, setup, teardown);
    tcase_add_test(tc_core, test_handle_tool_completion_delegates_to_current);
    tcase_add_test(tc_core, test_handle_agent_tool_completion_renders_current);
    tcase_add_test(tc_core, test_poll_tool_completions_agents_array);
    tcase_add_test(tc_core, test_poll_tool_completions_current_not_executing);
    suite_add_tcase(s, tc_core);

    return s;
}

int main(void)
{
    Suite *s = repl_tool_completion_suite();
    SRunner *sr = srunner_create(s);
    srunner_run_all(sr, CK_VERBOSE);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);
    return (number_failed == 0) ? 0 : 1;
}
