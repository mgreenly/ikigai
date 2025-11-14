/**
 * @file repl_combined_render_test.c
 * @brief Unit tests for combined scrollback + workspace rendering (Phase 4 Task 4.4)
 */

#include <check.h>
#include <talloc.h>
#include <stdlib.h>
#include <string.h>
#include "../../../src/repl.h"
#include "../../test_utils.h"

// Mock write() implementation to avoid actual terminal writes
static char *mock_write_buffer = NULL;
static size_t mock_write_size = 0;

// Mock wrapper declaration
ssize_t ik_write_wrapper(int fd, const void *buf, size_t count);

ssize_t ik_write_wrapper(int fd, const void *buf, size_t count)
{
    (void)fd; // Unused in mock

    // Capture the write
    if (mock_write_buffer != NULL) {
        free(mock_write_buffer);
    }
    mock_write_buffer = malloc(count + 1);
    if (mock_write_buffer != NULL) {
        memcpy(mock_write_buffer, buf, count);
        mock_write_buffer[count] = '\0';
        mock_write_size = count;
    }

    return (ssize_t)count;
}

static void mock_write_reset(void)
{
    if (mock_write_buffer != NULL) {
        free(mock_write_buffer);
        mock_write_buffer = NULL;
    }
    mock_write_size = 0;
}

/* Test: Render frame with empty scrollback (workspace only) */
START_TEST(test_render_frame_empty_scrollback) {
    mock_write_reset();
    void *ctx = talloc_new(NULL);

    // Create minimal REPL context for rendering
    ik_term_ctx_t *term = talloc_zero(ctx, ik_term_ctx_t);
    term->screen_rows = 24;
    term->screen_cols = 80;
    term->tty_fd = -1;  // Not actually writing

    ik_render_ctx_t *render = NULL;
    res_t res = ik_render_create(ctx, term->screen_rows, term->screen_cols, term->tty_fd, &render);
    ck_assert(is_ok(&res));

    ik_workspace_t *workspace = NULL;
    res = ik_workspace_create(ctx, &workspace);
    ck_assert(is_ok(&res));

    ik_scrollback_t *scrollback = NULL;
    res = ik_scrollback_create(ctx, term->screen_cols, &scrollback);
    ck_assert(is_ok(&res));

    ik_repl_ctx_t *repl = talloc_zero(ctx, ik_repl_ctx_t);
    repl->term = term;
    repl->render = render;
    repl->workspace = workspace;
    repl->scrollback = scrollback;
    repl->viewport_offset = 0;

    // Render frame - should succeed even with empty scrollback
    res = ik_repl_render_frame(repl);
    ck_assert(is_ok(&res));

    talloc_free(ctx);
    mock_write_reset();
}
END_TEST
/* Test: Render frame with scrollback content */
START_TEST(test_render_frame_with_scrollback)
{
    mock_write_reset();
    void *ctx = talloc_new(NULL);

    ik_term_ctx_t *term = talloc_zero(ctx, ik_term_ctx_t);
    term->screen_rows = 24;
    term->screen_cols = 80;
    term->tty_fd = -1;

    ik_render_ctx_t *render = NULL;
    res_t res = ik_render_create(ctx, term->screen_rows, term->screen_cols, term->tty_fd, &render);
    ck_assert(is_ok(&res));

    ik_workspace_t *workspace = NULL;
    res = ik_workspace_create(ctx, &workspace);
    ck_assert(is_ok(&res));

    // Add some content to workspace
    res = ik_workspace_insert_codepoint(workspace, 'h');
    ck_assert(is_ok(&res));
    res = ik_workspace_insert_codepoint(workspace, 'i');
    ck_assert(is_ok(&res));

    ik_scrollback_t *scrollback = NULL;
    res = ik_scrollback_create(ctx, term->screen_cols, &scrollback);
    ck_assert(is_ok(&res));

    // Add scrollback content
    res = ik_scrollback_append_line(scrollback, "line 1", 6);
    ck_assert(is_ok(&res));
    res = ik_scrollback_append_line(scrollback, "line 2", 6);
    ck_assert(is_ok(&res));

    ik_repl_ctx_t *repl = talloc_zero(ctx, ik_repl_ctx_t);
    repl->term = term;
    repl->render = render;
    repl->workspace = workspace;
    repl->scrollback = scrollback;
    repl->viewport_offset = 0;

    // Render frame - should render both scrollback and workspace
    res = ik_repl_render_frame(repl);
    ck_assert(is_ok(&res));

    talloc_free(ctx);
    mock_write_reset();
}

END_TEST

/* Create test suite */
static Suite *repl_combined_render_suite(void)
{
    Suite *s = suite_create("REPL Combined Rendering");

    TCase *tc_render = tcase_create("Combined Render");
    tcase_add_test(tc_render, test_render_frame_empty_scrollback);
    tcase_add_test(tc_render, test_render_frame_with_scrollback);
    suite_add_tcase(s, tc_render);

    return s;
}

int main(void)
{
    Suite *s = repl_combined_render_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
