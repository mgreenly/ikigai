/**
 * @file agent_transitions_test.c
 * @brief Unit tests for agent state transitions (refactor from repl to agent)
 */

#include <check.h>
#include "../../../src/agent.h"
#include "../../../src/shared.h"
#include "../../../src/repl.h"
#include "../../../src/wrapper_pthread.h"
#include <talloc.h>
#include <string.h>
#include "../../test_utils.h"

// Forward declaration for wrapper function
ssize_t posix_write_(int fd, const void *buf, size_t count);

// Mock write wrapper
ssize_t posix_write_(int fd, const void *buf, size_t count)
{
    (void)fd;
    (void)buf;
    return (ssize_t)count;
}

// Helper function to create a minimal agent for testing
static ik_agent_ctx_t *create_test_agent(void *ctx)
{
    ik_agent_ctx_t *agent = talloc_zero(ctx, ik_agent_ctx_t);
    ck_assert_ptr_nonnull(agent);

    // Initialize state to IDLE (default)
    agent->state = IK_AGENT_STATE_IDLE;

    // Initialize spinner state
    agent->spinner_state.frame_index = 0;
    agent->spinner_state.visible = false;

    // Initialize input buffer visibility
    agent->input_buffer_visible = true;

    // Initialize mutex for thread safety
    pthread_mutex_init_(&agent->tool_thread_mutex, NULL);

    return agent;
}

/* Test: Transition from IDLE to WAITING_FOR_LLM */
START_TEST(test_agent_transition_to_waiting_for_llm) {
    void *ctx = talloc_new(NULL);
    ik_agent_ctx_t *agent = create_test_agent(ctx);

    // Verify initial state is IDLE
    ck_assert_int_eq(agent->state, IK_AGENT_STATE_IDLE);
    ck_assert(!agent->spinner_state.visible);
    ck_assert(agent->input_buffer_visible);

    // Call transition function
    ik_agent_transition_to_waiting_for_llm(agent);

    // Verify state changed
    ck_assert_int_eq(agent->state, IK_AGENT_STATE_WAITING_FOR_LLM);
    ck_assert(agent->spinner_state.visible);
    ck_assert(!agent->input_buffer_visible);

    talloc_free(ctx);
}
END_TEST
/* Test: Transition from WAITING_FOR_LLM to IDLE */
START_TEST(test_agent_transition_to_idle)
{
    void *ctx = talloc_new(NULL);
    ik_agent_ctx_t *agent = create_test_agent(ctx);

    // Start in WAITING_FOR_LLM state
    agent->state = IK_AGENT_STATE_WAITING_FOR_LLM;
    agent->spinner_state.visible = true;
    agent->input_buffer_visible = false;

    ck_assert_int_eq(agent->state, IK_AGENT_STATE_WAITING_FOR_LLM);
    ck_assert(agent->spinner_state.visible);
    ck_assert(!agent->input_buffer_visible);

    // Call transition function
    ik_agent_transition_to_idle(agent);

    // Verify state changed
    ck_assert_int_eq(agent->state, IK_AGENT_STATE_IDLE);
    ck_assert(!agent->spinner_state.visible);
    ck_assert(agent->input_buffer_visible);

    talloc_free(ctx);
}

END_TEST
/* Test: Transition from WAITING_FOR_LLM to EXECUTING_TOOL */
START_TEST(test_agent_transition_to_executing_tool)
{
    void *ctx = talloc_new(NULL);
    ik_agent_ctx_t *agent = create_test_agent(ctx);

    // Start in WAITING_FOR_LLM state
    agent->state = IK_AGENT_STATE_WAITING_FOR_LLM;
    agent->spinner_state.visible = true;
    agent->input_buffer_visible = false;

    ck_assert_int_eq(agent->state, IK_AGENT_STATE_WAITING_FOR_LLM);

    // Call transition function
    ik_agent_transition_to_executing_tool(agent);

    // Verify state changed to EXECUTING_TOOL
    ck_assert_int_eq(agent->state, IK_AGENT_STATE_EXECUTING_TOOL);
    // Spinner stays visible, input stays hidden during tool execution
    ck_assert(agent->spinner_state.visible);
    ck_assert(!agent->input_buffer_visible);

    talloc_free(ctx);
}

END_TEST
/* Test: Transition from EXECUTING_TOOL back to WAITING_FOR_LLM */
START_TEST(test_agent_transition_from_executing_tool)
{
    void *ctx = talloc_new(NULL);
    ik_agent_ctx_t *agent = create_test_agent(ctx);

    // Start in EXECUTING_TOOL state
    agent->state = IK_AGENT_STATE_EXECUTING_TOOL;
    agent->spinner_state.visible = true;
    agent->input_buffer_visible = false;

    ck_assert_int_eq(agent->state, IK_AGENT_STATE_EXECUTING_TOOL);

    // Call transition function
    ik_agent_transition_from_executing_tool(agent);

    // Verify state changed back to WAITING_FOR_LLM
    ck_assert_int_eq(agent->state, IK_AGENT_STATE_WAITING_FOR_LLM);
    // Spinner stays visible, input stays hidden
    ck_assert(agent->spinner_state.visible);
    ck_assert(!agent->input_buffer_visible);

    talloc_free(ctx);
}

END_TEST
/* Test: Full tool execution cycle */
START_TEST(test_agent_full_tool_cycle)
{
    void *ctx = talloc_new(NULL);
    ik_agent_ctx_t *agent = create_test_agent(ctx);

    // Start in IDLE
    ck_assert_int_eq(agent->state, IK_AGENT_STATE_IDLE);

    // Transition to WAITING_FOR_LLM (user submitted request)
    ik_agent_transition_to_waiting_for_llm(agent);
    ck_assert_int_eq(agent->state, IK_AGENT_STATE_WAITING_FOR_LLM);
    ck_assert(agent->spinner_state.visible);
    ck_assert(!agent->input_buffer_visible);

    // Transition to EXECUTING_TOOL (LLM responded with tool call)
    ik_agent_transition_to_executing_tool(agent);
    ck_assert_int_eq(agent->state, IK_AGENT_STATE_EXECUTING_TOOL);
    ck_assert(agent->spinner_state.visible);
    ck_assert(!agent->input_buffer_visible);

    // Transition back to WAITING_FOR_LLM (tool completed, sending result to LLM)
    ik_agent_transition_from_executing_tool(agent);
    ck_assert_int_eq(agent->state, IK_AGENT_STATE_WAITING_FOR_LLM);
    ck_assert(agent->spinner_state.visible);
    ck_assert(!agent->input_buffer_visible);

    // Transition back to IDLE (LLM responded with final answer)
    ik_agent_transition_to_idle(agent);
    ck_assert_int_eq(agent->state, IK_AGENT_STATE_IDLE);
    ck_assert(!agent->spinner_state.visible);
    ck_assert(agent->input_buffer_visible);

    talloc_free(ctx);
}

END_TEST

static Suite *agent_transitions_suite(void)
{
    Suite *s = suite_create("Agent State Transitions");

    TCase *tc_transitions = tcase_create("State Transitions");
    tcase_set_timeout(tc_transitions, 30);
    tcase_set_timeout(tc_transitions, 30);
    tcase_set_timeout(tc_transitions, 30);
    tcase_set_timeout(tc_transitions, 30);
    tcase_set_timeout(tc_transitions, 30);
    tcase_add_test(tc_transitions, test_agent_transition_to_waiting_for_llm);
    tcase_add_test(tc_transitions, test_agent_transition_to_idle);
    tcase_add_test(tc_transitions, test_agent_transition_to_executing_tool);
    tcase_add_test(tc_transitions, test_agent_transition_from_executing_tool);
    tcase_add_test(tc_transitions, test_agent_full_tool_cycle);
    suite_add_tcase(s, tc_transitions);

    return s;
}

int main(void)
{
    int number_failed;
    Suite *s = agent_transitions_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    ik_test_reset_terminal();

    return (number_failed == 0) ? 0 : 1;
}
