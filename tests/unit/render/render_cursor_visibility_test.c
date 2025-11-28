/**
 * @file render_cursor_visibility_test.c
 * @brief Tests for cursor visibility when input buffer is scrolled off-screen (Bug #7)
 */

#include <check.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <sys/types.h>
#include <talloc.h>
#include "../../../src/render.h"
#include "../../../src/scrollback.h"
#include "../../../src/error.h"
#include "../../test_utils.h"

// Mock write() implementation
static char *mock_write_buffer = NULL;
static size_t mock_write_size = 0;
static ssize_t mock_write_return = 0;
static bool mock_write_should_fail = false;

// Mock wrapper declaration
ssize_t posix_write_(int fd, const void *buf, size_t count);

ssize_t posix_write_(int fd, const void *buf, size_t count)
{
    (void)fd; // Unused in mock

    if (mock_write_should_fail) {
        return -1;
    }

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

    return (mock_write_return > 0) ? mock_write_return : (ssize_t)count;
}

static void mock_write_reset(void)
{
    if (mock_write_buffer != NULL) {
        free(mock_write_buffer);
        mock_write_buffer = NULL;
    }
    mock_write_size = 0;
    mock_write_return = 0;
    mock_write_should_fail = false;
}

/**
 * Check if buffer contains ANSI cursor positioning escape sequence.
 *
 * Searches for pattern: \x1b[<row>;<col>H where row and col are digits.
 *
 * @param buffer Buffer to search
 * @param size Size of buffer
 * @return true if cursor positioning escape found, false otherwise
 */
static bool contains_cursor_positioning_escape(const char *buffer, size_t size)
{
    for (size_t i = 0; i < size - 4; i++) {
        if (buffer[i] != '\x1b' || buffer[i + 1] != '[') {
            continue;
        }

        // Found ESC[, check for <digits>;<digits>H pattern
        size_t j = i + 2;
        bool has_digit_before_semicolon = false;
        while (j < size && buffer[j] >= '0' && buffer[j] <= '9') {
            has_digit_before_semicolon = true;
            j++;
        }

        if (!has_digit_before_semicolon || j >= size || buffer[j] != ';') {
            continue;
        }

        j++; // Skip semicolon
        bool has_digit_after_semicolon = false;
        while (j < size && buffer[j] >= '0' && buffer[j] <= '9') {
            has_digit_after_semicolon = true;
            j++;
        }

        if (has_digit_after_semicolon && j < size && buffer[j] == 'H') {
            return true;
        }
    }

    return false;
}

/**
 * Test: Cursor hidden when input buffer scrolled off-screen (Bug #7)
 *
 * When render_input_buffer = false (input buffer is off-screen), no cursor positioning
 * escape sequence should be written to the output.
 */
START_TEST(test_cursor_hidden_when_input_buffer_off_screen) {
    void *ctx = talloc_new(NULL);
    ik_render_ctx_t *render_ctx = NULL;

    // Create render context
    res_t res = ik_render_create(ctx, 24, 80, 1, &render_ctx);
    ck_assert(is_ok(&res));

    // Create scrollback with some lines
    ik_scrollback_t *scrollback = ik_scrollback_create(ctx, 80);
    res = ik_scrollback_append_line(scrollback, "scrollback line 1", 17);
    ck_assert(is_ok(&res));
    res = ik_scrollback_append_line(scrollback, "scrollback line 2", 17);
    ck_assert(is_ok(&res));

    // Create input buffer text
    const char *input_text = "input buffer text";
    size_t input_len = strlen(input_text);

    // Render with input buffer OFF-SCREEN (render_input_buffer = false)
    mock_write_reset();
    res = ik_render_combined(render_ctx, scrollback, 0, 2, input_text, input_len, 0, true, false);
    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(mock_write_buffer);

    // Should contain scrollback content
    ck_assert(strstr(mock_write_buffer, "scrollback line 1") != NULL);
    ck_assert(strstr(mock_write_buffer, "scrollback line 2") != NULL);

    // Should NOT contain input buffer text
    ck_assert(strstr(mock_write_buffer, "input buffer text") == NULL);

    // Should NOT contain cursor positioning escape (critical - Bug #7 fix)
    bool found = contains_cursor_positioning_escape(mock_write_buffer, mock_write_size);
    ck_assert_msg(!found, "Cursor escape found when input buffer is off-screen (Bug #7)");

    mock_write_reset();
    talloc_free(ctx);
}
END_TEST
/**
 * Test: Cursor visible when input buffer is on-screen
 *
 * When render_input_buffer = true, cursor positioning escape SHOULD be present.
 */
START_TEST(test_cursor_visible_when_input_buffer_on_screen)
{
    void *ctx = talloc_new(NULL);
    ik_render_ctx_t *render_ctx = NULL;

    // Create render context
    res_t res = ik_render_create(ctx, 24, 80, 1, &render_ctx);
    ck_assert(is_ok(&res));

    // Create scrollback with some lines
    ik_scrollback_t *scrollback = ik_scrollback_create(ctx, 80);
    res = ik_scrollback_append_line(scrollback, "scrollback line 1", 17);
    ck_assert(is_ok(&res));

    // Create input buffer text
    const char *input_text = "input buffer text";
    size_t input_len = strlen(input_text);

    // Render with input buffer ON-SCREEN (render_input_buffer = true)
    mock_write_reset();
    res = ik_render_combined(render_ctx, scrollback, 0, 1, input_text, input_len, 5, true, true);
    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(mock_write_buffer);

    // Should contain input buffer text
    ck_assert(strstr(mock_write_buffer, "input buffer text") != NULL);

    // SHOULD contain cursor positioning escape
    bool found = contains_cursor_positioning_escape(mock_write_buffer, mock_write_size);
    ck_assert_msg(found, "Cursor escape should be present when input buffer is on-screen");

    mock_write_reset();
    talloc_free(ctx);
}

END_TEST
/**
 * Test: Last scrollback line fully visible when scrolled up
 *
 * When input buffer is off-screen, the last visible scrollback line should not
 * be overwritten by a blank cursor line.
 */
START_TEST(test_last_scrollback_line_visible_when_scrolled_up)
{
    void *ctx = talloc_new(NULL);
    ik_render_ctx_t *render_ctx = NULL;

    // Create render context
    res_t res = ik_render_create(ctx, 24, 80, 1, &render_ctx);
    ck_assert(is_ok(&res));

    // Create scrollback with multiple lines
    ik_scrollback_t *scrollback = ik_scrollback_create(ctx, 80);
    res = ik_scrollback_append_line(scrollback, "line 1", 6);
    ck_assert(is_ok(&res));
    res = ik_scrollback_append_line(scrollback, "line 2", 6);
    ck_assert(is_ok(&res));
    res = ik_scrollback_append_line(scrollback, "THIS IS THE LAST LINE", 21);
    ck_assert(is_ok(&res));

    // Render all scrollback lines with input buffer OFF-SCREEN
    mock_write_reset();
    res = ik_render_combined(render_ctx, scrollback, 0, 3, "input_buffer", 12, 0, true, false);
    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(mock_write_buffer);

    // Should contain the last scrollback line
    ck_assert(strstr(mock_write_buffer, "THIS IS THE LAST LINE") != NULL);

    // Should NOT contain input buffer
    ck_assert(strstr(mock_write_buffer, "input_buffer") == NULL);

    mock_write_reset();
    talloc_free(ctx);
}

END_TEST
/**
 * Test: Cursor visibility escape - hide cursor when input buffer off-screen (Bug #8)
 *
 * When render_input_buffer = false, output should contain \x1b[?25l (hide cursor).
 */
START_TEST(test_cursor_visibility_escape_hide_when_off_screen)
{
    void *ctx = talloc_new(NULL);
    ik_render_ctx_t *render_ctx = NULL;

    // Create render context
    res_t res = ik_render_create(ctx, 24, 80, 1, &render_ctx);
    ck_assert(is_ok(&res));

    // Create scrollback with some lines
    ik_scrollback_t *scrollback = ik_scrollback_create(ctx, 80);
    res = ik_scrollback_append_line(scrollback, "scrollback line 1", 17);
    ck_assert(is_ok(&res));

    // Create input buffer text
    const char *input_text = "input buffer text";
    size_t input_len = strlen(input_text);

    // Render with input buffer OFF-SCREEN (render_input_buffer = false)
    mock_write_reset();
    res = ik_render_combined(render_ctx, scrollback, 0, 1, input_text, input_len, 0, true, false);
    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(mock_write_buffer);

    // Should contain hide cursor escape: \x1b[?25l
    bool found_hide_cursor = false;
    for (size_t i = 0; i < mock_write_size - 5; i++) {
        if (mock_write_buffer[i] == '\x1b' &&
            mock_write_buffer[i + 1] == '[' &&
            mock_write_buffer[i + 2] == '?' &&
            mock_write_buffer[i + 3] == '2' &&
            mock_write_buffer[i + 4] == '5' &&
            mock_write_buffer[i + 5] == 'l') {
            found_hide_cursor = true;
            break;
        }
    }

    ck_assert_msg(found_hide_cursor, "Hide cursor escape (\\x1b[?25l) not found when input buffer is off-screen");

    mock_write_reset();
    talloc_free(ctx);
}

END_TEST
/**
 * Test: Cursor visibility escape - show cursor when input buffer on-screen (Bug #8)
 *
 * When render_input_buffer = true, output should contain \x1b[?25h (show cursor).
 */
START_TEST(test_cursor_visibility_escape_show_when_on_screen)
{
    void *ctx = talloc_new(NULL);
    ik_render_ctx_t *render_ctx = NULL;

    // Create render context
    res_t res = ik_render_create(ctx, 24, 80, 1, &render_ctx);
    ck_assert(is_ok(&res));

    // Create scrollback with some lines
    ik_scrollback_t *scrollback = ik_scrollback_create(ctx, 80);
    res = ik_scrollback_append_line(scrollback, "scrollback line 1", 17);
    ck_assert(is_ok(&res));

    // Create input buffer text
    const char *input_text = "input buffer text";
    size_t input_len = strlen(input_text);

    // Render with input buffer ON-SCREEN (render_input_buffer = true)
    mock_write_reset();
    res = ik_render_combined(render_ctx, scrollback, 0, 1, input_text, input_len, 5, true, true);
    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(mock_write_buffer);

    // Should contain show cursor escape: \x1b[?25h
    bool found_show_cursor = false;
    for (size_t i = 0; i < mock_write_size - 5; i++) {
        if (mock_write_buffer[i] == '\x1b' &&
            mock_write_buffer[i + 1] == '[' &&
            mock_write_buffer[i + 2] == '?' &&
            mock_write_buffer[i + 3] == '2' &&
            mock_write_buffer[i + 4] == '5' &&
            mock_write_buffer[i + 5] == 'h') {
            found_show_cursor = true;
            break;
        }
    }

    ck_assert_msg(found_show_cursor, "Show cursor escape (\\x1b[?25h) not found when input buffer is on-screen");

    mock_write_reset();
    talloc_free(ctx);
}

END_TEST

// Test Suite
static Suite *cursor_visibility_suite(void)
{
    Suite *s = suite_create("Render Cursor Visibility");

    TCase *tc_cursor = tcase_create("Cursor");
    tcase_set_timeout(tc_cursor, 30);
    tcase_add_test(tc_cursor, test_cursor_hidden_when_input_buffer_off_screen);
    tcase_add_test(tc_cursor, test_cursor_visible_when_input_buffer_on_screen);
    tcase_add_test(tc_cursor, test_last_scrollback_line_visible_when_scrolled_up);
    tcase_add_test(tc_cursor, test_cursor_visibility_escape_hide_when_off_screen);
    tcase_add_test(tc_cursor, test_cursor_visibility_escape_show_when_on_screen);
    suite_add_tcase(s, tc_cursor);

    return s;
}

int main(void)
{
    Suite *s = cursor_visibility_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
