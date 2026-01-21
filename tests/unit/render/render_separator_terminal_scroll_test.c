/**
 * @file render_separator_terminal_scroll_test.c
 * @brief Test that separator doesn't cause terminal scrolling when it's the last line
 *
 * Bug #9: When separator is the last visible line and we add \r\n after it,
 * the terminal scrolls up by one line, causing the separator to disappear.
 */

#include <check.h>
#include <talloc.h>
#include <string.h>
#include <unistd.h>
#include <inttypes.h>
#include "../../../src/render.h"
#include "../../../src/scrollback.h"
#include "../../test_utils_helper.h"

/**
 * Test: Separator as last line should NOT have trailing \r\n
 *
 * Scenario:
 *   - Terminal: 10 rows
 *   - Scrollback fills rows 0-8 (9 physical rows)
 *   - Separator on row 9 (last visible row)
 *   - Input buffer off-screen (render_input_buffer = false)
 *
 * Bug: Adding \r\n after separator causes terminal to scroll
 * Fix: Don't add \r\n after separator when render_input_buffer is false
 */
START_TEST(test_separator_no_trailing_newline_when_last_line) {
    void *ctx = talloc_new(NULL);

    // Create render context: 10 rows x 80 cols
    ik_render_ctx_t *render = NULL;
    res_t res = ik_render_create(ctx, 10, 80, 1, &render);
    ck_assert(is_ok(&res));

    // Create scrollback with 9 lines (fills rows 0-8)
    ik_scrollback_t *scrollback = ik_scrollback_create(ctx, 80);
    for (int32_t i = 0; i < 9; i++) {
        char buf[32];
        snprintf(buf, sizeof(buf), "line%" PRId32, i);
        res = ik_scrollback_append_line(scrollback, buf, strlen(buf));
        ck_assert(is_ok(&res));
    }

    // Ensure layout
    ik_scrollback_ensure_layout(scrollback, 80);

    // Render: separator visible, input buffer off-screen
    int pipefd[2];
    ck_assert_int_eq(pipe(pipefd), 0);
    int saved_stdout = dup(1);
    dup2(pipefd[1], 1);

    res = ik_render_combined(render,
                             scrollback,
                             0,                 // scrollback_start_line
                             9,                 // scrollback_line_count (all 9 lines)
                             "",                // input_text (empty)
                             0,                 // input_text_len
                             0,                 // input_cursor_offset
                             true,              // render_separator (visible)
                             false);            // render_input_buffer (off-screen)
    ck_assert(is_ok(&res));

    fflush(stdout);
    dup2(saved_stdout, 1);
    close(pipefd[1]);

    char output[8192] = {0};
    ssize_t bytes_read = read(pipefd[0], output, sizeof(output) - 1);
    ck_assert(bytes_read > 0);
    close(pipefd[0]);
    close(saved_stdout);

    // Find the separator (80 consecutive dashes)
    const char *separator_start = NULL;
    for (ssize_t i = 0; i < bytes_read - 80; i++) {
        if (output[i] == '-') {
            bool found = true;
            for (int32_t j = 1; j < 80; j++) {
                if (output[i + j] != '-') {
                    found = false;
                    break;
                }
            }
            if (found) {
                separator_start = &output[i];
                break;
            }
        }
    }

    ck_assert_msg(separator_start != NULL, "Separator not found in output");

    // Check what comes after the separator
    const char *after_separator = separator_start + 80;

    // BUG: If there's \r\n after separator, terminal scrolls and separator disappears
    // FIX: When render_input_buffer is false, separator should be the LAST thing written
    //      (except for cursor visibility escape)
    //      So after the 80 dashes, we should see cursor visibility escape \x1b[?25l
    //      NOT \r\n

    // Expected: cursor visibility escape starts immediately after separator
    ck_assert_msg(after_separator[0] == '\x1b',
                  "Expected cursor visibility escape after separator, got '%c' (0x%02x)",
                  after_separator[0], (unsigned char)after_separator[0]);

    // Should be \x1b[?25l (hide cursor)
    ck_assert_msg(after_separator[1] == '[', "Expected '[' in escape sequence");
    ck_assert_msg(after_separator[2] == '?', "Expected '?' in escape sequence");
    ck_assert_msg(after_separator[3] == '2', "Expected '2' in escape sequence");
    ck_assert_msg(after_separator[4] == '5', "Expected '5' in escape sequence");
    ck_assert_msg(after_separator[5] == 'l', "Expected 'l' (hide cursor)");

    talloc_free(ctx);
}
END_TEST
/**
 * Test: Separator with input buffer visible SHOULD have trailing \r\n
 *
 * When input buffer is visible after separator, we need \r\n to advance to next line
 */
START_TEST(test_separator_has_trailing_newline_when_input_buffer_visible) {
    void *ctx = talloc_new(NULL);

    // Create render context
    ik_render_ctx_t *render = NULL;
    res_t res = ik_render_create(ctx, 10, 80, 1, &render);
    ck_assert(is_ok(&res));

    // Create scrollback with 5 lines
    ik_scrollback_t *scrollback = ik_scrollback_create(ctx, 80);
    for (int32_t i = 0; i < 5; i++) {
        char buf[32];
        snprintf(buf, sizeof(buf), "line%" PRId32, i);
        res = ik_scrollback_append_line(scrollback, buf, strlen(buf));
        ck_assert(is_ok(&res));
    }

    ik_scrollback_ensure_layout(scrollback, 80);

    // Render: separator and input buffer both visible
    int pipefd[2];
    ck_assert_int_eq(pipe(pipefd), 0);
    int saved_stdout = dup(1);
    dup2(pipefd[1], 1);

    res = ik_render_combined(render,
                             scrollback,
                             0,                 // scrollback_start_line
                             5,                 // scrollback_line_count
                             "input_buf",       // input_text
                             9,                 // input_text_len
                             0,                 // input_cursor_offset
                             true,              // render_separator
                             true);             // render_input_buffer
    ck_assert(is_ok(&res));

    fflush(stdout);
    dup2(saved_stdout, 1);
    close(pipefd[1]);

    char output[8192] = {0};
    ssize_t bytes_read = read(pipefd[0], output, sizeof(output) - 1);
    ck_assert(bytes_read > 0);
    close(pipefd[0]);
    close(saved_stdout);

    // Find the separator
    const char *separator_start = NULL;
    for (ssize_t i = 0; i < bytes_read - 80; i++) {
        if (output[i] == '-') {
            bool found = true;
            for (int32_t j = 1; j < 80; j++) {
                if (output[i + j] != '-') {
                    found = false;
                    break;
                }
            }
            if (found) {
                separator_start = &output[i];
                break;
            }
        }
    }

    ck_assert_msg(separator_start != NULL, "Separator not found");

    // When input buffer is visible, separator SHOULD have \r\n after it
    const char *after_separator = separator_start + 80;
    ck_assert_msg(after_separator[0] == '\r', "Expected \\r after separator");
    ck_assert_msg(after_separator[1] == '\n', "Expected \\n after separator");

    // Then input buffer text should follow
    ck_assert_msg(after_separator[2] == 'i', "Expected input buffer text after separator");

    talloc_free(ctx);
}

END_TEST
/**
 * Test: Input buffer visible without separator
 *
 * When input buffer is visible but separator is not rendered,
 * scrollback should have \r\n after last line to advance to input buffer
 */
START_TEST(test_input_buffer_without_separator) {
    void *ctx = talloc_new(NULL);

    // Create render context
    ik_render_ctx_t *render = NULL;
    res_t res = ik_render_create(ctx, 10, 80, 1, &render);
    ck_assert(is_ok(&res));

    // Create scrollback with 3 lines
    ik_scrollback_t *scrollback = ik_scrollback_create(ctx, 80);
    for (int32_t i = 0; i < 3; i++) {
        char buf[32];
        snprintf(buf, sizeof(buf), "line%" PRId32, i);
        res = ik_scrollback_append_line(scrollback, buf, strlen(buf));
        ck_assert(is_ok(&res));
    }

    ik_scrollback_ensure_layout(scrollback, 80);

    // Render: no separator, but input buffer visible
    int pipefd[2];
    ck_assert_int_eq(pipe(pipefd), 0);
    int saved_stdout = dup(1);
    dup2(pipefd[1], 1);

    res = ik_render_combined(render,
                             scrollback,
                             0,                 // scrollback_start_line
                             3,                 // scrollback_line_count
                             "input",           // input_text
                             5,                 // input_text_len
                             0,                 // input_cursor_offset
                             false,             // render_separator (NOT visible)
                             true);             // render_input_buffer (visible)
    ck_assert(is_ok(&res));

    fflush(stdout);
    dup2(saved_stdout, 1);
    close(pipefd[1]);

    char output[8192] = {0};
    ssize_t bytes_read = read(pipefd[0], output, sizeof(output) - 1);
    ck_assert(bytes_read > 0);
    close(pipefd[0]);
    close(saved_stdout);

    // Should contain both scrollback and input buffer
    ck_assert_msg(strstr(output, "line2") != NULL, "Expected scrollback content");
    ck_assert_msg(strstr(output, "input") != NULL, "Expected input buffer content");

    // Should NOT contain separator (80 consecutive dashes)
    bool found_separator = false;
    for (ssize_t i = 0; i < bytes_read - 80; i++) {
        if (output[i] == '-') {
            bool all_dashes = true;
            for (int32_t j = 1; j < 80; j++) {
                if (output[i + j] != '-') {
                    all_dashes = false;
                    break;
                }
            }
            if (all_dashes) {
                found_separator = true;
                break;
            }
        }
    }
    ck_assert_msg(!found_separator, "Should NOT have separator when render_separator=false");

    talloc_free(ctx);
}

END_TEST

/* Create test suite */
static Suite *separator_scroll_suite(void)
{
    Suite *s = suite_create("Render: Separator Terminal Scroll Bug");

    TCase *tc_sep = tcase_create("Separator");
    tcase_set_timeout(tc_sep, IK_TEST_TIMEOUT);
    tcase_add_test(tc_sep, test_separator_no_trailing_newline_when_last_line);
    tcase_add_test(tc_sep, test_separator_has_trailing_newline_when_input_buffer_visible);
    tcase_add_test(tc_sep, test_input_buffer_without_separator);
    suite_add_tcase(s, tc_sep);

    return s;
}

int main(void)
{
    Suite *s = separator_scroll_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/render/render_separator_terminal_scroll_test.xml");

    srunner_run_all(sr, CK_VERBOSE);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    ik_test_reset_terminal();

    return (number_failed == 0) ? 0 : 1;
}
