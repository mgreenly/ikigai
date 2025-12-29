#include "agent.h"
/**
 * @file repl_separator_wrapped_test.c
 * @brief Test Separator visibility with wrapped lines (lines that span multiple physical rows)
 *
 * The bug may only manifest when scrollback lines wrap across multiple terminal rows.
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
 * Test: Viewport calculation with wrapped lines
 *
 * Create scrollback lines that each wrap to exactly 2 physical rows.
 * Verify that the viewport calculation correctly counts wrapped lines.
 */
START_TEST(test_separator_with_wrapped_lines) {
    void *ctx = talloc_new(NULL);

    // Terminal: 10 rows x 80 cols
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

    // Create scrollback with lines that wrap (each line is 81+ chars to force wrapping)
    ik_scrollback_t *scrollback = ik_scrollback_create(ctx, 80);

    // Each line is exactly 81 characters (wraps to 2 physical rows)
    for (int32_t i = 0; i < 30; i++) {
        char buf[128];
        snprintf(buf, sizeof(buf), "line%02" PRId32 " ", i);
        // Pad to exactly 81 characters
        size_t current_len = strlen(buf);
        while (current_len < 81) {
            buf[current_len++] = 'x';
        }
        buf[81] = '\0';

        res = ik_scrollback_append_line(scrollback, buf, strlen(buf));
        ck_assert(is_ok(&res));
    }

    // Verify that lines are wrapping (each should be 2 physical rows)
    ik_scrollback_ensure_layout(scrollback, 80);
    ck_assert_uint_eq(scrollback->layouts[0].physical_lines, 2);

    // Total document: 30 lines * 2 rows each = 60 scrollback rows + 1 sep + 1 ws = 62 rows

    // Create render context
    ik_render_ctx_t *render_ctx = NULL;
    res = ik_render_create(ctx, 10, 80, 1, &render_ctx);
    ck_assert(is_ok(&res));

    // Create REPL and scroll to show middle of scrollback
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

    // Scroll to show document rows 20-29 (which should be lines 10-14)
    // Each line is 2 physical rows, so:
    // - Line 0-9: rows 0-19
    // - Line 10-14: rows 20-29
    // viewport_offset = 62 - 1 - 29 = 32
    repl->current->viewport_offset = 32;

    // Calculate viewport
    ik_viewport_t viewport;
    res = ik_repl_calculate_viewport(repl, &viewport);
    ck_assert(is_ok(&res));

    // We should see 5 logical lines (lines 10-14), covering 10 physical rows
    printf("Viewport: start_line=%zu, lines_count=%zu\n",
           viewport.scrollback_start_line, viewport.scrollback_lines_count);

    // Expected: start at line 10, count 5 lines
    ck_assert_uint_eq(viewport.scrollback_start_line, 10);
    ck_assert_uint_eq(viewport.scrollback_lines_count, 5);

    // Now render and check output
    int pipefd[2];
    ck_assert_int_eq(pipe(pipefd), 0);
    int saved_stdout = dup(1);
    dup2(pipefd[1], 1);

    res = ik_repl_render_frame(repl);
    ck_assert(is_ok(&res));

    fflush(stdout);
    dup2(saved_stdout, 1);
    close(pipefd[1]);

    char output[16384] = {0};
    ssize_t bytes_read = read(pipefd[0], output, sizeof(output) - 1);
    ck_assert(bytes_read > 0);
    close(pipefd[0]);
    close(saved_stdout);

    // Verify all expected lines appear in output
    ck_assert_ptr_ne(strstr(output, "line10"), NULL);
    ck_assert_ptr_ne(strstr(output, "line14"), NULL);

    // Count how many of our expected lines appear
    int32_t lines_found = 0;
    for (int32_t i = 10; i <= 14; i++) {
        char search[16];
        snprintf(search, sizeof(search), "line%02" PRId32, i);
        if (strstr(output, search) != NULL) {
            lines_found++;
        }
    }

    // Separator visibility would cause the last line to be cut off
    ck_assert_int_eq(lines_found, 5);

    talloc_free(ctx);
}
END_TEST

/* Create test suite */
static Suite *separator_wrapped_suite(void)
{
    Suite *s = suite_create("Separator visibility: Wrapped Lines");

    TCase *tc_wrapped = tcase_create("Wrapped");
    tcase_set_timeout(tc_wrapped, 30);
    tcase_set_timeout(tc_wrapped, 30);
    tcase_set_timeout(tc_wrapped, 30);
    tcase_set_timeout(tc_wrapped, 30);
    tcase_set_timeout(tc_wrapped, 30);
    tcase_add_test(tc_wrapped, test_separator_with_wrapped_lines);
    suite_add_tcase(s, tc_wrapped);

    return s;
}

int main(void)
{
    Suite *s = separator_wrapped_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_VERBOSE);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    ik_test_reset_terminal();

    return (number_failed == 0) ? 0 : 1;
}
