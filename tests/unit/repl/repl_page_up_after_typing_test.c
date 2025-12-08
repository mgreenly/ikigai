/**
 * @file repl_page_up_after_typing_test.c
 * @brief Test Bug #10: Page Up after typing in input buffer
 *
 * Exact user scenario:
 * 1. Type a, b, c, d (each with Enter)
 * 2. Page Up - shows a, b, c, d, separator (correct)
 * 3. Type e (auto-scrolls to bottom)
 * 4. Page Up - should show a, b, c, d, e but shows b, c, d, e, blank
 */

#include <check.h>
#include "../../../src/shared.h"
#include <talloc.h>
#include <string.h>
#include <stdio.h>
#include "../../../src/repl.h"
#include "../../../src/repl_actions.h"
#include "../../../src/scrollback.h"
#include "../../../src/input_buffer/core.h"
#include "../../../src/terminal.h"
#include "../../test_utils.h"

START_TEST(test_page_up_after_typing_in_input_buffer) {
    void *ctx = talloc_new(NULL);

    // Terminal: 5 rows x 80 cols
    ik_term_ctx_t *term = talloc_zero(ctx, ik_term_ctx_t);
    res_t res;
    term->screen_rows = 5;
    term->screen_cols = 80;
    term->tty_fd = 1;

    // Create input buffer
    ik_input_buffer_t *input_buf = NULL;
    input_buf = ik_input_buffer_create(ctx);

    // Create scrollback
    ik_scrollback_t *scrollback = ik_scrollback_create(ctx, 80);

    // Create render context
    ik_render_ctx_t *render = NULL;
    res = ik_render_create(ctx, 5, 80, 1, &render);
    ck_assert(is_ok(&res));

    // Create REPL
    ik_repl_ctx_t *repl = talloc_zero(ctx, ik_repl_ctx_t);
    ik_shared_ctx_t *shared = talloc_zero(repl, ik_shared_ctx_t);
    repl->shared = shared;
    shared->term = term;
    repl->input_buffer = input_buf;
    repl->scrollback = scrollback;
    shared->render = render;
    repl->viewport_offset = 0;

    // Step 1-4: Type a, b, c, d (with Enter after each)
    for (char ch = 'a'; ch <= 'd'; ch++) {
        res = ik_input_buffer_insert_codepoint(input_buf, (uint32_t)ch);
        ck_assert(is_ok(&res));
        res = ik_repl_submit_line(repl);
        ck_assert(is_ok(&res));
    }

    ik_scrollback_ensure_layout(scrollback, 80);
    ik_input_buffer_ensure_layout(input_buf, 80);

    // At this point: scrollback has a, b, c, d with blank lines (8 rows), input buffer empty (occupies 1 row)
    // Document: 8 (scrollback) + 1 (upper_sep) + 1 (input, empty but takes 1 row) + 1 (lower_sep) = 11 rows
    // Terminal: 5 rows
    // At bottom (offset=0): shows rows 6-10 (last 3 scrollback lines + sep + input + lower_sep)
    // max_offset = 11 - 5 = 6

    // Step 5: Page Up (scrolls up by 5 rows)
    // offset = min(0 + 5, 6) = 5
    // Shows rows 1-5 (scrollback lines 1-4 + separator, or similar)
    ik_input_action_t page_up_action = {.type = IK_INPUT_PAGE_UP};
    res = ik_repl_process_action(repl, &page_up_action);
    ck_assert(is_ok(&res));

    ik_viewport_t viewport_after_first_pageup;
    res = ik_repl_calculate_viewport(repl, &viewport_after_first_pageup);
    ck_assert(is_ok(&res));

    // After Page Up with offset=5: shows rows 1-5
    // Scrollback occupies rows 0-7 (8 rows), separator at row 8, input at row 9, lower_sep at row 10
    // Visible rows 1-5 means we see scrollback rows 1-5 (logical lines based on physical rows)
    ck_assert_uint_eq(viewport_after_first_pageup.scrollback_start_line, 1);
    ck_assert_uint_eq(viewport_after_first_pageup.scrollback_lines_count, 5);

    // Step 6: Type 'e' (stays in input buffer, auto-scrolls to bottom)
    ik_input_action_t type_e_action = {.type = IK_INPUT_CHAR, .codepoint = 'e'};
    res = ik_repl_process_action(repl, &type_e_action);
    ck_assert(is_ok(&res));

    ik_scrollback_ensure_layout(scrollback, 80);
    ik_input_buffer_ensure_layout(input_buf, 80);

    // After typing 'e', auto-scroll should reset to bottom
    ck_assert_uint_eq(repl->viewport_offset, 0);

    // Document now: 8 (scrollback with blank lines) + 1 (upper_sep) + 1 (input buffer 'e') + 1 (lower_sep) = 11 rows
    // Terminal: 5 rows
    // At bottom (offset=0): shows rows 6-10 (last 3 scrollback lines + separator + e + lower_sep)

    size_t scrollback_rows = ik_scrollback_get_total_physical_lines(scrollback);
    size_t input_buf_rows = ik_input_buffer_get_physical_lines(input_buf);
    ck_assert_uint_eq(scrollback_rows, 8);
    ck_assert_uint_eq(input_buf_rows, 1);

    ik_viewport_t viewport_at_bottom;
    res = ik_repl_calculate_viewport(repl, &viewport_at_bottom);
    ck_assert(is_ok(&res));

    printf("At bottom after typing 'e':\n");
    printf("  viewport_offset: %zu\n", repl->viewport_offset);
    printf("  scrollback_start_line: %zu\n", viewport_at_bottom.scrollback_start_line);
    printf("  scrollback_lines_count: %zu\n", viewport_at_bottom.scrollback_lines_count);
    printf("  input_buffer_start_row: %zu\n", viewport_at_bottom.input_buffer_start_row);

    // At bottom (offset=0), document rows 6-10 are visible
    // Scrollback occupies rows 0-7, so rows 6-7 are visible (which is line 6 if each line takes 2 rows)
    ck_assert_uint_eq(viewport_at_bottom.scrollback_start_line, 6);

    // Step 7: Page Up again
    res = ik_repl_process_action(repl, &page_up_action);
    ck_assert(is_ok(&res));

    printf("\nAfter Page Up:\n");
    printf("  viewport_offset: %zu\n", repl->viewport_offset);

    // max_offset = 11 - 5 = 6
    // After Page Up: offset = min(0 + 5, 6) = 5
    ck_assert_uint_eq(repl->viewport_offset, 5);

    ik_viewport_t viewport_after_pageup;
    res = ik_repl_calculate_viewport(repl, &viewport_after_pageup);
    ck_assert(is_ok(&res));

    printf("  scrollback_start_line: %zu\n", viewport_after_pageup.scrollback_start_line);
    printf("  scrollback_lines_count: %zu\n", viewport_after_pageup.scrollback_lines_count);
    printf("  input_buffer_start_row: %zu\n", viewport_after_pageup.input_buffer_start_row);
    printf("  separator_visible: %d\n", viewport_after_pageup.separator_visible);

    // After Page Up with offset=5, should show rows 1-5
    // With scrollback rows 0-7, showing rows 1-5 means we start at scrollback line 1
    ck_assert_msg(viewport_after_pageup.scrollback_start_line == 1,
                  "Expected scrollback_start_line = 1, got %zu",
                  viewport_after_pageup.scrollback_start_line);

    // Should see 5 scrollback lines
    ck_assert_msg(viewport_after_pageup.scrollback_lines_count == 5,
                  "Expected scrollback_lines_count = 5, got %zu",
                  viewport_after_pageup.scrollback_lines_count);

    // Separator should NOT be visible (terminal is only 5 rows, all filled with scrollback)
    ck_assert_msg(!viewport_after_pageup.separator_visible,
                  "Expected separator to NOT be visible");

    // Input buffer should be off-screen (start_row = 5)
    ck_assert_msg(viewport_after_pageup.input_buffer_start_row == 5,
                  "Expected input_buffer_start_row = 5, got %zu",
                  viewport_after_pageup.input_buffer_start_row);

    talloc_free(ctx);
}
END_TEST

static Suite *page_up_typing_suite(void)
{
    Suite *s = suite_create("REPL: Page Up After Typing");

    TCase *tc = tcase_create("PageUpTyping");
    tcase_set_timeout(tc, 30);
    tcase_add_test(tc, test_page_up_after_typing_in_input_buffer);
    suite_add_tcase(s, tc);

    return s;
}

int main(void)
{
    Suite *s = page_up_typing_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_VERBOSE);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
