/**
 * @file repl_separator_partial_line_test.c
 * @brief Test Separator visibility: Partial first line in viewport
 *
 * This test checks the case where the viewport starts in the middle of a
 * wrapped scrollback line. The viewport calculation must account for the
 * partial first line when counting visible lines.
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
 * Test: Viewport starts mid-line (row_offset != 0)
 *
 * Setup:
 *   - Line 0: 3 physical rows (document rows 0-2)
 *   - Line 1: 3 physical rows (document rows 3-5)
 *   - Line 2: 3 physical rows (document rows 6-8)
 *   - Line 3: 3 physical rows (document rows 9-11)
 *   - Line 4: 3 physical rows (document rows 12-14)
 *   - Separator: row 15
 *   - input buffer: row 16
 *
 *   Viewport shows rows 1-10 (10 rows):
 *     - Row 1-2: end of line 0 (2 rows, partial)
 *     - Row 3-5: line 1 (3 rows, complete)
 *     - Row 6-8: line 2 (3 rows, complete)
 *     - Row 9-10: start of line 3 (2 rows, partial)
 *
 *   Expected: Include lines 0, 1, 2, 3 (4 lines total)
 *   Bug: Counting doesn't account for row_offset, includes wrong number of lines
 */
START_TEST(test_separator_partial_first_line) {
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

    // Create scrollback with long lines that wrap to exactly 3 physical rows
    ik_scrollback_t *scrollback = ik_scrollback_create(ctx, 80);

    // Each line is 161 characters (wraps to 3 rows: 80 + 80 + 1)
    for (int32_t i = 0; i < 10; i++) {
        char buf[256];
        snprintf(buf, sizeof(buf), "LINE%02" PRId32 " ", i);
        // Pad to exactly 161 characters
        size_t current_len = strlen(buf);
        while (current_len < 161) {
            buf[current_len] = (char)('A' + (current_len % 26));
            current_len++;
        }
        buf[161] = '\0';

        res = ik_scrollback_append_line(scrollback, buf, strlen(buf));
        ck_assert(is_ok(&res));
    }

    // Verify lines wrap to 3 physical rows each
    ik_scrollback_ensure_layout(scrollback, 80);
    ck_assert_uint_eq(scrollback->layouts[0].physical_lines, 3);
    ck_assert_uint_eq(scrollback->layouts[1].physical_lines, 3);

    // Total: 10 lines * 3 rows = 30 scrollback rows + 1 sep + 1 ws = 32 rows

    // Create render context
    ik_render_ctx_t *render_ctx = NULL;
    res = ik_render_create(ctx, 10, 80, 1, &render_ctx);
    ck_assert(is_ok(&res));

    // Create REPL and scroll to show rows 1-10 (starts mid-line)
    ik_repl_ctx_t *repl = talloc_zero(ctx, ik_repl_ctx_t);
    repl->term = term;
    repl->input_buffer = input_buf;
    repl->scrollback = scrollback;
    repl->render = render_ctx;

    // To show document rows 1-10:
    // last_visible_row = 10
    // first_visible_row = 1
    // document_height = 32
    // last_visible_row = document_height - 1 - offset
    // 10 = 32 - 1 - offset
    // offset = 21
    repl->viewport_offset = 21;

    // Calculate viewport
    ik_viewport_t viewport;
    res = ik_repl_calculate_viewport(repl, &viewport);
    ck_assert(is_ok(&res));

    printf("Viewport: start_line=%zu, lines_count=%zu, input_buffer_start_row=%zu\n",
           viewport.scrollback_start_line, viewport.scrollback_lines_count,
           viewport.input_buffer_start_row);

    // Row 1 is in the middle of line 0 (row_offset = 1)
    // Lines 0-3 should be visible:
    //   - Line 0: rows 0-2, visible rows 1-2 (partial, 2 rows shown)
    //   - Line 1: rows 3-5, all visible (3 rows)
    //   - Line 2: rows 6-8, all visible (3 rows)
    //   - Line 3: rows 9-11, visible rows 9-10 (partial, 2 rows shown)
    //   Total: 2 + 3 + 3 + 2 = 10 rows ✓
    //
    // Expected: start_line=0, lines_count=4
    // Bug: May calculate incorrect count due to not using row_offset
    ck_assert_uint_eq(viewport.scrollback_start_line, 0);

    // This is the key assertion - we should include 4 lines to fill the viewport
    // Separator visibility would cause this to fail (might be 3 or 5 depending on the bug)
    ck_assert_uint_eq(viewport.scrollback_lines_count, 4);

    talloc_free(ctx);
}
END_TEST
/**
 * Test: Verify row_offset is used correctly
 *
 * Simpler test that directly checks the impact of starting mid-line
 */
START_TEST(test_separator_row_offset_impact)
{
    void *ctx = talloc_new(NULL);

    // Terminal: 5 rows x 80 cols (small to make math easier)
    ik_term_ctx_t *term = talloc_zero(ctx, ik_term_ctx_t);
    res_t res;
    term->screen_rows = 5;
    term->screen_cols = 80;

    // Create input buffer
    ik_input_buffer_t *input_buf = NULL;
    input_buf = ik_input_buffer_create(ctx);
    res = ik_input_buffer_insert_codepoint(input_buf, 'w');
    ck_assert(is_ok(&res));
    ik_input_buffer_ensure_layout(input_buf, 80);

    // Create scrollback with 2-row lines
    ik_scrollback_t *scrollback = ik_scrollback_create(ctx, 80);

    // Each line wraps to exactly 2 rows (81 chars)
    for (int32_t i = 0; i < 20; i++) {
        char buf[128];
        snprintf(buf, sizeof(buf), "L%02" PRId32 " ", i);
        size_t len = strlen(buf);
        while (len < 81) {
            buf[len++] = 'x';
        }
        buf[81] = '\0';
        res = ik_scrollback_append_line(scrollback, buf, strlen(buf));
        ck_assert(is_ok(&res));
    }

    ik_scrollback_ensure_layout(scrollback, 80);
    ck_assert_uint_eq(scrollback->layouts[0].physical_lines, 2);

    // Total: 20 lines * 2 rows = 40 rows + 1 sep + 1 ws = 42 rows

    // Create render context
    ik_render_ctx_t *render_ctx = NULL;
    res = ik_render_create(ctx, 5, 80, 1, &render_ctx);
    ck_assert(is_ok(&res));

    // Create REPL
    ik_repl_ctx_t *repl = talloc_zero(ctx, ik_repl_ctx_t);
    repl->term = term;
    repl->input_buffer = input_buf;
    repl->scrollback = scrollback;
    repl->render = render_ctx;

    // Show rows 1-5 (starts in middle of line 0)
    // Line 0: rows 0-1
    // Line 1: rows 2-3
    // Line 2: rows 4-5
    // Line 3: rows 6-7
    //
    // Visible:
    //   Row 1: end of line 0
    //   Rows 2-3: line 1
    //   Rows 4-5: line 2
    //
    // Should include lines 0, 1, 2 (3 lines)
    //
    // last_visible = 5, first_visible = 1
    // offset = 42 - 1 - 5 = 36
    repl->viewport_offset = 36;

    ik_viewport_t viewport;
    res = ik_repl_calculate_viewport(repl, &viewport);
    ck_assert(is_ok(&res));

    printf("Viewport: start=%zu, count=%zu\n",
           viewport.scrollback_start_line, viewport.scrollback_lines_count);

    ck_assert_uint_eq(viewport.scrollback_start_line, 0);
    ck_assert_uint_eq(viewport.scrollback_lines_count, 3);

    talloc_free(ctx);
}

END_TEST

/* Create test suite */
static Suite *separator_partial_suite(void)
{
    Suite *s = suite_create("Separator visibility: Partial Line");

    TCase *tc_partial = tcase_create("Partial");
    tcase_set_timeout(tc_partial, 30);
    tcase_add_test(tc_partial, test_separator_partial_first_line);
    tcase_add_test(tc_partial, test_separator_row_offset_impact);
    suite_add_tcase(s, tc_partial);

    return s;
}

int main(void)
{
    Suite *s = separator_partial_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_VERBOSE);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
