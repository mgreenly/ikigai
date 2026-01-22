#include "agent.h"
/**
 * @file repl_page_up_scrollback_test.c
 * @brief Test Page Up scrolling to earlier scrollback content
 *
 * Tests that Page Up actually scrolls to show earlier scrollback lines,
 * not just hides the input buffer.
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
#include "../../test_utils_helper.h"

/**
 * Test: Page Up should scroll to show earlier scrollback lines
 *
 * Scenario from user report:
 * - Terminal: 5 rows
 * - Scrollback has 9 lines (5 initial + A + B + C + D)
 * - Initially showing B, C, D, separator, input buffer
 * - After Page Up: should show lines from earlier in scrollback
 */
START_TEST(test_page_up_shows_earlier_scrollback) {
    void *ctx = talloc_new(NULL);

    // Terminal: 5 rows x 80 cols (matches user's scenario)
    ik_term_ctx_t *term = talloc_zero(ctx, ik_term_ctx_t);
    res_t res;
    term->screen_rows = 5;
    term->screen_cols = 80;

    // Create input buffer
    ik_input_buffer_t *input_buf = NULL;
    input_buf = ik_input_buffer_create(ctx);
    ik_input_buffer_ensure_layout(input_buf, 80);

    // Create scrollback with 9 lines (5 initial + A, B, C, D)
    ik_scrollback_t *scrollback = ik_scrollback_create(ctx, 80);

    // 5 initial lines
    for (int32_t i = 0; i < 5; i++) {
        char buf[32];
        snprintf(buf, sizeof(buf), "initial%" PRId32, i);
        res = ik_scrollback_append_line(scrollback, buf, strlen(buf));
        ck_assert(is_ok(&res));
    }

    // A, B, C, D
    res = ik_scrollback_append_line(scrollback, "A", 1);
    ck_assert(is_ok(&res));
    res = ik_scrollback_append_line(scrollback, "B", 1);
    ck_assert(is_ok(&res));
    res = ik_scrollback_append_line(scrollback, "C", 1);
    ck_assert(is_ok(&res));
    res = ik_scrollback_append_line(scrollback, "D", 1);
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

    // Document: 9 scrollback + 1 upper_separator + 1 input buffer + 1 lower_separator = 12 rows
    // (input buffer always occupies 1 row even when empty, for cursor visibility)
    // Terminal: 5 rows
    // At bottom (offset=0), showing rows 7-11:
    //   Row 7: C (scrollback line 7)
    //   Row 8: D (scrollback line 8)
    //   Row 9: upper separator
    //   Row 10: input buffer
    //   Row 11: lower separator

    // Calculate viewport at bottom
    ik_viewport_t viewport_bottom;
    res = ik_repl_calculate_viewport(repl, &viewport_bottom);
    ck_assert(is_ok(&res));

    fprintf(stderr, "\n=== At Bottom (offset=0) ===\n");
    fprintf(stderr, "scrollback_start_line: %zu\n", viewport_bottom.scrollback_start_line);
    fprintf(stderr, "scrollback_lines_count: %zu\n", viewport_bottom.scrollback_lines_count);
    fprintf(stderr, "separator_visible: %d\n", viewport_bottom.separator_visible);
    fprintf(stderr, "input_buffer_start_row: %zu\n", viewport_bottom.input_buffer_start_row);

    // Should show scrollback lines 7, 8 (C, D)
    ck_assert_uint_eq(viewport_bottom.scrollback_start_line, 7);
    ck_assert_uint_eq(viewport_bottom.scrollback_lines_count, 2);
    ck_assert(viewport_bottom.separator_visible);
    ck_assert_uint_eq(viewport_bottom.input_buffer_start_row, 3);  // Input buffer visible at row 3

    // Now press Page Up (scroll to max offset to see earliest lines)
    // Document height = 12, terminal = 5, max offset = 7
    repl->current->viewport_offset = 7;

    // After Page Up to max, showing rows 0-4:
    //   Row 0: initial0 (scrollback line 0)
    //   Row 1: initial1 (scrollback line 1)
    //   Row 2: initial2 (scrollback line 2)
    //   Row 3: initial3 (scrollback line 3)
    //   Row 4: initial4 (scrollback line 4)

    ik_viewport_t viewport_scrolled;
    res = ik_repl_calculate_viewport(repl, &viewport_scrolled);
    ck_assert(is_ok(&res));

    fprintf(stderr, "\n=== After Page Up (offset=6) ===\n");
    fprintf(stderr, "scrollback_start_line: %zu\n", viewport_scrolled.scrollback_start_line);
    fprintf(stderr, "scrollback_lines_count: %zu\n", viewport_scrolled.scrollback_lines_count);
    fprintf(stderr, "separator_visible: %d\n", viewport_scrolled.separator_visible);
    fprintf(stderr, "input_buffer_start_row: %zu\n", viewport_scrolled.input_buffer_start_row);

    // Should show scrollback lines 0-4
    fprintf(stderr, "Expected: start_line=0, lines_count=5\n");
    fprintf(stderr, "Actual: start_line=%zu, lines_count=%zu\n",
            viewport_scrolled.scrollback_start_line,
            viewport_scrolled.scrollback_lines_count);

    ck_assert_uint_eq(viewport_scrolled.scrollback_start_line, 0);
    ck_assert_uint_eq(viewport_scrolled.scrollback_lines_count, 5);
    ck_assert(!viewport_scrolled.separator_visible);  // Separator off-screen
    ck_assert_uint_eq(viewport_scrolled.input_buffer_start_row, 5);  // Off-screen

    // Render and verify actual output changes
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

    char *sanitized = ik_test_sanitize_ansi(ctx, output, (size_t)bytes_read);
    fprintf(stderr, "\n=== Rendered Output ===\n%s\n===\n", sanitized ? sanitized : "(sanitize failed)");

    // Verify earlier lines are now visible (lines 0-4)
    fprintf(stderr, "Checking for earlier lines:\n");
    fprintf(stderr, "  initial0: %s\n", strstr(output, "initial0") ? "FOUND" : "NOT FOUND");
    fprintf(stderr, "  initial1: %s\n", strstr(output, "initial1") ? "FOUND" : "NOT FOUND");
    fprintf(stderr, "  initial4: %s\n", strstr(output, "initial4") ? "FOUND" : "NOT FOUND");
    fprintf(stderr, "Checking that A, B, C, D are NOT visible:\n");
    fprintf(stderr, "  A: %s\n", strstr(output, "A") ? "FOUND (BUG!)" : "NOT FOUND (correct)");
    fprintf(stderr, "  B: %s\n", strstr(output, "B") ? "FOUND (BUG!)" : "NOT FOUND (correct)");
    fprintf(stderr, "  C: %s\n", strstr(output, "C") ? "FOUND (BUG!)" : "NOT FOUND (correct)");
    fprintf(stderr, "  D: %s\n", strstr(output, "D") ? "FOUND (BUG!)" : "NOT FOUND (correct)");

    ck_assert_ptr_ne(strstr(output, "initial0"), NULL);
    ck_assert_ptr_ne(strstr(output, "initial1"), NULL);
    ck_assert_ptr_ne(strstr(output, "initial4"), NULL);

    // Verify A, B, C, D are NOT visible (scrolled off bottom)
    ck_assert_ptr_eq(strstr(output, "A"), NULL);
    ck_assert_ptr_eq(strstr(output, "B"), NULL);
    ck_assert_ptr_eq(strstr(output, "C"), NULL);
    ck_assert_ptr_eq(strstr(output, "D"), NULL);

    talloc_free(ctx);
}
END_TEST

/* Create test suite */
static Suite *page_up_scrollback_suite(void)
{
    Suite *s = suite_create("Page Up Scrollback");

    TCase *tc_pageup = tcase_create("PageUp");
    tcase_set_timeout(tc_pageup, IK_TEST_TIMEOUT);
    tcase_set_timeout(tc_pageup, IK_TEST_TIMEOUT);
    tcase_set_timeout(tc_pageup, IK_TEST_TIMEOUT);
    tcase_set_timeout(tc_pageup, IK_TEST_TIMEOUT);
    tcase_set_timeout(tc_pageup, IK_TEST_TIMEOUT);
    tcase_add_test(tc_pageup, test_page_up_shows_earlier_scrollback);
    suite_add_tcase(s, tc_pageup);

    return s;
}

int main(void)
{
    Suite *s = page_up_scrollback_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/repl/repl_page_up_scrollback_test.xml");

    srunner_run_all(sr, CK_VERBOSE);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    ik_test_reset_terminal();

    return (number_failed == 0) ? 0 : 1;
}
