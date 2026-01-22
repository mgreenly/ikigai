/**
 * @file scrollback_newline_reflow_test.c
 * @brief Tests for scrollback line counting with embedded newlines
 */

#include <check.h>
#include <string.h>
#include <talloc.h>

#include "../../../src/scrollback.h"
#include "../../test_utils_helper.h"

/**
 * Test: Line with embedded newlines counts correctly
 *
 * A line with embedded newlines like "Line1\nLine2\nLine3" should count
 * as at least 3 physical lines (one per newline-delimited segment).
 */
START_TEST(test_scrollback_newline_basic_count) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_scrollback_t *sb = ik_scrollback_create(ctx, 80);

    // Append line with 3 newline-delimited segments
    const char *text = "Line1\nLine2\nLine3";
    res_t res = ik_scrollback_append_line(sb, text, strlen(text));
    ck_assert(is_ok(&res));

    // Should count as 3 physical lines (one per segment)
    size_t physical_lines = sb->layouts[0].physical_lines;
    ck_assert_uint_eq(physical_lines, 3);

    talloc_free(ctx);
}
END_TEST
/**
 * Test: Newline reflow preserves minimum line count
 *
 * When terminal width changes, lines with embedded newlines should still
 * count at least as many physical lines as they have newline-delimited segments.
 */
START_TEST(test_scrollback_newline_reflow) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_scrollback_t *sb = ik_scrollback_create(ctx, 80);

    // Append line with 3 newline-delimited segments
    const char *text = "Line1\nLine2\nLine3";
    res_t res = ik_scrollback_append_line(sb, text, strlen(text));
    ck_assert(is_ok(&res));

    // Initially should be 3 physical lines
    ck_assert_uint_eq(sb->layouts[0].physical_lines, 3);

    // Resize to narrower width (40 columns)
    ik_scrollback_ensure_layout(sb, 40);

    // Should still be at least 3 physical lines (one per segment)
    // Could be more if any segment is longer than 40 columns
    size_t physical_lines = sb->layouts[0].physical_lines;
    ck_assert_uint_ge(physical_lines, 3);

    talloc_free(ctx);
}

END_TEST
/**
 * Test: Trailing newline counts as empty line
 *
 * A line ending with a newline like "content\n" should count the trailing
 * empty segment as an additional physical line.
 */
START_TEST(test_scrollback_trailing_newline) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_scrollback_t *sb = ik_scrollback_create(ctx, 80);

    // Append line with trailing newline
    const char *text = "content\n";
    res_t res = ik_scrollback_append_line(sb, text, strlen(text));
    ck_assert(is_ok(&res));

    // Should count as 2 physical lines (content + trailing empty line)
    size_t physical_lines = sb->layouts[0].physical_lines;
    ck_assert_uint_eq(physical_lines, 2);

    // Resize and verify count is preserved
    ik_scrollback_ensure_layout(sb, 40);
    physical_lines = sb->layouts[0].physical_lines;
    ck_assert_uint_ge(physical_lines, 2);

    talloc_free(ctx);
}

END_TEST
/**
 * Test: Long segments wrap correctly after reflow
 *
 * When a newline-delimited segment is longer than terminal width,
 * it should wrap to multiple rows. This should work both on initial
 * append and after resize.
 */
START_TEST(test_scrollback_long_segment_reflow) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_scrollback_t *sb = ik_scrollback_create(ctx, 80);

    // Create a line with one short segment and one long segment
    // "Short\n" + 90 'x' characters = 2 segments
    // At width 80: segment 1 = 1 row, segment 2 = 2 rows (90/80 = 1.125 -> ceil = 2)
    // Total: 3 rows
    char text[128];
    strcpy(text, "Short\n");
    memset(text + 6, 'x', 90);
    text[96] = '\0';

    res_t res = ik_scrollback_append_line(sb, text, strlen(text));
    ck_assert(is_ok(&res));

    // At width 80: "Short" = 1 row, 90 x's = 2 rows (ceiling of 90/80)
    // Total: 3 rows
    size_t physical_lines = sb->layouts[0].physical_lines;
    ck_assert_uint_eq(physical_lines, 3);

    // Resize to width 40
    // "Short" = 1 row, 90 x's = 3 rows (ceiling of 90/40)
    // Total: 4 rows
    ik_scrollback_ensure_layout(sb, 40);
    physical_lines = sb->layouts[0].physical_lines;
    ck_assert_uint_eq(physical_lines, 4);

    // Resize to width 100
    // "Short" = 1 row, 90 x's = 1 row (fits in 100)
    // Total: 2 rows
    ik_scrollback_ensure_layout(sb, 100);
    physical_lines = sb->layouts[0].physical_lines;
    ck_assert_uint_eq(physical_lines, 2);

    talloc_free(ctx);
}

END_TEST
/**
 * Test: Multiple newlines create empty line segments
 *
 * Lines like "A\n\nB" should count empty segments between consecutive newlines.
 */
START_TEST(test_scrollback_consecutive_newlines) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_scrollback_t *sb = ik_scrollback_create(ctx, 80);

    // Three segments: "A", "", "B"
    const char *text = "A\n\nB";
    res_t res = ik_scrollback_append_line(sb, text, strlen(text));
    ck_assert(is_ok(&res));

    // Should count as 3 physical lines (A, empty, B)
    size_t physical_lines = sb->layouts[0].physical_lines;
    ck_assert_uint_eq(physical_lines, 3);

    // Verify count is preserved on reflow
    ik_scrollback_ensure_layout(sb, 40);
    physical_lines = sb->layouts[0].physical_lines;
    ck_assert_uint_eq(physical_lines, 3);

    talloc_free(ctx);
}

END_TEST

static Suite *scrollback_newline_reflow_suite(void)
{
    Suite *s;
    TCase *tc_core;

    s = suite_create("Scrollback Newline Reflow");
    tc_core = tcase_create("Core");

    tcase_add_test(tc_core, test_scrollback_newline_basic_count);
    tcase_add_test(tc_core, test_scrollback_newline_reflow);
    tcase_add_test(tc_core, test_scrollback_trailing_newline);
    tcase_add_test(tc_core, test_scrollback_long_segment_reflow);
    tcase_add_test(tc_core, test_scrollback_consecutive_newlines);

    suite_add_tcase(s, tc_core);
    return s;
}

int32_t main(void)
{
    int32_t number_failed;
    Suite *s;
    SRunner *sr;

    s = scrollback_newline_reflow_suite();
    sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/scrollback/scrollback_newline_reflow_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
