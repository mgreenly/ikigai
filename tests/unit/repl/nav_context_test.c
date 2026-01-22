#include "../../test_constants.h"
// tests/unit/repl/nav_context_test.c - Navigation context wiring tests
#include <check.h>
#include <talloc.h>
#include "repl.h"
#include "shared.h"
#include "agent.h"
#include "layer_wrappers.h"

// Forward declaration from repl.c (to be implemented)
void ik_repl_update_nav_context(ik_repl_ctx_t *repl);

// Test fixture
static ik_shared_ctx_t *shared;
static ik_repl_ctx_t *repl;

static void setup(void)
{
    shared = talloc_zero(NULL, ik_shared_ctx_t);
    ck_assert_ptr_nonnull(shared);

    repl = talloc_zero(shared, ik_repl_ctx_t);
    ck_assert_ptr_nonnull(repl);
    repl->shared = shared;

    // Initialize agent array
    repl->agent_capacity = 8;
    repl->agents = talloc_zero_array(repl, ik_agent_ctx_t *, 16);
    ck_assert_ptr_nonnull(repl->agents);
    repl->agent_count = 0;
}

static void teardown(void)
{
    talloc_free(shared);
}

// Helper to create agent with separator layer
static ik_agent_ctx_t *create_agent_with_separator(const char *uuid, const char *parent_uuid)
{
    ik_agent_ctx_t *agent = talloc_zero(repl, ik_agent_ctx_t);
    ck_assert_ptr_nonnull(agent);

    agent->uuid = talloc_strdup(agent, uuid);
    if (parent_uuid != NULL) {
        agent->parent_uuid = talloc_strdup(agent, parent_uuid);
    }
    agent->created_at = (int64_t)repl->agent_count * 1000;  // Sequential timestamps

    // Create separator layer
    bool visible = true;
    agent->separator_layer = ik_separator_layer_create(agent, "test_separator", &visible);
    ck_assert_ptr_nonnull(agent->separator_layer);

    // Add to array
    repl->agents[repl->agent_count++] = agent;

    return agent;
}

START_TEST(test_nav_context_called_with_simple_hierarchy) {
    // Create parent and child
    (void)create_agent_with_separator("parent-uuid", NULL);
    ik_agent_ctx_t *child = create_agent_with_separator("child-uuid", "parent-uuid");

    // Set current to child
    repl->current = child;

    // Call the update function (should not crash)
    ik_repl_update_nav_context(repl);

    // Basic assertion: function was called and didn't crash
    ck_assert_ptr_nonnull(child->separator_layer);
}
END_TEST

START_TEST(test_nav_context_called_with_siblings) {
    // Create parent with 3 children
    (void)create_agent_with_separator("parent-uuid", NULL);
    (void)create_agent_with_separator("child1-uuid", "parent-uuid");
    ik_agent_ctx_t *child2 = create_agent_with_separator("child2-uuid", "parent-uuid");
    (void)create_agent_with_separator("child3-uuid", "parent-uuid");

    // Set current to middle child
    repl->current = child2;

    // Call the update function (should not crash)
    ik_repl_update_nav_context(repl);

    // Basic assertion: function was called and didn't crash
    ck_assert_ptr_nonnull(child2->separator_layer);
}

END_TEST

START_TEST(test_nav_context_called_with_children) {
    // Create parent with 2 children
    ik_agent_ctx_t *parent = create_agent_with_separator("parent-uuid", NULL);
    (void)create_agent_with_separator("child1-uuid", "parent-uuid");
    (void)create_agent_with_separator("child2-uuid", "parent-uuid");

    // Set current to parent
    repl->current = parent;

    // Call the update function (should not crash)
    ik_repl_update_nav_context(repl);

    // Basic assertion: function was called and didn't crash
    ck_assert_ptr_nonnull(parent->separator_layer);
}

END_TEST

START_TEST(test_nav_context_null_separator) {
    // Create agent without separator layer
    ik_agent_ctx_t *agent = talloc_zero(repl, ik_agent_ctx_t);
    agent->uuid = talloc_strdup(agent, "test-uuid");
    agent->separator_layer = NULL;

    repl->agents[repl->agent_count++] = agent;
    repl->current = agent;

    // Should not crash when separator is NULL
    ik_repl_update_nav_context(repl);

    // If we reach here, the function handled NULL correctly
    ck_assert_ptr_null(agent->separator_layer);
}

END_TEST

START_TEST(test_nav_context_null_current) {
    // Create some agents
    create_agent_with_separator("agent-uuid", NULL);

    // Set current to NULL
    repl->current = NULL;

    // Should not crash when current is NULL
    ik_repl_update_nav_context(repl);

    // If we reach here, the function handled NULL correctly
    ck_assert_ptr_null(repl->current);
}

END_TEST

static Suite *nav_context_suite(void)
{
    Suite *s = suite_create("Navigation Context Wiring");
    TCase *tc_core = tcase_create("Core");
    tcase_set_timeout(tc_core, IK_TEST_TIMEOUT);
    tcase_set_timeout(tc_core, IK_TEST_TIMEOUT);
    tcase_set_timeout(tc_core, IK_TEST_TIMEOUT);
    tcase_set_timeout(tc_core, IK_TEST_TIMEOUT);
    tcase_set_timeout(tc_core, IK_TEST_TIMEOUT);

    tcase_add_checked_fixture(tc_core, setup, teardown);
    tcase_add_test(tc_core, test_nav_context_called_with_simple_hierarchy);
    tcase_add_test(tc_core, test_nav_context_called_with_siblings);
    tcase_add_test(tc_core, test_nav_context_called_with_children);
    tcase_add_test(tc_core, test_nav_context_null_separator);
    tcase_add_test(tc_core, test_nav_context_null_current);

    suite_add_tcase(s, tc_core);
    return s;
}

int main(void)
{
    Suite *s = nav_context_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/repl/nav_context_test.xml");
    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);
    return (number_failed == 0) ? 0 : 1;
}
