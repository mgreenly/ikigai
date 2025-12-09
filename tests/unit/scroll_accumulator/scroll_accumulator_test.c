/**
 * @file scroll_accumulator_test.c
 * @brief Unit tests for scroll accumulator module
 */

#include "../../../src/scroll_accumulator.h"
#include "../../../src/input.h"
#include "../../test_utils.h"

#include <check.h>
#include <talloc.h>

// Forward declaration for suite function
static Suite *scroll_accumulator_suite(void);

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

// Test 1: Slow arrow (keyboard) emits cursor
START_TEST(test_slow_arrow_emits_cursor)
{
    ik_scroll_accumulator_t *acc = ik_scroll_accumulator_create(ctx);

    // Arrow after 500ms (way above 15ms threshold)
    ik_scroll_result_t r = ik_scroll_accumulator_process_arrow(
        acc, IK_INPUT_ARROW_UP, 500);
    ck_assert_int_eq(r, IK_SCROLL_RESULT_ARROW_UP);
}
END_TEST

// Test 2: Three rapid arrows emit one scroll
START_TEST(test_three_rapid_arrows_scroll)
{
    ik_scroll_accumulator_t *acc = ik_scroll_accumulator_create(ctx);

    // Simulate rapid mouse wheel (3ms apart)
    ik_scroll_result_t r;

    r = ik_scroll_accumulator_process_arrow(acc, IK_INPUT_ARROW_UP, 0);
    ck_assert_int_eq(r, IK_SCROLL_RESULT_NONE);  // elapsed=0, swallow

    r = ik_scroll_accumulator_process_arrow(acc, IK_INPUT_ARROW_UP, 3);
    ck_assert_int_eq(r, IK_SCROLL_RESULT_NONE);  // acc=10, swallow

    r = ik_scroll_accumulator_process_arrow(acc, IK_INPUT_ARROW_UP, 6);
    ck_assert_int_eq(r, IK_SCROLL_RESULT_SCROLL_UP);  // acc=5-5=0, scroll!
}
END_TEST

// Test 3: Scroll down direction
START_TEST(test_scroll_down)
{
    ik_scroll_accumulator_t *acc = ik_scroll_accumulator_create(ctx);

    ik_scroll_accumulator_process_arrow(acc, IK_INPUT_ARROW_DOWN, 0);
    ik_scroll_accumulator_process_arrow(acc, IK_INPUT_ARROW_DOWN, 3);
    ik_scroll_result_t r = ik_scroll_accumulator_process_arrow(
        acc, IK_INPUT_ARROW_DOWN, 6);
    ck_assert_int_eq(r, IK_SCROLL_RESULT_SCROLL_DOWN);
}
END_TEST

// Test 4: Accumulator resets after scroll
START_TEST(test_accumulator_resets_after_scroll)
{
    ik_scroll_accumulator_t *acc = ik_scroll_accumulator_create(ctx);

    // Drain to scroll
    ik_scroll_accumulator_process_arrow(acc, IK_INPUT_ARROW_UP, 0);
    ik_scroll_accumulator_process_arrow(acc, IK_INPUT_ARROW_UP, 3);
    ik_scroll_accumulator_process_arrow(acc, IK_INPUT_ARROW_UP, 6);  // scroll

    // Next 3 should also scroll (accumulator reset to 15)
    ik_scroll_result_t r;
    r = ik_scroll_accumulator_process_arrow(acc, IK_INPUT_ARROW_UP, 9);
    ck_assert_int_eq(r, IK_SCROLL_RESULT_NONE);  // acc=10

    r = ik_scroll_accumulator_process_arrow(acc, IK_INPUT_ARROW_UP, 12);
    ck_assert_int_eq(r, IK_SCROLL_RESULT_NONE);  // acc=5

    r = ik_scroll_accumulator_process_arrow(acc, IK_INPUT_ARROW_UP, 15);
    ck_assert_int_eq(r, IK_SCROLL_RESULT_SCROLL_UP);  // scroll!
}
END_TEST

// Test 5: Non-arrow key refills accumulator
START_TEST(test_non_arrow_refills)
{
    ik_scroll_accumulator_t *acc = ik_scroll_accumulator_create(ctx);

    // Start draining
    ik_scroll_accumulator_process_arrow(acc, IK_INPUT_ARROW_UP, 0);
    ik_scroll_accumulator_process_arrow(acc, IK_INPUT_ARROW_UP, 3);  // acc=10

    // Type a character after 50ms
    ik_scroll_accumulator_process_other(acc, 53);  // acc = min(15, 10+50) = 15

    // Now need 3 more rapid arrows to scroll
    ik_scroll_result_t r;
    r = ik_scroll_accumulator_process_arrow(acc, IK_INPUT_ARROW_UP, 56);
    ck_assert_int_eq(r, IK_SCROLL_RESULT_NONE);  // acc=10

    r = ik_scroll_accumulator_process_arrow(acc, IK_INPUT_ARROW_UP, 59);
    ck_assert_int_eq(r, IK_SCROLL_RESULT_NONE);  // acc=5

    r = ik_scroll_accumulator_process_arrow(acc, IK_INPUT_ARROW_UP, 62);
    ck_assert_int_eq(r, IK_SCROLL_RESULT_SCROLL_UP);
}
END_TEST

// Test 6: Key repeat (33ms) always emits cursor
START_TEST(test_key_repeat_emits_cursor)
{
    ik_scroll_accumulator_t *acc = ik_scroll_accumulator_create(ctx);

    // Simulate held arrow key at 30Hz (33ms)
    ik_scroll_result_t r;
    r = ik_scroll_accumulator_process_arrow(acc, IK_INPUT_ARROW_UP, 33);
    ck_assert_int_eq(r, IK_SCROLL_RESULT_ARROW_UP);

    r = ik_scroll_accumulator_process_arrow(acc, IK_INPUT_ARROW_UP, 66);
    ck_assert_int_eq(r, IK_SCROLL_RESULT_ARROW_UP);

    r = ik_scroll_accumulator_process_arrow(acc, IK_INPUT_ARROW_UP, 99);
    ck_assert_int_eq(r, IK_SCROLL_RESULT_ARROW_UP);
}
END_TEST

// Test 7: First event handling (previous_time not set)
START_TEST(test_first_event)
{
    ik_scroll_accumulator_t *acc = ik_scroll_accumulator_create(ctx);

    // Very first arrow event - elapsed is large (from init time of 0)
    ik_scroll_result_t r = ik_scroll_accumulator_process_arrow(
        acc, IK_INPUT_ARROW_UP, 1000);
    ck_assert_int_eq(r, IK_SCROLL_RESULT_ARROW_UP);
}
END_TEST

// Test 8: Direction change - both emit appropriately
START_TEST(test_direction_change)
{
    ik_scroll_accumulator_t *acc = ik_scroll_accumulator_create(ctx);

    // Rapid up arrows
    ik_scroll_accumulator_process_arrow(acc, IK_INPUT_ARROW_UP, 0);
    ik_scroll_accumulator_process_arrow(acc, IK_INPUT_ARROW_UP, 3);
    ik_scroll_result_t r = ik_scroll_accumulator_process_arrow(
        acc, IK_INPUT_ARROW_UP, 6);
    ck_assert_int_eq(r, IK_SCROLL_RESULT_SCROLL_UP);

    // Rapid down arrows (accumulator was reset)
    ik_scroll_accumulator_process_arrow(acc, IK_INPUT_ARROW_DOWN, 9);
    ik_scroll_accumulator_process_arrow(acc, IK_INPUT_ARROW_DOWN, 12);
    r = ik_scroll_accumulator_process_arrow(acc, IK_INPUT_ARROW_DOWN, 15);
    ck_assert_int_eq(r, IK_SCROLL_RESULT_SCROLL_DOWN);
}
END_TEST

// Test 9: Reset clears state
START_TEST(test_reset)
{
    ik_scroll_accumulator_t *acc = ik_scroll_accumulator_create(ctx);

    // Drain partially
    ik_scroll_accumulator_process_arrow(acc, IK_INPUT_ARROW_UP, 0);
    ik_scroll_accumulator_process_arrow(acc, IK_INPUT_ARROW_UP, 3);

    // Reset
    ik_scroll_accumulator_reset(acc);

    // Should need 3 arrows again
    ik_scroll_result_t r;
    r = ik_scroll_accumulator_process_arrow(acc, IK_INPUT_ARROW_UP, 100);
    // After reset, elapsed from 0 to 100 is large, so emit cursor
    ck_assert_int_eq(r, IK_SCROLL_RESULT_ARROW_UP);
}
END_TEST

// Test 10: Exactly at threshold (15ms) - should drain accumulator
START_TEST(test_at_threshold_drains)
{
    ik_scroll_accumulator_t *acc = ik_scroll_accumulator_create(ctx);

    // Set baseline
    ik_scroll_accumulator_process_arrow(acc, IK_INPUT_ARROW_UP, 0);

    // Exactly 15ms later - NOT > 15, so drains accumulator
    ik_scroll_result_t r = ik_scroll_accumulator_process_arrow(
        acc, IK_INPUT_ARROW_UP, 15);
    ck_assert_int_eq(r, IK_SCROLL_RESULT_NONE);  // acc=10, swallow
}
END_TEST

// Test 11: Just above threshold (16ms) - should emit cursor
START_TEST(test_above_threshold_emits_cursor)
{
    ik_scroll_accumulator_t *acc = ik_scroll_accumulator_create(ctx);

    ik_scroll_accumulator_process_arrow(acc, IK_INPUT_ARROW_UP, 0);

    // 16ms later - above threshold
    ik_scroll_result_t r = ik_scroll_accumulator_process_arrow(
        acc, IK_INPUT_ARROW_UP, 16);
    ck_assert_int_eq(r, IK_SCROLL_RESULT_ARROW_UP);
}
END_TEST

// Suite definition
static Suite *scroll_accumulator_suite(void)
{
    Suite *s;
    TCase *tc_core;

    s = suite_create("ScrollAccumulator");

    tc_core = tcase_create("Core");

    tcase_add_checked_fixture(tc_core, setup, teardown);
    tcase_add_test(tc_core, test_slow_arrow_emits_cursor);
    tcase_add_test(tc_core, test_three_rapid_arrows_scroll);
    tcase_add_test(tc_core, test_scroll_down);
    tcase_add_test(tc_core, test_accumulator_resets_after_scroll);
    tcase_add_test(tc_core, test_non_arrow_refills);
    tcase_add_test(tc_core, test_key_repeat_emits_cursor);
    tcase_add_test(tc_core, test_first_event);
    tcase_add_test(tc_core, test_direction_change);
    tcase_add_test(tc_core, test_reset);
    tcase_add_test(tc_core, test_at_threshold_drains);
    tcase_add_test(tc_core, test_above_threshold_emits_cursor);

    suite_add_tcase(s, tc_core);

    return s;
}

int main(void)
{
    int number_failed;
    Suite *s;
    SRunner *sr;

    s = scroll_accumulator_suite();
    sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
