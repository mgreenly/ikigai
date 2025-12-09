/**
 * @file arrow_burst_test.c
 * @brief Unit tests for arrow burst detector module
 */

#include "../../../src/arrow_burst.h"
#include "../../../src/input.h"
#include "../../test_utils.h"

#include <check.h>
#include <talloc.h>

// Forward declaration for suite function
static Suite *arrow_burst_suite(void);

// Test fixture
static void *ctx;

static void setup(void)
{
    ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);
}

static void teardown(void)
{
    talloc_free(ctx);
}

// Test 1: Single arrow up, then timeout → CURSOR_UP
START_TEST(test_single_arrow_up_timeout)
{
    ik_arrow_burst_detector_t *det = ik_arrow_burst_create(ctx);

    // First arrow at T=0
    ik_arrow_burst_result_t result = ik_arrow_burst_process(det, IK_INPUT_ARROW_UP, 0);
    ck_assert_int_eq(result, IK_ARROW_BURST_RESULT_NONE);  // Buffering

    // Timeout at T=20 (past 15ms threshold)
    result = ik_arrow_burst_check_timeout(det, 20);
    ck_assert_int_eq(result, IK_ARROW_BURST_RESULT_CURSOR_UP);
}
END_TEST

// Test 2: Two rapid arrows → SCROLL
START_TEST(test_two_rapid_arrows_scroll)
{
    ik_arrow_burst_detector_t *det = ik_arrow_burst_create(ctx);

    // First arrow at T=0
    ik_arrow_burst_result_t result = ik_arrow_burst_process(det, IK_INPUT_ARROW_UP, 0);
    ck_assert_int_eq(result, IK_ARROW_BURST_RESULT_NONE);

    // Second arrow at T=5 (within 15ms)
    result = ik_arrow_burst_process(det, IK_INPUT_ARROW_UP, 5);
    ck_assert_int_eq(result, IK_ARROW_BURST_RESULT_SCROLL_UP);
}
END_TEST

// Test 3: Two slow arrows → two CURSOR moves
START_TEST(test_two_slow_arrows_cursor)
{
    ik_arrow_burst_detector_t *det = ik_arrow_burst_create(ctx);

    // First arrow at T=0
    ik_arrow_burst_result_t result = ik_arrow_burst_process(det, IK_INPUT_ARROW_UP, 0);
    ck_assert_int_eq(result, IK_ARROW_BURST_RESULT_NONE);

    // Timeout at T=20
    result = ik_arrow_burst_check_timeout(det, 20);
    ck_assert_int_eq(result, IK_ARROW_BURST_RESULT_CURSOR_UP);

    // Second arrow at T=100 (new event)
    result = ik_arrow_burst_process(det, IK_INPUT_ARROW_UP, 100);
    ck_assert_int_eq(result, IK_ARROW_BURST_RESULT_NONE);

    // Timeout at T=120
    result = ik_arrow_burst_check_timeout(det, 120);
    ck_assert_int_eq(result, IK_ARROW_BURST_RESULT_CURSOR_UP);
}
END_TEST

// Test 4: Burst of 5 arrows → multiple SCROLL results
START_TEST(test_burst_of_five)
{
    ik_arrow_burst_detector_t *det = ik_arrow_burst_create(ctx);

    ik_arrow_burst_result_t r;
    r = ik_arrow_burst_process(det, IK_INPUT_ARROW_DOWN, 0);
    ck_assert_int_eq(r, IK_ARROW_BURST_RESULT_NONE);

    r = ik_arrow_burst_process(det, IK_INPUT_ARROW_DOWN, 3);
    ck_assert_int_eq(r, IK_ARROW_BURST_RESULT_SCROLL_DOWN);

    r = ik_arrow_burst_process(det, IK_INPUT_ARROW_DOWN, 6);
    ck_assert_int_eq(r, IK_ARROW_BURST_RESULT_SCROLL_DOWN);

    r = ik_arrow_burst_process(det, IK_INPUT_ARROW_DOWN, 9);
    ck_assert_int_eq(r, IK_ARROW_BURST_RESULT_SCROLL_DOWN);

    r = ik_arrow_burst_process(det, IK_INPUT_ARROW_DOWN, 12);
    ck_assert_int_eq(r, IK_ARROW_BURST_RESULT_SCROLL_DOWN);
}
END_TEST

// Test 5: Direction change mid-buffer → emit cursor for first
START_TEST(test_direction_change)
{
    ik_arrow_burst_detector_t *det = ik_arrow_burst_create(ctx);

    // Arrow up at T=0
    ik_arrow_burst_result_t r = ik_arrow_burst_process(det, IK_INPUT_ARROW_UP, 0);
    ck_assert_int_eq(r, IK_ARROW_BURST_RESULT_NONE);

    // Arrow DOWN at T=5 (different direction)
    r = ik_arrow_burst_process(det, IK_INPUT_ARROW_DOWN, 5);
    ck_assert_int_eq(r, IK_ARROW_BURST_RESULT_CURSOR_UP);  // Emit previous

    // Now buffering down, timeout
    r = ik_arrow_burst_check_timeout(det, 25);
    ck_assert_int_eq(r, IK_ARROW_BURST_RESULT_CURSOR_DOWN);
}
END_TEST

// Test 6: get_timeout_ms returns correct remaining time
START_TEST(test_get_timeout)
{
    ik_arrow_burst_detector_t *det = ik_arrow_burst_create(ctx);

    // No event → no timeout
    int64_t timeout = ik_arrow_burst_get_timeout_ms(det, 0);
    ck_assert_int_eq(timeout, -1);

    // Event at T=0 → timeout at T=15
    ik_arrow_burst_process(det, IK_INPUT_ARROW_UP, 0);

    timeout = ik_arrow_burst_get_timeout_ms(det, 0);
    ck_assert_int_eq(timeout, 15);

    timeout = ik_arrow_burst_get_timeout_ms(det, 10);
    ck_assert_int_eq(timeout, 5);

    timeout = ik_arrow_burst_get_timeout_ms(det, 15);
    ck_assert_int_eq(timeout, 0);

    timeout = ik_arrow_burst_get_timeout_ms(det, 20);
    ck_assert_int_eq(timeout, 0);  // Already expired
}
END_TEST

// Test 7: Arrow outside threshold starts new buffer
START_TEST(test_outside_threshold_new_buffer)
{
    ik_arrow_burst_detector_t *det = ik_arrow_burst_create(ctx);

    // Arrow at T=0
    ik_arrow_burst_result_t r = ik_arrow_burst_process(det, IK_INPUT_ARROW_UP, 0);
    ck_assert_int_eq(r, IK_ARROW_BURST_RESULT_NONE);

    // Arrow at T=50 (outside threshold, same direction)
    // Should emit CURSOR_UP for previous and start new buffering
    r = ik_arrow_burst_process(det, IK_INPUT_ARROW_UP, 50);
    ck_assert_int_eq(r, IK_ARROW_BURST_RESULT_CURSOR_UP);

    // Now in BUFFERING for new event, timeout
    r = ik_arrow_burst_check_timeout(det, 70);
    ck_assert_int_eq(r, IK_ARROW_BURST_RESULT_CURSOR_UP);
}
END_TEST

// Test 8: Reset clears state
START_TEST(test_reset)
{
    ik_arrow_burst_detector_t *det = ik_arrow_burst_create(ctx);

    ik_arrow_burst_process(det, IK_INPUT_ARROW_UP, 0);
    ik_arrow_burst_reset(det);

    // Should be idle, no timeout
    int64_t timeout = ik_arrow_burst_get_timeout_ms(det, 10);
    ck_assert_int_eq(timeout, -1);
}
END_TEST

// Test 10: Scroll continues to emit for each additional burst event
START_TEST(test_continued_scroll_burst)
{
    ik_arrow_burst_detector_t *det = ik_arrow_burst_create(ctx);

    // Start burst
    ik_arrow_burst_process(det, IK_INPUT_ARROW_UP, 0);
    ik_arrow_burst_result_t r = ik_arrow_burst_process(det, IK_INPUT_ARROW_UP, 3);
    ck_assert_int_eq(r, IK_ARROW_BURST_RESULT_SCROLL_UP);

    // Burst continues - each new rapid event should scroll
    r = ik_arrow_burst_process(det, IK_INPUT_ARROW_UP, 6);
    ck_assert_int_eq(r, IK_ARROW_BURST_RESULT_SCROLL_UP);
}
END_TEST

// Test 11: Check timeout when detector is idle
START_TEST(test_check_timeout_when_idle)
{
    ik_arrow_burst_detector_t *det = ik_arrow_burst_create(ctx);

    // Detector is idle - check_timeout should return NONE
    ik_arrow_burst_result_t r = ik_arrow_burst_check_timeout(det, 100);
    ck_assert_int_eq(r, IK_ARROW_BURST_RESULT_NONE);
}
END_TEST

// Test 12: Check timeout before threshold is reached
START_TEST(test_check_timeout_before_threshold)
{
    ik_arrow_burst_detector_t *det = ik_arrow_burst_create(ctx);

    // Process arrow at T=0
    ik_arrow_burst_process(det, IK_INPUT_ARROW_UP, 0);

    // Check timeout at T=10 (before 15ms threshold)
    ik_arrow_burst_result_t r = ik_arrow_burst_check_timeout(det, 10);
    ck_assert_int_eq(r, IK_ARROW_BURST_RESULT_NONE);

    // Should still be buffering, timeout at T=20 should work
    r = ik_arrow_burst_check_timeout(det, 20);
    ck_assert_int_eq(r, IK_ARROW_BURST_RESULT_CURSOR_UP);
}
END_TEST

// Suite definition
static Suite *arrow_burst_suite(void)
{
    Suite *s;
    TCase *tc_core;

    s = suite_create("ArrowBurst");

    tc_core = tcase_create("Core");

    tcase_add_checked_fixture(tc_core, setup, teardown);
    tcase_add_test(tc_core, test_single_arrow_up_timeout);
    tcase_add_test(tc_core, test_two_rapid_arrows_scroll);
    tcase_add_test(tc_core, test_two_slow_arrows_cursor);
    tcase_add_test(tc_core, test_burst_of_five);
    tcase_add_test(tc_core, test_direction_change);
    tcase_add_test(tc_core, test_get_timeout);
    tcase_add_test(tc_core, test_outside_threshold_new_buffer);
    tcase_add_test(tc_core, test_reset);
    tcase_add_test(tc_core, test_continued_scroll_burst);
    tcase_add_test(tc_core, test_check_timeout_when_idle);
    tcase_add_test(tc_core, test_check_timeout_before_threshold);

    suite_add_tcase(s, tc_core);

    return s;
}

int main(void)
{
    int number_failed;
    Suite *s;
    SRunner *sr;

    s = arrow_burst_suite();
    sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
