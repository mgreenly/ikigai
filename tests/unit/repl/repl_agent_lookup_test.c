/**
 * @file repl_agent_lookup_test.c
 * @brief Unit tests for agent lookup by UUID with prefix matching
 */

#include <check.h>
#include <talloc.h>
#include <string.h>
#include "../../../src/repl.h"
#include "../../../src/agent.h"
#include "../../../src/shared.h"
#include "../../test_utils_helper.h"
#include "../../helpers/test_contexts_helper.h"

// Test fixture data
static ik_repl_ctx_t *repl = NULL;
static ik_shared_ctx_t *shared = NULL;
static TALLOC_CTX *test_ctx = NULL;

// Setup function - runs before each test
static void setup(void)
{
    test_ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(test_ctx);

    // Create minimal repl context for testing (no terminal needed)
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
    shared = NULL;
}

// Helper: Create agent with specific UUID
static ik_agent_ctx_t *create_agent_with_uuid(const char *uuid)
{
    // Create minimal agent for testing (no full initialization needed)
    ik_agent_ctx_t *agent = talloc_zero(repl, ik_agent_ctx_t);
    ck_assert_ptr_nonnull(agent);

    // Set UUID
    agent->uuid = talloc_strdup(agent, uuid);
    ck_assert_ptr_nonnull(agent->uuid);

    return agent;
}

// Test: Exact match returns correct agent
START_TEST(test_exact_match) {
    // Create agents with different UUIDs
    ik_agent_ctx_t *agent1 = create_agent_with_uuid("abc123def456ghi789jklm");
    ik_agent_ctx_t *agent2 = create_agent_with_uuid("xyz789uvw456rst123opqn");
    ik_agent_ctx_t *agent3 = create_agent_with_uuid("abc999def888ghi777jklm");

    res_t result = ik_repl_add_agent(repl, agent1);
    ck_assert(is_ok(&result));
    result = ik_repl_add_agent(repl, agent2);
    ck_assert(is_ok(&result));
    result = ik_repl_add_agent(repl, agent3);
    ck_assert(is_ok(&result));

    // Exact match should return the correct agent
    ik_agent_ctx_t *found = ik_repl_find_agent(repl, "abc123def456ghi789jklm");
    ck_assert_ptr_eq(found, agent1);

    found = ik_repl_find_agent(repl, "xyz789uvw456rst123opqn");
    ck_assert_ptr_eq(found, agent2);
}
END_TEST
// Test: Prefix match (6 chars) returns correct agent
START_TEST(test_prefix_match) {
    // Create agents with different UUIDs
    ik_agent_ctx_t *agent1 = create_agent_with_uuid("abc123def456ghi789jklm");
    ik_agent_ctx_t *agent2 = create_agent_with_uuid("xyz789uvw456rst123opqn");

    res_t result = ik_repl_add_agent(repl, agent1);
    ck_assert(is_ok(&result));
    result = ik_repl_add_agent(repl, agent2);
    ck_assert(is_ok(&result));

    // Prefix match (6 chars) should return the correct agent
    ik_agent_ctx_t *found = ik_repl_find_agent(repl, "abc123");
    ck_assert_ptr_eq(found, agent1);

    found = ik_repl_find_agent(repl, "xyz789");
    ck_assert_ptr_eq(found, agent2);
}

END_TEST
// Test: Ambiguous prefix returns NULL
START_TEST(test_ambiguous_prefix) {
    // Create agents with UUIDs that share a prefix
    ik_agent_ctx_t *agent1 = create_agent_with_uuid("abc123def456ghi789jklm");
    ik_agent_ctx_t *agent2 = create_agent_with_uuid("abc456def789ghi123jklm");

    res_t result = ik_repl_add_agent(repl, agent1);
    ck_assert(is_ok(&result));
    result = ik_repl_add_agent(repl, agent2);
    ck_assert(is_ok(&result));

    // Ambiguous prefix should return NULL
    ik_agent_ctx_t *found = ik_repl_find_agent(repl, "abc");
    ck_assert_ptr_null(found);

    // More specific prefix should work
    found = ik_repl_find_agent(repl, "abc123");
    ck_assert_ptr_eq(found, agent1);

    found = ik_repl_find_agent(repl, "abc456");
    ck_assert_ptr_eq(found, agent2);
}

END_TEST
// Test: uuid_ambiguous returns true for ambiguous prefix
START_TEST(test_uuid_ambiguous) {
    // Create agents with UUIDs that share a 4-char prefix
    ik_agent_ctx_t *agent1 = create_agent_with_uuid("abcd123def456ghi789jklm");
    ik_agent_ctx_t *agent2 = create_agent_with_uuid("abcd456def789ghi123jklm");

    res_t result = ik_repl_add_agent(repl, agent1);
    ck_assert(is_ok(&result));
    result = ik_repl_add_agent(repl, agent2);
    ck_assert(is_ok(&result));

    // Ambiguous prefix (4 chars) should return true
    bool ambiguous = ik_repl_uuid_ambiguous(repl, "abcd");
    ck_assert(ambiguous);

    // Unambiguous prefix should return false
    ambiguous = ik_repl_uuid_ambiguous(repl, "abcd123");
    ck_assert(!ambiguous);

    ambiguous = ik_repl_uuid_ambiguous(repl, "abcd456");
    ck_assert(!ambiguous);

    // Prefix too short (less than 4 chars) should return false
    ambiguous = ik_repl_uuid_ambiguous(repl, "abc");
    ck_assert(!ambiguous);
}

END_TEST
// Test: Minimum 4 char prefix enforced
START_TEST(test_minimum_prefix_length) {
    // Create agent
    ik_agent_ctx_t *agent1 = create_agent_with_uuid("abc123def456ghi789jklm");

    res_t result = ik_repl_add_agent(repl, agent1);
    ck_assert(is_ok(&result));

    // Prefix too short (less than 4 chars) should return NULL
    ik_agent_ctx_t *found = ik_repl_find_agent(repl, "a");
    ck_assert_ptr_null(found);

    found = ik_repl_find_agent(repl, "ab");
    ck_assert_ptr_null(found);

    found = ik_repl_find_agent(repl, "abc");
    ck_assert_ptr_null(found);

    // Minimum 4 chars should work
    found = ik_repl_find_agent(repl, "abc1");
    ck_assert_ptr_eq(found, agent1);
}

END_TEST
// Test: No match returns NULL
START_TEST(test_no_match) {
    // Create agent
    ik_agent_ctx_t *agent1 = create_agent_with_uuid("abc123def456ghi789jklm");

    res_t result = ik_repl_add_agent(repl, agent1);
    ck_assert(is_ok(&result));

    // Non-matching prefix should return NULL
    ik_agent_ctx_t *found = ik_repl_find_agent(repl, "xyz789");
    ck_assert_ptr_null(found);
}

END_TEST
// Test: Empty agent array returns NULL
START_TEST(test_empty_array) {
    // No agents added
    ik_agent_ctx_t *found = ik_repl_find_agent(repl, "abc123");
    ck_assert_ptr_null(found);

    bool ambiguous = ik_repl_uuid_ambiguous(repl, "abc123");
    ck_assert(!ambiguous);
}

END_TEST
// Test: Exact match takes priority over prefix
START_TEST(test_exact_match_priority) {
    // Create agents where one UUID is a prefix of another (unlikely but test boundary)
    ik_agent_ctx_t *agent1 = create_agent_with_uuid("abc123");
    ik_agent_ctx_t *agent2 = create_agent_with_uuid("abc123def456ghi789jklm");

    res_t result = ik_repl_add_agent(repl, agent1);
    ck_assert(is_ok(&result));
    result = ik_repl_add_agent(repl, agent2);
    ck_assert(is_ok(&result));

    // Exact match should take priority
    ik_agent_ctx_t *found = ik_repl_find_agent(repl, "abc123");
    ck_assert_ptr_eq(found, agent1);
}

END_TEST

// Create test suite
static Suite *repl_agent_lookup_suite(void)
{
    Suite *s = suite_create("Agent Lookup");

    TCase *tc_exact = tcase_create("Exact Match");
    tcase_set_timeout(tc_exact, IK_TEST_TIMEOUT);
    tcase_set_timeout(tc_exact, IK_TEST_TIMEOUT);
    tcase_set_timeout(tc_exact, IK_TEST_TIMEOUT);
    tcase_set_timeout(tc_exact, IK_TEST_TIMEOUT);
    tcase_set_timeout(tc_exact, IK_TEST_TIMEOUT);
    tcase_add_checked_fixture(tc_exact, setup, teardown);
    tcase_add_test(tc_exact, test_exact_match);
    suite_add_tcase(s, tc_exact);

    TCase *tc_prefix = tcase_create("Prefix Match");
    tcase_set_timeout(tc_prefix, IK_TEST_TIMEOUT);
    tcase_set_timeout(tc_prefix, IK_TEST_TIMEOUT);
    tcase_set_timeout(tc_prefix, IK_TEST_TIMEOUT);
    tcase_set_timeout(tc_prefix, IK_TEST_TIMEOUT);
    tcase_set_timeout(tc_prefix, IK_TEST_TIMEOUT);
    tcase_add_checked_fixture(tc_prefix, setup, teardown);
    tcase_add_test(tc_prefix, test_prefix_match);
    tcase_add_test(tc_prefix, test_ambiguous_prefix);
    tcase_add_test(tc_prefix, test_minimum_prefix_length);
    suite_add_tcase(s, tc_prefix);

    TCase *tc_ambiguous = tcase_create("Ambiguous Detection");
    tcase_set_timeout(tc_ambiguous, IK_TEST_TIMEOUT);
    tcase_set_timeout(tc_ambiguous, IK_TEST_TIMEOUT);
    tcase_set_timeout(tc_ambiguous, IK_TEST_TIMEOUT);
    tcase_set_timeout(tc_ambiguous, IK_TEST_TIMEOUT);
    tcase_set_timeout(tc_ambiguous, IK_TEST_TIMEOUT);
    tcase_add_checked_fixture(tc_ambiguous, setup, teardown);
    tcase_add_test(tc_ambiguous, test_uuid_ambiguous);
    suite_add_tcase(s, tc_ambiguous);

    TCase *tc_edge = tcase_create("Edge Cases");
    tcase_set_timeout(tc_edge, IK_TEST_TIMEOUT);
    tcase_set_timeout(tc_edge, IK_TEST_TIMEOUT);
    tcase_set_timeout(tc_edge, IK_TEST_TIMEOUT);
    tcase_set_timeout(tc_edge, IK_TEST_TIMEOUT);
    tcase_set_timeout(tc_edge, IK_TEST_TIMEOUT);
    tcase_add_checked_fixture(tc_edge, setup, teardown);
    tcase_add_test(tc_edge, test_no_match);
    tcase_add_test(tc_edge, test_empty_array);
    tcase_add_test(tc_edge, test_exact_match_priority);
    suite_add_tcase(s, tc_edge);

    return s;
}

int main(void)
{
    int number_failed;
    Suite *s = repl_agent_lookup_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/repl/repl_agent_lookup_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
