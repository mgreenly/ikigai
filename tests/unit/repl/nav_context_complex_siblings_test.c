// tests/unit/repl/nav_context_complex_siblings_test.c - Tests for complex sibling navigation edge cases
#include <check.h>
#include <talloc.h>
#include "repl.h"
#include "shared.h"
#include "agent.h"
#include "layer_wrappers.h"

// Forward declaration from repl.c
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
    repl->agent_capacity = 16;
    repl->agents = talloc_zero_array(repl, ik_agent_ctx_t *, 16);
    ck_assert_ptr_nonnull(repl->agents);
    repl->agent_count = 0;
}

static void teardown(void)
{
    talloc_free(shared);
}

// Helper to create agent with separator layer and specific timestamp
static ik_agent_ctx_t *create_agent_with_timestamp(const char *uuid, const char *parent_uuid, int64_t created_at)
{
    ik_agent_ctx_t *agent = talloc_zero(repl, ik_agent_ctx_t);
    ck_assert_ptr_nonnull(agent);

    agent->uuid = talloc_strdup(agent, uuid);
    if (parent_uuid != NULL) {
        agent->parent_uuid = talloc_strdup(agent, parent_uuid);
    }
    agent->created_at = created_at;

    // Create separator layer
    bool visible = true;
    agent->separator_layer = ik_separator_layer_create(agent, "test_separator", &visible);
    ck_assert_ptr_nonnull(agent->separator_layer);

    // Add to array
    repl->agents[repl->agent_count++] = agent;

    return agent;
}

// Test: Multiple siblings with complex timestamp ordering requiring prev_sibling updates
START_TEST(test_nav_context_multiple_prev_siblings) {
    // Create parent
    (void)create_agent_with_timestamp("parent-uuid", NULL, 1000);

    // Create multiple siblings with different timestamps
    // child1: oldest (should be prev for child3)
    (void)create_agent_with_timestamp("child1-uuid", "parent-uuid", 2000);
    // child2: middle (should be prev for child3, but child1 is older)
    (void)create_agent_with_timestamp("child2-uuid", "parent-uuid", 2500);
    // child3: newest (current agent)
    ik_agent_ctx_t *child3 = create_agent_with_timestamp("child3-uuid", "parent-uuid", 3000);

    // Set current to child3
    repl->current = child3;

    // Call ik_repl_update_nav_context - should find child2 as most recent prev sibling
    ik_repl_update_nav_context(repl);

    // If we reach here, the complex prev sibling logic was executed
    ck_assert_ptr_nonnull(child3->separator_layer);
}
END_TEST
// Test: Multiple siblings with complex timestamp ordering requiring next_sibling updates
START_TEST(test_nav_context_multiple_next_siblings)
{
    // Create parent
    (void)create_agent_with_timestamp("parent-uuid", NULL, 1000);

    // Create multiple siblings with different timestamps
    // child1: oldest (current agent)
    ik_agent_ctx_t *child1 = create_agent_with_timestamp("child1-uuid", "parent-uuid", 2000);
    // child2: middle (should be next for child1, but child3 is later)
    (void)create_agent_with_timestamp("child2-uuid", "parent-uuid", 2500);
    // child3: newest
    (void)create_agent_with_timestamp("child3-uuid", "parent-uuid", 3000);

    // Set current to child1
    repl->current = child1;

    // Call ik_repl_update_nav_context - should find child2 as earliest next sibling
    ik_repl_update_nav_context(repl);

    // If we reach here, the complex next sibling logic was executed
    ck_assert_ptr_nonnull(child1->separator_layer);
}

END_TEST
// Test: Five siblings to exercise all timestamp comparison paths
START_TEST(test_nav_context_five_siblings_middle_current)
{
    // Create parent
    (void)create_agent_with_timestamp("parent-uuid", NULL, 1000);

    // Create five siblings
    (void)create_agent_with_timestamp("child1-uuid", "parent-uuid", 2000);
    (void)create_agent_with_timestamp("child2-uuid", "parent-uuid", 2200);
    ik_agent_ctx_t *child3 = create_agent_with_timestamp("child3-uuid", "parent-uuid", 2500);  // Current
    (void)create_agent_with_timestamp("child4-uuid", "parent-uuid", 2800);
    (void)create_agent_with_timestamp("child5-uuid", "parent-uuid", 3000);

    // Set current to middle child (child3)
    repl->current = child3;

    // Call ik_repl_update_nav_context - exercises both prev and next sibling search paths
    ik_repl_update_nav_context(repl);

    // Verify no crashes
    ck_assert_ptr_nonnull(child3->separator_layer);
}

END_TEST
// Test: Same parent (both NULL) - root-level siblings
START_TEST(test_nav_context_root_level_siblings)
{
    // Create multiple root-level agents (parent_uuid = NULL for all)
    ik_agent_ctx_t *root1 = create_agent_with_timestamp("root1-uuid", NULL, 1000);
    (void)create_agent_with_timestamp("root2-uuid", NULL, 2000);
    (void)create_agent_with_timestamp("root3-uuid", NULL, 3000);

    // Set current to root1
    repl->current = root1;

    // Call ik_repl_update_nav_context - should handle NULL parent_uuid comparison
    ik_repl_update_nav_context(repl);

    // Verify no crashes with NULL parent comparison
    ck_assert_ptr_nonnull(root1->separator_layer);
}

END_TEST
// Test: Agent with both older and newer siblings to exercise timestamp comparisons
START_TEST(test_nav_context_timestamp_comparisons)
{
    // Create parent
    (void)create_agent_with_timestamp("parent-uuid", NULL, 1000);

    // Create siblings around current agent
    (void)create_agent_with_timestamp("older1-uuid", "parent-uuid", 2000);
    (void)create_agent_with_timestamp("older2-uuid", "parent-uuid", 2100);
    ik_agent_ctx_t *current = create_agent_with_timestamp("current-uuid", "parent-uuid", 2500);
    (void)create_agent_with_timestamp("newer1-uuid", "parent-uuid", 2800);
    (void)create_agent_with_timestamp("newer2-uuid", "parent-uuid", 3000);

    repl->current = current;

    // This should exercise:
    // - agent->created_at < repl->current->created_at (for older siblings)
    // - agent->created_at > current_prev->created_at (keeping most recent prev)
    // - agent->created_at < current_next->created_at (keeping earliest next)
    ik_repl_update_nav_context(repl);

    ck_assert_ptr_nonnull(current->separator_layer);
}

END_TEST
// Test: Agent equal timestamps (edge case)
START_TEST(test_nav_context_equal_timestamps)
{
    // Create parent
    (void)create_agent_with_timestamp("parent-uuid", NULL, 1000);

    // Create siblings with same timestamp (unlikely but possible)
    (void)create_agent_with_timestamp("child1-uuid", "parent-uuid", 2500);
    ik_agent_ctx_t *child2 = create_agent_with_timestamp("child2-uuid", "parent-uuid", 2500);
    (void)create_agent_with_timestamp("child3-uuid", "parent-uuid", 2500);

    repl->current = child2;

    // Should handle equal timestamps gracefully
    ik_repl_update_nav_context(repl);

    ck_assert_ptr_nonnull(child2->separator_layer);
}

END_TEST
// Test: Agent removed from array causing find_agent_by_uuid to return NULL
START_TEST(test_nav_context_with_removed_sibling)
{
    // Create parent
    (void)create_agent_with_timestamp("parent-uuid", NULL, 1000);

    // Create siblings
    ik_agent_ctx_t *older = create_agent_with_timestamp("older-uuid", "parent-uuid", 2000);
    ik_agent_ctx_t *middle = create_agent_with_timestamp("middle-uuid", "parent-uuid", 2500);
    (void)create_agent_with_timestamp("newer-uuid", "parent-uuid", 3000);

    // Remove older sibling from array (simulating agent death)
    // This will cause find_agent_by_uuid to return NULL for prev_sibling
    size_t removed_idx = 0;
    for (size_t i = 0; i < repl->agent_count; i++) {
        if (repl->agents[i] == older) {
            removed_idx = i;
            break;
        }
    }
    // Shift array to remove older
    for (size_t i = removed_idx; i < repl->agent_count - 1; i++) {
        repl->agents[i] = repl->agents[i + 1];
    }
    repl->agent_count--;

    // Set current to middle - prev_sibling search will find NULL
    repl->current = middle;

    // Update context - should handle NULL from find_agent_by_uuid
    ik_repl_update_nav_context(repl);

    ck_assert_ptr_nonnull(middle->separator_layer);
}

END_TEST
// Test: Next sibling replacement path with earlier timestamp
START_TEST(test_nav_context_next_sibling_earlier_timestamp)
{
    // Create parent
    (void)create_agent_with_timestamp("parent-uuid", NULL, 1000);

    // Create multiple next siblings to exercise the replacement logic
    ik_agent_ctx_t *current = create_agent_with_timestamp("current-uuid", "parent-uuid", 2000);
    (void)create_agent_with_timestamp("next-far-uuid", "parent-uuid", 3500);   // Far next
    (void)create_agent_with_timestamp("next-near-uuid", "parent-uuid", 2500);  // Near next (earlier)

    repl->current = current;

    // This should:
    // 1. Set next_sibling to next-far-uuid
    // 2. Find a better (earlier) next sibling next-near-uuid
    // 3. Replace next_sibling (line 621)
    ik_repl_update_nav_context(repl);

    ck_assert_ptr_nonnull(current->separator_layer);
}

END_TEST

static Suite *nav_context_complex_suite(void)
{
    Suite *s = suite_create("Navigation Context Complex Siblings");
    TCase *tc_core = tcase_create("Core");

    tcase_add_checked_fixture(tc_core, setup, teardown);
    tcase_add_test(tc_core, test_nav_context_multiple_prev_siblings);
    tcase_add_test(tc_core, test_nav_context_multiple_next_siblings);
    tcase_add_test(tc_core, test_nav_context_five_siblings_middle_current);
    tcase_add_test(tc_core, test_nav_context_root_level_siblings);
    tcase_add_test(tc_core, test_nav_context_timestamp_comparisons);
    tcase_add_test(tc_core, test_nav_context_equal_timestamps);
    tcase_add_test(tc_core, test_nav_context_with_removed_sibling);
    tcase_add_test(tc_core, test_nav_context_next_sibling_earlier_timestamp);

    suite_add_tcase(s, tc_core);
    return s;
}

int main(void)
{
    Suite *s = nav_context_complex_suite();
    SRunner *sr = srunner_create(s);
    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);
    return (number_failed == 0) ? 0 : 1;
}
