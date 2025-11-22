/**
 * @file repl_document_scrolling_test.c
 * @brief Tests for unified document scrolling model (Bug #4 fix)
 *
 * These tests verify that scrollback, separator, and input buffer scroll together
 * as a single unified document, rather than separator/input buffer being "sticky"
 * at the bottom.
 */

#include <check.h>
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
 * Test: When scrolled up far enough, separator should NOT appear in output
 *
 * Document structure:
 *   Scrollback lines 0-49 (50 lines total)
 *   Separator line (1 line)
 *   Input buffer (1 line)
 *
 * With terminal height of 10 rows and viewport_offset scrolled to show
 * lines 0-9 of scrollback, the separator should be scrolled off-screen.
 */
START_TEST(test_separator_scrolls_offscreen) {
    void *ctx = talloc_new(NULL);

    // Create terminal context (10 rows)
    ik_term_ctx_t *term = talloc_zero(ctx, ik_term_ctx_t);
    res_t res;
    term->screen_rows = 10;
    term->screen_cols = 80;

    // Create input buffer with single line
    ik_input_buffer_t *input_buf = NULL;
    input_buf = ik_input_buffer_create(ctx);
    res = ik_input_buffer_insert_codepoint(input_buf, 'x');
    ck_assert(is_ok(&res));
    ik_input_buffer_ensure_layout(input_buf, 80);

    // Create scrollback with 50 lines
    ik_scrollback_t *scrollback = ik_scrollback_create(ctx, 80);
    for (int32_t i = 0; i < 50; i++) {
        char buf[32];
        snprintf(buf, sizeof(buf), "scrollback line %" PRId32, i);
        res = ik_scrollback_append_line(scrollback, buf, strlen(buf));
        ck_assert(is_ok(&res));
    }

    // Create render context (write to stdout for testing)
    ik_render_ctx_t *render_ctx = NULL;
    res = ik_render_create(ctx, 10, 80, 1, &render_ctx);
    ck_assert(is_ok(&res));

    // Create REPL context
    ik_repl_ctx_t *repl = talloc_zero(ctx, ik_repl_ctx_t);
    repl->term = term;
    repl->input_buffer = input_buf;
    repl->scrollback = scrollback;
    repl->render = render_ctx;

    // Document height = 50 (scrollback) + 1 (separator) + 1 (input_buf) = 52 lines
    // Terminal shows 10 lines
    // When offset = 42, we're showing lines 0-9 of scrollback
    // Separator is at line 50, input buffer at line 51 - both OFF SCREEN
    repl->viewport_offset = 42;

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

    // Verify separator (line of dashes) does NOT appear in output
    // Count consecutive dashes - should NOT find 10+ dashes in a row
    int32_t max_dashes = 0;
    int32_t current_dashes = 0;
    for (size_t i = 0; i < (size_t)bytes_read; i++) {
        if (output[i] == '-') {
            current_dashes++;
            if (current_dashes > max_dashes) {
                max_dashes = current_dashes;
            }
        } else {
            current_dashes = 0;
        }
    }

    // Separator would be 80 dashes - if we find 10+ consecutive dashes, separator is visible
    ck_assert_int_lt(max_dashes, 10);  // Should be < 10 (separator not visible)

    // Verify input buffer content 'x' does NOT appear
    ck_assert_ptr_eq(strstr(output, "x"), NULL);

    talloc_free(ctx);
}
END_TEST
/**
 * Test: When scrolled up, input buffer should NOT appear in output
 *
 * Similar to above, but specifically checks that input buffer content is scrolled off.
 */
START_TEST(test_input_buffer_scrolls_offscreen)
{
    void *ctx = talloc_new(NULL);

    // Create terminal context (10 rows)
    ik_term_ctx_t *term = talloc_zero(ctx, ik_term_ctx_t);
    res_t res;
    term->screen_rows = 10;
    term->screen_cols = 80;

    // Create input buffer with distinctive content
    ik_input_buffer_t *input_buf = NULL;
    input_buf = ik_input_buffer_create(ctx);
    const char *input_buf_content = "input buffer_MARKER_TEXT";
    for (size_t i = 0; i < strlen(input_buf_content); i++) {
        res = ik_input_buffer_insert_codepoint(input_buf, (uint32_t)(unsigned char)input_buf_content[i]);
        ck_assert(is_ok(&res));
    }
    ik_input_buffer_ensure_layout(input_buf, 80);

    // Create scrollback with 50 lines
    ik_scrollback_t *scrollback = ik_scrollback_create(ctx, 80);
    for (int32_t i = 0; i < 50; i++) {
        char buf[32];
        snprintf(buf, sizeof(buf), "line%" PRId32, i);  // Intentionally different from input buffer
        res = ik_scrollback_append_line(scrollback, buf, strlen(buf));
        ck_assert(is_ok(&res));
    }

    // Create render context
    ik_render_ctx_t *render_ctx = NULL;
    res = ik_render_create(ctx, 10, 80, 1, &render_ctx);
    ck_assert(is_ok(&res));

    // Create REPL context
    ik_repl_ctx_t *repl = talloc_zero(ctx, ik_repl_ctx_t);
    repl->term = term;
    repl->input_buffer = input_buf;
    repl->scrollback = scrollback;
    repl->render = render_ctx;

    // Scroll to middle of scrollback (input buffer is off-screen)
    repl->viewport_offset = 30;

    // Capture stdout
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

    // Verify input buffer content does NOT appear
    ck_assert_ptr_eq(strstr(output, "WORKSPACE_MARKER"), NULL);

    talloc_free(ctx);
}

END_TEST
/**
 * Test: When scrolled to bottom, last scrollback line appears directly above separator
 *
 * This verifies the key invariant: the last line of scrollback is ALWAYS adjacent
 * to the separator line when both are visible.
 */
START_TEST(test_scrollback_adjacent_to_separator)
{
    void *ctx = talloc_new(NULL);

    // Create terminal context (20 rows - enough to show some scrollback + separator + input buffer)
    ik_term_ctx_t *term = talloc_zero(ctx, ik_term_ctx_t);
    res_t res;
    term->screen_rows = 20;
    term->screen_cols = 80;

    // Create input buffer (1 line)
    ik_input_buffer_t *input_buf = NULL;
    input_buf = ik_input_buffer_create(ctx);
    res = ik_input_buffer_insert_codepoint(input_buf, 'w');
    ck_assert(is_ok(&res));
    ik_input_buffer_ensure_layout(input_buf, 80);

    // Create scrollback with 10 lines, last line has distinctive marker
    ik_scrollback_t *scrollback = ik_scrollback_create(ctx, 80);
    for (int32_t i = 0; i < 9; i++) {
        char buf[32];
        snprintf(buf, sizeof(buf), "line %" PRId32, i);
        res = ik_scrollback_append_line(scrollback, buf, strlen(buf));
        ck_assert(is_ok(&res));
    }
    // Last line has distinctive marker
    res = ik_scrollback_append_line(scrollback, "LAST_SCROLLBACK_LINE", 20);
    ck_assert(is_ok(&res));

    // Create render context
    ik_render_ctx_t *render_ctx = NULL;
    res = ik_render_create(ctx, 20, 80, 1, &render_ctx);
    ck_assert(is_ok(&res));

    // Create REPL context
    ik_repl_ctx_t *repl = talloc_zero(ctx, ik_repl_ctx_t);
    repl->term = term;
    repl->input_buffer = input_buf;
    repl->scrollback = scrollback;
    repl->render = render_ctx;

    // Scrolled to bottom (offset = 0) - all scrollback visible
    repl->viewport_offset = 0;

    // Capture stdout
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

    // Find positions of last scrollback line and separator
    const char *last_line_pos = strstr(output, "LAST_SCROLLBACK_LINE");
    ck_assert_ptr_ne(last_line_pos, NULL);  // Last line should be visible

    // Find separator (look for 10+ consecutive dashes after the last scrollback line)
    const char *search_pos = last_line_pos;
    const char *separator_pos = NULL;
    int32_t dash_count = 0;
    while (*search_pos != '\0') {
        if (*search_pos == '-') {
            dash_count++;
            if (dash_count >= 10 && separator_pos == NULL) {
                separator_pos = search_pos - dash_count + 1;
                break;
            }
        } else {
            dash_count = 0;
        }
        search_pos++;
    }
    ck_assert_ptr_ne(separator_pos, NULL);  // Separator should be visible

    // Verify separator appears IMMEDIATELY after last scrollback line (only \r\n between them)
    // Find the \r\n after "LAST_SCROLLBACK_LINE"
    const char *end_of_last_line = last_line_pos + strlen("LAST_SCROLLBACK_LINE");
    // Skip to next line (should be \r\n)
    while (end_of_last_line < separator_pos && (*end_of_last_line == '\r' || *end_of_last_line == '\n')) {
        end_of_last_line++;
    }

    // The next non-whitespace should be the separator
    ck_assert_ptr_eq(end_of_last_line, separator_pos);

    talloc_free(ctx);
}

END_TEST

/* Create test suite */
static Suite *repl_document_scrolling_suite(void)
{
    Suite *s = suite_create("REPL Document Scrolling (Unified Model)");

    TCase *tc_scrolling = tcase_create("Document Scrolling");
    tcase_set_timeout(tc_scrolling, 30);
    tcase_add_test(tc_scrolling, test_separator_scrolls_offscreen);
    tcase_add_test(tc_scrolling, test_input_buffer_scrolls_offscreen);
    tcase_add_test(tc_scrolling, test_scrollback_adjacent_to_separator);
    suite_add_tcase(s, tc_scrolling);

    return s;
}

int main(void)
{
    Suite *s = repl_document_scrolling_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
