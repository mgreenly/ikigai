/**
 * @file repl_page_up_bug10_test.c
 * @brief Test for Bug #10: Page Up doesn't show earliest scrollback lines
 *
 * User scenario:
 * 1. Type a, b, c, d (each followed by Enter to submit to scrollback)
 * 2. Type e (in input buffer)
 * 3. Press Page Up
 *
 * Expected: Should show a, b, c, d, e (all 5 lines)
 * Actual: Shows b, c, d, e, blank line (missing 'a')
 */

#include <check.h>
#include <talloc.h>
#include <string.h>
#include <unistd.h>
#include <inttypes.h>
#include "../../../src/repl.h"
#include "../../../src/repl_actions.h"
#include "../../../src/scrollback.h"
#include "../../../src/input_buffer.h"
#include "../../../src/terminal.h"
#include "../../../src/input.h"
#include "../../test_utils.h"

/**
 * Test exact user scenario from Bug #10
 */
START_TEST(test_page_up_shows_earliest_line) {
    void *ctx = talloc_new(NULL);

    // Terminal: 5 rows x 80 cols (small to reproduce the issue)
    ik_term_ctx_t *term = talloc_zero(ctx, ik_term_ctx_t);
    term->screen_rows = 5;
    term->screen_cols = 80;
    term->tty_fd = 1;  // stdout

    // Create input buffer
    ik_input_buffer_t *input_buf = NULL;
    res_t res = ik_input_buffer_create(ctx, &input_buf);
    ck_assert(is_ok(&res));

    // Create scrollback
    ik_scrollback_t *scrollback = NULL;
    res = ik_scrollback_create(ctx, 80, &scrollback);
    ck_assert(is_ok(&res));

    // Create render context
    ik_render_ctx_t *render = NULL;
    res = ik_render_create(ctx, 5, 80, 1, &render);
    ck_assert(is_ok(&res));

    // Create REPL
    ik_repl_ctx_t *repl = talloc_zero(ctx, ik_repl_ctx_t);
    repl->term = term;
    repl->input_buffer = input_buf;
    repl->scrollback = scrollback;
    repl->render = render;
    repl->viewport_offset = 0;

    // Simulate: type a, Enter
    res = ik_input_buffer_insert_codepoint(input_buf, 'a');
    ck_assert(is_ok(&res));
    res = ik_repl_submit_line(repl);  // Moves 'a' to scrollback
    ck_assert(is_ok(&res));

    // Simulate: type b, Enter
    res = ik_input_buffer_insert_codepoint(input_buf, 'b');
    ck_assert(is_ok(&res));
    res = ik_repl_submit_line(repl);
    ck_assert(is_ok(&res));

    // Simulate: type c, Enter
    res = ik_input_buffer_insert_codepoint(input_buf, 'c');
    ck_assert(is_ok(&res));
    res = ik_repl_submit_line(repl);
    ck_assert(is_ok(&res));

    // Simulate: type d, Enter
    res = ik_input_buffer_insert_codepoint(input_buf, 'd');
    ck_assert(is_ok(&res));
    res = ik_repl_submit_line(repl);
    ck_assert(is_ok(&res));

    // Simulate: type e (DON'T submit - stays in input buffer)
    res = ik_input_buffer_insert_codepoint(input_buf, 'e');
    ck_assert(is_ok(&res));

    // Ensure layouts are calculated
    ik_input_buffer_ensure_layout(input_buf, 80);
    ik_scrollback_ensure_layout(scrollback, 80);

    // At this point:
    // - Scrollback: a, b, c, d (4 lines, 4 physical rows)
    // - Separator: 1 row
    // - Input buffer: e (1 line, 1 physical row)
    // - Document: 6 rows total
    // - Terminal: 5 rows
    // - At bottom (offset=0): Shows rows 1-5 (b, c, d, separator, e)
    //   Row 0 (a) is off-screen

    // Verify document structure
    size_t scrollback_rows = ik_scrollback_get_total_physical_lines(scrollback);
    size_t input_buf_rows = ik_input_buffer_get_physical_lines(input_buf);
    ck_assert_uint_eq(scrollback_rows, 4);
    ck_assert_uint_eq(input_buf_rows, 1);

    // Calculate viewport at bottom
    ik_viewport_t viewport_bottom;
    res = ik_repl_calculate_viewport(repl, &viewport_bottom);
    ck_assert(is_ok(&res));

    // At bottom, first scrollback line visible should be line 1 (b)
    ck_assert_uint_eq(viewport_bottom.scrollback_start_line, 1);

    // Simulate: Press Page Up
    ik_input_action_t action = {.type = IK_INPUT_PAGE_UP};
    res = ik_repl_process_action(repl, &action);
    ck_assert(is_ok(&res));

    // After Page Up, viewport_offset should be 1 (max_offset)
    // max_offset = document_height - terminal_rows = 6 - 5 = 1
    ck_assert_uint_eq(repl->viewport_offset, 1);

    // Calculate viewport after Page Up
    ik_viewport_t viewport_up;
    res = ik_repl_calculate_viewport(repl, &viewport_up);
    ck_assert(is_ok(&res));

    // After Page Up, should show rows 0-4 (a, b, c, d, separator)
    // First scrollback line should be line 0 (a)
    ck_assert_msg(viewport_up.scrollback_start_line == 0,
                  "Expected first scrollback line to be 0 (a), got %zu",
                  viewport_up.scrollback_start_line);

    // Should see 4 scrollback lines (a, b, c, d)
    ck_assert_msg(viewport_up.scrollback_lines_count == 4,
                  "Expected 4 scrollback lines visible, got %zu",
                  viewport_up.scrollback_lines_count);

    // Separator should be visible at row 4
    ck_assert_msg(viewport_up.separator_visible,
                  "Expected separator to be visible");

    // input buffer should be off-screen
    ck_assert_msg(viewport_up.input_buffer_start_row == 5,
                  "Expected input buffer off-screen (start_row=5), got %zu",
                  viewport_up.input_buffer_start_row);

    talloc_free(ctx);
}
END_TEST

/* Create test suite */
static Suite *page_up_bug10_suite(void)
{
    Suite *s = suite_create("REPL: Page Up Bug #10");

    TCase *tc_page_up = tcase_create("PageUp");
    tcase_set_timeout(tc_page_up, 30);
    tcase_add_test(tc_page_up, test_page_up_shows_earliest_line);
    suite_add_tcase(s, tc_page_up);

    return s;
}

int main(void)
{
    Suite *s = page_up_bug10_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_VERBOSE);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
