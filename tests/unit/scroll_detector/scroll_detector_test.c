/**
 * @file scroll_detector_test.c
 * @brief Unit tests for scroll detector module (deferred detection)
 */

#include "../../../src/scroll_detector.h"
#include "../../../src/input.h"
#include "../../test_utils.h"

#include <check.h>
#include <talloc.h>

// Forward declaration for suite function
static Suite *scroll_detector_suite(void);

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

// Test 1: First arrow is buffered (returns NONE)
START_TEST(test_first_arrow_buffered)
{
    ik_scroll_detector_t *det = ik_scroll_detector_create(ctx);

    ik_scroll_result_t r = ik_scroll_detector_process_arrow(
        det, IK_INPUT_ARROW_UP, 1000);
    ck_assert_int_eq(r, IK_SCROLL_RESULT_NONE);
}
END_TEST

// Test 2: Rapid second arrow emits SCROLL
START_TEST(test_rapid_second_arrow_emits_scroll)
{
    ik_scroll_detector_t *det = ik_scroll_detector_create(ctx);

    ik_scroll_detector_process_arrow(det, IK_INPUT_ARROW_UP, 1000);
    ik_scroll_result_t r = ik_scroll_detector_process_arrow(
        det, IK_INPUT_ARROW_UP, 1001);  // 1ms later
    ck_assert_int_eq(r, IK_SCROLL_RESULT_SCROLL_UP);
}
END_TEST

// Test 3: Slow second arrow emits ARROW for first, buffers second
START_TEST(test_slow_second_arrow_emits_arrow)
{
    ik_scroll_detector_t *det = ik_scroll_detector_create(ctx);

    ik_scroll_detector_process_arrow(det, IK_INPUT_ARROW_UP, 1000);
    ik_scroll_result_t r = ik_scroll_detector_process_arrow(
        det, IK_INPUT_ARROW_UP, 1030);  // 30ms later
    ck_assert_int_eq(r, IK_SCROLL_RESULT_ARROW_UP);
}
END_TEST

// Test 4: Timeout flushes pending as ARROW
START_TEST(test_timeout_flushes_arrow)
{
    ik_scroll_detector_t *det = ik_scroll_detector_create(ctx);

    ik_scroll_detector_process_arrow(det, IK_INPUT_ARROW_UP, 1000);
    ik_scroll_result_t r = ik_scroll_detector_check_timeout(det, 1015);
    ck_assert_int_eq(r, IK_SCROLL_RESULT_ARROW_UP);
}
END_TEST

// Test 5: Timeout before threshold returns NONE
START_TEST(test_timeout_before_threshold_returns_none)
{
    ik_scroll_detector_t *det = ik_scroll_detector_create(ctx);

    ik_scroll_detector_process_arrow(det, IK_INPUT_ARROW_UP, 1000);
    ik_scroll_result_t r = ik_scroll_detector_check_timeout(det, 1005);
    ck_assert_int_eq(r, IK_SCROLL_RESULT_NONE);
}
END_TEST

// Test 6: get_timeout_ms returns correct value
START_TEST(test_get_timeout_ms)
{
    ik_scroll_detector_t *det = ik_scroll_detector_create(ctx);

    // No pending - returns -1
    int64_t t = ik_scroll_detector_get_timeout_ms(det, 1000);
    ck_assert_int_eq(t, -1);

    // With pending at t=1000, check at t=1003 - should return 7ms
    ik_scroll_detector_process_arrow(det, IK_INPUT_ARROW_UP, 1000);
    t = ik_scroll_detector_get_timeout_ms(det, 1003);
    ck_assert_int_eq(t, 7);

    // At t=1015 - already expired, return 0
    t = ik_scroll_detector_get_timeout_ms(det, 1015);
    ck_assert_int_eq(t, 0);
}
END_TEST

// Test 7: flush() emits pending ARROW
START_TEST(test_flush_emits_arrow)
{
    ik_scroll_detector_t *det = ik_scroll_detector_create(ctx);

    ik_scroll_detector_process_arrow(det, IK_INPUT_ARROW_DOWN, 1000);
    ik_scroll_result_t r = ik_scroll_detector_flush(det);
    ck_assert_int_eq(r, IK_SCROLL_RESULT_ARROW_DOWN);

    // Second flush returns NONE
    r = ik_scroll_detector_flush(det);
    ck_assert_int_eq(r, IK_SCROLL_RESULT_NONE);
}
END_TEST

// Test 8: Scroll direction preserved
START_TEST(test_scroll_direction)
{
    ik_scroll_detector_t *det = ik_scroll_detector_create(ctx);

    ik_scroll_detector_process_arrow(det, IK_INPUT_ARROW_DOWN, 1000);
    ik_scroll_result_t r = ik_scroll_detector_process_arrow(
        det, IK_INPUT_ARROW_DOWN, 1001);
    ck_assert_int_eq(r, IK_SCROLL_RESULT_SCROLL_DOWN);
}
END_TEST

// Test 9: Mixed directions - each burst independent
START_TEST(test_mixed_directions)
{
    ik_scroll_detector_t *det = ik_scroll_detector_create(ctx);

    // Up burst
    ik_scroll_detector_process_arrow(det, IK_INPUT_ARROW_UP, 1000);
    ik_scroll_result_t r = ik_scroll_detector_process_arrow(
        det, IK_INPUT_ARROW_UP, 1001);
    ck_assert_int_eq(r, IK_SCROLL_RESULT_SCROLL_UP);

    // Wait, then down burst
    ik_scroll_detector_check_timeout(det, 1050);  // Flush any pending
    ik_scroll_detector_process_arrow(det, IK_INPUT_ARROW_DOWN, 1100);
    r = ik_scroll_detector_process_arrow(det, IK_INPUT_ARROW_DOWN, 1101);
    ck_assert_int_eq(r, IK_SCROLL_RESULT_SCROLL_DOWN);
}
END_TEST

// Test 10: Reset clears pending
START_TEST(test_reset_clears_pending)
{
    ik_scroll_detector_t *det = ik_scroll_detector_create(ctx);

    ik_scroll_detector_process_arrow(det, IK_INPUT_ARROW_UP, 1000);
    ik_scroll_detector_reset(det);

    // Timeout should return NONE (nothing pending)
    ik_scroll_result_t r = ik_scroll_detector_check_timeout(det, 1020);
    ck_assert_int_eq(r, IK_SCROLL_RESULT_NONE);
}
END_TEST

// Test 11: Continuous scroll (rapid second event also buffers)
START_TEST(test_continuous_scroll)
{
    ik_scroll_detector_t *det = ik_scroll_detector_create(ctx);

    // Arrow 1
    ik_scroll_result_t r = ik_scroll_detector_process_arrow(
        det, IK_INPUT_ARROW_UP, 1000);
    ck_assert_int_eq(r, IK_SCROLL_RESULT_NONE);  // buffered

    // Arrow 2 rapid - emits SCROLL, buffers Arrow 2
    r = ik_scroll_detector_process_arrow(det, IK_INPUT_ARROW_UP, 1001);
    ck_assert_int_eq(r, IK_SCROLL_RESULT_SCROLL_UP);

    // Arrow 3 rapid - emits SCROLL, buffers Arrow 3
    r = ik_scroll_detector_process_arrow(det, IK_INPUT_ARROW_UP, 1002);
    ck_assert_int_eq(r, IK_SCROLL_RESULT_SCROLL_UP);

    // Timeout flushes last pending arrow
    r = ik_scroll_detector_check_timeout(det, 1020);
    ck_assert_int_eq(r, IK_SCROLL_RESULT_ARROW_UP);
}
END_TEST

// Test 12: Key repeat (30ms intervals) - each emits ARROW
START_TEST(test_key_repeat)
{
    ik_scroll_detector_t *det = ik_scroll_detector_create(ctx);

    ik_scroll_result_t r;

    // First arrow buffered
    r = ik_scroll_detector_process_arrow(det, IK_INPUT_ARROW_UP, 1000);
    ck_assert_int_eq(r, IK_SCROLL_RESULT_NONE);

    // Second arrow 30ms later - slow, emits ARROW for first
    r = ik_scroll_detector_process_arrow(det, IK_INPUT_ARROW_UP, 1030);
    ck_assert_int_eq(r, IK_SCROLL_RESULT_ARROW_UP);

    // Third arrow 30ms later
    r = ik_scroll_detector_process_arrow(det, IK_INPUT_ARROW_UP, 1060);
    ck_assert_int_eq(r, IK_SCROLL_RESULT_ARROW_UP);

    // Flush last pending
    r = ik_scroll_detector_check_timeout(det, 1080);
    ck_assert_int_eq(r, IK_SCROLL_RESULT_ARROW_UP);
}
END_TEST

// Test 13: Exactly at threshold (10ms)
START_TEST(test_at_threshold)
{
    ik_scroll_detector_t *det = ik_scroll_detector_create(ctx);

    ik_scroll_detector_process_arrow(det, IK_INPUT_ARROW_UP, 1000);
    ik_scroll_result_t r = ik_scroll_detector_process_arrow(
        det, IK_INPUT_ARROW_UP, 1010);  // exactly 10ms
    // Spec says "<= 10ms" is burst
    ck_assert_int_eq(r, IK_SCROLL_RESULT_SCROLL_UP);
}
END_TEST

// Test 14: Just above threshold (11ms)
START_TEST(test_above_threshold)
{
    ik_scroll_detector_t *det = ik_scroll_detector_create(ctx);

    ik_scroll_detector_process_arrow(det, IK_INPUT_ARROW_UP, 1000);
    ik_scroll_result_t r = ik_scroll_detector_process_arrow(
        det, IK_INPUT_ARROW_UP, 1011);  // 11ms - above threshold
    ck_assert_int_eq(r, IK_SCROLL_RESULT_ARROW_UP);
}
END_TEST

// Suite definition
static Suite *scroll_detector_suite(void)
{
    Suite *s;
    TCase *tc_core;

    s = suite_create("ScrollAccumulator");

    tc_core = tcase_create("Core");

    tcase_add_checked_fixture(tc_core, setup, teardown);
    tcase_add_test(tc_core, test_first_arrow_buffered);
    tcase_add_test(tc_core, test_rapid_second_arrow_emits_scroll);
    tcase_add_test(tc_core, test_slow_second_arrow_emits_arrow);
    tcase_add_test(tc_core, test_timeout_flushes_arrow);
    tcase_add_test(tc_core, test_timeout_before_threshold_returns_none);
    tcase_add_test(tc_core, test_get_timeout_ms);
    tcase_add_test(tc_core, test_flush_emits_arrow);
    tcase_add_test(tc_core, test_scroll_direction);
    tcase_add_test(tc_core, test_mixed_directions);
    tcase_add_test(tc_core, test_reset_clears_pending);
    tcase_add_test(tc_core, test_continuous_scroll);
    tcase_add_test(tc_core, test_key_repeat);
    tcase_add_test(tc_core, test_at_threshold);
    tcase_add_test(tc_core, test_above_threshold);

    suite_add_tcase(s, tc_core);

    return s;
}

int main(void)
{
    int number_failed;
    Suite *s;
    SRunner *sr;

    s = scroll_detector_suite();
    sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
