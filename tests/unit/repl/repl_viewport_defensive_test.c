/**
 * @file repl_viewport_defensive_test.c
 * @brief Tests for defensive boundary checks in repl_viewport.c
 *
 * This file tests edge cases that were previously marked with LCOV exclusions:
 * - Line 79-80: Error handling when scrollback lookup fails
 * - Line 101-106: Boundary case where input buffer starts before viewport
 * - Line 131: Error propagation from calculate_viewport
 */

#include <check.h>
#include "../../../src/shared.h"
#include <talloc.h>
#include <string.h>
#include "../../../src/repl.h"
#include "../../../src/scrollback.h"
#include "../../test_utils.h"

/*
 * Note: Lines 79-80 test is not included here because it would cause a PANIC.
 * The defensive check at line 79-80 handles errors from ik_scrollback_find_logical_line_at_physical_row
 * by calling PANIC. This should be refactored to return ERR instead.
 *
 * Once refactored, add this test:
 * - Corrupt scrollback->total_physical_lines to be larger than actual
 * - Call ik_repl_calculate_viewport
 * - Verify it returns an error instead of PANICing
 */

/**
 * Test: Lines 101-106 - Input buffer starts before viewport
 *
 * This tests the "shouldn't happen" case where input_buffer_start_doc_row
 * is less than first_visible_row. This can occur with carefully crafted
 * viewport offset values.
 *
 * To trigger this:
 * - We need first_visible_row to be greater than input_buffer_start_doc_row
 * - input_buffer_start_doc_row = scrollback_rows + 1
 * - first_visible_row depends on viewport_offset
 *
 * Scenario:
 * - 100 scrollback rows + 1 separator + 1 input buffer = 102 total rows
 * - Terminal shows 10 rows
 * - With offset=0: last_visible = 101, first_visible = 92
 * - input_buffer_start = 101 (scrollback_rows + 1)
 * - 101 >= 92, so normal case
 *
 * - With offset=10: last_visible = 91, first_visible = 82
 * - input_buffer_start = 101
 * - 101 > 91, so input buffer not visible (line 108 branch)
 *
 * To get input_buffer_start < first_visible AND input_buffer visible:
 * - last_visible >= input_buffer_start (input buffer visible)
 * - first_visible > input_buffer_start (the else branch)
 *
 * This is actually impossible with the current logic! The else branch
 * at line 104 is unreachable because:
 * - We only enter the if at line 99 when input_buffer_start_doc_row <= last_visible_row
 * - input_buffer_start_doc_row = scrollback_rows + 1
 * - first_visible_row = last_visible_row + 1 - terminal_rows
 * - For the else: first_visible > input_buffer_start
 * - So: last_visible + 1 - terminal_rows > scrollback_rows + 1
 * - Rearranged: last_visible > scrollback_rows + terminal_rows
 * - But last_visible = document_height - 1 - offset
 * - document_height = scrollback_rows + 1 + input_display_rows
 * - With offset=0: last_visible = scrollback_rows + input_display_rows
 * - For else: scrollback_rows + input_display_rows > scrollback_rows + terminal_rows
 * - So: input_display_rows > terminal_rows
 * - Then: document_height > terminal_rows
 * - And: input_buffer_start = scrollback_rows + 1
 * - first_visible = last_visible + 1 - terminal_rows
 * -             = (scrollback_rows + input_display_rows) + 1 - terminal_rows
 * - For else: first_visible > input_buffer_start
 * - scrollback_rows + input_display_rows + 1 - terminal_rows > scrollback_rows + 1
 * - input_display_rows > terminal_rows
 * - When this is true, we're in the overflow case
 * - last_visible = document_height - 1 - offset
 * -             = scrollback_rows + 1 + input_display_rows - 1 - offset
 * -             = scrollback_rows + input_display_rows - offset
 * - For input buffer visible: last_visible >= input_buffer_start
 * - scrollback_rows + input_display_rows - offset >= scrollback_rows + 1
 * - input_display_rows - offset >= 1
 * - offset <= input_display_rows - 1
 *
 * Actually, let me try a concrete example:
 * - scrollback_rows = 5
 * - input_display_rows = 10
 * - terminal_rows = 8
 * - document_height = 5 + 1 + 10 = 16
 * - input_buffer_start = 6
 * - With offset=0: last_visible=15, first_visible=8
 * - 6 >= 8? No, so else branch!
 * - 15 >= 6? Yes, so input buffer visible
 */
START_TEST(test_viewport_input_buffer_before_viewport) {
    void *ctx = talloc_new(NULL);

    // Create REPL context with specific dimensions to trigger the edge case
    ik_term_ctx_t *term = talloc_zero(ctx, ik_term_ctx_t);
    term->screen_rows = 8;   // Terminal shows 8 rows
    term->screen_cols = 40;  // Narrow to cause wrapping

    ik_input_buffer_t *input_buf = ik_input_buffer_create(ctx);

    // Add content that wraps to 10 physical lines (more than terminal height)
    // At width 40, we need 40*10 = 400 characters
    for (int i = 0; i < 400; i++) {
        res_t res = ik_input_buffer_insert_codepoint(input_buf, 'a');
        ck_assert(is_ok(&res));
    }
    ik_input_buffer_ensure_layout(input_buf, 40);
    size_t input_buf_rows = ik_input_buffer_get_physical_lines(input_buf);
    ck_assert_uint_eq(input_buf_rows, 10);  // 400 chars / 40 width = 10 lines

    // Create scrollback with 5 lines (each 1 row)
    ik_scrollback_t *scrollback = ik_scrollback_create(ctx, 40);
    for (int i = 0; i < 5; i++) {
        char buf[32];
        snprintf(buf, sizeof(buf), "line %d", i);
        res_t res = ik_scrollback_append_line(scrollback, buf, strlen(buf));
        ck_assert(is_ok(&res));
    }
    size_t scrollback_rows = ik_scrollback_get_total_physical_lines(scrollback);
    ck_assert_uint_eq(scrollback_rows, 5);

    ik_repl_ctx_t *repl = talloc_zero(ctx, ik_repl_ctx_t);
    ik_shared_ctx_t *shared = talloc_zero(repl, ik_shared_ctx_t);
    repl->shared = shared;
    shared->term = term;
    repl->input_buffer = input_buf;
    repl->scrollback = scrollback;
    repl->viewport_offset = 0;  // At bottom

    // Calculate viewport
    // Document: 5 scrollback + 1 separator + 10 input buffer = 16 rows
    // Terminal: 8 rows
    // offset=0: last_visible=15, first_visible=8
    // input_buffer_start_doc_row = 5 + 1 = 6
    // Condition at line 99: 6 <= 15 (true - input buffer visible)
    // Condition at line 101: 6 >= 8? No!
    // So we hit the else branch at line 104 (the "shouldn't happen" case)
    ik_viewport_t viewport;
    res_t res = ik_repl_calculate_viewport(repl, &viewport);
    ck_assert(is_ok(&res));

    // The defensive code sets input_buffer_start_row = 0
    ck_assert_uint_eq(viewport.input_buffer_start_row, 0);

    // Verify this is the edge case we're testing
    // first_visible_row should be 8, input_buffer_start_doc_row should be 6
    // So the condition input_buffer_start_doc_row >= first_visible_row is false

    talloc_free(ctx);
}
END_TEST

/*
 * Note: Line 131 test is not included here because it depends on line 79-80 being refactored.
 * The error propagation at line 131 would only be triggered if calculate_viewport returns
 * an error, which currently requires the PANIC at line 80 to be converted to ERR.
 *
 * Once lines 79-80 are refactored to return ERR, add this test:
 * - Corrupt scrollback state to make calculate_viewport fail
 * - Call ik_repl_render_frame
 * - Verify it propagates the error correctly
 */

static Suite *repl_viewport_defensive_suite(void)
{
    Suite *s = suite_create("REPL Viewport Defensive");

    TCase *tc_core = tcase_create("Core");
    tcase_set_timeout(tc_core, 30);

    // Line 101-106: Input buffer before viewport (this is testable!)
    tcase_add_test(tc_core, test_viewport_input_buffer_before_viewport);

    suite_add_tcase(s, tc_core);
    return s;
}

int main(void)
{
    int number_failed;
    Suite *s = repl_viewport_defensive_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    ik_test_reset_terminal();

    return (number_failed == 0) ? 0 : 1;
}
