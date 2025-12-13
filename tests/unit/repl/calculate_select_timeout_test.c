#include <check.h>
#include <talloc.h>
#include <pthread.h>
#include "agent.h"
#include "repl.h"
#include "repl_event_handlers.h"
#include "scroll_detector.h"
#include "wrapper.h"

/* Test fixtures */
static TALLOC_CTX *ctx = NULL;
static ik_repl_ctx_t *repl = NULL;

static void setup(void)
{
    ctx = talloc_new(NULL);
    repl = talloc_zero(ctx, ik_repl_ctx_t);

    /* Initialize thread infrastructure */
    pthread_mutex_init_(&repl->tool_thread_mutex, NULL);
    repl->tool_thread_running = false;
    repl->tool_thread_complete = false;

    /* Initialize spinner state */
    repl->spinner_state.visible = false;
    repl->spinner_state.frame_index = 0;

    /* Initialize current agent context (minimal setup for testing) */
    repl->current = talloc_zero(repl, ik_agent_ctx_t);
    repl->current->state = IK_AGENT_STATE_IDLE;

    /* Initialize scroll detector (rel-05) */
    repl->scroll_det = ik_scroll_detector_create(repl);
}

static void teardown(void)
{
    if (repl != NULL) {
        pthread_mutex_destroy_(&repl->tool_thread_mutex);
    }
    talloc_free(ctx);
    ctx = NULL;
    repl = NULL;
}

/*
 * Test: No timeouts active (all -1) - should return 1000ms default
 */
START_TEST(test_calculate_timeout_all_disabled) {
    /* All timeouts disabled */
    repl->spinner_state.visible = false;  // spinner_timeout = -1
    repl->current->state = IK_AGENT_STATE_IDLE;     // tool_poll_timeout = -1
    long curl_timeout_ms = -1;

    long timeout = calculate_select_timeout_ms(repl, curl_timeout_ms);

    /* Should return default 1000ms when all timeouts are -1 */
    ck_assert_int_eq(timeout, 1000);
}
END_TEST
/*
 * Test: Single valid timeout (spinner only)
 * Covers: min_timeout < 0 being true (first timeout found)
 */
START_TEST(test_calculate_timeout_spinner_only)
{
    /* Only spinner timeout active */
    repl->spinner_state.visible = true;   // spinner_timeout = 80
    repl->current->state = IK_AGENT_STATE_IDLE;     // tool_poll_timeout = -1
    long curl_timeout_ms = -1;

    long timeout = calculate_select_timeout_ms(repl, curl_timeout_ms);

    /* Should return spinner timeout (80ms) */
    ck_assert_int_eq(timeout, 80);
}

END_TEST
/*
 * Test: Single valid timeout (curl only)
 * Covers: min_timeout < 0 being true (first timeout found)
 */
START_TEST(test_calculate_timeout_curl_only)
{
    /* Only curl timeout active */
    repl->spinner_state.visible = false;  // spinner_timeout = -1
    repl->current->state = IK_AGENT_STATE_IDLE;     // tool_poll_timeout = -1
    long curl_timeout_ms = 500;

    long timeout = calculate_select_timeout_ms(repl, curl_timeout_ms);

    /* Should return curl timeout (500ms) */
    ck_assert_int_eq(timeout, 500);
}

END_TEST
/*
 * Test: Single valid timeout (tool poll only)
 * Covers: min_timeout < 0 being true (first timeout found)
 */
START_TEST(test_calculate_timeout_tool_poll_only)
{
    /* Only tool poll timeout active */
    repl->spinner_state.visible = false;            // spinner_timeout = -1
    repl->current->state = IK_AGENT_STATE_EXECUTING_TOOL;     // tool_poll_timeout = 50
    long curl_timeout_ms = -1;

    long timeout = calculate_select_timeout_ms(repl, curl_timeout_ms);

    /* Should return tool poll timeout (50ms) */
    ck_assert_int_eq(timeout, 50);
}

END_TEST
/*
 * Test: Multiple timeouts with decreasing values (80, 50)
 * Covers: min_timeout < 0 being false AND timeouts[i] < min_timeout being true
 */
START_TEST(test_calculate_timeout_decreasing_spinner_tool)
{
    /* Spinner (80ms) and tool poll (50ms) both active */
    repl->spinner_state.visible = true;             // spinner_timeout = 80
    repl->current->state = IK_AGENT_STATE_EXECUTING_TOOL;     // tool_poll_timeout = 50
    long curl_timeout_ms = -1;

    long timeout = calculate_select_timeout_ms(repl, curl_timeout_ms);

    /* Should return minimum (50ms) */
    ck_assert_int_eq(timeout, 50);
}

END_TEST
/*
 * Test: Multiple timeouts with decreasing values (80, 25)
 * Covers: min_timeout < 0 being false AND timeouts[i] < min_timeout being true
 */
START_TEST(test_calculate_timeout_decreasing_spinner_curl)
{
    /* Spinner (80ms) and curl (25ms) both active */
    repl->spinner_state.visible = true;   // spinner_timeout = 80
    repl->current->state = IK_AGENT_STATE_IDLE;     // tool_poll_timeout = -1
    long curl_timeout_ms = 25;

    long timeout = calculate_select_timeout_ms(repl, curl_timeout_ms);

    /* Should return minimum (25ms) */
    ck_assert_int_eq(timeout, 25);
}

END_TEST
/*
 * Test: Multiple timeouts - later one NOT smaller
 * Covers: min_timeout < 0 being false AND timeouts[i] < min_timeout being false
 */
START_TEST(test_calculate_timeout_increasing_spinner_curl)
{
    /* Spinner (80ms) and curl (500ms) both active */
    repl->spinner_state.visible = true;   // spinner_timeout = 80
    repl->current->state = IK_AGENT_STATE_IDLE;     // tool_poll_timeout = -1
    long curl_timeout_ms = 500;

    long timeout = calculate_select_timeout_ms(repl, curl_timeout_ms);

    /* Should return minimum (80ms), not the later larger value */
    ck_assert_int_eq(timeout, 80);
}

END_TEST
/*
 * Test: Multiple timeouts - later one NOT smaller (tool poll first)
 * Covers: min_timeout < 0 being false AND timeouts[i] < min_timeout being false
 */
START_TEST(test_calculate_timeout_increasing_curl_tool)
{
    /* Curl (100ms) comes first, tool poll (50ms) comes later */
    repl->spinner_state.visible = false;            // spinner_timeout = -1
    repl->current->state = IK_AGENT_STATE_EXECUTING_TOOL;     // tool_poll_timeout = 50
    long curl_timeout_ms = 100;

    long timeout = calculate_select_timeout_ms(repl, curl_timeout_ms);

    /* Should return minimum (50ms) */
    ck_assert_int_eq(timeout, 50);
}

END_TEST
/*
 * Test: All three timeouts active with decreasing values
 * Covers: multiple iterations of both branches
 */
START_TEST(test_calculate_timeout_all_active_decreasing)
{
    /* All timeouts active: spinner (80), curl (60), tool (50) */
    repl->spinner_state.visible = true;             // spinner_timeout = 80
    repl->current->state = IK_AGENT_STATE_EXECUTING_TOOL;     // tool_poll_timeout = 50
    long curl_timeout_ms = 60;

    long timeout = calculate_select_timeout_ms(repl, curl_timeout_ms);

    /* Should return minimum (50ms) */
    ck_assert_int_eq(timeout, 50);
}

END_TEST
/*
 * Test: All three timeouts active with mixed values
 * Covers: cases where later timeouts are NOT smaller
 */
START_TEST(test_calculate_timeout_all_active_mixed)
{
    /* All timeouts active: spinner (80), curl (100), tool (50) */
    repl->spinner_state.visible = true;             // spinner_timeout = 80
    repl->current->state = IK_AGENT_STATE_EXECUTING_TOOL;     // tool_poll_timeout = 50
    long curl_timeout_ms = 100;

    long timeout = calculate_select_timeout_ms(repl, curl_timeout_ms);

    /* Should return minimum (50ms) */
    ck_assert_int_eq(timeout, 50);
}

END_TEST
/*
 * Test: Mix of disabled and valid timeouts (spinner + curl)
 */
START_TEST(test_calculate_timeout_mixed_disabled_spinner_curl)
{
    /* Spinner (80ms) and curl (200ms), tool disabled */
    repl->spinner_state.visible = true;   // spinner_timeout = 80
    repl->current->state = IK_AGENT_STATE_IDLE;     // tool_poll_timeout = -1
    long curl_timeout_ms = 200;

    long timeout = calculate_select_timeout_ms(repl, curl_timeout_ms);

    /* Should return minimum (80ms) */
    ck_assert_int_eq(timeout, 80);
}

END_TEST
/*
 * Test: Mix of disabled and valid timeouts (spinner + tool)
 */
START_TEST(test_calculate_timeout_mixed_disabled_spinner_tool)
{
    /* Spinner (80ms) and tool (50ms), curl disabled */
    repl->spinner_state.visible = true;             // spinner_timeout = 80
    repl->current->state = IK_AGENT_STATE_EXECUTING_TOOL;     // tool_poll_timeout = 50
    long curl_timeout_ms = -1;

    long timeout = calculate_select_timeout_ms(repl, curl_timeout_ms);

    /* Should return minimum (50ms) */
    ck_assert_int_eq(timeout, 50);
}

END_TEST
/*
 * Test: Mix of disabled and valid timeouts (curl + tool)
 */
START_TEST(test_calculate_timeout_mixed_disabled_curl_tool)
{
    /* Curl (300ms) and tool (50ms), spinner disabled */
    repl->spinner_state.visible = false;            // spinner_timeout = -1
    repl->current->state = IK_AGENT_STATE_EXECUTING_TOOL;     // tool_poll_timeout = 50
    long curl_timeout_ms = 300;

    long timeout = calculate_select_timeout_ms(repl, curl_timeout_ms);

    /* Should return minimum (50ms) */
    ck_assert_int_eq(timeout, 50);
}

END_TEST
/*
 * Test: WAITING_FOR_LLM state (no tool poll timeout)
 */
START_TEST(test_calculate_timeout_waiting_for_llm)
{
    /* Spinner visible, waiting for LLM (no tool poll) */
    repl->spinner_state.visible = true;             // spinner_timeout = 80
    repl->current->state = IK_AGENT_STATE_WAITING_FOR_LLM;    // tool_poll_timeout = -1
    long curl_timeout_ms = 100;

    long timeout = calculate_select_timeout_ms(repl, curl_timeout_ms);

    /* Should return minimum of spinner and curl (80ms) */
    ck_assert_int_eq(timeout, 80);
}

END_TEST
/*
 * Test: Zero curl timeout (edge case)
 */
START_TEST(test_calculate_timeout_zero_curl)
{
    /* Zero timeout is valid and should be chosen */
    repl->spinner_state.visible = true;   // spinner_timeout = 80
    repl->current->state = IK_AGENT_STATE_IDLE;     // tool_poll_timeout = -1
    long curl_timeout_ms = 0;

    long timeout = calculate_select_timeout_ms(repl, curl_timeout_ms);

    /* Should return 0 (immediate return) */
    ck_assert_int_eq(timeout, 0);
}

END_TEST

/*
 * Test suite
 */
static Suite *calculate_select_timeout_suite(void)
{
    Suite *s = suite_create("Calculate Select Timeout");
    TCase *tc_core = tcase_create("Core");

    tcase_add_checked_fixture(tc_core, setup, teardown);

    /* All disabled */
    tcase_add_test(tc_core, test_calculate_timeout_all_disabled);

    /* Single timeout (covers min_timeout < 0 being true) */
    tcase_add_test(tc_core, test_calculate_timeout_spinner_only);
    tcase_add_test(tc_core, test_calculate_timeout_curl_only);
    tcase_add_test(tc_core, test_calculate_timeout_tool_poll_only);

    /* Multiple timeouts - decreasing (covers timeouts[i] < min_timeout being true) */
    tcase_add_test(tc_core, test_calculate_timeout_decreasing_spinner_tool);
    tcase_add_test(tc_core, test_calculate_timeout_decreasing_spinner_curl);

    /* Multiple timeouts - increasing (covers timeouts[i] < min_timeout being false) */
    tcase_add_test(tc_core, test_calculate_timeout_increasing_spinner_curl);
    tcase_add_test(tc_core, test_calculate_timeout_increasing_curl_tool);

    /* All three active */
    tcase_add_test(tc_core, test_calculate_timeout_all_active_decreasing);
    tcase_add_test(tc_core, test_calculate_timeout_all_active_mixed);

    /* Mixed disabled/enabled */
    tcase_add_test(tc_core, test_calculate_timeout_mixed_disabled_spinner_curl);
    tcase_add_test(tc_core, test_calculate_timeout_mixed_disabled_spinner_tool);
    tcase_add_test(tc_core, test_calculate_timeout_mixed_disabled_curl_tool);

    /* State variations */
    tcase_add_test(tc_core, test_calculate_timeout_waiting_for_llm);

    /* Edge cases */
    tcase_add_test(tc_core, test_calculate_timeout_zero_curl);

    suite_add_tcase(s, tc_core);

    return s;
}

int main(void)
{
    Suite *s = calculate_select_timeout_suite();
    SRunner *sr = srunner_create(s);
    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);
    return (number_failed == 0) ? 0 : 1;
}
