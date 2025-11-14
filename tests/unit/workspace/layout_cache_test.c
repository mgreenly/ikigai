/**
 * @file layout_cache_test.c
 * @brief Unit tests for workspace layout caching
 */

#include <check.h>
#include <signal.h>
#include <talloc.h>
#include "../../../src/workspace.h"
#include "../../test_utils.h"

/* Test: Initial state - no layout cached */
START_TEST(test_workspace_initial_state) {
    void *ctx = talloc_new(NULL);
    ik_workspace_t *workspace = NULL;

    /* Create workspace */
    res_t res = ik_workspace_create(ctx, &workspace);
    ck_assert(is_ok(&res));

    /* Initial state: layout should be dirty, physical_lines should be 0 */
    ck_assert_int_eq(workspace->layout_dirty, 1);
    ck_assert_uint_eq(workspace->physical_lines, 0);
    ck_assert_int_eq(workspace->cached_width, 0);

    talloc_free(ctx);
}
END_TEST
/* Test: Ensure layout - first time (dirty) */
START_TEST(test_workspace_ensure_layout_initial)
{
    void *ctx = talloc_new(NULL);
    ik_workspace_t *workspace = NULL;
    int32_t terminal_width = 80;

    ik_workspace_create(ctx, &workspace);

    /* Add single-line text (no wrapping) */
    ik_workspace_insert_codepoint(workspace, 'h');
    ik_workspace_insert_codepoint(workspace, 'e');
    ik_workspace_insert_codepoint(workspace, 'l');
    ik_workspace_insert_codepoint(workspace, 'l');
    ik_workspace_insert_codepoint(workspace, 'o');

    /* Ensure layout */
    res_t res = ik_workspace_ensure_layout(workspace, terminal_width);
    ck_assert(is_ok(&res));

    /* Verify layout was calculated */
    ck_assert_int_eq(workspace->layout_dirty, 0);
    ck_assert_int_eq(workspace->cached_width, terminal_width);
    ck_assert_uint_eq(workspace->physical_lines, 1);  /* Single line, no wrapping */

    talloc_free(ctx);
}

END_TEST
/* Test: Ensure layout - clean cache (no recalculation) */
START_TEST(test_workspace_ensure_layout_clean)
{
    void *ctx = talloc_new(NULL);
    ik_workspace_t *workspace = NULL;
    int32_t terminal_width = 80;

    ik_workspace_create(ctx, &workspace);
    ik_workspace_insert_codepoint(workspace, 'h');
    ik_workspace_insert_codepoint(workspace, 'i');

    /* First ensure layout */
    ik_workspace_ensure_layout(workspace, terminal_width);

    /* Manually mark as clean to verify no recalculation */
    workspace->layout_dirty = 0;
    size_t prev_physical = workspace->physical_lines;

    /* Second ensure layout with same width - should be no-op */
    res_t res = ik_workspace_ensure_layout(workspace, terminal_width);
    ck_assert(is_ok(&res));
    ck_assert_int_eq(workspace->layout_dirty, 0);
    ck_assert_uint_eq(workspace->physical_lines, prev_physical);

    talloc_free(ctx);
}

END_TEST
/* Test: Ensure layout - terminal resize */
START_TEST(test_workspace_ensure_layout_resize)
{
    void *ctx = talloc_new(NULL);
    ik_workspace_t *workspace = NULL;

    ik_workspace_create(ctx, &workspace);

    /* Add text that will wrap differently at different widths */
    const char *text = "This is a long line that will wrap at different terminal widths";
    for (const char *p = text; *p; p++) {
        ik_workspace_insert_codepoint(workspace, (uint32_t)*p);
    }

    /* Ensure layout at width 80 */
    ik_workspace_ensure_layout(workspace, 80);
    size_t lines_at_80 = workspace->physical_lines;

    /* Ensure layout at width 40 (should wrap more) */
    ik_workspace_ensure_layout(workspace, 40);
    size_t lines_at_40 = workspace->physical_lines;

    /* More wrapping at narrower width */
    ck_assert_uint_gt(lines_at_40, lines_at_80);
    ck_assert_int_eq(workspace->cached_width, 40);

    talloc_free(ctx);
}

END_TEST
/* Test: Invalidate layout */
START_TEST(test_workspace_invalidate_layout)
{
    void *ctx = talloc_new(NULL);
    ik_workspace_t *workspace = NULL;
    int32_t terminal_width = 80;

    ik_workspace_create(ctx, &workspace);
    ik_workspace_insert_codepoint(workspace, 'h');
    ik_workspace_insert_codepoint(workspace, 'i');

    /* Ensure layout */
    ik_workspace_ensure_layout(workspace, terminal_width);
    ck_assert_int_eq(workspace->layout_dirty, 0);

    /* Invalidate layout */
    ik_workspace_invalidate_layout(workspace);
    ck_assert_int_eq(workspace->layout_dirty, 1);

    talloc_free(ctx);
}

END_TEST
/* Test: Get physical lines */
START_TEST(test_workspace_get_physical_lines)
{
    void *ctx = talloc_new(NULL);
    ik_workspace_t *workspace = NULL;
    int32_t terminal_width = 80;

    ik_workspace_create(ctx, &workspace);
    ik_workspace_insert_codepoint(workspace, 'h');
    ik_workspace_insert_codepoint(workspace, 'i');

    /* Before ensuring layout */
    size_t physical = ik_workspace_get_physical_lines(workspace);
    ck_assert_uint_eq(physical, 0);

    /* After ensuring layout */
    ik_workspace_ensure_layout(workspace, terminal_width);
    physical = ik_workspace_get_physical_lines(workspace);
    ck_assert_uint_eq(physical, 1);

    talloc_free(ctx);
}

END_TEST
/* Test: Layout calculation - empty workspace */
START_TEST(test_workspace_layout_empty)
{
    void *ctx = talloc_new(NULL);
    ik_workspace_t *workspace = NULL;

    ik_workspace_create(ctx, &workspace);

    /* Ensure layout for empty workspace */
    res_t res = ik_workspace_ensure_layout(workspace, 80);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(workspace->physical_lines, 1);  /* Empty workspace still occupies 1 line */

    talloc_free(ctx);
}

END_TEST
/* Test: Layout calculation - single line (no newline) */
START_TEST(test_workspace_layout_single_line_no_wrap)
{
    void *ctx = talloc_new(NULL);
    ik_workspace_t *workspace = NULL;

    ik_workspace_create(ctx, &workspace);

    /* Add short text */
    ik_workspace_insert_codepoint(workspace, 'h');
    ik_workspace_insert_codepoint(workspace, 'i');

    ik_workspace_ensure_layout(workspace, 80);
    ck_assert_uint_eq(workspace->physical_lines, 1);

    talloc_free(ctx);
}

END_TEST
/* Test: Layout calculation - single line with wrapping */
START_TEST(test_workspace_layout_single_line_wrap)
{
    void *ctx = talloc_new(NULL);
    ik_workspace_t *workspace = NULL;

    ik_workspace_create(ctx, &workspace);

    /* Add text that wraps at width 10: "hello world" = 11 chars, wraps to 2 lines */
    const char *text = "hello world";
    for (const char *p = text; *p; p++) {
        ik_workspace_insert_codepoint(workspace, (uint32_t)*p);
    }

    ik_workspace_ensure_layout(workspace, 10);
    ck_assert_uint_eq(workspace->physical_lines, 2);  /* 11 chars / 10 width = 2 lines */

    talloc_free(ctx);
}

END_TEST
/* Test: Layout calculation - multi-line with newlines */
START_TEST(test_workspace_layout_multiline)
{
    void *ctx = talloc_new(NULL);
    ik_workspace_t *workspace = NULL;

    ik_workspace_create(ctx, &workspace);

    /* Add 3 logical lines */
    ik_workspace_insert_codepoint(workspace, 'l');
    ik_workspace_insert_codepoint(workspace, 'i');
    ik_workspace_insert_codepoint(workspace, 'n');
    ik_workspace_insert_codepoint(workspace, 'e');
    ik_workspace_insert_codepoint(workspace, '1');
    ik_workspace_insert_newline(workspace);
    ik_workspace_insert_codepoint(workspace, 'l');
    ik_workspace_insert_codepoint(workspace, 'i');
    ik_workspace_insert_codepoint(workspace, 'n');
    ik_workspace_insert_codepoint(workspace, 'e');
    ik_workspace_insert_codepoint(workspace, '2');
    ik_workspace_insert_newline(workspace);
    ik_workspace_insert_codepoint(workspace, 'l');
    ik_workspace_insert_codepoint(workspace, 'i');
    ik_workspace_insert_codepoint(workspace, 'n');
    ik_workspace_insert_codepoint(workspace, 'e');
    ik_workspace_insert_codepoint(workspace, '3');

    ik_workspace_ensure_layout(workspace, 80);
    ck_assert_uint_eq(workspace->physical_lines, 3);  /* 3 logical lines, no wrapping */

    talloc_free(ctx);
}

END_TEST
/* Test: Layout calculation - multi-line with wrapping */
START_TEST(test_workspace_layout_multiline_wrap)
{
    void *ctx = talloc_new(NULL);
    ik_workspace_t *workspace = NULL;

    ik_workspace_create(ctx, &workspace);

    /* Line 1: "hello world" (11 chars, wraps to 2 physical at width 10) */
    const char *line1 = "hello world";
    for (const char *p = line1; *p; p++) {
        ik_workspace_insert_codepoint(workspace, (uint32_t)*p);
    }
    ik_workspace_insert_newline(workspace);

    /* Line 2: "hi" (2 chars, 1 physical line) */
    ik_workspace_insert_codepoint(workspace, 'h');
    ik_workspace_insert_codepoint(workspace, 'i');

    ik_workspace_ensure_layout(workspace, 10);
    ck_assert_uint_eq(workspace->physical_lines, 3);  /* 2 + 1 = 3 physical lines */

    talloc_free(ctx);
}

END_TEST
/* Test: Layout calculation - UTF-8 content */
START_TEST(test_workspace_layout_utf8)
{
    void *ctx = talloc_new(NULL);
    ik_workspace_t *workspace = NULL;

    ik_workspace_create(ctx, &workspace);

    /* Add UTF-8 content: "你好" (2 wide chars = 4 display width) */
    ik_workspace_insert_codepoint(workspace, 0x4F60);  /* 你 */
    ik_workspace_insert_codepoint(workspace, 0x597D);  /* 好 */

    ik_workspace_ensure_layout(workspace, 80);
    ck_assert_uint_eq(workspace->physical_lines, 1);  /* Fits in one line */

    /* Narrow width: should wrap */
    ik_workspace_ensure_layout(workspace, 3);
    ck_assert_uint_eq(workspace->physical_lines, 2);  /* 4 display width / 3 = 2 lines */

    talloc_free(ctx);
}

END_TEST
/* Test: Text modifications invalidate layout */
START_TEST(test_workspace_text_modification_invalidates_layout)
{
    void *ctx = talloc_new(NULL);
    ik_workspace_t *workspace = NULL;

    ik_workspace_create(ctx, &workspace);
    ik_workspace_insert_codepoint(workspace, 'h');
    ik_workspace_insert_codepoint(workspace, 'i');

    /* Ensure layout */
    ik_workspace_ensure_layout(workspace, 80);
    ck_assert_int_eq(workspace->layout_dirty, 0);

    /* Insert character - should invalidate */
    ik_workspace_insert_codepoint(workspace, '!');
    ck_assert_int_eq(workspace->layout_dirty, 1);

    /* Re-ensure */
    ik_workspace_ensure_layout(workspace, 80);
    ck_assert_int_eq(workspace->layout_dirty, 0);

    /* Backspace - should invalidate */
    ik_workspace_backspace(workspace);
    ck_assert_int_eq(workspace->layout_dirty, 1);

    /* Re-ensure */
    ik_workspace_ensure_layout(workspace, 80);
    ck_assert_int_eq(workspace->layout_dirty, 0);

    /* Delete - should invalidate */
    ik_workspace_cursor_left(workspace);
    ik_workspace_delete(workspace);
    ck_assert_int_eq(workspace->layout_dirty, 1);

    talloc_free(ctx);
}

END_TEST
/* Test: Layout calculation - empty lines (just newlines) */
START_TEST(test_workspace_layout_empty_lines)
{
    void *ctx = talloc_new(NULL);
    ik_workspace_t *workspace = NULL;

    ik_workspace_create(ctx, &workspace);

    /* Add 3 newlines: "\n\n\n" creates 4 lines (3 terminated + 1 after last) */
    ik_workspace_insert_newline(workspace);
    ik_workspace_insert_newline(workspace);
    ik_workspace_insert_newline(workspace);

    ik_workspace_ensure_layout(workspace, 80);
    ck_assert_uint_eq(workspace->physical_lines, 4);  /* 3 newlines = 4 lines */

    talloc_free(ctx);
}

END_TEST
/* Test: Layout calculation - zero-width characters */
START_TEST(test_workspace_layout_zero_width)
{
    void *ctx = talloc_new(NULL);
    ik_workspace_t *workspace = NULL;

    ik_workspace_create(ctx, &workspace);

    /* Add zero-width space (U+200B) */
    ik_workspace_insert_codepoint(workspace, 0x200B);
    ik_workspace_insert_codepoint(workspace, 0x200B);

    ik_workspace_ensure_layout(workspace, 80);
    ck_assert_uint_eq(workspace->physical_lines, 1);  /* Zero-width characters still occupy 1 line */

    talloc_free(ctx);
}

END_TEST
/* Test: NULL parameter assertions */
START_TEST(test_workspace_ensure_layout_null_asserts)
{
    /* workspace cannot be NULL - should abort */
    ik_workspace_ensure_layout(NULL, 80);
}

END_TEST START_TEST(test_workspace_invalidate_layout_null_asserts)
{
    /* workspace cannot be NULL - should abort */
    ik_workspace_invalidate_layout(NULL);
}

END_TEST START_TEST(test_workspace_get_physical_lines_null_asserts)
{
    /* workspace cannot be NULL - should abort */
    ik_workspace_get_physical_lines(NULL);
}

END_TEST

static Suite *workspace_layout_cache_suite(void)
{
    Suite *s = suite_create("Workspace Layout Cache");
    TCase *tc_core = tcase_create("Core");
    TCase *tc_assertions = tcase_create("Assertions");

    /* Normal tests */
    tcase_add_test(tc_core, test_workspace_initial_state);
    tcase_add_test(tc_core, test_workspace_ensure_layout_initial);
    tcase_add_test(tc_core, test_workspace_ensure_layout_clean);
    tcase_add_test(tc_core, test_workspace_ensure_layout_resize);
    tcase_add_test(tc_core, test_workspace_invalidate_layout);
    tcase_add_test(tc_core, test_workspace_get_physical_lines);
    tcase_add_test(tc_core, test_workspace_layout_empty);
    tcase_add_test(tc_core, test_workspace_layout_single_line_no_wrap);
    tcase_add_test(tc_core, test_workspace_layout_single_line_wrap);
    tcase_add_test(tc_core, test_workspace_layout_multiline);
    tcase_add_test(tc_core, test_workspace_layout_multiline_wrap);
    tcase_add_test(tc_core, test_workspace_layout_utf8);
    tcase_add_test(tc_core, test_workspace_text_modification_invalidates_layout);
    tcase_add_test(tc_core, test_workspace_layout_empty_lines);
    tcase_add_test(tc_core, test_workspace_layout_zero_width);

    /* Assertion tests */
    tcase_add_test_raise_signal(tc_assertions, test_workspace_ensure_layout_null_asserts, SIGABRT);
    tcase_add_test_raise_signal(tc_assertions, test_workspace_invalidate_layout_null_asserts, SIGABRT);
    tcase_add_test_raise_signal(tc_assertions, test_workspace_get_physical_lines_null_asserts, SIGABRT);

    suite_add_tcase(s, tc_core);
    suite_add_tcase(s, tc_assertions);
    return s;
}

int main(void)
{
    int number_failed;
    Suite *s = workspace_layout_cache_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
