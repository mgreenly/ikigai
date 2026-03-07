#include "apps/ikigai/agent.h"
/**
 * @file repl_page_up_down_cursor_test.c
 * @brief Regression test for cursor off-by-one after Page Up + Page Down round-trip.
 *
 * Root cause: ik_repl_calculate_document_height previously read stale
 * input_buffer_visible and spinner_state.visible from the agent struct.
 * After Page Up, those flags were set to false (input off-screen), so the
 * next viewport calculation used the wrong document height, landing the
 * cursor 1 row too low.
 *
 * Fix: ik_repl_calculate_document_height now derives visibility from
 * agent state (IDLE/WAITING_FOR_LLM/EXECUTING_TOOL + dead flag), eliminating
 * the cross-frame state dependency.
 */

#include <check.h>
#include "apps/ikigai/agent.h"
#include "apps/ikigai/shared.h"
#include <talloc.h>
#include <string.h>
#include <inttypes.h>
#include "apps/ikigai/repl.h"
#include "apps/ikigai/scrollback.h"
#include "apps/ikigai/input_buffer/core.h"
#include "tests/helpers/test_utils_helper.h"

/**
 * Regression test: Page Up + Page Down round-trip preserves cursor position.
 *
 * Scenario:
 * - 30 scrollback lines, 1 input buffer line, 10-row terminal
 * - Document height = 32 (IDLE: 30 scrollback + 1 sep + 1 input)
 * - At bottom (offset=0): input visible at screen row 9
 * - After stale-flag simulation: set input_buffer_visible = false on struct
 *   (mimics state left by Page Up), but atomic state remains IDLE
 * - Verify: input_buffer_start_row is still 9 (not 10 which would be off-screen)
 */
START_TEST(test_page_up_down_cursor_position_unchanged) {
    void *ctx = talloc_new(NULL);

    // Terminal: 10 rows x 80 cols
    ik_term_ctx_t *term = talloc_zero(ctx, ik_term_ctx_t);
    res_t res;
    term->screen_rows = 10;
    term->screen_cols = 80;

    // Create input buffer with one character
    ik_input_buffer_t *input_buf = ik_input_buffer_create(ctx);
    res = ik_input_buffer_insert_codepoint(input_buf, 'x');
    ck_assert(is_ok(&res));
    ik_input_buffer_ensure_layout(input_buf, 80);

    // Create scrollback with 30 short lines
    ik_scrollback_t *scrollback = ik_scrollback_create(ctx, 80);
    for (int32_t i = 0; i < 30; i++) {
        char buf[32];
        snprintf(buf, sizeof(buf), "line %" PRId32, i);
        res = ik_scrollback_append_line(scrollback, buf, strlen(buf));
        ck_assert(is_ok(&res));
    }
    ik_scrollback_ensure_layout(scrollback, 80);

    // Build REPL context
    ik_repl_ctx_t *repl = talloc_zero(ctx, ik_repl_ctx_t);
    ik_shared_ctx_t *shared = talloc_zero(repl, ik_shared_ctx_t);
    repl->shared = shared;
    shared->term = term;

    // Create agent in IDLE state (zero-init = IK_AGENT_STATE_IDLE)
    ik_agent_ctx_t *agent = talloc_zero(repl, ik_agent_ctx_t);
    repl->current = agent;
    agent->input_buffer = input_buf;
    agent->scrollback = scrollback;
    agent->viewport_offset = 0;
    // state is zero-init = IK_AGENT_STATE_IDLE
    // dead is zero-init = false

    // Verify document structure:
    // IDLE state => spinner_visible=false, input_buf_visible=true
    // doc_height = 30 (scrollback) + 1 (sep) + 1 (input) = 32
    // terminal=10 rows => at offset=0: shows rows 22-31
    // input at doc row 31 => screen row = 31 - 22 = 9

    // Calculate viewport at bottom (fresh state, no stale flags)
    ik_viewport_t viewport_bottom;
    res = ik_repl_calculate_viewport(repl, &viewport_bottom);
    ck_assert(is_ok(&res));

    size_t expected_input_row = 9;  // screen row 9 (0-indexed)
    ck_assert_msg(viewport_bottom.input_buffer_start_row == expected_input_row,
                  "At bottom: expected input_buffer_start_row=%zu, got %zu",
                  expected_input_row, viewport_bottom.input_buffer_start_row);

    // Simulate stale flag: set input_buffer_visible = false on struct.
    // This is what ik_repl_render_frame would leave behind after a Page Up
    // render where the input buffer was scrolled off-screen.
    // With the old (buggy) code, calculate_viewport would read this stale
    // value and compute doc_height = 31 instead of 32, placing the input
    // buffer off-screen when it should be at row 9.
    agent->input_buffer_visible = false;

    // Recalculate viewport at same offset=0 with stale flag set.
    // After the fix: ignores input_buffer_visible, reads atomic state=IDLE,
    // derives input_buf_visible=true => doc_height=32 => input at row 9.
    ik_viewport_t viewport_after;
    res = ik_repl_calculate_viewport(repl, &viewport_after);
    ck_assert(is_ok(&res));

    ck_assert_msg(viewport_after.input_buffer_start_row == expected_input_row,
                  "After stale flag: expected input_buffer_start_row=%zu, got %zu "
                  "(stale input_buffer_visible=false caused off-by-one before fix)",
                  expected_input_row, viewport_after.input_buffer_start_row);

    talloc_free(ctx);
}
END_TEST

/* Create test suite */
static Suite *page_up_down_cursor_suite(void)
{
    Suite *s = suite_create("REPL: Page Up + Page Down cursor position");

    TCase *tc = tcase_create("CursorRoundTrip");
    tcase_set_timeout(tc, IK_TEST_TIMEOUT);
    tcase_add_test(tc, test_page_up_down_cursor_position_unchanged);
    suite_add_tcase(s, tc);

    return s;
}

int main(void)
{
    Suite *s = page_up_down_cursor_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/apps/ikigai/repl/repl_page_up_down_cursor_test.xml");

    srunner_run_all(sr, CK_VERBOSE);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
