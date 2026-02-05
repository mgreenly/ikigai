#include "tests/test_constants.h"
#include "apps/ikigai/agent.h"
/**
 * @file repl_tool_loop_continuation_test.c
 * @brief Unit tests for REPL tool loop continuation check
 *
 * Tests the ik_repl_should_continue_tool_loop function which determines
 * whether to continue the tool loop based on finish_reason.
 */

#include "apps/ikigai/repl.h"
#include "apps/ikigai/agent.h"
#include "apps/ikigai/config.h"
#include "apps/ikigai/shared.h"
#include "apps/ikigai/scrollback.h"
#include <check.h>
#include <talloc.h>

static void *ctx;
static ik_repl_ctx_t *repl;

static void setup(void)
{
    ctx = talloc_new(NULL);

    /* Create minimal REPL context for testing */
    repl = talloc_zero(ctx, ik_repl_ctx_t);
    repl->current = talloc_zero(repl, ik_agent_ctx_t);

    /* Create shared context */
    ik_shared_ctx_t *shared = talloc_zero(ctx, ik_shared_ctx_t);
    shared->cfg = talloc_zero(ctx, ik_config_t);
    shared->cfg->max_tool_turns = 10;  /* Set reasonable limit */
    repl->shared = shared;

    /* Create agent context for display state */
    ik_agent_ctx_t *agent = talloc_zero(repl, ik_agent_ctx_t);
    repl->current = agent;
    agent->repl = repl;  // Set backpointer

    repl->current->scrollback = ik_scrollback_create(repl, 80);
    repl->current->response_finish_reason = NULL;
    repl->current->tool_iteration_count = 0;  /* Initialize iteration count */
}

static void teardown(void)
{
    talloc_free(ctx);
}

/*
 * Test: Should continue when finish_reason is "tool_use"
 */
START_TEST(test_should_continue_with_tool_calls) {
    /* Set finish_reason to "tool_use" (normalized internal value) */
    repl->current->response_finish_reason = talloc_strdup(repl, "tool_use");

    /* Should return true */
    bool should_continue = ik_agent_should_continue_tool_loop(repl->current);
    ck_assert(should_continue);
}

END_TEST
/*
 * Test: Should not continue when finish_reason is "stop"
 */
START_TEST(test_should_not_continue_with_stop) {
    /* Set finish_reason to "stop" */
    repl->current->response_finish_reason = talloc_strdup(repl, "stop");

    /* Should return false */
    bool should_continue = ik_agent_should_continue_tool_loop(repl->current);
    ck_assert(!should_continue);
}

END_TEST
/*
 * Test: Should not continue when finish_reason is "length"
 */
START_TEST(test_should_not_continue_with_length) {
    /* Set finish_reason to "length" */
    repl->current->response_finish_reason = talloc_strdup(repl, "length");

    /* Should return false */
    bool should_continue = ik_agent_should_continue_tool_loop(repl->current);
    ck_assert(!should_continue);
}

END_TEST
/*
 * Test: Should not continue when finish_reason is NULL
 */
START_TEST(test_should_not_continue_with_null) {
    /* finish_reason is NULL */
    repl->current->response_finish_reason = NULL;

    /* Should return false */
    bool should_continue = ik_agent_should_continue_tool_loop(repl->current);
    ck_assert(!should_continue);
}

END_TEST
/*
 * Test: Should not continue when finish_reason is empty string
 */
START_TEST(test_should_not_continue_with_empty_string) {
    /* Set finish_reason to empty string */
    repl->current->response_finish_reason = talloc_strdup(repl, "");

    /* Should return false */
    bool should_continue = ik_agent_should_continue_tool_loop(repl->current);
    ck_assert(!should_continue);
}

END_TEST
/*
 * Test: Should not continue with unknown finish_reason
 */
START_TEST(test_should_not_continue_with_unknown) {
    /* Set finish_reason to unknown value */
    repl->current->response_finish_reason = talloc_strdup(repl, "content_filter");

    /* Should return false */
    bool should_continue = ik_agent_should_continue_tool_loop(repl->current);
    ck_assert(!should_continue);
}

END_TEST

/*
 * Test suite
 */
static Suite *repl_tool_loop_continuation_suite(void)
{
    Suite *s = suite_create("REPL Tool Loop Continuation");

    TCase *tc_core = tcase_create("Core");
    tcase_set_timeout(tc_core, IK_TEST_TIMEOUT);
    tcase_set_timeout(tc_core, IK_TEST_TIMEOUT);
    tcase_set_timeout(tc_core, IK_TEST_TIMEOUT);
    tcase_set_timeout(tc_core, IK_TEST_TIMEOUT);
    tcase_set_timeout(tc_core, IK_TEST_TIMEOUT);
    tcase_add_checked_fixture(tc_core, setup, teardown);
    tcase_add_test(tc_core, test_should_continue_with_tool_calls);
    tcase_add_test(tc_core, test_should_not_continue_with_stop);
    tcase_add_test(tc_core, test_should_not_continue_with_length);
    tcase_add_test(tc_core, test_should_not_continue_with_null);
    tcase_add_test(tc_core, test_should_not_continue_with_empty_string);
    tcase_add_test(tc_core, test_should_not_continue_with_unknown);
    suite_add_tcase(s, tc_core);

    return s;
}

int main(void)
{
    Suite *s = repl_tool_loop_continuation_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/apps/ikigai/repl/repl_tool_loop_continuation_test.xml");
    srunner_run_all(sr, CK_VERBOSE);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);
    return (number_failed == 0) ? 0 : 1;
}
