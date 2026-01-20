#include "agent.h"
/**
 * @file repl_cursor_position_basic_test.c
 * @brief Test for cursor position in basic viewport scenarios
 *
 * Bug: When scrollback content leaves exactly one blank line at the bottom
 * of the viewport, the cursor renders on the separator line instead of the
 * input line where the text is being typed.
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
#include "../../test_utils_helper.h"

// Mock write tracking
static int32_t mock_write_calls = 0;
static char mock_write_buffer[8192];
static size_t mock_write_buffer_len = 0;
static bool mock_write_should_fail = false;

// Mock write wrapper declaration
ssize_t posix_write_(int fd, const void *buf, size_t count);

// Mock write wrapper for testing
ssize_t posix_write_(int fd, const void *buf, size_t count)
{
    (void)fd;
    mock_write_calls++;

    if (mock_write_should_fail) {
        return -1;
    }

    if (mock_write_buffer_len + count < sizeof(mock_write_buffer)) {
        memcpy(mock_write_buffer + mock_write_buffer_len, buf, count);
        mock_write_buffer_len += count;
    }
    return (ssize_t)count;
}

// Helper to initialize layer cake for REPL context
static void init_layer_cake(ik_repl_ctx_t *repl, int32_t rows)
{
    res_t res;
    repl->current->spinner_state.frame_index = 0;
    repl->current->spinner_state.visible = false;
    repl->current->separator_visible = true;
    repl->lower_separator_visible = true;
    repl->current->input_buffer_visible = true;
    repl->current->input_text = "";
    repl->current->input_text_len = 0;

    repl->current->layer_cake = ik_layer_cake_create(repl, (size_t)rows);
    repl->current->scrollback_layer = ik_scrollback_layer_create(repl, "scrollback", repl->current->scrollback);
    repl->current->spinner_layer = ik_spinner_layer_create(repl, "spinner", &repl->current->spinner_state);
    repl->current->separator_layer = ik_separator_layer_create(repl, "separator", &repl->current->separator_visible);
    repl->current->input_layer = ik_input_layer_create(repl, "input", &repl->current->input_buffer_visible,
                                                       &repl->current->input_text, &repl->current->input_text_len);
    repl->lower_separator_layer = ik_separator_layer_create(repl, "lower_separator",
                                                            &repl->lower_separator_visible);

    res = ik_layer_cake_add_layer(repl->current->layer_cake, repl->current->scrollback_layer);
    ck_assert(is_ok(&res));
    res = ik_layer_cake_add_layer(repl->current->layer_cake, repl->current->spinner_layer);
    ck_assert(is_ok(&res));
    res = ik_layer_cake_add_layer(repl->current->layer_cake, repl->current->separator_layer);
    ck_assert(is_ok(&res));
    res = ik_layer_cake_add_layer(repl->current->layer_cake, repl->current->input_layer);
    ck_assert(is_ok(&res));
    res = ik_layer_cake_add_layer(repl->current->layer_cake, repl->lower_separator_layer);
    ck_assert(is_ok(&res));
}

// Test fixture structure
typedef struct {
    void *ctx;
    ik_repl_ctx_t *repl;
    ik_agent_ctx_t *agent;
    ik_term_ctx_t *term;
} test_fixture_t;

// Helper to create test fixture with configurable input text and scrollback lines
static test_fixture_t *create_test_fixture(const char *input_text, int32_t scrollback_lines)
{
    void *ctx = talloc_new(NULL);
    test_fixture_t *fixture = talloc_zero(ctx, test_fixture_t);
    fixture->ctx = ctx;

    // Terminal: 20 rows x 80 cols
    ik_term_ctx_t *term = talloc_zero(ctx, ik_term_ctx_t);
    term->screen_rows = 20;
    term->screen_cols = 80;
    term->tty_fd = 1;
    fixture->term = term;

    res_t res;

    // Create input buffer with specified text
    ik_input_buffer_t *input_buf = ik_input_buffer_create(ctx);
    for (size_t i = 0; i < strlen(input_text); i++) {
        res = ik_input_buffer_insert_codepoint(input_buf, (uint32_t)input_text[i]);
        ck_assert(is_ok(&res));
    }
    ik_input_buffer_ensure_layout(input_buf, 80);

    // Create scrollback with specified number of lines
    ik_scrollback_t *scrollback = ik_scrollback_create(ctx, 80);
    for (int32_t i = 0; i < scrollback_lines; i++) {
        char buf[64];
        snprintf(buf, sizeof(buf), "line %" PRId32, i);
        res = ik_scrollback_append_line(scrollback, buf, strlen(buf));
        ck_assert(is_ok(&res));
    }
    ik_scrollback_ensure_layout(scrollback, 80);

    // Create render context
    ik_render_ctx_t *render = NULL;
    res = ik_render_create(ctx, term->screen_rows, term->screen_cols, term->tty_fd, &render);
    ck_assert(is_ok(&res));

    // Create REPL context with layers
    ik_repl_ctx_t *repl = talloc_zero(ctx, ik_repl_ctx_t);
    repl->current = talloc_zero(repl, ik_agent_ctx_t);
    ik_shared_ctx_t *shared = talloc_zero(repl, ik_shared_ctx_t);
    repl->shared = shared;
    shared->term = term;
    shared->render = render;
    fixture->repl = repl;

    // Create agent context
    ik_agent_ctx_t *agent = NULL;
    res = ik_test_create_agent(ctx, &agent);
    ck_assert(is_ok(&res));
    repl->current = agent;
    fixture->agent = agent;

    // Override agent's input buffer and scrollback with test fixtures
    talloc_free(agent->input_buffer);
    agent->input_buffer = input_buf;
    talloc_free(agent->scrollback);
    agent->scrollback = scrollback;
    agent->viewport_offset = 0;

    init_layer_cake(repl, term->screen_rows);

    return fixture;
}

// Helper to check if position at buffer[i] is a cursor position escape
static bool is_cursor_escape(const char *buffer, size_t len, size_t i)
{
    if (i + 5 >= len) return false;
    if (buffer[i] != '\x1b' || buffer[i + 1] != '[') return false;

    size_t j = i + 2;
    while (j < len && buffer[j] >= '0' && buffer[j] <= '9') j++;
    if (j >= len || buffer[j] != ';') return false;

    size_t k = j + 1;
    while (k < len && buffer[k] >= '0' && buffer[k] <= '9') k++;
    return (k < len && buffer[k] == 'H');
}

/**
 * Helper to extract cursor position from ANSI escape sequence
 * Looks for \x1b[<row>;<col>H pattern in the output buffer
 */
static bool extract_cursor_position(const char *buffer, size_t len, int32_t *row_out, int32_t *col_out)
{
    // Find the LAST cursor position escape sequence
    const char *last_pos = NULL;
    for (size_t i = 0; i < len - 2; i++) {
        if (is_cursor_escape(buffer, len, i)) {
            last_pos = buffer + i;
        }
    }

    if (last_pos == NULL) return false;

    // Parse the position: \x1b[<row>;<col>H
    const char *p = last_pos + 2;  // Skip \x1b[
    int32_t row = 0, col = 0;

    while (*p >= '0' && *p <= '9') {
        row = row * 10 + (*p - '0');
        p++;
    }

    p++;  // Skip ';'

    while (*p >= '0' && *p <= '9') {
        col = col * 10 + (*p - '0');
        p++;
    }

    *row_out = row;
    *col_out = col;
    return true;
}

/**
 * Test: Cursor position when viewport has exactly one blank line at bottom
 *
 * This is the core bug scenario:
 * - Terminal height = 20 lines
 * - Fill scrollback to leave exactly 1 blank line at bottom
 * - Type "/clear" in input buffer
 * - Cursor should be on input line (after "r"), not on separator line
 */
START_TEST(test_cursor_position_with_one_blank_line) {
    // Create test fixture: 16 scrollback lines + separator + input + lower_sep = 19 rows (1 blank)
    test_fixture_t *fixture = create_test_fixture("/clear", 16);
    res_t res;

    // Reset mock state
    mock_write_calls = 0;
    mock_write_buffer_len = 0;
    mock_write_should_fail = false;

    // Render the frame
    res = ik_repl_render_frame(fixture->repl);
    ck_assert(is_ok(&res));
    ck_assert_int_gt(mock_write_calls, 0);

    // Extract cursor position
    int32_t cursor_row = 0, cursor_col = 0;
    bool found = extract_cursor_position(mock_write_buffer, mock_write_buffer_len,
                                         &cursor_row, &cursor_col);
    ck_assert_msg(found, "Could not find cursor position in rendered output");

    // Cursor should be on input line (row 18, 1-indexed), not separator (17) or lower sep (19)
    ck_assert_int_ne(cursor_row, 17);
    ck_assert_int_ne(cursor_row, 19);
    ck_assert_int_eq(cursor_row, 18);  // Input line
    ck_assert_int_eq(cursor_col, 7);   // After "/clear"

    talloc_free(fixture->ctx);
}
END_TEST
/**
 * Test: Cursor position when viewport is full (no blank lines)
 *
 * Verify cursor is still correct when viewport is completely full.
 */
START_TEST(test_cursor_position_viewport_full) {
    test_fixture_t *fixture = create_test_fixture("test", 100);
    res_t res;

    // Reset mock and render
    mock_write_calls = 0;
    mock_write_buffer_len = 0;
    res = ik_repl_render_frame(fixture->repl);
    ck_assert(is_ok(&res));

    // Extract cursor position
    int32_t cursor_row = 0, cursor_col = 0;
    bool found = extract_cursor_position(mock_write_buffer, mock_write_buffer_len,
                                         &cursor_row, &cursor_col);
    ck_assert(found);

    // Document: 100 scrollback + separator + input + lower sep = 103 rows
    // Terminal shows last 20 rows; cursor should be on input line (row 19), not lower sep (20)
    ck_assert_int_ne(cursor_row, 20);  // Not on lower separator
    ck_assert_int_eq(cursor_row, 19);  // On input line
    ck_assert_int_eq(cursor_col, 5);   // After "test"

    talloc_free(fixture->ctx);
}

END_TEST
/**
 * Test: Cursor position when viewport is half full
 */
START_TEST(test_cursor_position_viewport_half_full) {
    test_fixture_t *fixture = create_test_fixture("hi", 5);
    res_t res;

    // Reset mock and render
    mock_write_calls = 0;
    mock_write_buffer_len = 0;
    res = ik_repl_render_frame(fixture->repl);
    ck_assert(is_ok(&res));

    // Extract cursor position
    int32_t cursor_row = 0, cursor_col = 0;
    bool found = extract_cursor_position(mock_write_buffer, mock_write_buffer_len,
                                         &cursor_row, &cursor_col);
    ck_assert(found);

    // Document: 5 scrollback + separator + input + lower sep = 8 rows (fits in terminal)
    // Cursor should be on input line (row 7, 1-indexed)
    ck_assert_int_eq(cursor_row, 7);  // Input line
    ck_assert_int_eq(cursor_col, 3);  // After "hi"

    talloc_free(fixture->ctx);
}

END_TEST

/* Create test suite */
static Suite *cursor_position_suite(void)
{
    Suite *s = suite_create("REPL Cursor Position - Basic");

    TCase *tc_core = tcase_create("Core");
    tcase_set_timeout(tc_core, IK_TEST_TIMEOUT);
    tcase_set_timeout(tc_core, IK_TEST_TIMEOUT);
    tcase_set_timeout(tc_core, IK_TEST_TIMEOUT);
    tcase_set_timeout(tc_core, IK_TEST_TIMEOUT);
    tcase_set_timeout(tc_core, IK_TEST_TIMEOUT);
    tcase_add_test(tc_core, test_cursor_position_with_one_blank_line);
    tcase_add_test(tc_core, test_cursor_position_viewport_full);
    tcase_add_test(tc_core, test_cursor_position_viewport_half_full);
    suite_add_tcase(s, tc_core);

    return s;
}

int main(void)
{
    Suite *s = cursor_position_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_VERBOSE);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    ik_test_reset_terminal();

    return (number_failed == 0) ? 0 : 1;
}
