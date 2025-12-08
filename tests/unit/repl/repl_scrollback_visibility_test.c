/**
 * @file repl_scrollback_visibility_test.c
 * @brief Test scrollback visibility when scrolled up
 *
 * Tests that when scrolled up to view scrollback, all viewport rows
 * display scrollback content. Verifies that no lines are missing when
 * the input buffer is scrolled off-screen.
 */

#include <check.h>
#include "../../../src/shared.h"
#include <talloc.h>
#include <string.h>
#include <unistd.h>
#include <inttypes.h>
#include "../../../src/repl.h"
#include "../../../src/scrollback.h"
#include "../../../src/render.h"
#include "../../../src/input_buffer/core.h"
#include "../../test_utils.h"

/**
 * Test: All viewport rows should show scrollback when scrolled up
 *
 * Setup:
 *   - Terminal: 10 rows x 80 cols
 *   - Scrollback: 50 simple lines ("line 0", "line 1", ..., "line 49")
 *   - Workspace: 1 line ("input buffer")
 *   - Scroll to show lines 10-19 (middle of scrollback)
 *
 * Expected: All 10 terminal rows should contain scrollback text
 */
START_TEST(test_scrollback_fills_viewport_when_scrolled_up) {
    void *ctx = talloc_new(NULL);

    // Create terminal context (10 rows x 80 cols)
    ik_term_ctx_t *term = talloc_zero(ctx, ik_term_ctx_t);
    res_t res;
    term->screen_rows = 10;
    term->screen_cols = 80;

    // Create input buffer with simple content
    ik_input_buffer_t *input_buf = NULL;
    input_buf = ik_input_buffer_create(ctx);
    const char *ws_text = "input buffer";
    for (size_t i = 0; i < strlen(ws_text); i++) {
        res = ik_input_buffer_insert_codepoint(input_buf, (uint32_t)(unsigned char)ws_text[i]);
        ck_assert(is_ok(&res));
    }
    ik_input_buffer_ensure_layout(input_buf, 80);

    // Create scrollback with 50 simple lines (no wrapping)
    ik_scrollback_t *scrollback = ik_scrollback_create(ctx, 80);
    for (int32_t i = 0; i < 50; i++) {
        char buf[32];
        snprintf(buf, sizeof(buf), "line %" PRId32, i);
        res = ik_scrollback_append_line(scrollback, buf, strlen(buf));
        ck_assert(is_ok(&res));
    }

    // Create render context
    ik_render_ctx_t *render_ctx = NULL;
    res = ik_render_create(ctx, 10, 80, 1, &render_ctx);
    ck_assert(is_ok(&res));

    // Create REPL context
    ik_repl_ctx_t *repl = talloc_zero(ctx, ik_repl_ctx_t);
    ik_shared_ctx_t *shared = talloc_zero(repl, ik_shared_ctx_t);
    repl->shared = shared;
    shared->term = term;
    repl->input_buffer = input_buf;
    repl->scrollback = scrollback;
    shared->render = render_ctx;

    // Document structure:
    //   Lines 0-49: scrollback (50 lines)
    //   Line 50: separator
    //   Line 51: input buffer
    // Total: 52 lines
    //
    // Set viewport_offset to show lines 10-19 of scrollback
    // When offset = 32, we show document lines 10-19 (all scrollback)
    // last_visible_row = 52 - 1 - 32 = 19
    // first_visible_row = 19 + 1 - 10 = 10
    repl->viewport_offset = 32;

    // Capture stdout to verify output
    int pipefd[2];
    ck_assert_int_eq(pipe(pipefd), 0);
    int saved_stdout = dup(1);
    dup2(pipefd[1], 1);

    // Render frame
    res = ik_repl_render_frame(repl);
    ck_assert(is_ok(&res));

    // Restore stdout and read output
    fflush(stdout);
    dup2(saved_stdout, 1);
    close(pipefd[1]);

    char output[8192] = {0};
    ssize_t bytes_read = read(pipefd[0], output, sizeof(output) - 1);
    ck_assert(bytes_read > 0);
    close(pipefd[0]);
    close(saved_stdout);

    // Parse output to count lines with scrollback content
    // Output format: \x1b[2J (clear) \x1b[H (home) then lines with \r\n endings

    // Find start of content (after clear and home escapes)
    const char *content_start = output;
    // Skip clear screen: \x1b[2J
    if (strncmp(content_start, "\x1b[2J", 4) == 0) {
        content_start += 4;
    }
    // Skip home: \x1b[H
    if (strncmp(content_start, "\x1b[H", 3) == 0) {
        content_start += 3;
    }

    // Count lines containing "line XX" pattern
    int32_t lines_found = 0;
    const char *search_pos = content_start;
    for (int32_t expected_line = 10; expected_line <= 19; expected_line++) {
        char expected_text[32];
        snprintf(expected_text, sizeof(expected_text), "line %" PRId32, expected_line);

        const char *found = strstr(search_pos, expected_text);
        if (found != NULL) {
            lines_found++;
            search_pos = found + strlen(expected_text);
        }
    }

    // Verify all 10 lines are visible (no missing lines)
    ck_assert_int_eq(lines_found, 10);

    // Also verify that input buffer content is NOT visible (it's scrolled off)
    ck_assert_ptr_eq(strstr(output, "input_buf"), NULL);

    talloc_free(ctx);
}
END_TEST
/**
 * Test: Last line visible when scrolled to top
 *
 * Simpler test: scroll all the way to top and verify line 9 is visible
 */
START_TEST(test_scrollback_visible_when_scrolled_to_top)
{
    void *ctx = talloc_new(NULL);

    // Create terminal context (10 rows)
    ik_term_ctx_t *term = talloc_zero(ctx, ik_term_ctx_t);
    res_t res;
    term->screen_rows = 10;
    term->screen_cols = 80;

    // Create input buffer
    ik_input_buffer_t *input_buf = NULL;
    input_buf = ik_input_buffer_create(ctx);
    res = ik_input_buffer_insert_codepoint(input_buf, 'w');
    ck_assert(is_ok(&res));
    ik_input_buffer_ensure_layout(input_buf, 80);

    // Create scrollback with 50 lines
    ik_scrollback_t *scrollback = ik_scrollback_create(ctx, 80);
    for (int32_t i = 0; i < 50; i++) {
        char buf[32];
        snprintf(buf, sizeof(buf), "scrollback %" PRId32, i);
        res = ik_scrollback_append_line(scrollback, buf, strlen(buf));
        ck_assert(is_ok(&res));
    }

    // Create render context
    ik_render_ctx_t *render_ctx = NULL;
    res = ik_render_create(ctx, 10, 80, 1, &render_ctx);
    ck_assert(is_ok(&res));

    // Create REPL and scroll to top (maximum offset)
    ik_repl_ctx_t *repl = talloc_zero(ctx, ik_repl_ctx_t);
    ik_shared_ctx_t *shared = talloc_zero(repl, ik_shared_ctx_t);
    repl->shared = shared;
    shared->term = term;
    repl->input_buffer = input_buf;
    repl->scrollback = scrollback;
    shared->render = render_ctx;

    // Document: 50 scrollback + 1 sep + 1 input buffer = 52 lines
    // Max offset = 52 - 10 = 42, shows lines 0-9
    repl->viewport_offset = 100;  // Will be clamped to 42

    // Capture output
    int pipefd[2];
    ck_assert_int_eq(pipe(pipefd), 0);
    int saved_stdout = dup(1);
    dup2(pipefd[1], 1);

    res = ik_repl_render_frame(repl);
    ck_assert(is_ok(&res));

    fflush(stdout);
    dup2(saved_stdout, 1);
    close(pipefd[1]);

    char output[8192] = {0};
    ssize_t bytes_read = read(pipefd[0], output, sizeof(output) - 1);
    ck_assert(bytes_read > 0);
    close(pipefd[0]);
    close(saved_stdout);

    // Verify we can see all 10 expected lines (0-9)
    ck_assert_ptr_ne(strstr(output, "scrollback 0"), NULL);
    ck_assert_ptr_ne(strstr(output, "scrollback 9"), NULL);

    // Count all visible lines
    int32_t count = 0;
    for (int32_t i = 0; i < 10; i++) {
        char search[32];
        snprintf(search, sizeof(search), "scrollback %" PRId32, i);
        if (strstr(output, search) != NULL) {
            count++;
        }
    }
    ck_assert_int_eq(count, 10);

    talloc_free(ctx);
}

END_TEST

/* Create test suite */
static Suite *scrollback_visibility_suite(void)
{
    Suite *s = suite_create("Scrollback Visibility");

    TCase *tc_visibility = tcase_create("Visibility");
    tcase_set_timeout(tc_visibility, 30);
    tcase_add_test(tc_visibility, test_scrollback_fills_viewport_when_scrolled_up);
    tcase_add_test(tc_visibility, test_scrollback_visible_when_scrolled_to_top);
    suite_add_tcase(s, tc_visibility);

    return s;
}

int main(void)
{
    Suite *s = scrollback_visibility_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    ik_test_reset_terminal();

    return (number_failed == 0) ? 0 : 1;
}
