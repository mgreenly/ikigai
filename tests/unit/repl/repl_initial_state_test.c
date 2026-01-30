#include "agent.h"
/**
 * @file repl_initial_state_test.c
 * @brief Test REPL initial state at startup (Bug #10 regression test)
 *
 * Ensures cursor is visible at startup with empty input buffer.
 * This test would have caught the Bug #10 cursor visibility regression.
 */

#include <check.h>
#include "../../../src/agent.h"
#include "../../../src/shared.h"
#include <talloc.h>
#include <string.h>
#include <unistd.h>
#include "../../../src/repl.h"
#include "../../../src/scrollback.h"
#include "../../../src/render.h"
#include "../../../src/input_buffer/core.h"
#include "../../test_utils_helper.h"

/**
 * Check if buffer contains ANSI cursor positioning escape sequence.
 *
 * Searches for pattern: \x1b[<row>;<col>H where row and col are digits.
 *
 * @param buffer Buffer to search
 * @param size Size of buffer
 * @return true if cursor positioning escape found, false otherwise
 */
static bool contains_cursor_positioning_escape(const char *buffer, size_t size)
{
    for (size_t i = 0; i < size - 4; i++) {
        if (buffer[i] != '\x1b' || buffer[i + 1] != '[') {
            continue;
        }

        // Found ESC[, check for <digits>;<digits>H pattern
        size_t j = i + 2;
        bool has_digit_before_semicolon = false;
        while (j < size && buffer[j] >= '0' && buffer[j] <= '9') {
            has_digit_before_semicolon = true;
            j++;
        }

        if (!has_digit_before_semicolon || j >= size || buffer[j] != ';') {
            continue;
        }

        j++; // Skip semicolon
        bool has_digit_after_semicolon = false;
        while (j < size && buffer[j] >= '0' && buffer[j] <= '9') {
            has_digit_after_semicolon = true;
            j++;
        }

        if (has_digit_after_semicolon && j < size && buffer[j] == 'H') {
            return true;
        }
    }

    return false;
}

/**
 * Test: Initial state - cursor visible at startup with empty input buffer
 *
 * Scenario:
 * - Fresh REPL, no scrollback, empty input buffer
 * - viewport_offset = 0 (at bottom)
 * - Expected: input buffer visible, cursor visible at row 1 (after separator)
 *
 * This test catches the Bug #10 regression where empty input buffer (0 physical lines)
 * was incorrectly marked as off-screen, hiding the cursor.
 */
START_TEST(test_initial_state_cursor_visible) {
    void *ctx = talloc_new(NULL);

    // Terminal: 5 rows x 80 cols
    ik_term_ctx_t *term = talloc_zero(ctx, ik_term_ctx_t);
    res_t res;
    term->screen_rows = 5;
    term->screen_cols = 80;

    // Create empty input buffer
    ik_input_buffer_t *input_buf = NULL;
    input_buf = ik_input_buffer_create(ctx);
    ik_input_buffer_ensure_layout(input_buf, 80);

    // Verify input buffer has 0 physical lines
    size_t physical_lines = ik_input_buffer_get_physical_lines(input_buf);
    ck_assert_uint_eq(physical_lines, 0);

    // Create empty scrollback
    ik_scrollback_t *scrollback = ik_scrollback_create(ctx, 80);

    // Create render context
    ik_render_ctx_t *render_ctx = NULL;
    res = ik_render_create(ctx, 5, 80, 1, &render_ctx);
    ck_assert(is_ok(&res));

    // Create REPL at bottom (offset=0)
    ik_repl_ctx_t *repl = talloc_zero(ctx, ik_repl_ctx_t);
    repl->current = talloc_zero(repl, ik_agent_ctx_t);
    ik_shared_ctx_t *shared = talloc_zero(repl, ik_shared_ctx_t);
    repl->shared = shared;
    shared->term = term;
    shared->render = render_ctx;

    // Create agent context for display state
    ik_agent_ctx_t *agent = talloc_zero(repl, ik_agent_ctx_t);
    repl->current = agent;
    repl->current->input_buffer = input_buf;
    repl->current->scrollback = scrollback;
    repl->current->viewport_offset = 0;  // At bottom (initial state)

    // Calculate viewport
    ik_viewport_t viewport;
    res = ik_repl_calculate_viewport(repl, &viewport);
    ck_assert(is_ok(&res));

    fprintf(stderr, "\n=== Initial State (empty input buffer) ===\n");
    fprintf(stderr, "viewport_offset: %zu\n", repl->current->viewport_offset);
    fprintf(stderr, "input_buffer_start_row: %zu\n", viewport.input_buffer_start_row);
    fprintf(stderr, "separator_visible: %d\n", viewport.separator_visible);
    fprintf(stderr, "terminal_rows: %d\n", term->screen_rows);

    // CRITICAL: input buffer must be visible (not off-screen)
    // input_buffer_start_row < terminal_rows means visible
    ck_assert_uint_lt(viewport.input_buffer_start_row, (size_t)term->screen_rows);

    // Input buffer should be at row 1 (after separator at row 0)
    ck_assert_uint_eq(viewport.input_buffer_start_row, 1);

    // Separator should be visible at row 0
    ck_assert(viewport.separator_visible);

    // Render and verify cursor is visible
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

    fprintf(stderr, "\n=== Rendered Output ===\n%s\n===\n", output);

    // Verify cursor visibility escape is present (show cursor)
    ck_assert_ptr_ne(strstr(output, "\x1b[?25h"), NULL);

    // Verify cursor is positioned (ESC[row;colH format)
    // Should be at row 2 (1-indexed), column 1
    ck_assert_ptr_ne(strstr(output, "\x1b[2;1H"), NULL);

    talloc_free(ctx);
}
END_TEST
/**
 * Test: Initial state with some scrollback - cursor still visible
 */
START_TEST(test_initial_state_with_scrollback_cursor_visible) {
    void *ctx = talloc_new(NULL);

    // Terminal: 5 rows x 80 cols
    ik_term_ctx_t *term = talloc_zero(ctx, ik_term_ctx_t);
    res_t res;
    term->screen_rows = 5;
    term->screen_cols = 80;

    // Create empty input buffer
    ik_input_buffer_t *input_buf = NULL;
    input_buf = ik_input_buffer_create(ctx);
    ik_input_buffer_ensure_layout(input_buf, 80);

    // Create scrollback with 2 lines
    ik_scrollback_t *scrollback = ik_scrollback_create(ctx, 80);
    res = ik_scrollback_append_line(scrollback, "line1", 5);
    ck_assert(is_ok(&res));
    res = ik_scrollback_append_line(scrollback, "line2", 5);
    ck_assert(is_ok(&res));

    // Create render context
    ik_render_ctx_t *render_ctx = NULL;
    res = ik_render_create(ctx, 5, 80, 1, &render_ctx);
    ck_assert(is_ok(&res));

    // Create REPL at bottom (offset=0)
    ik_repl_ctx_t *repl = talloc_zero(ctx, ik_repl_ctx_t);
    repl->current = talloc_zero(repl, ik_agent_ctx_t);
    ik_shared_ctx_t *shared = talloc_zero(repl, ik_shared_ctx_t);
    repl->shared = shared;
    shared->term = term;
    shared->render = render_ctx;

    // Create agent context for display state
    ik_agent_ctx_t *agent = talloc_zero(repl, ik_agent_ctx_t);
    repl->current = agent;
    repl->current->input_buffer = input_buf;
    repl->current->scrollback = scrollback;
    repl->current->viewport_offset = 0;

    // Calculate viewport
    ik_viewport_t viewport;
    res = ik_repl_calculate_viewport(repl, &viewport);
    ck_assert(is_ok(&res));

    // Input buffer must be visible
    ck_assert_uint_lt(viewport.input_buffer_start_row, (size_t)term->screen_rows);

    // Document: 2 scrollback + 1 separator + 1 input + 2 status = 6 rows
    // Terminal: 5 rows, so document overflows by 1
    // first_visible_row = 6 - 5 = 1
    // Input at doc row 3, viewport row = 3 - 1 = 2
    ck_assert_uint_eq(viewport.input_buffer_start_row, 2);

    // Render and verify cursor positioning
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

    // Verify cursor is visible and positioned
    ck_assert_ptr_ne(strstr(output, "\x1b[?25h"), NULL);
    ck_assert_ptr_ne(strstr(output, "\x1b[3;1H"), NULL);  // Row 3 (1-indexed)

    talloc_free(ctx);
}

END_TEST
/**
 * Test: After scrolling up, cursor hidden (verify Bug #8 fix still works)
 */
START_TEST(test_scrolled_up_cursor_hidden) {
    void *ctx = talloc_new(NULL);

    // Terminal: 5 rows x 80 cols
    ik_term_ctx_t *term = talloc_zero(ctx, ik_term_ctx_t);
    res_t res;
    term->screen_rows = 5;
    term->screen_cols = 80;

    // Create empty input buffer
    ik_input_buffer_t *input_buf = NULL;
    input_buf = ik_input_buffer_create(ctx);
    ik_input_buffer_ensure_layout(input_buf, 80);

    // Create scrollback with many lines
    ik_scrollback_t *scrollback = ik_scrollback_create(ctx, 80);
    for (int i = 0; i < 10; i++) {
        char buf[32];
        snprintf(buf, sizeof(buf), "line%d", i);
        res = ik_scrollback_append_line(scrollback, buf, strlen(buf));
        ck_assert(is_ok(&res));
    }

    // Create render context
    ik_render_ctx_t *render_ctx = NULL;
    res = ik_render_create(ctx, 5, 80, 1, &render_ctx);
    ck_assert(is_ok(&res));

    // Create REPL scrolled up (offset=5)
    ik_repl_ctx_t *repl = talloc_zero(ctx, ik_repl_ctx_t);
    repl->current = talloc_zero(repl, ik_agent_ctx_t);
    ik_shared_ctx_t *shared = talloc_zero(repl, ik_shared_ctx_t);
    repl->shared = shared;
    shared->term = term;
    shared->render = render_ctx;

    // Create agent context for display state
    ik_agent_ctx_t *agent = talloc_zero(repl, ik_agent_ctx_t);
    repl->current = agent;
    repl->current->input_buffer = input_buf;
    repl->current->scrollback = scrollback;
    repl->current->viewport_offset = 5;  // Scrolled up

    // Calculate viewport
    ik_viewport_t viewport;
    res = ik_repl_calculate_viewport(repl, &viewport);
    ck_assert(is_ok(&res));

    // Input buffer must be OFF-screen when scrolled up
    ck_assert_uint_eq(viewport.input_buffer_start_row, (size_t)term->screen_rows);

    // Render and verify cursor is HIDDEN
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

    // Verify cursor is HIDDEN (hide cursor escape)
    ck_assert_ptr_ne(strstr(output, "\x1b[?25l"), NULL);

    // Verify NO cursor positioning (should not have ESC[row;colH)
    size_t output_len = (size_t)bytes_read;
    bool found = contains_cursor_positioning_escape(output, output_len);
    ck_assert(!found);

    talloc_free(ctx);
}

END_TEST

/* Create test suite */
static Suite *initial_state_suite(void)
{
    Suite *s = suite_create("REPL Initial State");

    TCase *tc_cursor = tcase_create("CursorVisibility");
    tcase_set_timeout(tc_cursor, IK_TEST_TIMEOUT);
    tcase_set_timeout(tc_cursor, IK_TEST_TIMEOUT);
    tcase_set_timeout(tc_cursor, IK_TEST_TIMEOUT);
    tcase_set_timeout(tc_cursor, IK_TEST_TIMEOUT);
    tcase_set_timeout(tc_cursor, IK_TEST_TIMEOUT);
    tcase_add_test(tc_cursor, test_initial_state_cursor_visible);
    tcase_add_test(tc_cursor, test_initial_state_with_scrollback_cursor_visible);
    tcase_add_test(tc_cursor, test_scrolled_up_cursor_hidden);
    suite_add_tcase(s, tc_cursor);

    return s;
}

int main(void)
{
    Suite *s = initial_state_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/repl/repl_initial_state_test.xml");

    srunner_run_all(sr, CK_VERBOSE);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    ik_test_reset_terminal();

    return (number_failed == 0) ? 0 : 1;
}
