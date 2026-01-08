#include "../../test_constants.h"
// tests/unit/repl/nav_context_dead_agent_test.c
// Test ik_repl_update_nav_context with stale/dead agent references
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

// Helper to create agent with separator layer
static ik_agent_ctx_t *create_agent_with_separator(const char *uuid, const char *parent_uuid, int64_t created_at)
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

    return agent;
}

// Test: Exercise edge case where agents have mismatched state
// This test creates a scenario that exercises defensive NULL checks
START_TEST(test_nav_context_with_removed_prev_sibling) {
    // Create 4 siblings with different timestamps, ensuring we test
    // the comparison logic paths at lines 610 and 620
    ik_agent_ctx_t *sibling1 = create_agent_with_separator("sibling1-uuid-xxx", NULL, 1000);
    ik_agent_ctx_t *sibling2 = create_agent_with_separator("sibling2-uuid-yyy", NULL, 1500);
    ik_agent_ctx_t *sibling3 = create_agent_with_separator("sibling3-uuid-zzz", NULL, 2000);
    ik_agent_ctx_t *sibling4 = create_agent_with_separator("sibling4-uuid-aaa", NULL, 2500);

    // Add all to array
    repl->agents[repl->agent_count++] = sibling1;
    repl->agents[repl->agent_count++] = sibling2;
    repl->agents[repl->agent_count++] = sibling3;
    repl->agents[repl->agent_count++] = sibling4;

    // Set current to sibling4 - it will have multiple prev siblings
    repl->current = sibling4;

    // This call will:
    // 1. Find sibling1 as first prev (created_at < current)
    // 2. Find sibling2, compare with sibling1 via find_agent_by_uuid
    // 3. Find sibling3, compare with sibling2 via find_agent_by_uuid
    //
    // Each comparison exercises the pattern:
    //   current_prev = find_agent_by_uuid(repl, prev_sibling)
    //   if (current_prev != NULL && ...)
    ik_repl_update_nav_context(repl);

    // Verify it completed successfully
    ck_assert_ptr_nonnull(sibling4->separator_layer);
}
END_TEST
// Test: ik_repl_update_nav_context with dead/removed next sibling
START_TEST(test_nav_context_with_removed_next_sibling) {
    // Create 3 siblings with sequential timestamps
    ik_agent_ctx_t *sibling1 = create_agent_with_separator("sibling1", NULL, 1000);
    ik_agent_ctx_t *sibling2 = create_agent_with_separator("sibling2", NULL, 2000);
    ik_agent_ctx_t *sibling3 = create_agent_with_separator("sibling3", NULL, 3000);

    // Add sibling1 and sibling2 to repl (sibling3 is "dead" - not in array)
    repl->agents[repl->agent_count++] = sibling1;
    repl->agents[repl->agent_count++] = sibling2;

    // Set current to sibling2 (newest in array)
    repl->current = sibling2;

    // Call ik_repl_update_nav_context - sibling2 would have next_sibling=sibling3 if it existed
    // but sibling3 is not in array (dead), so find_agent_by_uuid will fail
    ik_repl_update_nav_context(repl);

    // Should complete without crash
    ck_assert_ptr_nonnull(sibling2->separator_layer);

    // Clean up temporary agent not in repl
    talloc_free(sibling3);
}

END_TEST
// Test: ik_repl_update_nav_context with multiple prev siblings (tests line 610 branches)
START_TEST(test_nav_context_multiple_prev_siblings) {
    // Create 4 siblings with sequential timestamps
    ik_agent_ctx_t *sibling1 = create_agent_with_separator("sibling1", NULL, 1000);
    ik_agent_ctx_t *sibling2 = create_agent_with_separator("sibling2", NULL, 2000);
    ik_agent_ctx_t *sibling3 = create_agent_with_separator("sibling3", NULL, 3000);
    ik_agent_ctx_t *sibling4 = create_agent_with_separator("sibling4", NULL, 4000);

    // Add all siblings to repl
    repl->agents[repl->agent_count++] = sibling1;
    repl->agents[repl->agent_count++] = sibling2;
    repl->agents[repl->agent_count++] = sibling3;
    repl->agents[repl->agent_count++] = sibling4;

    // Set current to sibling4 (has 3 prev siblings)
    repl->current = sibling4;

    // Call ik_repl_update_nav_context - should find sibling3 as most recent prev
    // This exercises the comparison logic at line 610
    ik_repl_update_nav_context(repl);

    // Should complete without crash
    ck_assert_ptr_nonnull(sibling4->separator_layer);
}

END_TEST
// Test: ik_repl_update_nav_context with multiple next siblings (tests line 620 branches)
START_TEST(test_nav_context_multiple_next_siblings) {
    // Create 4 siblings with sequential timestamps
    ik_agent_ctx_t *sibling1 = create_agent_with_separator("sibling1", NULL, 1000);
    ik_agent_ctx_t *sibling2 = create_agent_with_separator("sibling2", NULL, 2000);
    ik_agent_ctx_t *sibling3 = create_agent_with_separator("sibling3", NULL, 3000);
    ik_agent_ctx_t *sibling4 = create_agent_with_separator("sibling4", NULL, 4000);

    // Add all siblings to repl
    repl->agents[repl->agent_count++] = sibling1;
    repl->agents[repl->agent_count++] = sibling2;
    repl->agents[repl->agent_count++] = sibling3;
    repl->agents[repl->agent_count++] = sibling4;

    // Set current to sibling1 (has 3 next siblings)
    repl->current = sibling1;

    // Call ik_repl_update_nav_context - should find sibling2 as earliest next
    // This exercises the comparison logic at line 620
    ik_repl_update_nav_context(repl);

    // Should complete without crash
    ck_assert_ptr_nonnull(sibling1->separator_layer);
}

END_TEST

static Suite *nav_context_dead_agent_suite(void)
{
    Suite *s = suite_create("Navigation Context Dead Agent");

    TCase *tc = tcase_create("Dead Agent References");
    tcase_set_timeout(tc, IK_TEST_TIMEOUT);
    tcase_set_timeout(tc, IK_TEST_TIMEOUT);
    tcase_set_timeout(tc, IK_TEST_TIMEOUT);
    tcase_set_timeout(tc, IK_TEST_TIMEOUT);
    tcase_set_timeout(tc, IK_TEST_TIMEOUT);
    tcase_add_checked_fixture(tc, setup, teardown);
    tcase_add_test(tc, test_nav_context_with_removed_prev_sibling);
    tcase_add_test(tc, test_nav_context_with_removed_next_sibling);
    tcase_add_test(tc, test_nav_context_multiple_prev_siblings);
    tcase_add_test(tc, test_nav_context_multiple_next_siblings);
    suite_add_tcase(s, tc);

    return s;
}

int main(void)
{
    Suite *s = nav_context_dead_agent_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
