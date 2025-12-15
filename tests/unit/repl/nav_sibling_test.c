/**
 * @file nav_sibling_test.c
 * @brief Unit tests for sibling navigation (Ctrl+Left/Right)
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

// Test: nav_next with siblings switches to next
START_TEST(test_nav_next_with_siblings_switches_to_next)
{
    // Create parent and 3 siblings
    ik_agent_ctx_t *parent = create_test_agent("parent-uuid", NULL, 100);
    ik_agent_ctx_t *sibling1 = create_test_agent("sibling1-uuid", "parent-uuid", 200);
    ik_agent_ctx_t *sibling2 = create_test_agent("sibling2-uuid", "parent-uuid", 300);
    ik_agent_ctx_t *sibling3 = create_test_agent("sibling3-uuid", "parent-uuid", 400);

    add_agent_to_array(parent);
    add_agent_to_array(sibling1);
    add_agent_to_array(sibling2);
    add_agent_to_array(sibling3);

    repl->current = sibling1;

    res_t result = ik_repl_nav_next_sibling(repl);
    ck_assert(is_ok(&result));
    ck_assert_ptr_eq(repl->current, sibling2);
}
END_TEST

// Test: nav_next wraps to first after last
START_TEST(test_nav_next_wraps_to_first_after_last)
{
    // Create parent and 3 siblings
    ik_agent_ctx_t *parent = create_test_agent("parent-uuid", NULL, 100);
    ik_agent_ctx_t *sibling1 = create_test_agent("sibling1-uuid", "parent-uuid", 200);
    ik_agent_ctx_t *sibling2 = create_test_agent("sibling2-uuid", "parent-uuid", 300);
    ik_agent_ctx_t *sibling3 = create_test_agent("sibling3-uuid", "parent-uuid", 400);

    add_agent_to_array(parent);
    add_agent_to_array(sibling1);
    add_agent_to_array(sibling2);
    add_agent_to_array(sibling3);

    repl->current = sibling3;

    res_t result = ik_repl_nav_next_sibling(repl);
    ck_assert(is_ok(&result));
    ck_assert_ptr_eq(repl->current, sibling1);
}
END_TEST

// Test: nav_prev switches to previous
START_TEST(test_nav_prev_switches_to_previous)
{
    // Create parent and 3 siblings
    ik_agent_ctx_t *parent = create_test_agent("parent-uuid", NULL, 100);
    ik_agent_ctx_t *sibling1 = create_test_agent("sibling1-uuid", "parent-uuid", 200);
    ik_agent_ctx_t *sibling2 = create_test_agent("sibling2-uuid", "parent-uuid", 300);
    ik_agent_ctx_t *sibling3 = create_test_agent("sibling3-uuid", "parent-uuid", 400);

    add_agent_to_array(parent);
    add_agent_to_array(sibling1);
    add_agent_to_array(sibling2);
    add_agent_to_array(sibling3);

    repl->current = sibling2;

    res_t result = ik_repl_nav_prev_sibling(repl);
    ck_assert(is_ok(&result));
    ck_assert_ptr_eq(repl->current, sibling1);
}
END_TEST

// Test: nav_prev wraps to last from first
START_TEST(test_nav_prev_wraps_to_last_from_first)
{
    // Create parent and 3 siblings
    ik_agent_ctx_t *parent = create_test_agent("parent-uuid", NULL, 100);
    ik_agent_ctx_t *sibling1 = create_test_agent("sibling1-uuid", "parent-uuid", 200);
    ik_agent_ctx_t *sibling2 = create_test_agent("sibling2-uuid", "parent-uuid", 300);
    ik_agent_ctx_t *sibling3 = create_test_agent("sibling3-uuid", "parent-uuid", 400);

    add_agent_to_array(parent);
    add_agent_to_array(sibling1);
    add_agent_to_array(sibling2);
    add_agent_to_array(sibling3);

    repl->current = sibling1;

    res_t result = ik_repl_nav_prev_sibling(repl);
    ck_assert(is_ok(&result));
    ck_assert_ptr_eq(repl->current, sibling3);
}
END_TEST

// Test: no siblings = no action
START_TEST(test_no_siblings_no_action)
{
    // Create parent and single child (no siblings)
    ik_agent_ctx_t *parent = create_test_agent("parent-uuid", NULL, 100);
    ik_agent_ctx_t *only_child = create_test_agent("child-uuid", "parent-uuid", 200);

    add_agent_to_array(parent);
    add_agent_to_array(only_child);

    repl->current = only_child;

    res_t result = ik_repl_nav_next_sibling(repl);
    ck_assert(is_ok(&result));
    ck_assert_ptr_eq(repl->current, only_child);  // No change

    result = ik_repl_nav_prev_sibling(repl);
    ck_assert(is_ok(&result));
    ck_assert_ptr_eq(repl->current, only_child);  // No change
}
END_TEST

// Test: only counts running siblings (all in agents[] are running)
START_TEST(test_only_counts_running_siblings)
{
    // Create parent and 2 siblings (both running, in agents[] array)
    ik_agent_ctx_t *parent = create_test_agent("parent-uuid", NULL, 100);
    ik_agent_ctx_t *sibling1 = create_test_agent("sibling1-uuid", "parent-uuid", 200);
    ik_agent_ctx_t *sibling2 = create_test_agent("sibling2-uuid", "parent-uuid", 300);

    add_agent_to_array(parent);
    add_agent_to_array(sibling1);
    add_agent_to_array(sibling2);

    repl->current = sibling1;

    // Navigate next - should go to sibling2
    res_t result = ik_repl_nav_next_sibling(repl);
    ck_assert(is_ok(&result));
    ck_assert_ptr_eq(repl->current, sibling2);

    // Navigate next again - should wrap to sibling1
    result = ik_repl_nav_next_sibling(repl);
    ck_assert(is_ok(&result));
    ck_assert_ptr_eq(repl->current, sibling1);
}
END_TEST

// Create suite
static Suite *nav_sibling_suite(void)
{
    Suite *s = suite_create("Sibling Navigation");

    TCase *tc_nav = tcase_create("Navigation");
    tcase_add_checked_fixture(tc_nav, setup, teardown);
    tcase_add_test(tc_nav, test_nav_next_with_siblings_switches_to_next);
    tcase_add_test(tc_nav, test_nav_next_wraps_to_first_after_last);
    tcase_add_test(tc_nav, test_nav_prev_switches_to_previous);
    tcase_add_test(tc_nav, test_nav_prev_wraps_to_last_from_first);
    tcase_add_test(tc_nav, test_no_siblings_no_action);
    tcase_add_test(tc_nav, test_only_counts_running_siblings);
    suite_add_tcase(s, tc_nav);

    return s;
}

int main(void)
{
    int failed = 0;
    Suite *s = nav_sibling_suite();
    SRunner *sr = srunner_create(s);
    srunner_run_all(sr, CK_NORMAL);
    failed = srunner_ntests_failed(sr);
    srunner_free(sr);
    return (failed == 0) ? 0 : 1;
}
