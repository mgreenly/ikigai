/**
 * @file repl_viewport_test.c
 * @brief Unit tests for REPL viewport calculation (Phase 4 Task 4.2)
 */

#include <check.h>
#include <talloc.h>
#include "../../../src/repl.h"
#include "../../../src/scrollback.h"
#include "../../test_utils.h"

/* Test: Viewport with empty scrollback (workspace fills screen) */
START_TEST(test_viewport_empty_scrollback) {
    void *ctx = talloc_new(NULL);

    // Create REPL context with mocked terminal (24 rows)
    ik_term_ctx_t *term = talloc_zero(ctx, ik_term_ctx_t);
    term->screen_rows = 24;
    term->screen_cols = 80;

    ik_workspace_t *workspace = NULL;
    res_t res = ik_workspace_create(ctx, &workspace);
    ck_assert(is_ok(&res));

    // Add a few lines to workspace
    res = ik_workspace_insert_codepoint(workspace, 'h');
    ck_assert(is_ok(&res));
    res = ik_workspace_insert_codepoint(workspace, 'i');
    ck_assert(is_ok(&res));

    // Ensure workspace layout
    ik_workspace_ensure_layout(workspace, 80);
    size_t workspace_rows = ik_workspace_get_physical_lines(workspace);
    ck_assert_uint_eq(workspace_rows, 1);  // "hi" is 1 line

    ik_scrollback_t *scrollback = NULL;
    res = ik_scrollback_create(ctx, 80, &scrollback);
    ck_assert(is_ok(&res));

    ik_repl_ctx_t *repl = talloc_zero(ctx, ik_repl_ctx_t);
    repl->term = term;
    repl->workspace = workspace;
    repl->scrollback = scrollback;
    repl->viewport_offset = 0;

    // Calculate viewport
    ik_viewport_t viewport;
    res = ik_repl_calculate_viewport(repl, &viewport);
    ck_assert(is_ok(&res));

    // With empty scrollback, all rows go to workspace
    ck_assert_uint_eq(viewport.scrollback_start_line, 0);
    ck_assert_uint_eq(viewport.scrollback_lines_count, 0);
    // Workspace should start at row 0 (no scrollback)
    ck_assert_uint_eq(viewport.workspace_start_row, 0);

    talloc_free(ctx);
}
END_TEST
/* Test: Viewport with small scrollback (both visible) */
START_TEST(test_viewport_small_scrollback)
{
    void *ctx = talloc_new(NULL);

    // Create REPL context with mocked terminal (24 rows)
    ik_term_ctx_t *term = talloc_zero(ctx, ik_term_ctx_t);
    term->screen_rows = 24;
    term->screen_cols = 80;

    ik_workspace_t *workspace = NULL;
    res_t res = ik_workspace_create(ctx, &workspace);
    ck_assert(is_ok(&res));

    // Add single line to workspace
    res = ik_workspace_insert_codepoint(workspace, 'h');
    ck_assert(is_ok(&res));
    ik_workspace_ensure_layout(workspace, 80);
    size_t workspace_rows = ik_workspace_get_physical_lines(workspace);
    ck_assert_uint_eq(workspace_rows, 1);

    // Create scrollback with 3 lines
    ik_scrollback_t *scrollback = NULL;
    res = ik_scrollback_create(ctx, 80, &scrollback);
    ck_assert(is_ok(&res));
    res = ik_scrollback_append_line(scrollback, "line 1", 6);
    ck_assert(is_ok(&res));
    res = ik_scrollback_append_line(scrollback, "line 2", 6);
    ck_assert(is_ok(&res));
    res = ik_scrollback_append_line(scrollback, "line 3", 6);
    ck_assert(is_ok(&res));

    size_t scrollback_rows = ik_scrollback_get_total_physical_lines(scrollback);
    ck_assert_uint_eq(scrollback_rows, 3);

    ik_repl_ctx_t *repl = talloc_zero(ctx, ik_repl_ctx_t);
    repl->term = term;
    repl->workspace = workspace;
    repl->scrollback = scrollback;
    repl->viewport_offset = 0;  // At bottom

    // Calculate viewport
    ik_viewport_t viewport;
    res = ik_repl_calculate_viewport(repl, &viewport);
    ck_assert(is_ok(&res));

    // Total: 3 scrollback rows + 1 workspace row = 4 rows (fits in 24)
    // All scrollback lines should be visible
    ck_assert_uint_eq(viewport.scrollback_start_line, 0);
    ck_assert_uint_eq(viewport.scrollback_lines_count, 3);
    // Workspace should start after scrollback (row 3)
    ck_assert_uint_eq(viewport.workspace_start_row, 3);

    talloc_free(ctx);
}

END_TEST
/* Test: Viewport with large scrollback (scrollback overflows) */
START_TEST(test_viewport_large_scrollback)
{
    void *ctx = talloc_new(NULL);

    // Create REPL context with small terminal (10 rows)
    ik_term_ctx_t *term = talloc_zero(ctx, ik_term_ctx_t);
    term->screen_rows = 10;
    term->screen_cols = 80;

    ik_workspace_t *workspace = NULL;
    res_t res = ik_workspace_create(ctx, &workspace);
    ck_assert(is_ok(&res));

    // Add 2 lines to workspace
    res = ik_workspace_insert_codepoint(workspace, 'h');
    ck_assert(is_ok(&res));
    res = ik_workspace_insert_newline(workspace);
    ck_assert(is_ok(&res));
    res = ik_workspace_insert_codepoint(workspace, 'i');
    ck_assert(is_ok(&res));
    ik_workspace_ensure_layout(workspace, 80);
    size_t workspace_rows = ik_workspace_get_physical_lines(workspace);
    ck_assert_uint_eq(workspace_rows, 2);

    // Create scrollback with 20 lines (more than terminal)
    ik_scrollback_t *scrollback = NULL;
    res = ik_scrollback_create(ctx, 80, &scrollback);
    ck_assert(is_ok(&res));
    for (int32_t i = 0; i < 20; i++) {
        char buf[32];
        snprintf(buf, sizeof(buf), "line %" PRId32, i);
        res = ik_scrollback_append_line(scrollback, buf, strlen(buf));
        ck_assert(is_ok(&res));
    }

    size_t scrollback_rows = ik_scrollback_get_total_physical_lines(scrollback);
    ck_assert_uint_eq(scrollback_rows, 20);

    ik_repl_ctx_t *repl = talloc_zero(ctx, ik_repl_ctx_t);
    repl->term = term;
    repl->workspace = workspace;
    repl->scrollback = scrollback;
    repl->viewport_offset = 0;  // At bottom

    // Calculate viewport
    ik_viewport_t viewport;
    res = ik_repl_calculate_viewport(repl, &viewport);
    ck_assert(is_ok(&res));

    // Terminal: 10 rows, separator: 1 row, workspace: 2 rows, available for scrollback: 7 rows
    // Should show last 7 lines of scrollback (lines 13-19)
    ck_assert_uint_eq(viewport.scrollback_start_line, 13);
    ck_assert_uint_eq(viewport.scrollback_lines_count, 7);
    // Workspace starts at row 7 (after 7 rows of scrollback)
    ck_assert_uint_eq(viewport.workspace_start_row, 7);

    talloc_free(ctx);
}

END_TEST
/* Test: Viewport offset clamping (don't scroll past top) */
START_TEST(test_viewport_offset_clamping)
{
    void *ctx = talloc_new(NULL);

    // Create REPL context with terminal (10 rows)
    ik_term_ctx_t *term = talloc_zero(ctx, ik_term_ctx_t);
    term->screen_rows = 10;
    term->screen_cols = 80;

    ik_workspace_t *workspace = NULL;
    res_t res = ik_workspace_create(ctx, &workspace);
    ck_assert(is_ok(&res));

    // Add 1 line to workspace
    res = ik_workspace_insert_codepoint(workspace, 'h');
    ck_assert(is_ok(&res));
    ik_workspace_ensure_layout(workspace, 80);
    size_t workspace_rows = ik_workspace_get_physical_lines(workspace);
    ck_assert_uint_eq(workspace_rows, 1);

    // Create scrollback with 20 lines (more than available space)
    ik_scrollback_t *scrollback = NULL;
    res = ik_scrollback_create(ctx, 80, &scrollback);
    ck_assert(is_ok(&res));
    for (int32_t i = 0; i < 20; i++) {
        char buf[32];
        snprintf(buf, sizeof(buf), "line %" PRId32, i);
        res = ik_scrollback_append_line(scrollback, buf, strlen(buf));
        ck_assert(is_ok(&res));
    }

    ik_repl_ctx_t *repl = talloc_zero(ctx, ik_repl_ctx_t);
    repl->term = term;
    repl->workspace = workspace;
    repl->scrollback = scrollback;
    repl->viewport_offset = 100;  // Try to scroll way past top

    // Calculate viewport - should clamp to valid range
    ik_viewport_t viewport;
    res = ik_repl_calculate_viewport(repl, &viewport);
    ck_assert(is_ok(&res));

    // Available space: 10 rows - 1 separator - 1 workspace = 8 rows for scrollback
    // Scrollback has 20 lines, so it overflows
    // With viewport_offset clamped, should show first lines
    ck_assert_uint_eq(viewport.scrollback_start_line, 0);
    ck_assert_uint_eq(viewport.scrollback_lines_count, 8);

    talloc_free(ctx);
}

END_TEST
/* Test: Viewport when terminal height equals workspace height (no room for scrollback) */
START_TEST(test_viewport_no_scrollback_room)
{
    void *ctx = talloc_new(NULL);

    // Create REPL context with terminal that exactly matches workspace height
    ik_term_ctx_t *term = talloc_zero(ctx, ik_term_ctx_t);
    term->screen_rows = 3;  // Exactly 3 rows
    term->screen_cols = 80;

    ik_workspace_t *workspace = NULL;
    res_t res = ik_workspace_create(ctx, &workspace);
    ck_assert(is_ok(&res));

    // Add content that results in 3 physical lines (equals terminal height)
    res = ik_workspace_insert_codepoint(workspace, 'a');
    ck_assert(is_ok(&res));
    res = ik_workspace_insert_newline(workspace);
    ck_assert(is_ok(&res));
    res = ik_workspace_insert_codepoint(workspace, 'b');
    ck_assert(is_ok(&res));
    res = ik_workspace_insert_newline(workspace);
    ck_assert(is_ok(&res));
    res = ik_workspace_insert_codepoint(workspace, 'c');
    ck_assert(is_ok(&res));
    ik_workspace_ensure_layout(workspace, 80);
    size_t workspace_rows = ik_workspace_get_physical_lines(workspace);
    ck_assert_uint_eq(workspace_rows, 3);  // 3 lines exactly

    // Create scrollback with some lines (but there's no room to show them)
    ik_scrollback_t *scrollback = NULL;
    res = ik_scrollback_create(ctx, 80, &scrollback);
    ck_assert(is_ok(&res));
    res = ik_scrollback_append_line(scrollback, "scrollback line 1", 17);
    ck_assert(is_ok(&res));
    res = ik_scrollback_append_line(scrollback, "scrollback line 2", 17);
    ck_assert(is_ok(&res));

    ik_repl_ctx_t *repl = talloc_zero(ctx, ik_repl_ctx_t);
    repl->term = term;
    repl->workspace = workspace;
    repl->scrollback = scrollback;
    repl->viewport_offset = 0;

    // Calculate viewport
    ik_viewport_t viewport;
    res = ik_repl_calculate_viewport(repl, &viewport);
    ck_assert(is_ok(&res));

    // Terminal has 3 rows, workspace needs 3 rows
    // available_for_scrollback = 3 - 3 = 0
    // The false branch of "if (available_for_scrollback > 0)" executes
    // No scrollback should be shown (no room for separator or scrollback)
    ck_assert_uint_eq(viewport.scrollback_start_line, 0);
    ck_assert_uint_eq(viewport.scrollback_lines_count, 0);
    ck_assert_uint_eq(viewport.workspace_start_row, 0);

    talloc_free(ctx);
}

END_TEST

/* Create test suite */
static Suite *repl_viewport_suite(void)
{
    Suite *s = suite_create("REPL Viewport Calculation");

    TCase *tc_viewport = tcase_create("Viewport");
    tcase_add_test(tc_viewport, test_viewport_empty_scrollback);
    tcase_add_test(tc_viewport, test_viewport_small_scrollback);
    tcase_add_test(tc_viewport, test_viewport_large_scrollback);
    tcase_add_test(tc_viewport, test_viewport_offset_clamping);
    tcase_add_test(tc_viewport, test_viewport_no_scrollback_room);
    suite_add_tcase(s, tc_viewport);

    return s;
}

int main(void)
{
    Suite *s = repl_viewport_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
