/**
 * @file nav_parent_child_test.c
 * @brief Unit tests for parent/child navigation (Ctrl+Up/Down)
 */

#include <check.h>
#include <talloc.h>
#include <string.h>
#include "../../../src/repl.h"
#include "../../../src/agent.h"
#include "../../../src/shared.h"
#include "../../../src/input_buffer/core.h"
#include "../../test_utils.h"
#include "../../helpers/test_contexts.h"

// Test fixture data
static ik_repl_ctx_t *repl = NULL;
static TALLOC_CTX *test_ctx = NULL;

// Setup function - runs before each test
static void setup(void)
{
    test_ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(test_ctx);

    // Create minimal repl context for testing
    repl = talloc_zero(test_ctx, ik_repl_ctx_t);
    ck_assert_ptr_nonnull(repl);

    // Initialize agent array
    repl->agents = NULL;
    repl->agent_count = 0;
    repl->agent_capacity = 0;
}

// Teardown function - runs after each test
static void teardown(void)
{
    talloc_free(test_ctx);
    test_ctx = NULL;
    repl = NULL;
}

// Helper: Create minimal agent with input buffer
static ik_agent_ctx_t *create_test_agent(const char *uuid, const char *parent_uuid, int64_t created_at)
{
    ik_agent_ctx_t *agent = talloc_zero(repl, ik_agent_ctx_t);
    ck_assert_ptr_nonnull(agent);

    agent->uuid = talloc_strdup(agent, uuid);
    ck_assert_ptr_nonnull(agent->uuid);

    if (parent_uuid != NULL) {
        agent->parent_uuid = talloc_strdup(agent, parent_uuid);
        ck_assert_ptr_nonnull(agent->parent_uuid);
    } else {
        agent->parent_uuid = NULL;
    }

    agent->created_at = created_at;

    // Create input buffer
    agent->input_buffer = ik_input_buffer_create(agent);
    ck_assert_ptr_nonnull(agent->input_buffer);

    // Initialize viewport state
    agent->viewport_offset = 0;

    return agent;
}

// Helper: Add agent to repl->agents array
static void add_agent_to_array(ik_agent_ctx_t *agent)
{
    if (repl->agent_count >= repl->agent_capacity) {
        size_t new_capacity = repl->agent_capacity == 0 ? (size_t)8 : repl->agent_capacity * (size_t)2;
        ik_agent_ctx_t **new_agents = talloc_realloc(repl, repl->agents, ik_agent_ctx_t *, (unsigned int)new_capacity);
        ck_assert_ptr_nonnull(new_agents);
        repl->agents = new_agents;
        repl->agent_capacity = new_capacity;
    }
    repl->agents[repl->agent_count++] = agent;
}

// Test: nav_parent switches to parent
START_TEST(test_nav_parent_switches_to_parent) {
    // Create parent and child
    ik_agent_ctx_t *parent = create_test_agent("parent-uuid", NULL, 100);
    ik_agent_ctx_t *child = create_test_agent("child-uuid", "parent-uuid", 200);

    add_agent_to_array(parent);
    add_agent_to_array(child);

    repl->current = child;

    res_t result = ik_repl_nav_parent(repl);
    ck_assert(is_ok(&result));
    ck_assert_ptr_eq(repl->current, parent);
}
END_TEST
// Test: nav_parent at root = no action
START_TEST(test_nav_parent_at_root_no_action) {
    // Create root agent
    ik_agent_ctx_t *root = create_test_agent("root-uuid", NULL, 100);

    add_agent_to_array(root);
    repl->current = root;

    res_t result = ik_repl_nav_parent(repl);
    ck_assert(is_ok(&result));
    ck_assert_ptr_eq(repl->current, root);  // No change
}

END_TEST
// Test: nav_child switches to child
START_TEST(test_nav_child_switches_to_child) {
    // Create parent and child
    ik_agent_ctx_t *parent = create_test_agent("parent-uuid", NULL, 100);
    ik_agent_ctx_t *child = create_test_agent("child-uuid", "parent-uuid", 200);

    add_agent_to_array(parent);
    add_agent_to_array(child);

    repl->current = parent;

    res_t result = ik_repl_nav_child(repl);
    ck_assert(is_ok(&result));
    ck_assert_ptr_eq(repl->current, child);
}

END_TEST
// Test: nav_child selects most recent running child
START_TEST(test_nav_child_selects_most_recent_child) {
    // Create parent and 3 children
    ik_agent_ctx_t *parent = create_test_agent("parent-uuid", NULL, 100);
    ik_agent_ctx_t *child1 = create_test_agent("child1-uuid", "parent-uuid", 200);
    ik_agent_ctx_t *child2 = create_test_agent("child2-uuid", "parent-uuid", 300);
    ik_agent_ctx_t *child3 = create_test_agent("child3-uuid", "parent-uuid", 400);

    add_agent_to_array(parent);
    add_agent_to_array(child1);
    add_agent_to_array(child2);
    add_agent_to_array(child3);

    repl->current = parent;

    res_t result = ik_repl_nav_child(repl);
    ck_assert(is_ok(&result));
    ck_assert_ptr_eq(repl->current, child3);  // Most recent
}

END_TEST
// Test: nav_child with no children = no action
START_TEST(test_nav_child_no_children_no_action) {
    // Create parent with no children
    ik_agent_ctx_t *parent = create_test_agent("parent-uuid", NULL, 100);

    add_agent_to_array(parent);
    repl->current = parent;

    res_t result = ik_repl_nav_child(repl);
    ck_assert(is_ok(&result));
    ck_assert_ptr_eq(repl->current, parent);  // No change
}

END_TEST
// Test: nav_child skips dead children (not in agents array)
START_TEST(test_nav_child_skips_dead_children) {
    // Create parent and 2 children
    // Only child1 is in agents[] (running), child2 is dead (not in array)
    ik_agent_ctx_t *parent = create_test_agent("parent-uuid", NULL, 100);
    ik_agent_ctx_t *child1 = create_test_agent("child1-uuid", "parent-uuid", 200);

    add_agent_to_array(parent);
    add_agent_to_array(child1);
    // child2 is not added - simulating it was killed

    repl->current = parent;

    res_t result = ik_repl_nav_child(repl);
    ck_assert(is_ok(&result));
    ck_assert_ptr_eq(repl->current, child1);  // Only running child
}

END_TEST
// Test: nav_parent with dead parent = no action
START_TEST(test_nav_parent_with_dead_parent_no_action) {
    // Create child with parent_uuid pointing to a dead parent (not in agents[])
    ik_agent_ctx_t *child = create_test_agent("child-uuid", "dead-parent-uuid", 200);

    add_agent_to_array(child);
    repl->current = child;

    res_t result = ik_repl_nav_parent(repl);
    ck_assert(is_ok(&result));
    ck_assert_ptr_eq(repl->current, child);  // No change - parent not found
}

END_TEST
// Test: nav_child with multiple children of different parents
START_TEST(test_nav_child_with_mixed_children) {
    // Create parent1, parent2, and their children
    ik_agent_ctx_t *parent1 = create_test_agent("parent1-uuid", NULL, 100);
    ik_agent_ctx_t *parent2 = create_test_agent("parent2-uuid", NULL, 150);
    ik_agent_ctx_t *child1_of_p1 = create_test_agent("child1-p1-uuid", "parent1-uuid", 200);
    ik_agent_ctx_t *child2_of_p2 = create_test_agent("child2-p2-uuid", "parent2-uuid", 300);
    ik_agent_ctx_t *child3_of_p1 = create_test_agent("child3-p1-uuid", "parent1-uuid", 400);

    add_agent_to_array(parent1);
    add_agent_to_array(parent2);
    add_agent_to_array(child1_of_p1);
    add_agent_to_array(child2_of_p2);  // Different parent - should be skipped
    add_agent_to_array(child3_of_p1);

    repl->current = parent1;

    // Navigate to child - should select most recent child of parent1
    res_t result = ik_repl_nav_child(repl);
    ck_assert(is_ok(&result));
    ck_assert_ptr_eq(repl->current, child3_of_p1);  // Most recent child of parent1
}

END_TEST
// Test: nav_child with zero created_at (legacy data)
START_TEST(test_nav_child_with_zero_created_at) {
    // Create parent and child with created_at = 0 (simulating legacy data)
    ik_agent_ctx_t *parent = create_test_agent("parent-uuid", NULL, 100);
    ik_agent_ctx_t *child = create_test_agent("child-uuid", "parent-uuid", 0);

    add_agent_to_array(parent);
    add_agent_to_array(child);

    repl->current = parent;

    // Navigate to child - should work despite created_at = 0
    res_t result = ik_repl_nav_child(repl);
    ck_assert(is_ok(&result));
    ck_assert_ptr_eq(repl->current, child);
}

END_TEST

// Create suite
static Suite *nav_parent_child_suite(void)
{
    Suite *s = suite_create("Parent/Child Navigation");

    TCase *tc_nav = tcase_create("Navigation");
    tcase_set_timeout(tc_nav, 30);
    tcase_set_timeout(tc_nav, 30);
    tcase_set_timeout(tc_nav, 30);
    tcase_set_timeout(tc_nav, 30);
    tcase_set_timeout(tc_nav, 30);
    tcase_add_checked_fixture(tc_nav, setup, teardown);
    tcase_add_test(tc_nav, test_nav_parent_switches_to_parent);
    tcase_add_test(tc_nav, test_nav_parent_at_root_no_action);
    tcase_add_test(tc_nav, test_nav_child_switches_to_child);
    tcase_add_test(tc_nav, test_nav_child_selects_most_recent_child);
    tcase_add_test(tc_nav, test_nav_child_no_children_no_action);
    tcase_add_test(tc_nav, test_nav_child_skips_dead_children);
    tcase_add_test(tc_nav, test_nav_parent_with_dead_parent_no_action);
    tcase_add_test(tc_nav, test_nav_child_with_mixed_children);
    tcase_add_test(tc_nav, test_nav_child_with_zero_created_at);
    suite_add_tcase(s, tc_nav);

    return s;
}

int main(void)
{
    int failed = 0;
    Suite *s = nav_parent_child_suite();
    SRunner *sr = srunner_create(s);
    srunner_run_all(sr, CK_NORMAL);
    failed = srunner_ntests_failed(sr);
    srunner_free(sr);
    return (failed == 0) ? 0 : 1;
}
