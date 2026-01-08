#include "agent.h"
/**
 * @file repl_full_viewport_test.c
 * @brief Test for layer positioning when viewport is full
 *
 * Bug: When scrollback fills the entire viewport, the document model calculation
 * doesn't account for the lower separator, causing layers to be positioned incorrectly.
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
#include "../../../src/layer.h"
#include "../../../src/layer_wrappers.h"
#include "../../../src/render.h"
#include "../../test_utils.h"

// Mock write wrapper for testing (required by render system)
ssize_t posix_write_(int fd, const void *buf, size_t count);
ssize_t posix_write_(int fd, const void *buf, size_t count)
{
    (void)fd;
    (void)buf;
    return (ssize_t)count;
}

// Helper function to create a basic REPL context for testing
static void create_test_repl(TALLOC_CTX *ctx, int32_t rows, int32_t cols, ik_repl_ctx_t **repl_out)
{
    res_t res;

    // Create render context
    ik_render_ctx_t *render = NULL;
    res = ik_render_create(ctx, rows, cols, 1, &render);
    ck_assert(is_ok(&res));

    // Create terminal context
    ik_term_ctx_t *term = talloc_zero(ctx, ik_term_ctx_t);
    term->screen_rows = rows;
    term->screen_cols = cols;
    term->tty_fd = 1;

    // Create REPL with layer cake
    ik_repl_ctx_t *repl = talloc_zero(ctx, ik_repl_ctx_t);
    repl->current = talloc_zero(repl, ik_agent_ctx_t);
    ck_assert_ptr_nonnull(repl);
    ik_shared_ctx_t *shared = talloc_zero(repl, ik_shared_ctx_t);
    repl->shared = shared;
    shared->render = render;
    shared->term = term;

    // Create agent context using ik_test_create_agent
    ik_agent_ctx_t *agent = NULL;
    res = ik_test_create_agent(ctx, &agent);
    ck_assert(is_ok(&res));
    repl->current = agent;
    agent->viewport_offset = 0;

    // Setup lower separator layer on repl
    repl->lower_separator_visible = true;
    repl->lower_separator_layer = ik_separator_layer_create(repl, "lower_separator", &repl->lower_separator_visible);
    res = ik_layer_cake_add_layer(agent->layer_cake, repl->lower_separator_layer);
    ck_assert(is_ok(&res));

    *repl_out = repl;
}

/**
 * Test: Layer positions correct when scrollback fills viewport
 *
 * Setup:
 * - Terminal: 20 rows x 80 cols
 * - Scrollback: 15+ lines (enough to fill most of viewport)
 * - Input buffer: 1 line
 * - Upper separator: 1 line
 * - Lower separator: 1 line
 *
 * Expected layout (from bottom):
 * Row 0-14: scrollback (15 lines)
 * Row 15: upper separator
 * Row 16: input buffer
 * Row 17: lower separator
 * Rows 18-19: empty (or scrollback if available)
 *
 * Bug manifestation:
 * - Without fix: document_height = scrollback + 1 (sep) + input = 17 rows
 * - With fix: document_height = scrollback + 1 (sep) + input + 1 (lower_sep) = 18 rows
 */
START_TEST(test_layer_positions_when_viewport_full) {
    void *ctx = talloc_new(NULL);

    // Create REPL with 20 rows x 80 cols
    ik_repl_ctx_t *repl = NULL;
    create_test_repl(ctx, 20, 80, &repl);
    res_t res;

    // Add 15 lines to scrollback (fills most of viewport)
    for (int32_t i = 0; i < 15; i++) {
        char buf[64];
        snprintf(buf, sizeof(buf), "scrollback line %d - content here", i + 1);
        res = ik_scrollback_append_line(repl->current->scrollback, buf, strlen(buf));
        ck_assert(is_ok(&res));
    }
    ik_scrollback_ensure_layout(repl->current->scrollback, 80);

    // Add text to input buffer
    res = ik_input_buffer_insert_codepoint(repl->current->input_buffer, '*');
    ck_assert(is_ok(&res));
    ik_input_buffer_ensure_layout(repl->current->input_buffer, 80);

    // Update input text pointers
    size_t text_len = 0;
    const char *text = ik_input_buffer_get_text(repl->current->input_buffer, &text_len);
    repl->current->input_text = text;
    repl->current->input_text_len = text_len;

    // Calculate viewport (viewport_offset = 0 means showing bottom of document)
    repl->current->viewport_offset = 0;

    ik_viewport_t viewport;
    res = ik_repl_calculate_viewport(repl, &viewport);
    ck_assert(is_ok(&res));

    // Verify scrollback is visible
    ck_assert_msg(viewport.scrollback_lines_count > 0,
                  "Expected scrollback to be visible, got count=%zu", viewport.scrollback_lines_count);

    // Verify separator is visible
    ck_assert_msg(viewport.separator_visible,
                  "Expected upper separator to be visible");

    // Verify input buffer is visible and positioned correctly
    // With the bug: input_buffer_start_row might be incorrect due to missing lower_sep in document_height
    // Expected: input should be at row 16 (after 15 scrollback + 1 separator)
    // But we're showing the bottom of the document, so need to calculate properly

    // Get actual component sizes
    size_t scrollback_rows = ik_scrollback_get_total_physical_lines(repl->current->scrollback);
    size_t input_buffer_rows = ik_input_buffer_get_physical_lines(repl->current->input_buffer);

    // Document model (CORRECT - including lower separator):
    // scrollback_rows (15) + 1 (upper_sep) + input_buffer_rows (1) + 1 (lower_sep) = 18 rows
    size_t expected_document_height = scrollback_rows + 1 + input_buffer_rows + 1;

    // The BUGGY implementation calculates document height as:
    // scrollback_rows (15) + 1 (upper_sep) + input_buffer_rows (1) = 17 rows (missing lower_sep!)

    // Since document fits within terminal (20 rows), everything should be visible from top
    // first_visible_row = 0, last_visible_row = 17

    // BUG TEST: With the buggy code, when we scroll such that the viewport is nearly full,
    // the viewport calculation will think the document is shorter than it actually is.
    // This causes the viewport to show incorrect ranges.

    // Let's scroll up a bit to trigger the bug - set viewport_offset so only 18 rows fit
    // If document_height is incorrectly 17, then max_offset = 17 - 20 = 0 (no scroll allowed)
    // If document_height is correctly 18, then max_offset = 18 - 20 = 0 (no scroll allowed)
    // Hmm, both fit. Let me create a scenario where scrollback is larger...

    // Actually, let's add more scrollback to make document > terminal height
    for (int32_t i = 15; i < 20; i++) {
        char buf[64];
        snprintf(buf, sizeof(buf), "scrollback line %d - content here", i + 1);
        res = ik_scrollback_append_line(repl->current->scrollback, buf, strlen(buf));
        ck_assert(is_ok(&res));
    }
    ik_scrollback_ensure_layout(repl->current->scrollback, 80);

    // Recompute viewport with larger scrollback
    res = ik_repl_calculate_viewport(repl, &viewport);
    ck_assert(is_ok(&res));

    // Now we have: 20 scrollback + 1 sep + 1 input + 1 lower_sep = 23 rows total
    // Terminal is 20 rows, so document overflows
    // Expected: showing bottom of document (viewport_offset = 0)
    // Document rows 3-22 should be visible (20 rows)

    scrollback_rows = ik_scrollback_get_total_physical_lines(repl->current->scrollback);
    expected_document_height = scrollback_rows + 1 + input_buffer_rows + 1;  // 20 + 1 + 1 + 1 = 23

    // With BUGGY code: document_height = 20 + 1 + 1 = 22 (missing lower_sep)
    // last_visible_row = 22 - 1 - 0 = 21
    // first_visible_row = 21 + 1 - 20 = 2
    // Input buffer at doc row 21, viewport row = 21 - 2 = 19 (correct - at bottom of screen)

    // With CORRECT code: document_height = 20 + 1 + 1 + 1 = 23
    // last_visible_row = 23 - 1 - 0 = 22
    // first_visible_row = 22 + 1 - 20 = 3
    // Input buffer at doc row 21, viewport row = 21 - 3 = 18

    // BUG: With incorrect document_height, first_visible_row will be off by 1
    // This test will FAIL until we fix the document_height calculation

    ck_assert_msg(viewport.input_buffer_start_row == 18,
                  "Input buffer should be at viewport row 18 (with correct doc height), got %zu",
                  viewport.input_buffer_start_row);

    // Verify no blank line between separator and input
    // This is implicit in the above check - if input_buffer_start_row is correct,
    // there's no gap

    // Calculate total visible height using layer cake
    size_t total_layer_height = ik_layer_cake_get_total_height(repl->current->layer_cake, 80);

    // Expected: scrollback (15) + upper_sep (1) + input (1) + lower_sep (1) = 18 rows
    ck_assert_msg(total_layer_height == expected_document_height,
                  "Total layer height should be %zu, got %zu",
                  expected_document_height, total_layer_height);

    talloc_free(ctx);
}
END_TEST
/**
 * Test: Verify lower separator is accounted for in document height
 *
 * This test verifies that the document height calculation includes the lower separator.
 */
START_TEST(test_document_height_includes_lower_separator) {
    void *ctx = talloc_new(NULL);

    // Create REPL with small terminal (10 rows x 80 cols)
    ik_repl_ctx_t *repl = NULL;
    create_test_repl(ctx, 10, 80, &repl);
    res_t res;

    // Add 5 lines to scrollback
    for (int32_t i = 0; i < 5; i++) {
        char buf[64];
        snprintf(buf, sizeof(buf), "line %d", i + 1);
        res = ik_scrollback_append_line(repl->current->scrollback, buf, strlen(buf));
        ck_assert(is_ok(&res));
    }
    ik_scrollback_ensure_layout(repl->current->scrollback, 80);

    // Add text to input buffer (1 line)
    res = ik_input_buffer_insert_codepoint(repl->current->input_buffer, 'x');
    ck_assert(is_ok(&res));
    ik_input_buffer_ensure_layout(repl->current->input_buffer, 80);

    // Update input text pointers
    size_t text_len = 0;
    const char *text = ik_input_buffer_get_text(repl->current->input_buffer, &text_len);
    repl->current->input_text = text;
    repl->current->input_text_len = text_len;

    // Calculate total visible height using layer cake
    size_t total_layer_height = ik_layer_cake_get_total_height(repl->current->layer_cake, 80);

    // Expected: scrollback (5) + upper_sep (1) + input (1) + lower_sep (1) = 8 rows
    size_t expected_height = 5 + 1 + 1 + 1;

    ck_assert_msg(total_layer_height == expected_height,
                  "Total layer height should be %zu (scrollback 5 + sep 1 + input 1 + lower_sep 1), got %zu",
                  expected_height, total_layer_height);

    talloc_free(ctx);
}

END_TEST
/**
 * Test: Bottom separator visible when viewport is full
 *
 * When the viewport is filled with content, the bottom separator should still be visible
 * (not pushed off-screen).
 */
START_TEST(test_bottom_separator_visible_when_viewport_full) {
    void *ctx = talloc_new(NULL);

    // Create REPL with 20 rows x 80 cols
    ik_repl_ctx_t *repl = NULL;
    create_test_repl(ctx, 20, 80, &repl);
    res_t res;

    // Fill scrollback with 17 lines (leaves exactly 3 rows for sep + input + lower_sep)
    for (int32_t i = 0; i < 17; i++) {
        char buf[64];
        snprintf(buf, sizeof(buf), "scrollback line %d", i + 1);
        res = ik_scrollback_append_line(repl->current->scrollback, buf, strlen(buf));
        ck_assert(is_ok(&res));
    }
    ik_scrollback_ensure_layout(repl->current->scrollback, 80);

    // Add text to input buffer
    res = ik_input_buffer_insert_codepoint(repl->current->input_buffer, '*');
    ck_assert(is_ok(&res));
    ik_input_buffer_ensure_layout(repl->current->input_buffer, 80);

    // Update input text pointers
    size_t text_len = 0;
    const char *text = ik_input_buffer_get_text(repl->current->input_buffer, &text_len);
    repl->current->input_text = text;
    repl->current->input_text_len = text_len;

    // Render frame to test that lower separator is within viewport
    ik_output_buffer_t *output = ik_output_buffer_create(ctx, 4096);
    repl->current->layer_cake->viewport_row = 0;
    repl->current->layer_cake->viewport_height = 20;

    ik_layer_cake_render(repl->current->layer_cake, output, 80);

    // Verify output contains separator characters
    // The lower separator renders as a line of Unicode box-drawing characters (─)
    // UTF-8 encoding: 0xE2 0x94 0x80
    bool found_separator = false;
    for (size_t i = 0; i < output->size - 2; i++) {
        if ((unsigned char)output->data[i] == 0xE2 &&
            (unsigned char)output->data[i + 1] == 0x94 &&
            (unsigned char)output->data[i + 2] == 0x80) {
            found_separator = true;
            break;
        }
    }

    ck_assert_msg(found_separator,
                  "Lower separator should be visible in rendered output");

    talloc_free(ctx);
}

END_TEST

static Suite *repl_full_viewport_suite(void)
{
    Suite *s = suite_create("REPL Full Viewport");

    TCase *tc_core = tcase_create("Core");
    tcase_set_timeout(tc_core, IK_TEST_TIMEOUT);
    tcase_set_timeout(tc_core, IK_TEST_TIMEOUT);
    tcase_set_timeout(tc_core, IK_TEST_TIMEOUT);
    tcase_set_timeout(tc_core, IK_TEST_TIMEOUT);
    tcase_set_timeout(tc_core, IK_TEST_TIMEOUT);
    tcase_add_test(tc_core, test_layer_positions_when_viewport_full);
    tcase_add_test(tc_core, test_document_height_includes_lower_separator);
    tcase_add_test(tc_core, test_bottom_separator_visible_when_viewport_full);

    suite_add_tcase(s, tc_core);
    return s;
}

int main(void)
{
    Suite *s = repl_full_viewport_suite();
    SRunner *sr = srunner_create(s);
    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);
    return (number_failed == 0) ? 0 : 1;
}
