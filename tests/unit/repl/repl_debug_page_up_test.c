/**
 * @file repl_debug_page_up_test.c
 * @brief Debug Page Up issue with detailed output
 */

#include <check.h>
#include "../../../src/shared.h"
#include <talloc.h>
#include <string.h>
#include <unistd.h>
#include "../../../src/repl.h"
#include "../../../src/scrollback.h"
#include "../../../src/render.h"
#include "../../../src/input_buffer/core.h"
#include "../../../src/repl_actions.h"
#include "../../../src/input.h"
#include "../../test_utils.h"

START_TEST(test_page_up_with_4_lines) {
    void *ctx = talloc_new(NULL);

    // Terminal: 5 rows x 80 cols
    ik_term_ctx_t *term = talloc_zero(ctx, ik_term_ctx_t);
    res_t res;
    term->screen_rows = 5;
    term->screen_cols = 80;
    term->tty_fd = 1;

    // Create empty input buffer (1 empty line)
    ik_input_buffer_t *input_buf = NULL;
    input_buf = ik_input_buffer_create(ctx);

    // Create scrollback with A, B, C, D
    ik_scrollback_t *scrollback = ik_scrollback_create(ctx, 80);
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

    // Create REPL at bottom
    ik_repl_ctx_t *repl = talloc_zero(ctx, ik_repl_ctx_t);
    ik_shared_ctx_t *shared = talloc_zero(repl, ik_shared_ctx_t);
    repl->shared = shared;
    shared->term = term;
    repl->input_buffer = input_buf;
    repl->scrollback = scrollback;
    shared->render = render_ctx;
    repl->viewport_offset = 0;

    // Initialize input parser (not needed for this test)
    repl->input_parser = NULL;

    fprintf(stderr, "\n=== Initial State ===\n");
    fprintf(stderr, "Scrollback lines: 4 (A, B, C, D)\n");
    fprintf(stderr, "Input buffer lines: 1 (empty)\n");
    fprintf(stderr, "Document height: 4 + 1 (upper_sep) + 1 (input) + 1 (lower_sep) = 7 rows\n");
    fprintf(stderr, "Terminal rows: 5\n");
    fprintf(stderr, "viewport_offset: %zu\n", repl->viewport_offset);

    // Ensure layouts
    ik_scrollback_ensure_layout(scrollback, 80);
    ik_input_buffer_ensure_layout(input_buf, 80);

    size_t scrollback_rows = ik_scrollback_get_total_physical_lines(scrollback);
    size_t input_buf_rows = ik_input_buffer_get_physical_lines(input_buf);
    size_t input_buf_display_rows = (input_buf_rows == 0) ? 1 : input_buf_rows;
    size_t document_height = scrollback_rows + 1 + input_buf_display_rows + 1;  // +1 for lower separator

    fprintf(stderr, "Calculated: scrollback_rows=%zu, input_buf_rows=%zu, document_height=%zu\n",
            scrollback_rows, input_buf_rows, document_height);

    // Render at bottom
    int pipefd1[2];
    ck_assert_int_eq(pipe(pipefd1), 0);
    int saved_stdout1 = dup(1);
    dup2(pipefd1[1], 1);

    res = ik_repl_render_frame(repl);
    ck_assert(is_ok(&res));

    fflush(stdout);
    dup2(saved_stdout1, 1);
    close(pipefd1[1]);

    char output1[8192] = {0};
    ssize_t n1 = read(pipefd1[0], output1, sizeof(output1) - 1);
    (void)n1;  // Unused
    close(pipefd1[0]);
    close(saved_stdout1);

    fprintf(stderr, "\nAt bottom, should see B, C, D, separator, input buffer:\n");
    fprintf(stderr, "Output: %s\n", output1);

    // Now simulate Page Up
    fprintf(stderr, "\n=== Pressing Page Up ===\n");

    ik_input_action_t page_up_action;
    page_up_action.type = IK_INPUT_PAGE_UP;

    res = ik_repl_process_action(repl, &page_up_action);
    ck_assert(is_ok(&res));

    fprintf(stderr, "After Page Up, viewport_offset: %zu\n", repl->viewport_offset);

    // Calculate what should be visible
    size_t max_offset = (document_height > (size_t)term->screen_rows) ?
                        (document_height - (size_t)term->screen_rows) : 0;
    fprintf(stderr, "max_offset: %zu\n", max_offset);

    if (document_height <= (size_t)term->screen_rows) {
        fprintf(stderr, "Document fits entirely in terminal\n");
    } else {
        size_t last_visible_row = document_height - 1 - repl->viewport_offset;
        size_t first_visible_row = last_visible_row + 1 - (size_t)term->screen_rows;
        fprintf(stderr, "Visible rows: %zu-%zu\n", first_visible_row, last_visible_row);
    }

    // Render after Page Up
    int pipefd2[2];
    ck_assert_int_eq(pipe(pipefd2), 0);
    int saved_stdout2 = dup(1);
    dup2(pipefd2[1], 1);

    res = ik_repl_render_frame(repl);
    ck_assert(is_ok(&res));

    fflush(stdout);
    dup2(saved_stdout2, 1);
    close(pipefd2[1]);

    char output2[8192] = {0};
    ssize_t n2 = read(pipefd2[0], output2, sizeof(output2) - 1);
    (void)n2;  // Unused
    close(pipefd2[0]);
    close(saved_stdout2);

    fprintf(stderr, "\nAfter Page Up, should see A, B, C, D, separator:\n");
    fprintf(stderr, "Output: %s\n", output2);
    fprintf(stderr, "Contains A: %s\n", strstr(output2, "A") ? "YES" : "NO");
    fprintf(stderr, "Contains B: %s\n", strstr(output2, "B") ? "YES" : "NO");

    // Verify A is now visible
    ck_assert_ptr_ne(strstr(output2, "A"), NULL);

    talloc_free(ctx);
}
END_TEST

/* Create test suite */
static Suite *debug_page_up_suite(void)
{
    Suite *s = suite_create("Debug Page Up");

    TCase *tc_debug = tcase_create("Debug");
    tcase_set_timeout(tc_debug, 30);
    tcase_add_test(tc_debug, test_page_up_with_4_lines);
    suite_add_tcase(s, tc_debug);

    return s;
}

int main(void)
{
    Suite *s = debug_page_up_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_VERBOSE);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    ik_test_reset_terminal();

    return (number_failed == 0) ? 0 : 1;
}
