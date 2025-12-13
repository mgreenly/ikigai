#include "agent.h"
/**
 * @file repl_combined_render_test.c
 * @brief Unit tests for combined scrollback + input buffer rendering (Phase 4 Task 4.4)
 */

#include <check.h>
#include "../../../src/agent.h"
#include "../../../src/shared.h"
#include <talloc.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "../../../src/repl.h"
#include "../../../src/error.h"
#include "../../test_utils.h"

// Mock write() implementation to avoid actual terminal writes
static char *mock_write_buffer = NULL;
static size_t mock_write_size = 0;

// Mock wrapper declaration
ssize_t posix_write_(int fd, const void *buf, size_t count);

ssize_t posix_write_(int fd, const void *buf, size_t count)
{
    (void)fd; // Unused in mock

    // Capture the write
    if (mock_write_buffer != NULL) {
        free(mock_write_buffer);
    }
    mock_write_buffer = malloc(count + 1);
    if (mock_write_buffer != NULL) {
        memcpy(mock_write_buffer, buf, count);
        mock_write_buffer[count] = '\0';
        mock_write_size = count;
    }

    return (ssize_t)count;
}

static void mock_write_reset(void)
{
    if (mock_write_buffer != NULL) {
        free(mock_write_buffer);
        mock_write_buffer = NULL;
    }
    mock_write_size = 0;
}

/* Test: Render frame with empty scrollback (input buffer only) */
START_TEST(test_render_frame_empty_scrollback) {
    mock_write_reset();
    void *ctx = talloc_new(NULL);
    res_t res;

    // Create minimal REPL context for rendering
    ik_term_ctx_t *term = talloc_zero(ctx, ik_term_ctx_t);
    term->screen_rows = 24;
    term->screen_cols = 80;
    term->tty_fd = -1;  // Not actually writing

    ik_render_ctx_t *render = NULL;
    res = ik_render_create(ctx, term->screen_rows, term->screen_cols, term->tty_fd, &render);
    ck_assert(is_ok(&res));

    // Create agent context (with input_buffer and layer_cake)
    ik_agent_ctx_t *agent = NULL;
    res = ik_test_create_agent(ctx, &agent);
    ck_assert(is_ok(&res));

    ik_repl_ctx_t *repl = talloc_zero(ctx, ik_repl_ctx_t);
    repl->current = talloc_zero(repl, ik_agent_ctx_t);
    ik_shared_ctx_t *shared = talloc_zero(repl, ik_shared_ctx_t);
    repl->shared = shared;
    repl->current = agent;
    shared->term = term;
    shared->render = render;

    // Render frame - should succeed even with empty scrollback
    res = ik_repl_render_frame(repl);
    if (is_err(&res)) {
        error_fprintf(stderr, res.err);
    }
    ck_assert(is_ok(&res));

    // CRITICAL: Verify separator line appears even with empty scrollback
    ck_assert_msg(mock_write_buffer != NULL, "Expected render output");

    // Look for separator line - using box-drawing character U+2500 (0xE2 0x94 0x80 in UTF-8)
    int found_separator = 0;
    for (size_t i = 0; i + 2 < mock_write_size; i++) {
        if ((unsigned char)mock_write_buffer[i] == 0xE2 &&
            (unsigned char)mock_write_buffer[i + 1] == 0x94 &&
            (unsigned char)mock_write_buffer[i + 2] == 0x80) {
            found_separator = 1;
            break;
        }
    }
    ck_assert_msg(found_separator, "Expected separator line (box-drawing) even with empty scrollback");

    talloc_free(ctx);
    mock_write_reset();
}
END_TEST

/* Test: Render frame with scrollback content */
START_TEST(test_render_frame_with_scrollback)
{
    mock_write_reset();
    void *ctx = talloc_new(NULL);
    res_t res;

    ik_term_ctx_t *term = talloc_zero(ctx, ik_term_ctx_t);
    term->screen_rows = 24;
    term->screen_cols = 80;
    term->tty_fd = -1;

    ik_render_ctx_t *render = NULL;
    res = ik_render_create(ctx, term->screen_rows, term->screen_cols, term->tty_fd, &render);
    ck_assert(is_ok(&res));

    // Create agent context (with input_buffer and layer_cake)
    ik_agent_ctx_t *agent = NULL;
    res = ik_test_create_agent(ctx, &agent);
    ck_assert(is_ok(&res));
    ik_input_buffer_t *input_buf = agent->input_buffer;

    // Add some content to input buffer
    res = ik_input_buffer_insert_codepoint(input_buf, 'h');
    ck_assert(is_ok(&res));
    res = ik_input_buffer_insert_codepoint(input_buf, 'i');
    ck_assert(is_ok(&res));

    // Add scrollback content to the agent's existing scrollback
    res = ik_scrollback_append_line(agent->scrollback, "line 1", 6);
    ck_assert(is_ok(&res));
    res = ik_scrollback_append_line(agent->scrollback, "line 2", 6);
    ck_assert(is_ok(&res));

    ik_repl_ctx_t *repl = talloc_zero(ctx, ik_repl_ctx_t);
    repl->current = talloc_zero(repl, ik_agent_ctx_t);
    ik_shared_ctx_t *shared = talloc_zero(repl, ik_shared_ctx_t);
    repl->shared = shared;
    repl->current = agent;
    shared->term = term;
    shared->render = render;

    // Render frame - should render both scrollback and input buffer
    res = ik_repl_render_frame(repl);
    if (is_err(&res)) {
        error_fprintf(stderr, res.err);
    }
    ck_assert(is_ok(&res));

    // CRITICAL: Verify output contains BOTH scrollback and input buffer content
    ck_assert_msg(mock_write_buffer != NULL, "Expected render output");

    // Output should contain scrollback lines
    ck_assert_msg(strstr(mock_write_buffer, "line 1") != NULL,
                  "Expected 'line 1' in output");
    ck_assert_msg(strstr(mock_write_buffer, "line 2") != NULL,
                  "Expected 'line 2' in output");

    // Output should contain input buffer content
    ck_assert_msg(strstr(mock_write_buffer, "hi") != NULL,
                  "Expected 'hi' in output");

    // Verify scrollback appears before input buffer
    char *line1_pos = strstr(mock_write_buffer, "line 1");
    char *hi_pos = strstr(mock_write_buffer, "hi");
    ck_assert_msg(line1_pos < hi_pos,
                  "Scrollback should appear before input buffer");

    // Verify only ONE screen clear (should be "\x1b[2J" only once)
    const char *clear_seq = "\x1b[2J";
    char *first_clear = strstr(mock_write_buffer, clear_seq);
    ck_assert_msg(first_clear != NULL, "Expected screen clear");
    char *second_clear = strstr(first_clear + strlen(clear_seq), clear_seq);
    ck_assert_msg(second_clear == NULL,
                  "Should only have ONE screen clear, not two");

    talloc_free(ctx);
    mock_write_reset();
}

END_TEST

/* Create test suite */
static Suite *repl_combined_render_suite(void)
{
    Suite *s = suite_create("REPL Combined Rendering");

    TCase *tc_render = tcase_create("Combined Render");
    tcase_set_timeout(tc_render, 30);
    tcase_add_test(tc_render, test_render_frame_empty_scrollback);
    tcase_add_test(tc_render, test_render_frame_with_scrollback);
    suite_add_tcase(s, tc_render);

    return s;
}

int main(void)
{
    Suite *s = repl_combined_render_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    ik_test_reset_terminal();

    return (number_failed == 0) ? 0 : 1;
}
