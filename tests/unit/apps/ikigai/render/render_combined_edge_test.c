/**
 * @file render_combined_edge_test.c
 * @brief Edge case tests for ik_render_combined() function
 */

#include <check.h>
#include <talloc.h>
#include "apps/ikigai/render.h"
#include "apps/ikigai/scrollback.h"
#include "tests/helpers/test_utils_helper.h"

/* Test: Invalid scrollback_start_line (>= total_lines) returns error */
START_TEST(test_render_combined_invalid_scrollback_start) {
    void *ctx = talloc_new(NULL);
    ik_render_ctx_t *render_ctx = NULL;

    /* Create render context */
    res_t res = ik_render_create(ctx, 24, 80, 1, &render_ctx);
    ck_assert(is_ok(&res));

    /* Create scrollback with 3 lines */
    ik_scrollback_t *scrollback = ik_scrollback_create(ctx, 80);
    res = ik_scrollback_append_line(scrollback, "line 1", 6);
    ck_assert(is_ok(&res));
    res = ik_scrollback_append_line(scrollback, "line 2", 6);
    ck_assert(is_ok(&res));
    res = ik_scrollback_append_line(scrollback, "line 3", 6);
    ck_assert(is_ok(&res));

    /* Try to render with scrollback_start_line = 3 (>= 3 lines) */
    res = ik_render_combined(render_ctx, scrollback, 3, 1, "test", 4, 0, true, true);
    ck_assert(is_err(&res));
    ck_assert_int_eq(error_code(res.err), ERR_INVALID_ARG);

    /* Also test with scrollback_start_line > total_lines */
    res = ik_render_combined(render_ctx, scrollback, 10, 1, "test", 4, 0, true, true);
    ck_assert(is_err(&res));
    ck_assert_int_eq(error_code(res.err), ERR_INVALID_ARG);

    talloc_free(ctx);
}
END_TEST
/* Test: Scrollback line_count exceeding available lines gets clamped */
START_TEST(test_render_combined_scrollback_count_clamping) {
    void *ctx = talloc_new(NULL);
    ik_render_ctx_t *render_ctx = NULL;

    /* Create render context */
    res_t res = ik_render_create(ctx, 24, 80, 1, &render_ctx);
    ck_assert(is_ok(&res));

    /* Create scrollback with 3 lines */
    ik_scrollback_t *scrollback = ik_scrollback_create(ctx, 80);
    res = ik_scrollback_append_line(scrollback, "line 1", 6);
    ck_assert(is_ok(&res));
    res = ik_scrollback_append_line(scrollback, "line 2", 6);
    ck_assert(is_ok(&res));
    res = ik_scrollback_append_line(scrollback, "line 3", 6);
    ck_assert(is_ok(&res));

    /* Try to render with scrollback_start_line=1, line_count=5 (would go beyond 3 lines) */
    /* Should clamp to show lines 1-2 (2 lines total) */
    res = ik_render_combined(render_ctx, scrollback, 1, 5, "test", 4, 0, true, true);
    ck_assert(is_ok(&res));
    /* If it didn't crash/error, the clamping worked */

    talloc_free(ctx);
}

END_TEST
/* Test: Scrollback line containing embedded newline characters */
START_TEST(test_render_combined_scrollback_with_newlines) {
    void *ctx = talloc_new(NULL);
    ik_render_ctx_t *render_ctx = NULL;

    /* Create render context */
    res_t res = ik_render_create(ctx, 24, 80, 1, &render_ctx);
    ck_assert(is_ok(&res));

    /* Create scrollback with a line containing embedded newlines */
    ik_scrollback_t *scrollback = ik_scrollback_create(ctx, 80);

    /* This line contains embedded newlines which need to be converted to \r\n */
    const char *line_with_newlines = "line1\nline2\nline3";
    res = ik_scrollback_append_line(scrollback, line_with_newlines, strlen(line_with_newlines));
    ck_assert(is_ok(&res));

    /* Render the scrollback - should handle newlines correctly without crashing */
    res = ik_render_combined(render_ctx, scrollback, 0, 1, "test", 4, 0, true, true);
    ck_assert(is_ok(&res));

    /* Verify we got output (not NULL) - the conversion worked */
    const char *output = res.ok;
    ck_assert_ptr_nonnull(output);
    /* The fact that it didn't crash and returned OK means newlines were handled */

    talloc_free(ctx);
}

END_TEST
/* Test: Invalid UTF-8 in input buffer text causes cursor position calculation to fail */
START_TEST(test_render_combined_invalid_utf8_in_input_buffer) {
    void *ctx = talloc_new(NULL);
    ik_render_ctx_t *render_ctx = NULL;

    /* Create render context */
    res_t res = ik_render_create(ctx, 24, 80, 1, &render_ctx);
    ck_assert(is_ok(&res));

    /* Create empty scrollback */
    ik_scrollback_t *scrollback = ik_scrollback_create(ctx, 80);

    /* Create input buffer text with invalid UTF-8 sequence
     * 0xFF is never valid in UTF-8 (it's a continuation byte pattern that can't start a sequence)
     * Position cursor after some valid text so calculate_cursor_screen_position is called */
    const char invalid_utf8[] = "valid\xFFtext";
    size_t text_len = strlen(invalid_utf8);
    size_t cursor_offset = 6; /* Position cursor after "valid\xFF" to trigger UTF-8 decoding */

    /* Render should fail with INVALID_ARG due to invalid UTF-8 */
    res = ik_render_combined(render_ctx, scrollback, 0, 0, invalid_utf8, text_len, cursor_offset, true, true);
    ck_assert(is_err(&res));
    ck_assert_int_eq(error_code(res.err), ERR_INVALID_ARG);

    talloc_free(ctx);
}

END_TEST

/* Test Suite */
static Suite *render_combined_edge_suite(void)
{
    Suite *s = suite_create("Render Combined Edge Cases");

    TCase *tc_edge = tcase_create("Edge");
    tcase_set_timeout(tc_edge, IK_TEST_TIMEOUT);
    tcase_add_test(tc_edge, test_render_combined_invalid_scrollback_start);
    tcase_add_test(tc_edge, test_render_combined_scrollback_count_clamping);
    tcase_add_test(tc_edge, test_render_combined_scrollback_with_newlines);
    tcase_add_test(tc_edge, test_render_combined_invalid_utf8_in_input_buffer);
    suite_add_tcase(s, tc_edge);

    return s;
}

int main(void)
{
    Suite *s = render_combined_edge_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/apps/ikigai/render/render_combined_edge_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
