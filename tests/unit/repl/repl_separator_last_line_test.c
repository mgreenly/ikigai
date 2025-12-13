/**
 * @file repl_separator_last_line_test.c
 * @brief Test separator visibility when it's the last visible line
 *
 * This test verifies that when the separator should be the last visible line,
 * it is actually rendered (not blank).
 */

#include <check.h>
#include "../../../src/agent.h"
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
 * Test: Separator is last visible line
 *
 * Setup:
 *   - Terminal: 10 rows
 *   - Scrollback: 5 lines (rows 0-4)
 *   - Separator: row 5
 *   - Input buffer: row 6
 *
 *   Scroll to show rows 0-9:
 *     - Rows 0-4: scrollback lines 0-4
 *     - Row 5: separator (LAST visible row)
 *     - Rows 6-9: would be input buffer, but only 1 row, so rows 7-9 are blank
 *
 */
START_TEST(test_separator_as_last_visible_line) {
    void *ctx = talloc_new(NULL);

    // Terminal: 10 rows x 80 cols
    ik_term_ctx_t *term = talloc_zero(ctx, ik_term_ctx_t);
    res_t res;
    term->screen_rows = 10;
    term->screen_cols = 80;

    // Create input buffer (1 line)
    ik_input_buffer_t *input_buf = NULL;
    input_buf = ik_input_buffer_create(ctx);
    res = ik_input_buffer_insert_codepoint(input_buf, 'w');
    ck_assert(is_ok(&res));
    ik_input_buffer_ensure_layout(input_buf, 80);

    // Create scrollback with 5 short lines
    ik_scrollback_t *scrollback = ik_scrollback_create(ctx, 80);
    for (int32_t i = 0; i < 5; i++) {
        char buf[32];
        snprintf(buf, sizeof(buf), "scrollback%" PRId32, i);
        res = ik_scrollback_append_line(scrollback, buf, strlen(buf));
        ck_assert(is_ok(&res));
    }

    // Document: 5 scrollback rows (0-4) + separator (5) + input buffer (6) = 7 rows
    // Terminal: 10 rows
    // Entire document fits, no scrolling needed

    // Create render context
    ik_render_ctx_t *render_ctx = NULL;
    res = ik_render_create(ctx, 10, 80, 1, &render_ctx);
    ck_assert(is_ok(&res));

    // Create REPL at bottom (offset=0)
    ik_repl_ctx_t *repl = talloc_zero(ctx, ik_repl_ctx_t);
    ik_shared_ctx_t *shared = talloc_zero(repl, ik_shared_ctx_t);
    repl->shared = shared;
    shared->term = term;

    // Create agent context for display state
    ik_agent_ctx_t *agent = talloc_zero(repl, ik_agent_ctx_t);
    repl->current = agent;
    repl->current->input_buffer = input_buf;
    repl->current->scrollback = scrollback;
    shared->render = render_ctx;
    repl->current->viewport_offset = 0;  // Show entire document

    // Calculate viewport
    ik_viewport_t viewport;
    res = ik_repl_calculate_viewport(repl, &viewport);
    ck_assert(is_ok(&res));

    // Should see all 5 scrollback lines and input buffer at row 6
    ck_assert_uint_eq(viewport.scrollback_start_line, 0);
    ck_assert_uint_eq(viewport.scrollback_lines_count, 5);
    ck_assert_uint_eq(viewport.input_buffer_start_row, 6);  // Separator at row 5, input buffer at row 6

    // Determine separator visibility according to the fix
    // separator_visible when input_buffer_start_row in [1, terminal_rows]
    // input_buffer_start_row = 6, terminal_rows = 10
    // 6 in [1, 10]? Yes, separator should be visible
    bool separator_should_be_visible = viewport.input_buffer_start_row >= 1 &&
                                       viewport.input_buffer_start_row <= (size_t)term->screen_rows;
    ck_assert(separator_should_be_visible);

    // Capture rendered output
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

    // Verify separator appears in output (10+ consecutive dashes)
    int32_t max_dashes = 0;
    int32_t current_dashes = 0;
    for (ssize_t i = 0; i < bytes_read; i++) {
        if (output[i] == '-') {
            current_dashes++;
            if (current_dashes > max_dashes) {
                max_dashes = current_dashes;
            }
        } else {
            current_dashes = 0;
        }
    }

    // Separator visibility: separator not rendered, max_dashes < 10
    // Fix: separator rendered, max_dashes >= 10 (actually 80 for full width)
    ck_assert_int_ge(max_dashes, 10);

    talloc_free(ctx);
}
END_TEST
/**
 * Test: Separator exactly at last row when input buffer off-screen
 *
 * This is the specific Separator visibility case:
 *   - Separator should be at the last visible row (terminal_rows - 1)
 *   - Input buffer is off-screen (input_buffer_start_row == terminal_rows)
 */
START_TEST(test_separator_last_row_input_buffer_offscreen)
{
    void *ctx = talloc_new(NULL);

    // Terminal: 10 rows
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

    // Create scrollback with 19 lines
    ik_scrollback_t *scrollback = ik_scrollback_create(ctx, 80);
    for (int32_t i = 0; i < 19; i++) {
        char buf[32];
        snprintf(buf, sizeof(buf), "line%" PRId32, i);
        res = ik_scrollback_append_line(scrollback, buf, strlen(buf));
        ck_assert(is_ok(&res));
    }

    // Document: 19 scrollback (rows 0-18) + 1 upper_separator (row 19) + 1 input buffer (row 20) + 1 lower_separator (row 21) = 22 rows
    // We want to view rows 10-19 (10 rows):
    //   Rows 10-18: scrollback lines 10-18 (9 rows)
    //   Row 19: upper_separator (LAST visible row - this is Separator visibility!)
    //   Row 20: input buffer (off-screen)
    //   Row 21: lower_separator (off-screen)
    //
    // last_visible = 19, first_visible = 10
    // offset = 22 - 1 - 19 = 2

    // Create render context
    ik_render_ctx_t *render_ctx = NULL;
    res = ik_render_create(ctx, 10, 80, 1, &render_ctx);
    ck_assert(is_ok(&res));

    // Create REPL
    ik_repl_ctx_t *repl = talloc_zero(ctx, ik_repl_ctx_t);
    ik_shared_ctx_t *shared = talloc_zero(repl, ik_shared_ctx_t);
    repl->shared = shared;
    shared->term = term;

    // Create agent context for display state
    ik_agent_ctx_t *agent = talloc_zero(repl, ik_agent_ctx_t);
    repl->current = agent;
    repl->current->input_buffer = input_buf;
    repl->current->scrollback = scrollback;
    shared->render = render_ctx;
    repl->current->viewport_offset = 2;

    // Calculate viewport
    ik_viewport_t viewport;
    res = ik_repl_calculate_viewport(repl, &viewport);
    ck_assert(is_ok(&res));

    // input_buffer_start_row should be exactly terminal_rows (input buffer off-screen)
    // This means separator is at screen row terminal_rows - 1 (last visible row)
    ck_assert_uint_eq(viewport.input_buffer_start_row, 10);  // == terminal_rows

    // Separator should be visible!
    bool separator_visible = viewport.input_buffer_start_row >= 1 &&
                             viewport.input_buffer_start_row <= (size_t)term->screen_rows;
    ck_assert(separator_visible);  // This is the Separator visibility fix!

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

    // Verify separator in output
    int32_t max_dashes = 0;
    int32_t current_dashes = 0;
    for (ssize_t i = 0; i < bytes_read; i++) {
        if (output[i] == '-') {
            current_dashes++;
            if (current_dashes > max_dashes) {
                max_dashes = current_dashes;
            }
        } else {
            current_dashes = 0;
        }
    }

    // Separator should be visible (last line)
    ck_assert_int_ge(max_dashes, 10);

    // Input buffer should NOT be visible
    ck_assert_ptr_eq(strstr(output, "w"), NULL);

    talloc_free(ctx);
}

END_TEST

/* Create test suite */
static Suite *separator_separator_suite(void)
{
    Suite *s = suite_create("Separator visibility: Separator Last Line");

    TCase *tc_sep = tcase_create("Separator");
    tcase_set_timeout(tc_sep, 30);
    tcase_add_test(tc_sep, test_separator_as_last_visible_line);
    tcase_add_test(tc_sep, test_separator_last_row_input_buffer_offscreen);
    suite_add_tcase(s, tc_sep);

    return s;
}

int main(void)
{
    Suite *s = separator_separator_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_VERBOSE);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    ik_test_reset_terminal();

    return (number_failed == 0) ? 0 : 1;
}
