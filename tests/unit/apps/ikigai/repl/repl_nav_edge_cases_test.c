/**
 * @file repl_nav_edge_cases_test.c
 * @brief Unit tests for REPL navigation edge cases
 */

#include <check.h>
#include <talloc.h>
#include <string.h>
#include "apps/ikigai/repl.h"
#include "apps/ikigai/agent.h"
#include "tests/helpers/test_utils_helper.h"

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

/* Test: Navigation edge case - no siblings */
START_TEST(test_nav_prev_sibling_no_siblings) {
    // Create minimal repl context
    ik_repl_ctx_t *repl = talloc_zero(test_ctx, ik_repl_ctx_t);
    repl->agents = NULL;
    repl->agent_count = 0;
    repl->agent_capacity = 0;

    // Create single agent (no siblings)
    ik_agent_ctx_t *agent1 = create_test_agent(repl, "agent-uuid-1111");
    agent1->parent_uuid = NULL;

    res_t result = ik_repl_add_agent(repl, agent1);
    ck_assert(is_ok(&result));
    repl->current = agent1;

    // Navigate to previous sibling (should be no-op)
    result = ik_repl_nav_prev_sibling(repl);
    ck_assert(is_ok(&result));

    // Current should remain unchanged
    ck_assert_ptr_eq(repl->current, agent1);
}
END_TEST
/* Test: Navigation edge case - parent not found */
START_TEST(test_nav_parent_not_found) {
    // Create minimal repl context
    ik_repl_ctx_t *repl = talloc_zero(test_ctx, ik_repl_ctx_t);
    repl->agents = NULL;
    repl->agent_count = 0;
    repl->agent_capacity = 0;

    // Create child agent with parent UUID that doesn't exist
    ik_agent_ctx_t *child = create_test_agent(repl, "child-uuid-2222");
    child->parent_uuid = talloc_strdup(child, "nonexistent-parent");

    res_t result = ik_repl_add_agent(repl, child);
    ck_assert(is_ok(&result));
    repl->current = child;

    // Navigate to parent (should be no-op since parent not found)
    result = ik_repl_nav_parent(repl);
    ck_assert(is_ok(&result));

    // Current should remain unchanged
    ck_assert_ptr_eq(repl->current, child);
}

END_TEST
/* Test: Navigation edge case - no children */
START_TEST(test_nav_child_no_children) {
    // Create minimal repl context
    ik_repl_ctx_t *repl = talloc_zero(test_ctx, ik_repl_ctx_t);
    repl->agents = NULL;
    repl->agent_count = 0;
    repl->agent_capacity = 0;

    // Create parent agent with no children
    ik_agent_ctx_t *parent = create_test_agent(repl, "parent-uuid-1111");
    parent->parent_uuid = NULL;

    res_t result = ik_repl_add_agent(repl, parent);
    ck_assert(is_ok(&result));
    repl->current = parent;

    // Navigate to child (should be no-op since no children)
    result = ik_repl_nav_child(repl);
    ck_assert(is_ok(&result));

    // Current should remain unchanged
    ck_assert_ptr_eq(repl->current, parent);
}

END_TEST
/* Test: Navigation with siblings - wraps around */
START_TEST(test_nav_sibling_wrap_around) {
    // Create minimal repl context
    ik_repl_ctx_t *repl = talloc_zero(test_ctx, ik_repl_ctx_t);
    repl->agents = NULL;
    repl->agent_count = 0;
    repl->agent_capacity = 0;

    // Create three sibling agents
    ik_agent_ctx_t *agent1 = create_test_agent(repl, "agent-uuid-1111");
    agent1->parent_uuid = NULL;

    ik_agent_ctx_t *agent2 = create_test_agent(repl, "agent-uuid-2222");
    agent2->parent_uuid = NULL;

    ik_agent_ctx_t *agent3 = create_test_agent(repl, "agent-uuid-3333");
    agent3->parent_uuid = NULL;

    res_t result = ik_repl_add_agent(repl, agent1);
    ck_assert(is_ok(&result));
    result = ik_repl_add_agent(repl, agent2);
    ck_assert(is_ok(&result));
    result = ik_repl_add_agent(repl, agent3);
    ck_assert(is_ok(&result));

    // Set current to first agent
    repl->current = agent1;

    // Navigate to previous sibling (should wrap to last)
    result = ik_repl_nav_prev_sibling(repl);
    ck_assert(is_ok(&result));
    ck_assert_ptr_eq(repl->current, agent3);

    // Navigate to next sibling from last (should wrap to first)
    result = ik_repl_nav_next_sibling(repl);
    ck_assert(is_ok(&result));
    ck_assert_ptr_eq(repl->current, agent1);
}

END_TEST
/* Test: Nav child with multiple children - selects newest */
START_TEST(test_nav_child_selects_newest) {
    // Create minimal repl context
    ik_repl_ctx_t *repl = talloc_zero(test_ctx, ik_repl_ctx_t);
    repl->agents = NULL;
    repl->agent_count = 0;
    repl->agent_capacity = 0;

    // Create parent agent
    ik_agent_ctx_t *parent = create_test_agent(repl, "parent-uuid-1111");
    parent->parent_uuid = NULL;

    // Create two children with different timestamps
    ik_agent_ctx_t *child1 = create_test_agent(repl, "child-uuid-2222");
    child1->parent_uuid = talloc_strdup(child1, "parent-uuid-1111");
    child1->created_at = 1000;

    ik_agent_ctx_t *child2 = create_test_agent(repl, "child-uuid-3333");
    child2->parent_uuid = talloc_strdup(child2, "parent-uuid-1111");
    child2->created_at = 2000;  // Newer

    res_t result = ik_repl_add_agent(repl, parent);
    ck_assert(is_ok(&result));
    result = ik_repl_add_agent(repl, child1);
    ck_assert(is_ok(&result));
    result = ik_repl_add_agent(repl, child2);
    ck_assert(is_ok(&result));

    // Set current to parent
    repl->current = parent;

    // Navigate to child - should select child2 (newest)
    result = ik_repl_nav_child(repl);
    ck_assert(is_ok(&result));
    ck_assert_ptr_eq(repl->current, child2);
}

END_TEST

// Create test suite
static Suite *repl_nav_edge_cases_suite(void)
{
    Suite *s = suite_create("REPL Navigation Edge Cases");

    TCase *tc_nav_edge = tcase_create("Navigation Edge Cases");
    tcase_set_timeout(tc_nav_edge, IK_TEST_TIMEOUT);
    tcase_set_timeout(tc_nav_edge, IK_TEST_TIMEOUT);
    tcase_set_timeout(tc_nav_edge, IK_TEST_TIMEOUT);
    tcase_set_timeout(tc_nav_edge, IK_TEST_TIMEOUT);
    tcase_set_timeout(tc_nav_edge, IK_TEST_TIMEOUT);
    tcase_add_checked_fixture(tc_nav_edge, setup, teardown);
    tcase_add_test(tc_nav_edge, test_nav_prev_sibling_no_siblings);
    tcase_add_test(tc_nav_edge, test_nav_parent_not_found);
    tcase_add_test(tc_nav_edge, test_nav_child_no_children);
    tcase_add_test(tc_nav_edge, test_nav_sibling_wrap_around);
    tcase_add_test(tc_nav_edge, test_nav_child_selects_newest);
    suite_add_tcase(s, tc_nav_edge);

    return s;
}

int main(void)
{
    int number_failed;
    Suite *s = repl_nav_edge_cases_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/apps/ikigai/repl/repl_nav_edge_cases_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
