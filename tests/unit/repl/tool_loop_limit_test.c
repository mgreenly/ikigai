/**
 * @file tool_loop_limit_test.c
 * @brief Unit tests for tool loop iteration counter and limit detection
 *
 * Tests that the conversation loop correctly counts tool call iterations
 * and detects when the max_tool_turns limit is reached.
 */

#include "repl.h"
#include "../../../src/agent.h"
#include "config.h"
#include "../../../src/shared.h"
#include "scrollback.h"
#include "openai/client.h"
#include <check.h>
#include <talloc.h>

static void *ctx;
static ik_repl_ctx_t *repl;
static ik_cfg_t *cfg;

static void setup(void)
{
    ctx = talloc_new(NULL);

    /* Create minimal config with max_tool_turns set to 3 */
    cfg = talloc_zero(ctx, ik_cfg_t);
    cfg->max_tool_turns = 3;

    /* Create minimal REPL context for testing */
    repl = talloc_zero(ctx, ik_repl_ctx_t);
    // Create shared context
    ik_shared_ctx_t *shared = talloc_zero(ctx, ik_shared_ctx_t);
    shared->cfg = cfg;
    repl->shared = shared;
    repl->current->scrollback = ik_scrollback_create(repl, 80);

    /* Create conversation */
    res_t res = ik_openai_conversation_create(ctx);
    ck_assert(is_ok(&res));
    repl->conversation = res.ok;

    /* Initialize counter to 0 (simulating start of user request) */
    repl->tool_iteration_count = 0;
}

static void teardown(void)
{
    talloc_free(ctx);
}

/*
 * Test: Counter initializes to 0 at start of request
 */
START_TEST(test_counter_initializes_to_zero) {
    /* Counter should be 0 after setup */
    ck_assert_int_eq(repl->tool_iteration_count, 0);
}
END_TEST
/*
 * Test: Counter increments after tool execution
 */
START_TEST(test_counter_increments_after_tool_execution)
{
    /* Simulate first tool call completed */
    repl->response_finish_reason = talloc_strdup(repl, "tool_calls");
    repl->tool_iteration_count = 0;

    /* Increment counter (simulating what happens after tool execution) */
    repl->tool_iteration_count++;

    ck_assert_int_eq(repl->tool_iteration_count, 1);

    /* Simulate second tool call */
    repl->tool_iteration_count++;
    ck_assert_int_eq(repl->tool_iteration_count, 2);

    /* Simulate third tool call */
    repl->tool_iteration_count++;
    ck_assert_int_eq(repl->tool_iteration_count, 3);
}

END_TEST
/*
 * Test: Should continue when under limit
 */
START_TEST(test_should_continue_when_under_limit)
{
    /* Set up: 2 iterations completed, limit is 3 */
    repl->tool_iteration_count = 2;
    repl->response_finish_reason = talloc_strdup(repl, "tool_calls");

    /* Should continue - we haven't hit the limit yet */
    bool should_continue = ik_repl_should_continue_tool_loop(repl);
    ck_assert(should_continue);
}

END_TEST
/*
 * Test: Should NOT continue when at limit
 */
START_TEST(test_should_not_continue_when_at_limit)
{
    /* Set up: 3 iterations completed, limit is 3 */
    repl->tool_iteration_count = 3;
    repl->response_finish_reason = talloc_strdup(repl, "tool_calls");

    /* Should NOT continue - we've hit the limit */
    bool should_continue = ik_repl_should_continue_tool_loop(repl);
    ck_assert(!should_continue);
}

END_TEST
/*
 * Test: Should NOT continue when over limit
 */
START_TEST(test_should_not_continue_when_over_limit)
{
    /* Set up: 4 iterations completed, limit is 3 */
    repl->tool_iteration_count = 4;
    repl->response_finish_reason = talloc_strdup(repl, "tool_calls");

    /* Should NOT continue - we're over the limit */
    bool should_continue = ik_repl_should_continue_tool_loop(repl);
    ck_assert(!should_continue);
}

END_TEST
/*
 * Test: Should NOT continue when finish_reason is not "tool_calls" even if under limit
 */
START_TEST(test_should_not_continue_when_finish_reason_is_stop)
{
    /* Set up: 1 iteration completed, limit is 3, but finish_reason is "stop" */
    repl->tool_iteration_count = 1;
    repl->response_finish_reason = talloc_strdup(repl, "stop");

    /* Should NOT continue - finish_reason is "stop" */
    bool should_continue = ik_repl_should_continue_tool_loop(repl);
    ck_assert(!should_continue);
}

END_TEST
/*
 * Test: Should continue at exactly limit-1
 */
START_TEST(test_should_continue_at_limit_minus_one)
{
    /* Set up: limit is 3, counter at 2 (one more allowed) */
    cfg->max_tool_turns = 3;
    repl->tool_iteration_count = 2;
    repl->response_finish_reason = talloc_strdup(repl, "tool_calls");

    /* Should continue */
    bool should_continue = ik_repl_should_continue_tool_loop(repl);
    ck_assert(should_continue);
}

END_TEST
/*
 * Test: Zero limit means no tool calls allowed
 */
START_TEST(test_zero_limit_means_no_tool_calls)
{
    /* Set up: limit is 0 */
    cfg->max_tool_turns = 0;
    repl->tool_iteration_count = 0;
    repl->response_finish_reason = talloc_strdup(repl, "tool_calls");

    /* Should NOT continue - limit is 0 */
    bool should_continue = ik_repl_should_continue_tool_loop(repl);
    ck_assert(!should_continue);
}

END_TEST
/*
 * Test: Negative limit (edge case - should treat as 0 or error)
 */
START_TEST(test_negative_limit)
{
    /* Set up: limit is -1 (invalid config, but we should handle gracefully) */
    cfg->max_tool_turns = -1;
    repl->tool_iteration_count = 0;
    repl->response_finish_reason = talloc_strdup(repl, "tool_calls");

    /* Should NOT continue - negative limit should be treated as invalid */
    bool should_continue = ik_repl_should_continue_tool_loop(repl);
    ck_assert(!should_continue);
}

END_TEST

/*
 * Test: Should continue when cfg is NULL (no limit enforcement)
 */
START_TEST(test_should_continue_when_cfg_is_null)
{
    /* Set up: cfg is NULL (defensive check) */
    repl->shared->cfg = NULL;
    repl->tool_iteration_count = 10;  // Any value
    repl->response_finish_reason = talloc_strdup(repl, "tool_calls");

    /* Should continue - when cfg is NULL, no limit is enforced */
    bool should_continue = ik_repl_should_continue_tool_loop(repl);
    ck_assert(should_continue);
}

END_TEST

/*
 * Test suite
 */
static Suite *tool_loop_limit_suite(void)
{
    Suite *s = suite_create("Tool Loop Limit");

    TCase *tc_core = tcase_create("Core");
    tcase_add_checked_fixture(tc_core, setup, teardown);
    tcase_add_test(tc_core, test_counter_initializes_to_zero);
    tcase_add_test(tc_core, test_counter_increments_after_tool_execution);
    tcase_add_test(tc_core, test_should_continue_when_under_limit);
    tcase_add_test(tc_core, test_should_not_continue_when_at_limit);
    tcase_add_test(tc_core, test_should_not_continue_when_over_limit);
    tcase_add_test(tc_core, test_should_not_continue_when_finish_reason_is_stop);
    tcase_add_test(tc_core, test_should_continue_at_limit_minus_one);
    tcase_add_test(tc_core, test_zero_limit_means_no_tool_calls);
    tcase_add_test(tc_core, test_negative_limit);
    tcase_add_test(tc_core, test_should_continue_when_cfg_is_null);
    suite_add_tcase(s, tc_core);

    return s;
}

int main(void)
{
    Suite *s = tool_loop_limit_suite();
    SRunner *sr = srunner_create(s);
    srunner_run_all(sr, CK_VERBOSE);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);
    return (number_failed == 0) ? 0 : 1;
}
