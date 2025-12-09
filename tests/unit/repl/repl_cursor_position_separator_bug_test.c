/**
 * @file repl_cursor_position_separator_bug_test.c
 * @brief Test for cursor position bug when viewport has one blank line
 *
 * Bug: When scrollback content leaves exactly one blank line at the bottom
 * of the viewport, the cursor renders on the separator line instead of the
 * input line where the text is being typed.
 */

#include <check.h>
#include "../../../src/shared.h"
#include <talloc.h>
#include <string.h>
#include <stdio.h>
#include <inttypes.h>
#include "../../../src/repl.h"
#include "../../../src/scrollback.h"
#include "../../../src/input_buffer/core.h"
#include "../../test_utils.h"

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
    repl->spinner_state.frame_index = 0;
    repl->spinner_state.visible = false;
    repl->separator_visible = true;
    repl->lower_separator_visible = true;
    repl->input_buffer_visible = true;
    repl->input_text = "";
    repl->input_text_len = 0;

    repl->layer_cake = ik_layer_cake_create(repl, (size_t)rows);
    repl->scrollback_layer = ik_scrollback_layer_create(repl, "scrollback", repl->scrollback);
    repl->spinner_layer = ik_spinner_layer_create(repl, "spinner", &repl->spinner_state);
    repl->separator_layer = ik_separator_layer_create(repl, "separator", &repl->separator_visible);
    repl->input_layer = ik_input_layer_create(repl, "input", &repl->input_buffer_visible,
                                              &repl->input_text, &repl->input_text_len);
    repl->lower_separator_layer = ik_separator_layer_create(repl, "lower_separator",
                                                             &repl->lower_separator_visible);

    res = ik_layer_cake_add_layer(repl->layer_cake, repl->scrollback_layer);
    ck_assert(is_ok(&res));
    res = ik_layer_cake_add_layer(repl->layer_cake, repl->spinner_layer);
    ck_assert(is_ok(&res));
    res = ik_layer_cake_add_layer(repl->layer_cake, repl->separator_layer);
    ck_assert(is_ok(&res));
    res = ik_layer_cake_add_layer(repl->layer_cake, repl->input_layer);
    ck_assert(is_ok(&res));
    res = ik_layer_cake_add_layer(repl->layer_cake, repl->lower_separator_layer);
    ck_assert(is_ok(&res));
}

// Helper to check if position at buffer[i] is a cursor position escape
static bool is_cursor_escape(const char *buffer, size_t len, size_t i)
{
    if (i + 5 >= len) return false;
    if (buffer[i] != '\x1b' || buffer[i+1] != '[') return false;

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
    void *ctx = talloc_new(NULL);

    // Terminal: 20 rows x 80 cols
    ik_term_ctx_t *term = talloc_zero(ctx, ik_term_ctx_t);
    term->screen_rows = 20;
    term->screen_cols = 80;
    term->tty_fd = 1;

    res_t res;

    // Create input buffer with "/clear" text
    ik_input_buffer_t *input_buf = ik_input_buffer_create(ctx);
    const char *input_text = "/clear";
    for (size_t i = 0; i < strlen(input_text); i++) {
        res = ik_input_buffer_insert_codepoint(input_buf, (uint32_t)input_text[i]);
        ck_assert(is_ok(&res));
    }
    ik_input_buffer_ensure_layout(input_buf, 80);

    // Create scrollback
    // Document model: scrollback + separator + input + lower_separator
    // To have exactly 1 blank line: scrollback + blank + separator + input + lower_sep = terminal rows
    // Terminal has 20 rows (0-19)
    // If scrollback has 16 lines:
    //   - Rows 0-15: scrollback (16 lines)
    //   - Row 16: blank line (not rendered, just empty)
    //   - Row 17: separator
    //   - Row 18: input line (cursor should be here)
    //   - Row 19: lower separator
    // Total: 16 + 1 (blank) + 1 (sep) + 1 (input) + 1 (lower sep) = 20 rows

    ik_scrollback_t *scrollback = ik_scrollback_create(ctx, 80);
    for (int32_t i = 0; i < 16; i++) {
        char buf[64];
        snprintf(buf, sizeof(buf), "scrollback line %" PRId32, i);
        res = ik_scrollback_append_line(scrollback, buf, strlen(buf));
        ck_assert(is_ok(&res));
    }
    ik_scrollback_ensure_layout(scrollback, 80);

    // Create render context
    ik_render_ctx_t *render = NULL;
    res = ik_render_create(ctx, term->screen_rows, term->screen_cols, term->tty_fd, &render);
    ck_assert(is_ok(&res));

    // Create REPL context with layer cake (to test layer-based rendering)
    ik_repl_ctx_t *repl = talloc_zero(ctx, ik_repl_ctx_t);
    ik_shared_ctx_t *shared = talloc_zero(repl, ik_shared_ctx_t);
    repl->shared = shared;
    shared->term = term;
    shared->render = render;
    repl->input_buffer = input_buf;
    repl->scrollback = scrollback;
    repl->viewport_offset = 0;

    init_layer_cake(repl, term->screen_rows);

    // Reset mock state
    mock_write_calls = 0;
    mock_write_buffer_len = 0;
    mock_write_should_fail = false;

    // Render the frame
    res = ik_repl_render_frame(repl);
    ck_assert(is_ok(&res));

    // Verify write was called
    ck_assert_int_gt(mock_write_calls, 0);
    ck_assert(mock_write_buffer_len > 0);

    // Extract cursor position from the rendered output
    int32_t cursor_row = 0, cursor_col = 0;
    bool found = extract_cursor_position(mock_write_buffer, mock_write_buffer_len,
                                         &cursor_row, &cursor_col);
    ck_assert_msg(found, "Could not find cursor position in rendered output");

    // Debug output
    printf("\n=== Cursor Position Test ===\n");
    printf("Terminal: %d rows x %d cols\n", term->screen_rows, term->screen_cols);
    printf("Scrollback lines: %zu\n", ik_scrollback_get_line_count(scrollback));
    printf("Input text: \"%s\"\n", input_text);
    printf("Cursor position (1-indexed): row %d, col %d\n", cursor_row, cursor_col);

    // Calculate expected cursor position with layer-based rendering
    // Document model (0-indexed):
    //   - Rows 0-15: scrollback (16 lines)
    //   - Row 16: separator
    //   - Row 17: input
    //   - Row 18: lower separator
    // Total document height: 19 rows
    // Terminal: 20 rows (0-19)
    // Document fits with 1 blank row at top:
    //   - Terminal row 0: blank (empty)
    //   - Terminal rows 1-16: scrollback (16 lines)
    //   - Terminal row 17: separator
    //   - Terminal row 18: input "/clear" with cursor
    //   - Terminal row 19: lower separator
    // Cursor should be at terminal row 18 (0-indexed) = row 19 (1-indexed)

    // The BUG places cursor one row too high - at row 19 instead of input row 18
    // CORRECT behavior: cursor on row 19 (input line in 1-indexed)

    int32_t expected_cursor_row = 19;  // Input line (1-indexed)
    int32_t expected_cursor_col = 7;   // After "/clear" (6 chars + 1 for 1-indexed)

    printf("Expected cursor: row %d, col %d\n", expected_cursor_row, expected_cursor_col);
    printf("\n");

    // Key assertion: cursor should NOT be on lower separator line (row 20)
    ck_assert_int_ne(cursor_row, 20);

    // Cursor should be on input line
    ck_assert_int_eq(cursor_row, expected_cursor_row);
    ck_assert_int_eq(cursor_col, expected_cursor_col);

    talloc_free(ctx);
}
END_TEST

/**
 * Test: Cursor position when viewport is full (no blank lines)
 *
 * Verify cursor is still correct when viewport is completely full.
 */
START_TEST(test_cursor_position_viewport_full) {
    void *ctx = talloc_new(NULL);

    // Terminal: 20 rows x 80 cols
    ik_term_ctx_t *term = talloc_zero(ctx, ik_term_ctx_t);
    term->screen_rows = 20;
    term->screen_cols = 80;
    term->tty_fd = 1;

    res_t res;

    // Create input buffer
    ik_input_buffer_t *input_buf = ik_input_buffer_create(ctx);
    res = ik_input_buffer_insert_codepoint(input_buf, 't');
    ck_assert(is_ok(&res));
    res = ik_input_buffer_insert_codepoint(input_buf, 'e');
    ck_assert(is_ok(&res));
    res = ik_input_buffer_insert_codepoint(input_buf, 's');
    ck_assert(is_ok(&res));
    res = ik_input_buffer_insert_codepoint(input_buf, 't');
    ck_assert(is_ok(&res));
    ik_input_buffer_ensure_layout(input_buf, 80);

    // Create scrollback with many lines (more than screen)
    ik_scrollback_t *scrollback = ik_scrollback_create(ctx, 80);
    for (int32_t i = 0; i < 100; i++) {
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
    ik_shared_ctx_t *shared = talloc_zero(repl, ik_shared_ctx_t);
    repl->shared = shared;
    shared->term = term;
    shared->render = render;
    repl->input_buffer = input_buf;
    repl->scrollback = scrollback;
    repl->viewport_offset = 0;

    init_layer_cake(repl, term->screen_rows);

    // Reset mock and render
    mock_write_calls = 0;
    mock_write_buffer_len = 0;
    res = ik_repl_render_frame(repl);
    ck_assert(is_ok(&res));

    // Extract cursor position
    int32_t cursor_row = 0, cursor_col = 0;
    bool found = extract_cursor_position(mock_write_buffer, mock_write_buffer_len,
                                         &cursor_row, &cursor_col);
    ck_assert(found);

    // Cursor should be visible and on input line (last visible row)
    // Input buffer should be at bottom of viewport
    ck_assert_int_eq(cursor_row, 20);  // Bottom row (1-indexed)
    ck_assert_int_eq(cursor_col, 5);   // After "test" (4 chars + 1)

    talloc_free(ctx);
}
END_TEST

/**
 * Test: Cursor position when viewport is half full
 */
START_TEST(test_cursor_position_viewport_half_full) {
    void *ctx = talloc_new(NULL);

    // Terminal: 20 rows x 80 cols
    ik_term_ctx_t *term = talloc_zero(ctx, ik_term_ctx_t);
    term->screen_rows = 20;
    term->screen_cols = 80;
    term->tty_fd = 1;

    res_t res;

    // Create input buffer
    ik_input_buffer_t *input_buf = ik_input_buffer_create(ctx);
    res = ik_input_buffer_insert_codepoint(input_buf, 'h');
    ck_assert(is_ok(&res));
    res = ik_input_buffer_insert_codepoint(input_buf, 'i');
    ck_assert(is_ok(&res));
    ik_input_buffer_ensure_layout(input_buf, 80);

    // Create small scrollback (only 5 lines)
    ik_scrollback_t *scrollback = ik_scrollback_create(ctx, 80);
    for (int32_t i = 0; i < 5; i++) {
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
    ik_shared_ctx_t *shared = talloc_zero(repl, ik_shared_ctx_t);
    repl->shared = shared;
    shared->term = term;
    shared->render = render;
    repl->input_buffer = input_buf;
    repl->scrollback = scrollback;
    repl->viewport_offset = 0;

    init_layer_cake(repl, term->screen_rows);

    // Reset mock and render
    mock_write_calls = 0;
    mock_write_buffer_len = 0;
    res = ik_repl_render_frame(repl);
    ck_assert(is_ok(&res));

    // Extract cursor position
    int32_t cursor_row = 0, cursor_col = 0;
    bool found = extract_cursor_position(mock_write_buffer, mock_write_buffer_len,
                                         &cursor_row, &cursor_col);
    ck_assert(found);

    // With 5 scrollback lines, separator, input, lower_separator:
    // - Rows 1-5: scrollback
    // - Row 6: separator
    // - Row 7: input "hi" (cursor should be here)
    // - Row 8: lower separator
    // Cursor at row 7, col 3 (after "hi" in 0-indexed becomes row 8 in 1-indexed with +1 fix)
    ck_assert_int_eq(cursor_row, 8);
    ck_assert_int_eq(cursor_col, 3);  // After "hi"

    talloc_free(ctx);
}
END_TEST

/* Create test suite */
static Suite *cursor_position_suite(void)
{
    Suite *s = suite_create("REPL Cursor Position - Separator Bug");

    TCase *tc_core = tcase_create("Core");
    tcase_set_timeout(tc_core, 30);
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
