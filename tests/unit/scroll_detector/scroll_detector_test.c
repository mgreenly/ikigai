/**
 * @file scroll_detector_test.c
 * @brief Unit tests for scroll detector module (state machine: IDLE, WAITING, ABSORBING)
 */

#include "../../../src/scroll_detector.h"
#include "../../../src/input.h"
#include "../../test_utils_helper.h"

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

// Test 1: First arrow is buffered (returns NONE, transitions to WAITING)
START_TEST(test_first_arrow_buffered) {
    ik_scroll_detector_t *det = ik_scroll_detector_create(ctx);

    ik_scroll_result_t r = ik_scroll_detector_process_arrow(
        det, IK_INPUT_ARROW_UP, 1000);
    ck_assert_int_eq(r, IK_SCROLL_RESULT_NONE);
    ck_assert_int_eq(det->state, IK_SCROLL_STATE_WAITING);
}
END_TEST
// Test 2: Rapid second arrow emits SCROLL, transitions to ABSORBING
START_TEST(test_rapid_second_arrow_emits_scroll) {
    ik_scroll_detector_t *det = ik_scroll_detector_create(ctx);

    ik_scroll_detector_process_arrow(det, IK_INPUT_ARROW_UP, 1000);
    ik_scroll_result_t r = ik_scroll_detector_process_arrow(
        det, IK_INPUT_ARROW_UP, 1001);  // 1ms later
    ck_assert_int_eq(r, IK_SCROLL_RESULT_SCROLL_UP);
    ck_assert_int_eq(det->state, IK_SCROLL_STATE_ABSORBING);
}

END_TEST
// Test 3: Slow second arrow emits ARROW for first, stays in WAITING with new arrow
START_TEST(test_slow_second_arrow_emits_arrow) {
    ik_scroll_detector_t *det = ik_scroll_detector_create(ctx);

    ik_scroll_detector_process_arrow(det, IK_INPUT_ARROW_UP, 1000);
    ik_scroll_result_t r = ik_scroll_detector_process_arrow(
        det, IK_INPUT_ARROW_UP, 1030);  // 30ms later
    ck_assert_int_eq(r, IK_SCROLL_RESULT_ARROW_UP);
    ck_assert_int_eq(det->state, IK_SCROLL_STATE_WAITING);
}

END_TEST
// Test 4: Timeout from WAITING flushes as ARROW, transitions to IDLE
START_TEST(test_timeout_flushes_arrow) {
    ik_scroll_detector_t *det = ik_scroll_detector_create(ctx);

    ik_scroll_detector_process_arrow(det, IK_INPUT_ARROW_UP, 1000);
    ik_scroll_result_t r = ik_scroll_detector_check_timeout(det, 1000 + IK_SCROLL_BURST_THRESHOLD_MS + 1);
    ck_assert_int_eq(r, IK_SCROLL_RESULT_ARROW_UP);
    ck_assert_int_eq(det->state, IK_SCROLL_STATE_IDLE);
}

END_TEST
// Test 5: Timeout before threshold returns NONE
START_TEST(test_timeout_before_threshold_returns_none) {
    ik_scroll_detector_t *det = ik_scroll_detector_create(ctx);

    ik_scroll_detector_process_arrow(det, IK_INPUT_ARROW_UP, 1000);
    ik_scroll_result_t r = ik_scroll_detector_check_timeout(det, 1005);
    ck_assert_int_eq(r, IK_SCROLL_RESULT_NONE);
    ck_assert_int_eq(det->state, IK_SCROLL_STATE_WAITING);
}

END_TEST
// Test 6: get_timeout_ms returns correct value
START_TEST(test_get_timeout_ms) {
    ik_scroll_detector_t *det = ik_scroll_detector_create(ctx);

    // IDLE - returns -1 (no timeout needed)
    int64_t t = ik_scroll_detector_get_timeout_ms(det, 1000);
    ck_assert_int_eq(t, -1);

    // WAITING at t=1000, check at t=1003 - should return threshold - 3
    ik_scroll_detector_process_arrow(det, IK_INPUT_ARROW_UP, 1000);
    t = ik_scroll_detector_get_timeout_ms(det, 1003);
    ck_assert_int_eq(t, IK_SCROLL_BURST_THRESHOLD_MS - 3);

    // At threshold + 1 - already expired, return 0
    t = ik_scroll_detector_get_timeout_ms(det, 1000 + IK_SCROLL_BURST_THRESHOLD_MS + 1);
    ck_assert_int_eq(t, 0);
}

END_TEST
// Test 7: flush() from WAITING emits ARROW
START_TEST(test_flush_emits_arrow) {
    ik_scroll_detector_t *det = ik_scroll_detector_create(ctx);

    ik_scroll_detector_process_arrow(det, IK_INPUT_ARROW_DOWN, 1000);
    ik_scroll_result_t r = ik_scroll_detector_flush(det);
    ck_assert_int_eq(r, IK_SCROLL_RESULT_ARROW_DOWN);
    ck_assert_int_eq(det->state, IK_SCROLL_STATE_IDLE);

    // Second flush returns NONE
    r = ik_scroll_detector_flush(det);
    ck_assert_int_eq(r, IK_SCROLL_RESULT_NONE);
}

END_TEST
// Test 8: Scroll direction preserved
START_TEST(test_scroll_direction) {
    ik_scroll_detector_t *det = ik_scroll_detector_create(ctx);

    ik_scroll_detector_process_arrow(det, IK_INPUT_ARROW_DOWN, 1000);
    ik_scroll_result_t r = ik_scroll_detector_process_arrow(
        det, IK_INPUT_ARROW_DOWN, 1001);
    ck_assert_int_eq(r, IK_SCROLL_RESULT_SCROLL_DOWN);
}

END_TEST
// Test 9: Mixed directions - each burst independent
START_TEST(test_mixed_directions) {
    ik_scroll_detector_t *det = ik_scroll_detector_create(ctx);

    // Up burst
    ik_scroll_detector_process_arrow(det, IK_INPUT_ARROW_UP, 1000);
    ik_scroll_result_t r = ik_scroll_detector_process_arrow(
        det, IK_INPUT_ARROW_UP, 1001);
    ck_assert_int_eq(r, IK_SCROLL_RESULT_SCROLL_UP);

    // Wait for absorb timeout, then down burst
    ik_scroll_detector_check_timeout(det, 1050);
    ck_assert_int_eq(det->state, IK_SCROLL_STATE_IDLE);

    ik_scroll_detector_process_arrow(det, IK_INPUT_ARROW_DOWN, 1100);
    r = ik_scroll_detector_process_arrow(det, IK_INPUT_ARROW_DOWN, 1101);
    ck_assert_int_eq(r, IK_SCROLL_RESULT_SCROLL_DOWN);
}

END_TEST
// Test 10: Reset clears state to IDLE
START_TEST(test_reset_clears_state) {
    ik_scroll_detector_t *det = ik_scroll_detector_create(ctx);

    ik_scroll_detector_process_arrow(det, IK_INPUT_ARROW_UP, 1000);
    ck_assert_int_eq(det->state, IK_SCROLL_STATE_WAITING);

    ik_scroll_detector_reset(det);
    ck_assert_int_eq(det->state, IK_SCROLL_STATE_IDLE);

    // Timeout should return NONE (nothing pending)
    ik_scroll_result_t r = ik_scroll_detector_check_timeout(det, 1050);
    ck_assert_int_eq(r, IK_SCROLL_RESULT_NONE);
}

END_TEST
// Test 11: Mouse wheel burst - emits ONE scroll, absorbs remaining arrows
START_TEST(test_mouse_wheel_burst_absorbs) {
    ik_scroll_detector_t *det = ik_scroll_detector_create(ctx);

    // Simulate Ghostty sending 3 arrows for one wheel notch
    ik_scroll_result_t r;

    // Arrow 1: IDLE -> WAITING
    r = ik_scroll_detector_process_arrow(det, IK_INPUT_ARROW_UP, 1000);
    ck_assert_int_eq(r, IK_SCROLL_RESULT_NONE);
    ck_assert_int_eq(det->state, IK_SCROLL_STATE_WAITING);

    // Arrow 2: WAITING -> ABSORBING, emit SCROLL
    r = ik_scroll_detector_process_arrow(det, IK_INPUT_ARROW_UP, 1001);
    ck_assert_int_eq(r, IK_SCROLL_RESULT_SCROLL_UP);
    ck_assert_int_eq(det->state, IK_SCROLL_STATE_ABSORBING);

    // Arrow 3: absorbed (returns ABSORBED)
    r = ik_scroll_detector_process_arrow(det, IK_INPUT_ARROW_UP, 1002);
    ck_assert_int_eq(r, IK_SCROLL_RESULT_ABSORBED);
    ck_assert_int_eq(det->state, IK_SCROLL_STATE_ABSORBING);

    // Timeout: ABSORBING -> IDLE (no additional output)
    r = ik_scroll_detector_check_timeout(det, 1030);
    ck_assert_int_eq(r, IK_SCROLL_RESULT_NONE);
    ck_assert_int_eq(det->state, IK_SCROLL_STATE_IDLE);
}

END_TEST
// Test 12: Kitty sends 10 arrows - still emits ONE scroll
START_TEST(test_kitty_10_arrows_one_scroll) {
    ik_scroll_detector_t *det = ik_scroll_detector_create(ctx);

    ik_scroll_result_t r;
    int64_t scroll_count = 0;

    // Simulate Kitty sending 10 arrows rapidly
    for (int64_t i = 0; i < 10; i++) {
        r = ik_scroll_detector_process_arrow(det, IK_INPUT_ARROW_UP, 1000 + i);
        if (r == IK_SCROLL_RESULT_SCROLL_UP) {
            scroll_count++;
        }
    }

    // Should have emitted exactly 1 scroll event
    ck_assert_int_eq(scroll_count, 1);
    ck_assert_int_eq(det->state, IK_SCROLL_STATE_ABSORBING);

    // Timeout completes without additional output
    r = ik_scroll_detector_check_timeout(det, 1050);
    ck_assert_int_eq(r, IK_SCROLL_RESULT_NONE);
}

END_TEST
// Test 13: Key repeat (30ms intervals) - each emits ARROW
START_TEST(test_key_repeat) {
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
    r = ik_scroll_detector_check_timeout(det, 1090);
    ck_assert_int_eq(r, IK_SCROLL_RESULT_ARROW_UP);
}

END_TEST
// Test 14: Exactly at threshold (10ms) is still a burst
START_TEST(test_at_threshold) {
    ik_scroll_detector_t *det = ik_scroll_detector_create(ctx);

    ik_scroll_detector_process_arrow(det, IK_INPUT_ARROW_UP, 1000);
    ik_scroll_result_t r = ik_scroll_detector_process_arrow(
        det, IK_INPUT_ARROW_UP, 1000 + IK_SCROLL_BURST_THRESHOLD_MS);  // exactly at threshold
    // Spec says "<= threshold" is burst
    ck_assert_int_eq(r, IK_SCROLL_RESULT_SCROLL_UP);
}

END_TEST
// Test 15: Just above threshold is keyboard
START_TEST(test_above_threshold) {
    ik_scroll_detector_t *det = ik_scroll_detector_create(ctx);

    ik_scroll_detector_process_arrow(det, IK_INPUT_ARROW_UP, 1000);
    ik_scroll_result_t r = ik_scroll_detector_process_arrow(
        det, IK_INPUT_ARROW_UP, 1000 + IK_SCROLL_BURST_THRESHOLD_MS + 1);  // Above threshold
    ck_assert_int_eq(r, IK_SCROLL_RESULT_ARROW_UP);
}

END_TEST
// Test 16: flush() from ABSORBING returns NONE (scroll already emitted)
START_TEST(test_flush_from_absorbing_returns_none) {
    ik_scroll_detector_t *det = ik_scroll_detector_create(ctx);

    // Get into ABSORBING state
    ik_scroll_detector_process_arrow(det, IK_INPUT_ARROW_UP, 1000);
    ik_scroll_detector_process_arrow(det, IK_INPUT_ARROW_UP, 1001);
    ck_assert_int_eq(det->state, IK_SCROLL_STATE_ABSORBING);

    // Flush should return NONE (scroll was already emitted)
    ik_scroll_result_t r = ik_scroll_detector_flush(det);
    ck_assert_int_eq(r, IK_SCROLL_RESULT_NONE);
    ck_assert_int_eq(det->state, IK_SCROLL_STATE_IDLE);
}

END_TEST
// Test 17: New burst after absorb timeout
START_TEST(test_new_burst_after_absorb) {
    ik_scroll_detector_t *det = ik_scroll_detector_create(ctx);

    // First burst
    ik_scroll_detector_process_arrow(det, IK_INPUT_ARROW_UP, 1000);
    ik_scroll_result_t r = ik_scroll_detector_process_arrow(det, IK_INPUT_ARROW_UP, 1001);
    ck_assert_int_eq(r, IK_SCROLL_RESULT_SCROLL_UP);

    // Wait for timeout
    ik_scroll_detector_check_timeout(det, 1050);
    ck_assert_int_eq(det->state, IK_SCROLL_STATE_IDLE);

    // Second burst should work
    ik_scroll_detector_process_arrow(det, IK_INPUT_ARROW_DOWN, 1100);
    r = ik_scroll_detector_process_arrow(det, IK_INPUT_ARROW_DOWN, 1101);
    ck_assert_int_eq(r, IK_SCROLL_RESULT_SCROLL_DOWN);
}

END_TEST
// Test 18: Arrow after absorb timeout (no intermediate input) starts new WAITING
START_TEST(test_arrow_after_absorb_timeout_starts_waiting) {
    ik_scroll_detector_t *det = ik_scroll_detector_create(ctx);

    // Get into ABSORBING
    ik_scroll_detector_process_arrow(det, IK_INPUT_ARROW_UP, 1000);
    ik_scroll_detector_process_arrow(det, IK_INPUT_ARROW_UP, 1001);
    ck_assert_int_eq(det->state, IK_SCROLL_STATE_ABSORBING);

    // Arrow arrives after timeout while still in ABSORBING
    // (event loop didn't call check_timeout yet)
    ik_scroll_result_t r = ik_scroll_detector_process_arrow(det, IK_INPUT_ARROW_UP, 1050);
    ck_assert_int_eq(r, IK_SCROLL_RESULT_NONE);
    ck_assert_int_eq(det->state, IK_SCROLL_STATE_WAITING);
}

END_TEST
// Test 19: Timeout from IDLE returns NONE
START_TEST(test_timeout_from_idle_returns_none) {
    ik_scroll_detector_t *det = ik_scroll_detector_create(ctx);

    ck_assert_int_eq(det->state, IK_SCROLL_STATE_IDLE);
    ik_scroll_result_t r = ik_scroll_detector_check_timeout(det, 1000);
    ck_assert_int_eq(r, IK_SCROLL_RESULT_NONE);
}

END_TEST
// Test 20: Timeout from ABSORBING returns NONE and transitions to IDLE
START_TEST(test_timeout_from_absorbing) {
    ik_scroll_detector_t *det = ik_scroll_detector_create(ctx);

    // Get into ABSORBING
    ik_scroll_detector_process_arrow(det, IK_INPUT_ARROW_UP, 1000);
    ik_scroll_detector_process_arrow(det, IK_INPUT_ARROW_UP, 1001);
    ck_assert_int_eq(det->state, IK_SCROLL_STATE_ABSORBING);

    // Timeout
    ik_scroll_result_t r = ik_scroll_detector_check_timeout(det, 1050);
    ck_assert_int_eq(r, IK_SCROLL_RESULT_NONE);
    ck_assert_int_eq(det->state, IK_SCROLL_STATE_IDLE);
}

END_TEST
// Test 21: get_timeout_ms works in ABSORBING state
START_TEST(test_get_timeout_ms_absorbing) {
    ik_scroll_detector_t *det = ik_scroll_detector_create(ctx);

    // Get into ABSORBING at t=1001
    ik_scroll_detector_process_arrow(det, IK_INPUT_ARROW_UP, 1000);
    ik_scroll_detector_process_arrow(det, IK_INPUT_ARROW_UP, 1001);

    // Check at t=1005 - should return remaining time
    int64_t t = ik_scroll_detector_get_timeout_ms(det, 1005);
    ck_assert_int_eq(t, IK_SCROLL_BURST_THRESHOLD_MS - 4);  // 1005 - 1001 = 4ms elapsed
}

END_TEST

// Suite definition
static Suite *scroll_detector_suite(void)
{
    Suite *s;
    TCase *tc_core;

    s = suite_create("ScrollDetector");

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
    tcase_add_test(tc_core, test_reset_clears_state);
    tcase_add_test(tc_core, test_mouse_wheel_burst_absorbs);
    tcase_add_test(tc_core, test_kitty_10_arrows_one_scroll);
    tcase_add_test(tc_core, test_key_repeat);
    tcase_add_test(tc_core, test_at_threshold);
    tcase_add_test(tc_core, test_above_threshold);
    tcase_add_test(tc_core, test_flush_from_absorbing_returns_none);
    tcase_add_test(tc_core, test_new_burst_after_absorb);
    tcase_add_test(tc_core, test_arrow_after_absorb_timeout_starts_waiting);
    tcase_add_test(tc_core, test_timeout_from_idle_returns_none);
    tcase_add_test(tc_core, test_timeout_from_absorbing);
    tcase_add_test(tc_core, test_get_timeout_ms_absorbing);

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
    srunner_set_xml(sr, "reports/check/unit/scroll_detector/scroll_detector_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
