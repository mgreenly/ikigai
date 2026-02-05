#include "tests/test_constants.h"
#include <check.h>
#include <talloc.h>
#include <string.h>
#include "apps/ikigai/layer_wrappers.h"
#include "apps/ikigai/scrollback.h"
#include "shared/error.h"

// Test: Render partial line with embedded newlines
START_TEST(test_partial_render_with_newlines) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    // Create scrollback with width=80
    ik_scrollback_t *sb = ik_scrollback_create(ctx, 80);
    ck_assert_ptr_nonnull(sb);

    // Append line with embedded newlines: "Line1\nLine2\nLine3"
    // This creates 3 segments, each on its own row
    const char *text = "Line1\nLine2\nLine3";
    ik_scrollback_append_line(sb, text, strlen(text));

    // Create layer and output buffer
    ik_layer_t *layer = ik_scrollback_layer_create(ctx, "scrollback", sb);
    ik_output_buffer_t *output = ik_output_buffer_create(ctx, 1000);

    // Render with start_row=1, row_count=2 (skip first segment "Line1")
    // Should render "Line2\x1b[K\r\nLine3\x1b[K\r\n"
    layer->render(layer, output, 80, 1, 2);

    // Verify output contains "Line2\x1b[K\r\nLine3\x1b[K\r\n"
    const char *expected = "Line2\x1b[K\r\nLine3\x1b[K\r\n";
    ck_assert_uint_eq(output->size, strlen(expected));
    ck_assert_mem_eq(output->data, expected, output->size);

    talloc_free(ctx);
}
END_TEST
// Test: Wrapped segment with newline
START_TEST(test_wrapped_segment_with_newline) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    // Create scrollback with width=80
    ik_scrollback_t *sb = ik_scrollback_create(ctx, 80);
    ck_assert_ptr_nonnull(sb);

    // Append line: "A" * 100 + "\nShort"
    // First segment wraps to 2 rows at width=80, second segment is 1 row
    char text[120];
    memset(text, 'A', 100);
    strcpy(text + 100, "\nShort");
    ik_scrollback_append_line(sb, text, strlen(text));

    // Create layer and output buffer
    ik_layer_t *layer = ik_scrollback_layer_create(ctx, "scrollback", sb);
    ik_output_buffer_t *output = ik_output_buffer_create(ctx, 1000);

    // Render with start_row=1, row_count=2
    // Skip first wrapped row (first 80 A's)
    // Should render: remaining 20 A's + "\x1b[K\r\n" + "Short" + "\x1b[K\r\n"
    layer->render(layer, output, 80, 1, 2);

    // Verify output contains the last 20 A's followed by "Short"
    char expected[40];
    memset(expected, 'A', 20);
    strcpy(expected + 20, "\x1b[K\r\nShort\x1b[K\r\n");
    ck_assert_uint_eq(output->size, strlen(expected));
    ck_assert_mem_eq(output->data, expected, output->size);

    talloc_free(ctx);
}

END_TEST
// Test: Skip multiple newline segments
START_TEST(test_skip_multiple_newline_segments) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    // Create scrollback with width=80
    ik_scrollback_t *sb = ik_scrollback_create(ctx, 80);
    ck_assert_ptr_nonnull(sb);

    // Append line: "A\nB\nC\nD"
    // This creates 4 segments, each on its own row
    const char *text = "A\nB\nC\nD";
    ik_scrollback_append_line(sb, text, strlen(text));

    // Create layer and output buffer
    ik_layer_t *layer = ik_scrollback_layer_create(ctx, "scrollback", sb);
    ik_output_buffer_t *output = ik_output_buffer_create(ctx, 1000);

    // Render with start_row=2, row_count=2 (skip A and B)
    // Should render "C\x1b[K\r\nD\x1b[K\r\n"
    layer->render(layer, output, 80, 2, 2);

    // Verify output contains "C\x1b[K\r\nD\x1b[K\r\n"
    const char *expected = "C\x1b[K\r\nD\x1b[K\r\n";
    ck_assert_uint_eq(output->size, strlen(expected));
    ck_assert_mem_eq(output->data, expected, output->size);

    talloc_free(ctx);
}

END_TEST

static Suite *layer_scrollback_partial_newline_suite(void)
{
    Suite *s = suite_create("Layer Scrollback Partial Newline");

    TCase *tc_core = tcase_create("Core");
    tcase_set_timeout(tc_core, IK_TEST_TIMEOUT);
    tcase_add_test(tc_core, test_partial_render_with_newlines);
    tcase_add_test(tc_core, test_wrapped_segment_with_newline);
    tcase_add_test(tc_core, test_skip_multiple_newline_segments);
    suite_add_tcase(s, tc_core);

    return s;
}

int main(void)
{
    int number_failed;
    Suite *s = layer_scrollback_partial_newline_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/apps/ikigai/layer/layer_scrollback_partial_newline_test.xml");
    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);
    return (number_failed == 0) ? 0 : 1;
}
