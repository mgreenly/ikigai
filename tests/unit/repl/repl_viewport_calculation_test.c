#include "agent.h"
/**
 * @file repl_separator_debug_test.c
 * @brief Debug test for Separator visibility with detailed output
 *
 * This test outputs detailed calculations to help identify the off-by-one error.
 */

#include <check.h>
#include "../../../src/agent.h"
#include "../../../src/shared.h"
#include <talloc.h>
#include <string.h>
#include <stdio.h>
#include <inttypes.h>
#include "../../../src/repl.h"
#include "../../../src/scrollback.h"
#include "../../../src/input_buffer/core.h"
#include "../../test_utils.h"

/**
 * Test: Simple case with short lines, scrolled to show only scrollback
 *
 * This should fill the entire terminal with scrollback (no separator, no input buffer)
 */
START_TEST(test_separator_debug_simple_case) {
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
    size_t input_buf_rows = ik_input_buffer_get_physical_lines(input_buf);

    // Create scrollback with 50 short lines (no wrapping)
    ik_scrollback_t *scrollback = ik_scrollback_create(ctx, 80);
    for (int32_t i = 0; i < 50; i++) {
        char buf[32];
        snprintf(buf, sizeof(buf), "line %" PRId32, i);
        res = ik_scrollback_append_line(scrollback, buf, strlen(buf));
        ck_assert(is_ok(&res));
    }
    ik_scrollback_ensure_layout(scrollback, 80);
    size_t scrollback_rows = ik_scrollback_get_total_physical_lines(scrollback);
    size_t scrollback_line_count = ik_scrollback_get_line_count(scrollback);

    // Document structure
    size_t separator_row = scrollback_rows;
    size_t input_start_doc_row = scrollback_rows + 1;
    size_t document_height = scrollback_rows + 1 + input_buf_rows;

    printf("\n=== Document Structure ===\n");
    printf("Scrollback: %zu lines, %zu physical rows (rows 0-%zu)\n",
           scrollback_line_count, scrollback_rows, scrollback_rows - 1);
    printf("Separator: row %zu\n", separator_row);
    printf("Input Buffer: row %zu, %zu physical rows\n", input_start_doc_row, input_buf_rows);
    printf("Document height: %zu rows\n", document_height);
    printf("Terminal height: %d rows\n\n", term->screen_rows);

    // Create REPL and scroll to middle of scrollback
    // We want to view scrollback lines 20-29 (10 lines)
    ik_repl_ctx_t *repl = talloc_zero(ctx, ik_repl_ctx_t);
    repl->current = talloc_zero(repl, ik_agent_ctx_t);
    ik_shared_ctx_t *shared = talloc_zero(repl, ik_shared_ctx_t);
    repl->shared = shared;
    shared->term = term;

    // Create agent context for display state
    ik_agent_ctx_t *agent = talloc_zero(repl, ik_agent_ctx_t);
    repl->current = agent;
    repl->current->input_buffer = input_buf;
    repl->current->scrollback = scrollback;

    // Desired view: document rows 20-29
    // last_visible_row = 29
    // last_visible_row = document_height - 1 - offset
    // 29 = 52 - 1 - offset
    // offset = 22
    repl->current->viewport_offset = 22;

    // Calculate what SHOULD be visible
    size_t expected_last_visible_row = document_height - 1 - repl->current->viewport_offset;
    size_t expected_first_visible_row = expected_last_visible_row + 1 - (size_t)term->screen_rows;

    printf("=== Expected Viewport ===\n");
    printf("viewport_offset: %zu\n", repl->current->viewport_offset);
    printf("first_visible_row: %zu (expected)\n", expected_first_visible_row);
    printf("last_visible_row: %zu (expected)\n", expected_last_visible_row);
    printf("Visible rows: %zu-%zu (should be 10 rows of scrollback)\n\n",
           expected_first_visible_row, expected_last_visible_row);

    // Calculate viewport
    ik_viewport_t viewport;
    res = ik_repl_calculate_viewport(repl, &viewport);
    ck_assert(is_ok(&res));

    printf("=== Actual Viewport ===\n");
    printf("scrollback_start_line: %zu\n", viewport.scrollback_start_line);
    printf("scrollback_lines_count: %zu\n", viewport.scrollback_lines_count);
    printf("input_buffer_start_row: %zu\n\n", viewport.input_buffer_start_row);

    // Expected: start_line=20, lines_count=10
    // (lines 20-29, each 1 physical row, filling rows 20-29)
    printf("=== Analysis ===\n");
    printf("Expected to see lines: 20-29 (10 lines)\n");
    printf("Actually seeing lines: %zu-%zu (%zu lines)\n",
           viewport.scrollback_start_line,
           viewport.scrollback_start_line + viewport.scrollback_lines_count - 1,
           viewport.scrollback_lines_count);

    if (viewport.scrollback_lines_count < 10) {
        printf("BUG: Missing %zu line(s) at the end!\n", 10 - viewport.scrollback_lines_count);
    } else if (viewport.scrollback_lines_count > 10) {
        printf("BUG: Including %zu extra line(s)!\n", viewport.scrollback_lines_count - 10);
    } else {
        printf("OK: Correct number of lines\n");
    }

    // The key assertion: should see 10 lines to fill the 10-row terminal
    ck_assert_uint_eq(viewport.scrollback_lines_count, 10);

    talloc_free(ctx);
}
END_TEST

/* Create test suite */
static Suite *separator_debug_suite(void)
{
    Suite *s = suite_create("Separator visibility: Debug");

    TCase *tc_debug = tcase_create("Debug");
    tcase_set_timeout(tc_debug, 30);
    tcase_add_test(tc_debug, test_separator_debug_simple_case);
    suite_add_tcase(s, tc_debug);

    return s;
}

int main(void)
{
    Suite *s = separator_debug_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_VERBOSE);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
