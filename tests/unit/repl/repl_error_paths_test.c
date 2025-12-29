/**
 * @file repl_error_paths_test.c
 * @brief Unit tests for REPL error handling paths
 */

#include <check.h>
#include <talloc.h>
#include <string.h>
#include "../../../src/repl.h"
#include "../../../src/repl_actions.h"
#include "../../../src/agent.h"
#include "../../../src/shared.h"
#include "../../../src/input.h"
#include "../../test_utils.h"

// Test fixture data
static TALLOC_CTX *test_ctx = NULL;

// Setup function - runs before each test
static void setup(void)
{
    test_ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(test_ctx);
}

// Teardown function - runs after each test
static void teardown(void)
{
    talloc_free(test_ctx);
    test_ctx = NULL;
}

// Helper: Create minimal agent for testing
static ik_agent_ctx_t *create_test_agent(TALLOC_CTX *parent, const char *uuid)
{
    ik_agent_ctx_t *agent = talloc_zero(parent, ik_agent_ctx_t);
    ck_assert_ptr_nonnull(agent);

    agent->uuid = talloc_strdup(agent, uuid);
    ck_assert_ptr_nonnull(agent->uuid);

    return agent;
}

/* Test: ik_repl_remove_agent returns error when agent not found */
START_TEST(test_repl_remove_agent_not_found) {
    // Create minimal repl context
    ik_repl_ctx_t *repl = talloc_zero(test_ctx, ik_repl_ctx_t);
    ck_assert_ptr_nonnull(repl);

    // Initialize agent array
    repl->agents = NULL;
    repl->agent_count = 0;
    repl->agent_capacity = 0;

    // Add one agent
    ik_agent_ctx_t *agent1 = create_test_agent(repl, "agent-uuid-1234");
    res_t result = ik_repl_add_agent(repl, agent1);
    ck_assert(is_ok(&result));

    // Try to remove non-existent agent
    result = ik_repl_remove_agent(repl, "nonexistent-uuid");

    // Verify error is returned
    ck_assert(is_err(&result));
    ck_assert_ptr_nonnull(result.err);
    ck_assert_int_eq(result.err->code, ERR_AGENT_NOT_FOUND);

    // Verify agent array is unchanged
    ck_assert_uint_eq(repl->agent_count, 1);

    talloc_free(result.err);
}
END_TEST
/* Test: ik_repl_remove_agent sets current to NULL when removing current agent */
START_TEST(test_repl_remove_agent_current)
{
    // Create minimal repl context
    ik_repl_ctx_t *repl = talloc_zero(test_ctx, ik_repl_ctx_t);
    ck_assert_ptr_nonnull(repl);

    // Initialize agent array
    repl->agents = NULL;
    repl->agent_count = 0;
    repl->agent_capacity = 0;

    // Add agents
    ik_agent_ctx_t *agent1 = create_test_agent(repl, "agent-uuid-1111");
    ik_agent_ctx_t *agent2 = create_test_agent(repl, "agent-uuid-2222");

    res_t result = ik_repl_add_agent(repl, agent1);
    ck_assert(is_ok(&result));
    result = ik_repl_add_agent(repl, agent2);
    ck_assert(is_ok(&result));

    // Set current to agent1
    repl->current = agent1;

    // Remove current agent
    result = ik_repl_remove_agent(repl, "agent-uuid-1111");
    ck_assert(is_ok(&result));

    // Verify current is set to NULL
    ck_assert_ptr_null(repl->current);

    // Verify array has only agent2
    ck_assert_uint_eq(repl->agent_count, 1);
    ck_assert_ptr_eq(repl->agents[0], agent2);
}

END_TEST
/* Test: Process NAV_PREV_SIBLING action */
START_TEST(test_repl_process_action_nav_prev_sibling)
{
    // Create minimal repl context
    ik_repl_ctx_t *repl = talloc_zero(test_ctx, ik_repl_ctx_t);
    ck_assert_ptr_nonnull(repl);

    // Initialize agent array
    repl->agents = NULL;
    repl->agent_count = 0;
    repl->agent_capacity = 0;

    // Create shared context (needed for repl_actions)
    ik_shared_ctx_t *shared = talloc_zero(test_ctx, ik_shared_ctx_t);
    repl->shared = shared;

    // Create agents (siblings with same parent)
    ik_agent_ctx_t *agent1 = create_test_agent(repl, "agent-uuid-1111");
    agent1->parent_uuid = NULL;  // Root level

    ik_agent_ctx_t *agent2 = create_test_agent(repl, "agent-uuid-2222");
    agent2->parent_uuid = NULL;  // Root level

    res_t result = ik_repl_add_agent(repl, agent1);
    ck_assert(is_ok(&result));
    result = ik_repl_add_agent(repl, agent2);
    ck_assert(is_ok(&result));

    // Set current to agent2
    repl->current = agent2;

    // Process NAV_PREV_SIBLING action
    ik_input_action_t action = {.type = IK_INPUT_NAV_PREV_SIBLING};
    result = ik_repl_process_action(repl, &action);
    ck_assert(is_ok(&result));

    // Verify current switched to agent1
    ck_assert_ptr_eq(repl->current, agent1);
}

END_TEST
/* Test: Process NAV_NEXT_SIBLING action */
START_TEST(test_repl_process_action_nav_next_sibling)
{
    // Create minimal repl context
    ik_repl_ctx_t *repl = talloc_zero(test_ctx, ik_repl_ctx_t);
    ck_assert_ptr_nonnull(repl);

    // Initialize agent array
    repl->agents = NULL;
    repl->agent_count = 0;
    repl->agent_capacity = 0;

    // Create shared context (needed for repl_actions)
    ik_shared_ctx_t *shared = talloc_zero(test_ctx, ik_shared_ctx_t);
    repl->shared = shared;

    // Create agents (siblings with same parent)
    ik_agent_ctx_t *agent1 = create_test_agent(repl, "agent-uuid-1111");
    agent1->parent_uuid = NULL;  // Root level

    ik_agent_ctx_t *agent2 = create_test_agent(repl, "agent-uuid-2222");
    agent2->parent_uuid = NULL;  // Root level

    res_t result = ik_repl_add_agent(repl, agent1);
    ck_assert(is_ok(&result));
    result = ik_repl_add_agent(repl, agent2);
    ck_assert(is_ok(&result));

    // Set current to agent1
    repl->current = agent1;

    // Process NAV_NEXT_SIBLING action
    ik_input_action_t action = {.type = IK_INPUT_NAV_NEXT_SIBLING};
    result = ik_repl_process_action(repl, &action);
    ck_assert(is_ok(&result));

    // Verify current switched to agent2
    ck_assert_ptr_eq(repl->current, agent2);
}

END_TEST
/* Test: Process NAV_PARENT action */
START_TEST(test_repl_process_action_nav_parent)
{
    // Create minimal repl context
    ik_repl_ctx_t *repl = talloc_zero(test_ctx, ik_repl_ctx_t);
    ck_assert_ptr_nonnull(repl);

    // Initialize agent array
    repl->agents = NULL;
    repl->agent_count = 0;
    repl->agent_capacity = 0;

    // Create shared context (needed for repl_actions)
    ik_shared_ctx_t *shared = talloc_zero(test_ctx, ik_shared_ctx_t);
    repl->shared = shared;

    // Create parent agent
    ik_agent_ctx_t *parent = create_test_agent(repl, "parent-uuid-1111");
    parent->parent_uuid = NULL;

    // Create child agent
    ik_agent_ctx_t *child = create_test_agent(repl, "child-uuid-2222");
    child->parent_uuid = talloc_strdup(child, "parent-uuid-1111");

    res_t result = ik_repl_add_agent(repl, parent);
    ck_assert(is_ok(&result));
    result = ik_repl_add_agent(repl, child);
    ck_assert(is_ok(&result));

    // Set current to child
    repl->current = child;

    // Process NAV_PARENT action
    ik_input_action_t action = {.type = IK_INPUT_NAV_PARENT};
    result = ik_repl_process_action(repl, &action);
    ck_assert(is_ok(&result));

    // Verify current switched to parent
    ck_assert_ptr_eq(repl->current, parent);
}

END_TEST
/* Test: Process NAV_CHILD action */
START_TEST(test_repl_process_action_nav_child)
{
    // Create minimal repl context
    ik_repl_ctx_t *repl = talloc_zero(test_ctx, ik_repl_ctx_t);
    ck_assert_ptr_nonnull(repl);

    // Initialize agent array
    repl->agents = NULL;
    repl->agent_count = 0;
    repl->agent_capacity = 0;

    // Create shared context (needed for repl_actions)
    ik_shared_ctx_t *shared = talloc_zero(test_ctx, ik_shared_ctx_t);
    repl->shared = shared;

    // Create parent agent
    ik_agent_ctx_t *parent = create_test_agent(repl, "parent-uuid-1111");
    parent->parent_uuid = NULL;

    // Create child agent
    ik_agent_ctx_t *child = create_test_agent(repl, "child-uuid-2222");
    child->parent_uuid = talloc_strdup(child, "parent-uuid-1111");
    child->created_at = 1000;

    res_t result = ik_repl_add_agent(repl, parent);
    ck_assert(is_ok(&result));
    result = ik_repl_add_agent(repl, child);
    ck_assert(is_ok(&result));

    // Set current to parent
    repl->current = parent;

    // Process NAV_CHILD action
    ik_input_action_t action = {.type = IK_INPUT_NAV_CHILD};
    result = ik_repl_process_action(repl, &action);
    ck_assert(is_ok(&result));

    // Verify current switched to child
    ck_assert_ptr_eq(repl->current, child);
}

END_TEST
/* Test: ik_repl_add_agent grows capacity when array is full */
START_TEST(test_repl_add_agent_grows_capacity)
{
    // Create minimal repl context
    ik_repl_ctx_t *repl = talloc_zero(test_ctx, ik_repl_ctx_t);
    ck_assert_ptr_nonnull(repl);

    // Initialize agent array with zero capacity
    repl->agents = NULL;
    repl->agent_count = 0;
    repl->agent_capacity = 0;

    // Add first agent - should grow capacity from 0 to 4
    ik_agent_ctx_t *agent1 = create_test_agent(repl, "agent-uuid-0001");
    res_t result = ik_repl_add_agent(repl, agent1);
    ck_assert(is_ok(&result));
    ck_assert_uint_eq(repl->agent_count, 1);
    ck_assert_uint_eq(repl->agent_capacity, 4);

    // Add agents 2, 3, 4 - should fill capacity
    ik_agent_ctx_t *agent2 = create_test_agent(repl, "agent-uuid-0002");
    result = ik_repl_add_agent(repl, agent2);
    ck_assert(is_ok(&result));

    ik_agent_ctx_t *agent3 = create_test_agent(repl, "agent-uuid-0003");
    result = ik_repl_add_agent(repl, agent3);
    ck_assert(is_ok(&result));

    ik_agent_ctx_t *agent4 = create_test_agent(repl, "agent-uuid-0004");
    result = ik_repl_add_agent(repl, agent4);
    ck_assert(is_ok(&result));

    ck_assert_uint_eq(repl->agent_count, 4);
    ck_assert_uint_eq(repl->agent_capacity, 4);

    // Add fifth agent - should grow capacity from 4 to 8
    ik_agent_ctx_t *agent5 = create_test_agent(repl, "agent-uuid-0005");
    result = ik_repl_add_agent(repl, agent5);
    ck_assert(is_ok(&result));

    ck_assert_uint_eq(repl->agent_count, 5);
    ck_assert_uint_eq(repl->agent_capacity, 8);
}

END_TEST

// Create test suite
static Suite *repl_error_paths_suite(void)
{
    Suite *s = suite_create("REPL Error Paths");

    TCase *tc_remove = tcase_create("Remove Agent");
    tcase_set_timeout(tc_remove, 30);
    tcase_set_timeout(tc_remove, 30);
    tcase_set_timeout(tc_remove, 30);
    tcase_set_timeout(tc_remove, 30);
    tcase_set_timeout(tc_remove, 30);
    tcase_add_checked_fixture(tc_remove, setup, teardown);
    tcase_add_test(tc_remove, test_repl_remove_agent_not_found);
    tcase_add_test(tc_remove, test_repl_remove_agent_current);
    suite_add_tcase(s, tc_remove);

    TCase *tc_nav = tcase_create("Navigation Actions");
    tcase_set_timeout(tc_nav, 30);
    tcase_set_timeout(tc_nav, 30);
    tcase_set_timeout(tc_nav, 30);
    tcase_set_timeout(tc_nav, 30);
    tcase_set_timeout(tc_nav, 30);
    tcase_add_checked_fixture(tc_nav, setup, teardown);
    tcase_add_test(tc_nav, test_repl_process_action_nav_prev_sibling);
    tcase_add_test(tc_nav, test_repl_process_action_nav_next_sibling);
    tcase_add_test(tc_nav, test_repl_process_action_nav_parent);
    tcase_add_test(tc_nav, test_repl_process_action_nav_child);
    suite_add_tcase(s, tc_nav);

    TCase *tc_add = tcase_create("Add Agent");
    tcase_set_timeout(tc_add, 30);
    tcase_set_timeout(tc_add, 30);
    tcase_set_timeout(tc_add, 30);
    tcase_set_timeout(tc_add, 30);
    tcase_set_timeout(tc_add, 30);
    tcase_add_checked_fixture(tc_add, setup, teardown);
    tcase_add_test(tc_add, test_repl_add_agent_grows_capacity);
    suite_add_tcase(s, tc_add);

    return s;
}

int main(void)
{
    int number_failed;
    Suite *s = repl_error_paths_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
