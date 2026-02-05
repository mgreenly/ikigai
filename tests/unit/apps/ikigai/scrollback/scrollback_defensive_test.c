/**
 * @file scrollback_defensive_test.c
 * @brief Tests for scrollback defensive error paths (should never happen cases)
 */

#include <check.h>
#include <talloc.h>
#include <string.h>

#include "apps/ikigai/scrollback.h"
#include "tests/helpers/test_utils_helper.h"

// Test: Defensive error in ik_scrollback_find_logical_line_at_physical_row (lines 324-325)
// This tests the "should never happen" case where total_physical_lines is inconsistent
START_TEST(test_find_line_defensive_error) {
    TALLOC_CTX *ctx = talloc_new(NULL);

    ik_scrollback_t *sb = ik_scrollback_create(ctx, 80);

    // Add a line
    res_t res = ik_scrollback_append_line(sb, "test line", 9);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(sb->count, 1);
    ck_assert_uint_eq(sb->total_physical_lines, 1);

    // Corrupt internal state to trigger defensive error
    // Make total_physical_lines larger than actual sum
    sb->total_physical_lines = 10;  // Actually only 1 physical line exists

    // Now try to find a line at physical row 5
    // This passes the initial range check (5 < 10)
    // But the loop won't find it because there's only 1 line with 1 physical row
    size_t line_index, row_offset;
    res = ik_scrollback_find_logical_line_at_physical_row(sb, 5, &line_index, &row_offset);

    // Should get the defensive error
    ck_assert(is_err(&res));
    ck_assert_int_eq(error_code(res.err), ERR_OUT_OF_RANGE);

    // Verify error message indicates the defensive failure
    const char *msg = error_message(res.err);
    ck_assert(strstr(msg, "Failed to find line") != NULL);

    talloc_free(ctx);
}
END_TEST
// Test: Defensive error with multiple lines where sum doesn't match
START_TEST(test_find_line_defensive_error_multiple_lines) {
    TALLOC_CTX *ctx = talloc_new(NULL);

    ik_scrollback_t *sb = ik_scrollback_create(ctx, 80);

    // Add multiple lines
    res_t res = ik_scrollback_append_line(sb, "line 1", 6);
    ck_assert(is_ok(&res));
    res = ik_scrollback_append_line(sb, "line 2", 6);
    ck_assert(is_ok(&res));
    res = ik_scrollback_append_line(sb, "line 3", 6);
    ck_assert(is_ok(&res));

    ck_assert_uint_eq(sb->count, 3);
    ck_assert_uint_eq(sb->total_physical_lines, 3);  // Each line is 1 physical row

    // Corrupt state: make total say there are 5 lines
    sb->total_physical_lines = 5;

    // Try to find row 4 (which should exist according to total, but doesn't)
    size_t line_index, row_offset;
    res = ik_scrollback_find_logical_line_at_physical_row(sb, 4, &line_index, &row_offset);

    // Should get defensive error
    ck_assert(is_err(&res));
    ck_assert_int_eq(error_code(res.err), ERR_OUT_OF_RANGE);

    talloc_free(ctx);
}

END_TEST
// Test: Defensive error with wrapped lines
START_TEST(test_find_line_defensive_error_wrapped) {
    TALLOC_CTX *ctx = talloc_new(NULL);

    ik_scrollback_t *sb = ik_scrollback_create(ctx, 40);

    // Add a line that wraps (80 chars at width 40 = 2 physical lines)
    char long_line[81];
    memset(long_line, 'a', 80);
    long_line[80] = '\0';

    res_t res = ik_scrollback_append_line(sb, long_line, 80);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(sb->total_physical_lines, 2);

    // Corrupt: say we have 4 physical lines
    sb->total_physical_lines = 4;

    // Try to find row 3 (doesn't exist)
    size_t line_index, row_offset;
    res = ik_scrollback_find_logical_line_at_physical_row(sb, 3, &line_index, &row_offset);

    // Should get defensive error
    ck_assert(is_err(&res));
    ck_assert_int_eq(error_code(res.err), ERR_OUT_OF_RANGE);

    talloc_free(ctx);
}

END_TEST
// Test: Verify defensive error with edge case: zero physical lines per line
START_TEST(test_find_line_defensive_error_zero_lines) {
    TALLOC_CTX *ctx = talloc_new(NULL);

    ik_scrollback_t *sb = ik_scrollback_create(ctx, 80);

    // Add a line
    res_t res = ik_scrollback_append_line(sb, "test", 4);
    ck_assert(is_ok(&res));

    // Corrupt: manually set physical_lines to 0 but keep total_physical_lines > 0
    sb->layouts[0].physical_lines = 0;
    sb->total_physical_lines = 1;

    // Try to find row 0
    size_t line_index, row_offset;
    res = ik_scrollback_find_logical_line_at_physical_row(sb, 0, &line_index, &row_offset);

    // Should get defensive error because loop will skip line with 0 physical_lines
    ck_assert(is_err(&res));
    ck_assert_int_eq(error_code(res.err), ERR_OUT_OF_RANGE);

    talloc_free(ctx);
}

END_TEST

static Suite *scrollback_defensive_suite(void)
{
    Suite *s = suite_create("Scrollback Defensive");
    TCase *tc_core = tcase_create("Core");
    tcase_set_timeout(tc_core, IK_TEST_TIMEOUT);
    tcase_set_timeout(tc_core, IK_TEST_TIMEOUT);
    tcase_set_timeout(tc_core, IK_TEST_TIMEOUT);
    tcase_set_timeout(tc_core, IK_TEST_TIMEOUT);
    tcase_set_timeout(tc_core, IK_TEST_TIMEOUT);

    tcase_add_test(tc_core, test_find_line_defensive_error);
    tcase_add_test(tc_core, test_find_line_defensive_error_multiple_lines);
    tcase_add_test(tc_core, test_find_line_defensive_error_wrapped);
    tcase_add_test(tc_core, test_find_line_defensive_error_zero_lines);

    suite_add_tcase(s, tc_core);
    return s;
}

int main(void)
{
    int number_failed;
    Suite *s = scrollback_defensive_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/apps/ikigai/scrollback/scrollback_defensive_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
