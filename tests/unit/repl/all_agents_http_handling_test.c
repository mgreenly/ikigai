/**
 * @file all_agents_http_handling_test.c
 * @brief Unit tests for ik_repl_handle_curl_events processing all agents
 *
 * Tests that ik_repl_handle_curl_events processes HTTP completions for all agents,
 * not just the current agent.
 */

#include <check.h>
#include <talloc.h>
#include <pthread.h>
#include <string.h>
#include "agent.h"
#include "logger.h"
#include "openai/client.h"
#include "openai/client_multi.h"
#include "repl.h"
#include "repl_event_handlers.h"
#include "scrollback.h"
#include "shared.h"
#include "terminal.h"
#include "wrapper.h"

#include "../../test_utils.h"

// Per-test state
static TALLOC_CTX *test_ctx;
static ik_repl_ctx_t *repl;
static ik_agent_ctx_t *agent_a;
static ik_agent_ctx_t *agent_b;

// Suite-level setup: Set log directory
static void suite_setup(void)
{
    ik_test_set_log_dir(__FILE__);
}

// Create test agent helper
static ik_agent_ctx_t *create_test_agent(ik_repl_ctx_t *parent, const char *uuid)
{
    ik_agent_ctx_t *agent = talloc_zero(parent, ik_agent_ctx_t);
    agent->uuid = talloc_strdup(agent, uuid);
    agent->state = IK_AGENT_STATE_IDLE;
    agent->repl = parent;

    // Initialize thread infrastructure
    pthread_mutex_init_(&agent->tool_thread_mutex, NULL);
    agent->tool_thread_running = false;
    agent->tool_thread_complete = false;

    // Initialize spinner
    agent->spinner_state.visible = false;
    agent->spinner_state.frame_index = 0;

    // Create curl_multi handle
    res_t multi_res = ik_openai_multi_create(agent);
    ck_assert(!is_err(&multi_res));
    agent->multi = multi_res.ok;
    agent->curl_still_running = 0;

    // Create conversation
    agent->conversation = ik_openai_conversation_create(agent);

    // Create scrollback
    agent->scrollback = ik_scrollback_create(agent, 1000);
    ck_assert_ptr_nonnull(agent->scrollback);

    // Create input buffer (required for rendering)
    agent->input_buffer = ik_input_buffer_create(agent);
    ck_assert_ptr_nonnull(agent->input_buffer);

    return agent;
}

// Per-test setup
static void setup(void)
{
    test_ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(test_ctx);

    // Create REPL context
    repl = talloc_zero(test_ctx, ik_repl_ctx_t);
    ck_assert_ptr_nonnull(repl);

    // Create shared context
    repl->shared = talloc_zero(test_ctx, ik_shared_ctx_t);
    ck_assert_ptr_nonnull(repl->shared);

    // Create logger (required for ik_repl_handle_curl_events)
    repl->shared->logger = ik_logger_create(repl->shared, "/tmp");
    ck_assert_ptr_nonnull(repl->shared->logger);

    // Create terminal (required for rendering)
    repl->shared->term = talloc_zero(repl->shared, ik_term_ctx_t);
    ck_assert_ptr_nonnull(repl->shared->term);
    repl->shared->term->screen_rows = 24;
    repl->shared->term->screen_cols = 80;
    repl->shared->term->tty_fd = -1;

    // Note: We don't set up full rendering infrastructure (render context, layers, etc.)
    // because the test only needs to verify HTTP processing for all agents.
    // Rendering will only be triggered if the current agent completes,
    // which is tested separately.

    // Initialize agent array
    repl->agent_count = 0;
    repl->agent_capacity = 4;
    repl->agents = talloc_array(repl, ik_agent_ctx_t *, (unsigned int)repl->agent_capacity);

    // Create Agent A
    agent_a = create_test_agent(repl, "agent-a-uuid");
    repl->agents[0] = agent_a;
    repl->agent_count++;

    // Create Agent B
    agent_b = create_test_agent(repl, "agent-b-uuid");
    repl->agents[1] = agent_b;
    repl->agent_count++;

    // Set current to Agent B (so Agent A is background)
    repl->current = agent_b;
}

// Per-test teardown
static void teardown(void)
{
    if (test_ctx != NULL) {
        talloc_free(test_ctx);
        test_ctx = NULL;
    }
}

// Test: Agent A completes HTTP request while Agent B is current
START_TEST(test_background_agent_http_completion) {
    // Setup: Agent A has HTTP request in progress
    agent_a->state = IK_AGENT_STATE_WAITING_FOR_LLM;
    agent_a->curl_still_running = 1;  // Will be set to 0 by curl_multi_perform
    agent_a->assistant_response = talloc_strdup(agent_a, "Background response");
    agent_a->response_finish_reason = talloc_strdup(agent_a, "stop");

    // Agent B is current (no HTTP in progress)
    agent_b->state = IK_AGENT_STATE_IDLE;
    agent_b->curl_still_running = 0;

    // Call ik_repl_handle_curl_events - should process Agent A even though Agent B is current
    // curl_multi_perform will complete immediately (no actual handles) and set curl_still_running to 0
    res_t result = ik_repl_handle_curl_events(repl, 0);
    ck_assert(is_ok(&result));

    // Verify Agent A was processed
    // - assistant_response should be added to conversation and cleared
    ck_assert_uint_eq(agent_a->conversation->message_count, 1);
    ck_assert_ptr_null(agent_a->assistant_response);

    // - state should transition to IDLE
    pthread_mutex_lock(&agent_a->tool_thread_mutex);
    ik_agent_state_t state_a = agent_a->state;
    pthread_mutex_unlock(&agent_a->tool_thread_mutex);
    ck_assert_int_eq(state_a, IK_AGENT_STATE_IDLE);

    // Verify Agent B was not affected
    ck_assert_uint_eq(agent_b->conversation->message_count, 0);
    pthread_mutex_lock(&agent_b->tool_thread_mutex);
    ik_agent_state_t state_b = agent_b->state;
    pthread_mutex_unlock(&agent_b->tool_thread_mutex);
    ck_assert_int_eq(state_b, IK_AGENT_STATE_IDLE);
}
END_TEST
// Test: Multiple background agents complete HTTP
START_TEST(test_multiple_background_agents_completion)
{
    // Create a third agent (C) and add it
    ik_agent_ctx_t *agent_c = create_test_agent(repl, "agent-c-uuid");
    repl->agents[2] = agent_c;
    repl->agent_count++;

    // Setup: Agent A and C have HTTP requests in progress (both background)
    agent_a->state = IK_AGENT_STATE_WAITING_FOR_LLM;
    agent_a->curl_still_running = 1;  // Will be set to 0 by curl_multi_perform
    agent_a->assistant_response = talloc_strdup(agent_a, "Agent A response");
    agent_a->response_finish_reason = talloc_strdup(agent_a, "stop");

    agent_c->state = IK_AGENT_STATE_WAITING_FOR_LLM;
    agent_c->curl_still_running = 1;  // Will be set to 0 by curl_multi_perform
    agent_c->assistant_response = talloc_strdup(agent_c, "Agent C response");
    agent_c->response_finish_reason = talloc_strdup(agent_c, "stop");

    // Agent B is current (no HTTP in progress)
    agent_b->state = IK_AGENT_STATE_IDLE;
    agent_b->curl_still_running = 0;

    // Call ik_repl_handle_curl_events - should process both Agent A and C
    // curl_multi_perform will complete immediately (no actual handles) and set both to 0
    res_t result = ik_repl_handle_curl_events(repl, 0);
    ck_assert(is_ok(&result));

    // Verify Agent A was processed
    ck_assert_uint_eq(agent_a->conversation->message_count, 1);
    ck_assert_ptr_null(agent_a->assistant_response);

    pthread_mutex_lock(&agent_a->tool_thread_mutex);
    ik_agent_state_t state_a = agent_a->state;
    pthread_mutex_unlock(&agent_a->tool_thread_mutex);
    ck_assert_int_eq(state_a, IK_AGENT_STATE_IDLE);

    // Verify Agent C was processed
    ck_assert_uint_eq(agent_c->conversation->message_count, 1);
    ck_assert_ptr_null(agent_c->assistant_response);

    pthread_mutex_lock(&agent_c->tool_thread_mutex);
    ik_agent_state_t state_c = agent_c->state;
    pthread_mutex_unlock(&agent_c->tool_thread_mutex);
    ck_assert_int_eq(state_c, IK_AGENT_STATE_IDLE);

    // Verify Agent B was not affected
    ck_assert_uint_eq(agent_b->conversation->message_count, 0);
    pthread_mutex_lock(&agent_b->tool_thread_mutex);
    ik_agent_state_t state_b = agent_b->state;
    pthread_mutex_unlock(&agent_b->tool_thread_mutex);
    ck_assert_int_eq(state_b, IK_AGENT_STATE_IDLE);
}

END_TEST
// Test: HTTP error on background agent
START_TEST(test_background_agent_http_error)
{
    // Setup: Agent A has HTTP request that fails
    agent_a->state = IK_AGENT_STATE_WAITING_FOR_LLM;
    agent_a->curl_still_running = 1;  // Will be set to 0 by curl_multi_perform
    agent_a->http_error_message = talloc_strdup(agent_a, "Connection failed");
    agent_a->assistant_response = talloc_strdup(agent_a, "Partial response");

    // Agent B is current
    agent_b->state = IK_AGENT_STATE_IDLE;
    agent_b->curl_still_running = 0;

    // Call ik_repl_handle_curl_events
    // curl_multi_perform will complete immediately (no actual handles) and set curl_still_running to 0
    res_t result = ik_repl_handle_curl_events(repl, 0);
    ck_assert(is_ok(&result));

    // Verify Agent A's error was handled
    // - error should be in scrollback
    ck_assert_uint_gt(ik_scrollback_get_line_count(agent_a->scrollback), 0);

    // - error message should be cleared
    ck_assert_ptr_null(agent_a->http_error_message);

    // - partial response should be cleared
    ck_assert_ptr_null(agent_a->assistant_response);

    // - state should transition to IDLE
    pthread_mutex_lock(&agent_a->tool_thread_mutex);
    ik_agent_state_t state_a = agent_a->state;
    pthread_mutex_unlock(&agent_a->tool_thread_mutex);
    ck_assert_int_eq(state_a, IK_AGENT_STATE_IDLE);

    // Verify Agent B was not affected
    ck_assert_uint_eq(agent_b->conversation->message_count, 0);
}

END_TEST

static Suite *all_agents_http_handling_suite(void)
{
    Suite *s = suite_create("All Agents HTTP Handling");

    TCase *tc_core = tcase_create("Core");
    tcase_add_unchecked_fixture(tc_core, suite_setup, NULL);
    tcase_add_checked_fixture(tc_core, setup, teardown);
    tcase_add_test(tc_core, test_background_agent_http_completion);
    tcase_add_test(tc_core, test_multiple_background_agents_completion);
    tcase_add_test(tc_core, test_background_agent_http_error);
    suite_add_tcase(s, tc_core);

    return s;
}

int main(void)
{
    Suite *s = all_agents_http_handling_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
