#include <check.h>
#include <inttypes.h>
#include <signal.h>
#include <string.h>
#include <talloc.h>

#include "../../../src/scrollback.h"
#include "../../test_utils.h"

// Test: ensure_layout does nothing when width matches cached_width
START_TEST(test_scrollback_ensure_layout_no_change) {
    TALLOC_CTX *ctx = talloc_new(NULL);

    ik_scrollback_t *sb = NULL;
    res_t res = ik_scrollback_create(ctx, 80, &sb);
    ck_assert(is_ok(&res));

    // Append some lines
    res = ik_scrollback_append_line(sb, "Line 1", 6);
    ck_assert(is_ok(&res));
    res = ik_scrollback_append_line(sb, "Line 2", 6);
    ck_assert(is_ok(&res));

    // Verify initial state
    ck_assert_int_eq(sb->cached_width, 80);
    ck_assert_uint_eq(sb->layouts[0].physical_lines, 1);
    ck_assert_uint_eq(sb->layouts[1].physical_lines, 1);
    ck_assert_uint_eq(sb->total_physical_lines, 2);

    // Ensure layout with same width - should do nothing
    ik_scrollback_ensure_layout(sb, 80);

    // Verify nothing changed
    ck_assert_int_eq(sb->cached_width, 80);
    ck_assert_uint_eq(sb->layouts[0].physical_lines, 1);
    ck_assert_uint_eq(sb->layouts[1].physical_lines, 1);
    ck_assert_uint_eq(sb->total_physical_lines, 2);

    talloc_free(ctx);
}
END_TEST
// Test: ensure_layout recalculates when width changes
START_TEST(test_scrollback_ensure_layout_resize)
{
    TALLOC_CTX *ctx = talloc_new(NULL);

    ik_scrollback_t *sb = NULL;
    res_t res = ik_scrollback_create(ctx, 80, &sb);
    ck_assert(is_ok(&res));

    // Append a line that will wrap at different widths
    // 120 chars will be:
    //   - 2 physical lines at width 80 (120 / 80 = 2)
    //   - 1 physical line at width 120 (120 / 120 = 1)
    char long_line[121];
    memset(long_line, 'a', 120);
    long_line[120] = '\0';

    res = ik_scrollback_append_line(sb, long_line, 120);
    ck_assert(is_ok(&res));

    // Verify initial layout at width 80
    ck_assert_int_eq(sb->cached_width, 80);
    ck_assert_uint_eq(sb->layouts[0].display_width, 120);
    ck_assert_uint_eq(sb->layouts[0].physical_lines, 2);
    ck_assert_uint_eq(sb->total_physical_lines, 2);

    // Change terminal width to 120
    ik_scrollback_ensure_layout(sb, 120);

    // Verify layout recalculated
    ck_assert_int_eq(sb->cached_width, 120);
    ck_assert_uint_eq(sb->layouts[0].display_width, 120);  // display_width unchanged
    ck_assert_uint_eq(sb->layouts[0].physical_lines, 1);   // physical_lines recalculated
    ck_assert_uint_eq(sb->total_physical_lines, 1);        // total updated

    talloc_free(ctx);
}

END_TEST
// Test: ensure_layout with multiple lines
START_TEST(test_scrollback_ensure_layout_multiple_lines)
{
    TALLOC_CTX *ctx = talloc_new(NULL);

    ik_scrollback_t *sb = NULL;
    res_t res = ik_scrollback_create(ctx, 40, &sb);
    ck_assert(is_ok(&res));

    // Append lines of various lengths
    res = ik_scrollback_append_line(sb, "Short", 5);  // 1 line at any width
    ck_assert(is_ok(&res));

    char medium[61];
    memset(medium, 'b', 60);
    medium[60] = '\0';
    res = ik_scrollback_append_line(sb, medium, 60);  // 2 lines at width 40, 1 at width 80
    ck_assert(is_ok(&res));

    char long_line[121];
    memset(long_line, 'c', 120);
    long_line[120] = '\0';
    res = ik_scrollback_append_line(sb, long_line, 120);  // 3 lines at width 40, 2 at width 80
    ck_assert(is_ok(&res));

    // Verify initial layout at width 40
    ck_assert_int_eq(sb->cached_width, 40);
    ck_assert_uint_eq(sb->layouts[0].physical_lines, 1);  // 5 / 40 = 1
    ck_assert_uint_eq(sb->layouts[1].physical_lines, 2);  // 60 / 40 = 2
    ck_assert_uint_eq(sb->layouts[2].physical_lines, 3);  // 120 / 40 = 3
    ck_assert_uint_eq(sb->total_physical_lines, 6);       // 1 + 2 + 3

    // Resize to width 80
    ik_scrollback_ensure_layout(sb, 80);

    // Verify all lines recalculated
    ck_assert_int_eq(sb->cached_width, 80);
    ck_assert_uint_eq(sb->layouts[0].physical_lines, 1);  // 5 / 80 = 1
    ck_assert_uint_eq(sb->layouts[1].physical_lines, 1);  // 60 / 80 = 1
    ck_assert_uint_eq(sb->layouts[2].physical_lines, 2);  // 120 / 80 = 2
    ck_assert_uint_eq(sb->total_physical_lines, 4);       // 1 + 1 + 2

    talloc_free(ctx);
}

END_TEST
// Test: ensure_layout with empty scrollback
START_TEST(test_scrollback_ensure_layout_empty)
{
    TALLOC_CTX *ctx = talloc_new(NULL);

    ik_scrollback_t *sb = NULL;
    res_t res = ik_scrollback_create(ctx, 80, &sb);
    ck_assert(is_ok(&res));

    ck_assert_uint_eq(sb->count, 0);
    ck_assert_int_eq(sb->cached_width, 80);

    // Ensure layout on empty scrollback
    ik_scrollback_ensure_layout(sb, 120);

    // Verify width updated but nothing else
    ck_assert_int_eq(sb->cached_width, 120);
    ck_assert_uint_eq(sb->count, 0);
    ck_assert_uint_eq(sb->total_physical_lines, 0);

    talloc_free(ctx);
}

END_TEST
// Test: ensure_layout handles empty lines correctly
START_TEST(test_scrollback_ensure_layout_with_empty_lines)
{
    TALLOC_CTX *ctx = talloc_new(NULL);

    ik_scrollback_t *sb = NULL;
    res_t res = ik_scrollback_create(ctx, 80, &sb);
    ck_assert(is_ok(&res));

    // Append empty line
    res = ik_scrollback_append_line(sb, "", 0);
    ck_assert(is_ok(&res));

    // Verify empty line takes 1 physical line at width 80
    ck_assert_uint_eq(sb->layouts[0].display_width, 0);
    ck_assert_uint_eq(sb->layouts[0].physical_lines, 1);

    // Resize
    ik_scrollback_ensure_layout(sb, 120);

    // Empty line should still take 1 physical line
    ck_assert_uint_eq(sb->layouts[0].display_width, 0);
    ck_assert_uint_eq(sb->layouts[0].physical_lines, 1);
    ck_assert_uint_eq(sb->total_physical_lines, 1);

    talloc_free(ctx);
}

END_TEST

static Suite *scrollback_layout_suite(void)
{
    Suite *s;
    TCase *tc_core;

    s = suite_create("Scrollback Layout");
    tc_core = tcase_create("Core");

    tcase_add_test(tc_core, test_scrollback_ensure_layout_no_change);
    tcase_add_test(tc_core, test_scrollback_ensure_layout_resize);
    tcase_add_test(tc_core, test_scrollback_ensure_layout_multiple_lines);
    tcase_add_test(tc_core, test_scrollback_ensure_layout_empty);
    tcase_add_test(tc_core, test_scrollback_ensure_layout_with_empty_lines);

    suite_add_tcase(s, tc_core);
    return s;
}

int32_t main(void)
{
    int32_t number_failed;
    Suite *s;
    SRunner *sr;

    s = scrollback_layout_suite();
    sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
