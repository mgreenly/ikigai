#include <check.h>
#include <inttypes.h>
#include <signal.h>
#include <string.h>
#include <talloc.h>

#include "../../../src/scrollback.h"
#include "../../test_utils.h"

// Test: Get line count from empty scrollback
START_TEST(test_scrollback_get_line_count_empty) {
    TALLOC_CTX *ctx = talloc_new(NULL);

    ik_scrollback_t *sb = ik_scrollback_create(ctx, 80);

    ck_assert_uint_eq(ik_scrollback_get_line_count(sb), 0);

    talloc_free(ctx);
}
END_TEST
// Test: Get line count with lines
START_TEST(test_scrollback_get_line_count_with_lines)
{
    TALLOC_CTX *ctx = talloc_new(NULL);

    ik_scrollback_t *sb = ik_scrollback_create(ctx, 80);

    res_t res = ik_scrollback_append_line(sb, "line 1", 6);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(ik_scrollback_get_line_count(sb), 1);

    res = ik_scrollback_append_line(sb, "line 2", 6);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(ik_scrollback_get_line_count(sb), 2);

    talloc_free(ctx);
}

END_TEST
// Test: Get total physical lines from empty scrollback
START_TEST(test_scrollback_get_total_physical_lines_empty)
{
    TALLOC_CTX *ctx = talloc_new(NULL);

    ik_scrollback_t *sb = ik_scrollback_create(ctx, 80);

    ck_assert_uint_eq(ik_scrollback_get_total_physical_lines(sb), 0);

    talloc_free(ctx);
}

END_TEST
// Test: Get total physical lines with single-line entries
START_TEST(test_scrollback_get_total_physical_lines_single)
{
    TALLOC_CTX *ctx = talloc_new(NULL);

    ik_scrollback_t *sb = ik_scrollback_create(ctx, 80);

    res_t res = ik_scrollback_append_line(sb, "short", 5);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(ik_scrollback_get_total_physical_lines(sb), 1);

    res = ik_scrollback_append_line(sb, "another", 7);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(ik_scrollback_get_total_physical_lines(sb), 2);

    talloc_free(ctx);
}

END_TEST
// Test: Get total physical lines with wrapping
START_TEST(test_scrollback_get_total_physical_lines_wrapping)
{
    TALLOC_CTX *ctx = talloc_new(NULL);

    ik_scrollback_t *sb = ik_scrollback_create(ctx, 40);

    // 80 chars will wrap to 2 physical lines at width 40
    char long_line[81];
    memset(long_line, 'a', 80);
    long_line[80] = '\0';

    res_t res = ik_scrollback_append_line(sb, long_line, 80);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(ik_scrollback_get_total_physical_lines(sb), 2);

    talloc_free(ctx);
}

END_TEST
// Test: Get line text - valid index
START_TEST(test_scrollback_get_line_text_valid)
{
    TALLOC_CTX *ctx = talloc_new(NULL);

    ik_scrollback_t *sb = ik_scrollback_create(ctx, 80);

    const char *line1 = "first line";
    const char *line2 = "second line";

    res_t res = ik_scrollback_append_line(sb, line1, strlen(line1));
    ck_assert(is_ok(&res));
    res = ik_scrollback_append_line(sb, line2, strlen(line2));
    ck_assert(is_ok(&res));

    // Get first line
    const char *text = NULL;
    size_t length = 0;
    res = ik_scrollback_get_line_text(sb, 0, &text, &length);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(length, strlen(line1));
    ck_assert_mem_eq(text, line1, length);

    // Get second line
    res = ik_scrollback_get_line_text(sb, 1, &text, &length);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(length, strlen(line2));
    ck_assert_mem_eq(text, line2, length);

    talloc_free(ctx);
}

END_TEST
// Test: Get line text - invalid index
START_TEST(test_scrollback_get_line_text_invalid)
{
    TALLOC_CTX *ctx = talloc_new(NULL);

    ik_scrollback_t *sb = ik_scrollback_create(ctx, 80);

    res_t res = ik_scrollback_append_line(sb, "line", 4);
    ck_assert(is_ok(&res));

    // Try to get line at index 1 (only line 0 exists)
    const char *text = NULL;
    size_t length = 0;
    res = ik_scrollback_get_line_text(sb, 1, &text, &length);
    ck_assert(is_err(&res));

    talloc_free(ctx);
}

END_TEST
// Test: Find logical line at physical row - single line entries
START_TEST(test_scrollback_find_line_single)
{
    TALLOC_CTX *ctx = talloc_new(NULL);

    ik_scrollback_t *sb = ik_scrollback_create(ctx, 80);

    res_t res = ik_scrollback_append_line(sb, "line 0", 6);
    ck_assert(is_ok(&res));
    res = ik_scrollback_append_line(sb, "line 1", 6);
    ck_assert(is_ok(&res));
    res = ik_scrollback_append_line(sb, "line 2", 6);
    ck_assert(is_ok(&res));

    // Physical row 0 should be line 0, offset 0
    size_t line_index, row_offset;
    res = ik_scrollback_find_logical_line_at_physical_row(sb, 0, &line_index, &row_offset);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(line_index, 0);
    ck_assert_uint_eq(row_offset, 0);

    // Physical row 1 should be line 1, offset 0
    res = ik_scrollback_find_logical_line_at_physical_row(sb, 1, &line_index, &row_offset);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(line_index, 1);
    ck_assert_uint_eq(row_offset, 0);

    // Physical row 2 should be line 2, offset 0
    res = ik_scrollback_find_logical_line_at_physical_row(sb, 2, &line_index, &row_offset);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(line_index, 2);
    ck_assert_uint_eq(row_offset, 0);

    talloc_free(ctx);
}

END_TEST
// Test: Find logical line at physical row - with wrapping
START_TEST(test_scrollback_find_line_wrapping)
{
    TALLOC_CTX *ctx = talloc_new(NULL);

    ik_scrollback_t *sb = ik_scrollback_create(ctx, 40);

    // First line: short (1 physical line)
    res_t res = ik_scrollback_append_line(sb, "short", 5);
    ck_assert(is_ok(&res));

    // Second line: 80 chars (2 physical lines at width 40)
    char long_line[81];
    memset(long_line, 'a', 80);
    long_line[80] = '\0';
    res = ik_scrollback_append_line(sb, long_line, 80);
    ck_assert(is_ok(&res));

    // Third line: short (1 physical line)
    res = ik_scrollback_append_line(sb, "end", 3);
    ck_assert(is_ok(&res));

    // Total: 4 physical lines (1 + 2 + 1)

    // Physical row 0: line 0, offset 0
    size_t line_index, row_offset;
    res = ik_scrollback_find_logical_line_at_physical_row(sb, 0, &line_index, &row_offset);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(line_index, 0);
    ck_assert_uint_eq(row_offset, 0);

    // Physical row 1: line 1, offset 0 (first row of long line)
    res = ik_scrollback_find_logical_line_at_physical_row(sb, 1, &line_index, &row_offset);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(line_index, 1);
    ck_assert_uint_eq(row_offset, 0);

    // Physical row 2: line 1, offset 1 (second row of long line)
    res = ik_scrollback_find_logical_line_at_physical_row(sb, 2, &line_index, &row_offset);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(line_index, 1);
    ck_assert_uint_eq(row_offset, 1);

    // Physical row 3: line 2, offset 0
    res = ik_scrollback_find_logical_line_at_physical_row(sb, 3, &line_index, &row_offset);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(line_index, 2);
    ck_assert_uint_eq(row_offset, 0);

    talloc_free(ctx);
}

END_TEST
// Test: Find logical line at physical row - out of range
START_TEST(test_scrollback_find_line_out_of_range)
{
    TALLOC_CTX *ctx = talloc_new(NULL);

    ik_scrollback_t *sb = ik_scrollback_create(ctx, 80);

    res_t res = ik_scrollback_append_line(sb, "line", 4);
    ck_assert(is_ok(&res));

    // Try to find line at physical row 1 (only row 0 exists)
    size_t line_index, row_offset;
    res = ik_scrollback_find_logical_line_at_physical_row(sb, 1, &line_index, &row_offset);
    ck_assert(is_err(&res));

    talloc_free(ctx);
}

END_TEST

static Suite *scrollback_query_suite(void)
{
    Suite *s = suite_create("Scrollback Query");
    TCase *tc_core = tcase_create("Core");
    tcase_set_timeout(tc_core, 30);

    tcase_add_test(tc_core, test_scrollback_get_line_count_empty);
    tcase_add_test(tc_core, test_scrollback_get_line_count_with_lines);
    tcase_add_test(tc_core, test_scrollback_get_total_physical_lines_empty);
    tcase_add_test(tc_core, test_scrollback_get_total_physical_lines_single);
    tcase_add_test(tc_core, test_scrollback_get_total_physical_lines_wrapping);
    tcase_add_test(tc_core, test_scrollback_get_line_text_valid);
    tcase_add_test(tc_core, test_scrollback_get_line_text_invalid);
    tcase_add_test(tc_core, test_scrollback_find_line_single);
    tcase_add_test(tc_core, test_scrollback_find_line_wrapping);
    tcase_add_test(tc_core, test_scrollback_find_line_out_of_range);

    suite_add_tcase(s, tc_core);
    return s;
}

int main(void)
{
    int number_failed;
    Suite *s = scrollback_query_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
