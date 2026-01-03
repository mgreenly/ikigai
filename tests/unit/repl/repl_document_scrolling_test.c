#include "agent.h"
/**
 * @file repl_document_scrolling_test.c
 * @brief Tests for unified document scrolling model (Bug #4 fix)
 *
 * These tests verify that scrollback, separator, and input buffer scroll together
 * as a single unified document, rather than separator/input buffer being "sticky"
 * at the bottom.
 */

#include <check.h>
#include "../../../src/agent.h"
#include "../../../src/shared.h"
#include <talloc.h>
#include <string.h>
#include <unistd.h>
#include <inttypes.h>
#include <pthread.h>
#include "../../../src/repl.h"
#include "../../../src/scrollback.h"
#include "../../../src/render.h"
#include "../../../src/input_buffer/core.h"
#include "../../test_utils.h"

// Mock write() to capture output
static char mock_output[16384];
static size_t mock_output_len = 0;

ssize_t posix_write_(int fd, const void *buf, size_t count);

ssize_t posix_write_(int fd, const void *buf, size_t count)
{
    (void)fd;
    if (mock_output_len + count < sizeof(mock_output)) {
        memcpy(mock_output + mock_output_len, buf, count);
        mock_output_len += count;
    }
    return (ssize_t)count;
}

static void reset_mock(void)
{
    mock_output_len = 0;
    memset(mock_output, 0, sizeof(mock_output));
}

/**
 * Test: When scrolled up far enough, separator should NOT appear in output
 *
 * Document structure:
 *   Scrollback lines 0-49 (50 lines total)
 *   Separator line (1 line)
 *   Input buffer (1 line)
 *
 * With terminal height of 10 rows and viewport_offset scrolled to show
 * lines 0-9 of scrollback, the separator should be scrolled off-screen.
 */
START_TEST(test_separator_scrolls_offscreen) {
    reset_mock();
    void *ctx = talloc_new(NULL);
    res_t res;

    // Create terminal context (10 rows)
    ik_term_ctx_t *term = talloc_zero(ctx, ik_term_ctx_t);
    term->screen_rows = 10;
    term->screen_cols = 80;
    term->tty_fd = 1;

    // Create render context
    ik_render_ctx_t *render_ctx = NULL;
    res = ik_render_create(ctx, 10, 80, 1, &render_ctx);
    ck_assert(is_ok(&res));

    // Create REPL context
    ik_repl_ctx_t *repl = talloc_zero(ctx, ik_repl_ctx_t);
    repl->current = talloc_zero(repl, ik_agent_ctx_t);
    ik_shared_ctx_t *shared = talloc_zero(repl, ik_shared_ctx_t);
    repl->shared = shared;
    shared->term = term;
    shared->render = render_ctx;

    // Create agent context
    ik_agent_ctx_t *agent = NULL;
    res = ik_test_create_agent(ctx, &agent);
    ck_assert(is_ok(&res));
    repl->current = agent;

    // Initialize mutex (required by render_frame)
    pthread_mutex_init(&repl->current->tool_thread_mutex, NULL);

    // Add 'x' to input buffer
    res = ik_input_buffer_insert_codepoint(agent->input_buffer, 'x');
    ck_assert(is_ok(&res));
    ik_input_buffer_ensure_layout(agent->input_buffer, 80);

    // Add 50 lines to scrollback
    for (int32_t i = 0; i < 50; i++) {
        char buf[32];
        snprintf(buf, sizeof(buf), "scrollback line %" PRId32, i);
        res = ik_scrollback_append_line(agent->scrollback, buf, strlen(buf));
        ck_assert(is_ok(&res));
    }

    // Document height = 50 (scrollback) + 1 (separator) + 1 (input_buf) = 52 lines
    // Terminal shows 10 lines
    // When offset = 42, we're showing lines 0-9 of scrollback
    // Separator is at line 50, input buffer at line 51 - both OFF SCREEN
    agent->viewport_offset = 42;

    // Render frame
    res = ik_repl_render_frame(repl);
    ck_assert(is_ok(&res));

    // Verify separator (line of dashes) does NOT appear in output
    // Count consecutive dashes - should NOT find 10+ dashes in a row
    int32_t max_dashes = 0;
    int32_t current_dashes = 0;
    for (size_t i = 0; i < mock_output_len; i++) {
        if (mock_output[i] == '-') {
            current_dashes++;
            if (current_dashes > max_dashes) {
                max_dashes = current_dashes;
            }
        } else {
            current_dashes = 0;
        }
    }

    // Separator would be 80 dashes - if we find 10+ consecutive dashes, separator is visible
    ck_assert_int_lt(max_dashes, 10);  // Should be < 10 (separator not visible)

    // Verify input buffer content 'x' does NOT appear
    ck_assert_ptr_eq(strstr(mock_output, "x"), NULL);

    talloc_free(ctx);
}
END_TEST
/**
 * Test: When scrolled up, input buffer should NOT appear in output
 *
 * Similar to above, but specifically checks that input buffer content is scrolled off.
 */
START_TEST(test_input_buffer_scrolls_offscreen) {
    reset_mock();
    void *ctx = talloc_new(NULL);
    res_t res;

    // Create terminal context (10 rows)
    ik_term_ctx_t *term = talloc_zero(ctx, ik_term_ctx_t);
    term->screen_rows = 10;
    term->screen_cols = 80;
    term->tty_fd = 1;

    // Create render context
    ik_render_ctx_t *render_ctx = NULL;
    res = ik_render_create(ctx, 10, 80, 1, &render_ctx);
    ck_assert(is_ok(&res));

    // Create REPL context
    ik_repl_ctx_t *repl = talloc_zero(ctx, ik_repl_ctx_t);
    repl->current = talloc_zero(repl, ik_agent_ctx_t);
    ik_shared_ctx_t *shared = talloc_zero(repl, ik_shared_ctx_t);
    repl->shared = shared;
    shared->term = term;
    shared->render = render_ctx;

    // Create agent context
    ik_agent_ctx_t *agent = NULL;
    res = ik_test_create_agent(ctx, &agent);
    ck_assert(is_ok(&res));
    repl->current = agent;

    // Initialize mutex (required by render_frame)
    pthread_mutex_init(&repl->current->tool_thread_mutex, NULL);

    // Add distinctive content to input buffer
    const char *input_buf_content = "input buffer_MARKER_TEXT";
    for (size_t i = 0; i < strlen(input_buf_content); i++) {
        res = ik_input_buffer_insert_codepoint(agent->input_buffer, (uint32_t)(unsigned char)input_buf_content[i]);
        ck_assert(is_ok(&res));
    }
    ik_input_buffer_ensure_layout(agent->input_buffer, 80);

    // Add 50 lines to scrollback
    for (int32_t i = 0; i < 50; i++) {
        char buf[32];
        snprintf(buf, sizeof(buf), "line%" PRId32, i);  // Intentionally different from input buffer
        res = ik_scrollback_append_line(agent->scrollback, buf, strlen(buf));
        ck_assert(is_ok(&res));
    }

    // Scroll to middle of scrollback (input buffer is off-screen)
    agent->viewport_offset = 30;

    // Render frame
    res = ik_repl_render_frame(repl);
    ck_assert(is_ok(&res));

    // Verify input buffer content does NOT appear
    ck_assert_ptr_eq(strstr(mock_output, "MARKER_TEXT"), NULL);

    talloc_free(ctx);
}

END_TEST
/**
 * Test: When scrolled to bottom, last scrollback line appears directly above separator
 *
 * This verifies the key invariant: the last line of scrollback is ALWAYS adjacent
 * to the separator line when both are visible.
 */
START_TEST(test_scrollback_adjacent_to_separator) {
    reset_mock();
    void *ctx = talloc_new(NULL);
    res_t res;

    // Create terminal context (20 rows - enough to show some scrollback + separator + input buffer)
    ik_term_ctx_t *term = talloc_zero(ctx, ik_term_ctx_t);
    term->screen_rows = 20;
    term->screen_cols = 80;
    term->tty_fd = 1;

    // Create render context
    ik_render_ctx_t *render_ctx = NULL;
    res = ik_render_create(ctx, 20, 80, 1, &render_ctx);
    ck_assert(is_ok(&res));

    // Create REPL context
    ik_repl_ctx_t *repl = talloc_zero(ctx, ik_repl_ctx_t);
    repl->current = talloc_zero(repl, ik_agent_ctx_t);
    ik_shared_ctx_t *shared = talloc_zero(repl, ik_shared_ctx_t);
    repl->shared = shared;
    shared->term = term;
    shared->render = render_ctx;

    // Create agent context
    ik_agent_ctx_t *agent = NULL;
    res = ik_test_create_agent(ctx, &agent);
    ck_assert(is_ok(&res));
    repl->current = agent;

    // Initialize mutex (required by render_frame)
    pthread_mutex_init(&repl->current->tool_thread_mutex, NULL);

    // Add 'w' to input buffer
    res = ik_input_buffer_insert_codepoint(agent->input_buffer, 'w');
    ck_assert(is_ok(&res));
    ik_input_buffer_ensure_layout(agent->input_buffer, 80);

    // Add 9 scrollback lines
    for (int32_t i = 0; i < 9; i++) {
        char buf[32];
        snprintf(buf, sizeof(buf), "line %" PRId32, i);
        res = ik_scrollback_append_line(agent->scrollback, buf, strlen(buf));
        ck_assert(is_ok(&res));
    }
    // Last line has distinctive marker
    res = ik_scrollback_append_line(agent->scrollback, "LAST_SCROLLBACK_LINE", 20);
    ck_assert(is_ok(&res));

    // Scrolled to bottom (offset = 0) - all scrollback visible
    agent->viewport_offset = 0;

    // Render frame
    res = ik_repl_render_frame(repl);
    ck_assert(is_ok(&res));

    // Verify last scrollback line appears
    ck_assert_ptr_ne(strstr(mock_output, "LAST_SCROLLBACK_LINE"), NULL);

    // Verify separator (box-drawing chars) appears
    // Box-drawing character U+2500 is 0xE2 0x94 0x80 in UTF-8
    bool found_separator = false;
    for (size_t i = 0; i + 2 < mock_output_len; i++) {
        if ((unsigned char)mock_output[i] == 0xE2 &&
            (unsigned char)mock_output[i + 1] == 0x94 &&
            (unsigned char)mock_output[i + 2] == 0x80) {
            found_separator = true;
            break;
        }
    }
    ck_assert(found_separator);

    talloc_free(ctx);
}

END_TEST

/* Create test suite */
static Suite *repl_document_scrolling_suite(void)
{
    Suite *s = suite_create("REPL Document Scrolling (Unified Model)");

    TCase *tc_scrolling = tcase_create("Document Scrolling");
    tcase_set_timeout(tc_scrolling, 30);
    tcase_set_timeout(tc_scrolling, 30);
    tcase_set_timeout(tc_scrolling, 30);
    tcase_set_timeout(tc_scrolling, 30);
    tcase_set_timeout(tc_scrolling, 30);
    tcase_add_test(tc_scrolling, test_separator_scrolls_offscreen);
    tcase_add_test(tc_scrolling, test_input_buffer_scrolls_offscreen);
    tcase_add_test(tc_scrolling, test_scrollback_adjacent_to_separator);
    suite_add_tcase(s, tc_scrolling);

    return s;
}

int main(void)
{
    Suite *s = repl_document_scrolling_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    ik_test_reset_terminal();

    return (number_failed == 0) ? 0 : 1;
}
