/**
 * @file arrow_burst_integration_test.c
 * @brief Integration tests for arrow burst detector in REPL event loop
 */

#include <check.h>
#include <talloc.h>
#include "../../../src/arrow_burst.h"
#include "../../../src/input.h"
#include "../../../src/input_buffer/core.h"
#include "../../../src/repl.h"
#include "../../../src/repl_actions.h"
#include "../../../src/scrollback.h"
#include "../../../src/shared.h"
#include "../../test_utils.h"

// Test: Arrow detector is initialized
START_TEST(test_arrow_detector_initialized)
{
    void *ctx = talloc_new(NULL);

    // Create minimal REPL context
    ik_repl_ctx_t *repl = talloc_zero(ctx, ik_repl_ctx_t);
    ck_assert_ptr_nonnull(repl);

    // Create arrow detector
    repl->arrow_detector = ik_arrow_burst_create(repl);
    ck_assert_ptr_nonnull(repl->arrow_detector);

    // Verify initial state
    ck_assert_int_eq(repl->arrow_detector->state, IK_ARROW_BURST_IDLE);

    talloc_free(ctx);
}
END_TEST

// Test: Rapid arrows should scroll viewport (integration)
START_TEST(test_rapid_arrows_scroll_viewport)
{
    void *ctx = talloc_new(NULL);

    // Create REPL components
    ik_repl_ctx_t *repl = talloc_zero(ctx, ik_repl_ctx_t);
    ck_assert_ptr_nonnull(repl);

    repl->input_buffer = ik_input_buffer_create(repl);
    repl->scrollback = ik_scrollback_create(repl, 80);
    repl->arrow_detector = ik_arrow_burst_create(repl);
    repl->viewport_offset = 0;

    // Add scrollback content
    for (int i = 0; i < 50; i++) {
        char line[32];
        snprintf(line, sizeof(line), "Line %d", i);
        ik_scrollback_append_line(repl->scrollback, line, strlen(line));
    }

    // Initial viewport at bottom
    ck_assert_uint_eq(repl->viewport_offset, 0);

    // Simulate rapid arrow up events (mouse wheel scenario)
    // This tests the detector API directly
    int64_t now_ms = 100;
    ik_arrow_burst_result_t result1 = ik_arrow_burst_process(
        repl->arrow_detector, IK_INPUT_ARROW_UP, now_ms);
    ck_assert_int_eq(result1, IK_ARROW_BURST_RESULT_NONE); // Buffering

    ik_arrow_burst_result_t result2 = ik_arrow_burst_process(
        repl->arrow_detector, IK_INPUT_ARROW_UP, now_ms + 10);
    ck_assert_int_eq(result2, IK_ARROW_BURST_RESULT_SCROLL_UP); // Burst detected

    talloc_free(ctx);
}
END_TEST

// Test: Single arrow should trigger cursor movement after timeout
START_TEST(test_single_arrow_timeout)
{
    void *ctx = talloc_new(NULL);

    ik_repl_ctx_t *repl = talloc_zero(ctx, ik_repl_ctx_t);
    ck_assert_ptr_nonnull(repl);

    repl->input_buffer = ik_input_buffer_create(repl);
    repl->arrow_detector = ik_arrow_burst_create(repl);

    // Setup multi-line input
    const char *text = "Line 1\nLine 2\nLine 3";
    res_t res = ik_input_buffer_set_text(repl->input_buffer, text, strlen(text));
    ck_assert(is_ok(&res));

    // Process single arrow up event
    int64_t now_ms = 100;
    ik_arrow_burst_result_t result = ik_arrow_burst_process(
        repl->arrow_detector, IK_INPUT_ARROW_UP, now_ms);
    ck_assert_int_eq(result, IK_ARROW_BURST_RESULT_NONE); // Buffering

    // Check timeout after threshold
    result = ik_arrow_burst_check_timeout(repl->arrow_detector, now_ms + 20);
    ck_assert_int_eq(result, IK_ARROW_BURST_RESULT_CURSOR_UP);

    talloc_free(ctx);
}
END_TEST

// Test: Timeout getter returns correct values
START_TEST(test_timeout_getter)
{
    void *ctx = talloc_new(NULL);

    ik_arrow_burst_detector_t *det = ik_arrow_burst_create(ctx);
    ck_assert_ptr_nonnull(det);

    // Initially idle - no timeout
    int64_t timeout = ik_arrow_burst_get_timeout_ms(det, 100);
    ck_assert_int_eq(timeout, -1);

    // Process arrow event (starts buffering)
    int64_t now_ms = 100;
    ik_arrow_burst_process(det, IK_INPUT_ARROW_UP, now_ms);

    // Check that detector is buffering with valid timeout
    timeout = ik_arrow_burst_get_timeout_ms(det, now_ms + 5);
    ck_assert_int_ge(timeout, 0);
    ck_assert_int_le(timeout, IK_ARROW_BURST_THRESHOLD_MS);

    talloc_free(ctx);
}
END_TEST

static Suite *arrow_burst_integration_suite(void)
{
    Suite *s = suite_create("Arrow Burst Integration");
    TCase *tc_integration = tcase_create("Integration");

    tcase_add_test(tc_integration, test_arrow_detector_initialized);
    tcase_add_test(tc_integration, test_rapid_arrows_scroll_viewport);
    tcase_add_test(tc_integration, test_single_arrow_timeout);
    tcase_add_test(tc_integration, test_timeout_getter);

    suite_add_tcase(s, tc_integration);
    return s;
}

int main(void)
{
    Suite *s = arrow_burst_integration_suite();
    SRunner *sr = srunner_create(s);
    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);
    return (number_failed == 0) ? 0 : 1;
}
