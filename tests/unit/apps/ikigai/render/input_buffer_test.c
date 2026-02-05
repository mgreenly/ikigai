// input_buffer rendering unit tests
#include <check.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <sys/types.h>
#include <talloc.h>
#include "apps/ikigai/render.h"
#include "shared/error.h"
#include "tests/helpers/test_utils_helper.h"

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

// Test: render simple text
START_TEST(test_render_input_buffer_simple_text) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_render_ctx_t *render = NULL;

    res_t res = ik_render_create(ctx, 24, 80, 1, &render);
    ck_assert(is_ok(&res));

    mock_write_reset();
    const char *text = "hello";
    res = ik_render_input_buffer(render, text, 5, 5);

    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(mock_write_buffer);

    // Should contain: clear screen, home escape, text, cursor position escape
    ck_assert(strstr(mock_write_buffer, "\x1b[2J") != NULL); // Clear screen
    ck_assert(strstr(mock_write_buffer, "\x1b[H") != NULL);  // Home cursor
    ck_assert(strstr(mock_write_buffer, "hello") != NULL);   // Text
    // Cursor at position (1,6) -> "\x1b[1;6H"
    ck_assert(strstr(mock_write_buffer, "\x1b[1;6H") != NULL);

    mock_write_reset();
    talloc_free(ctx);
}
END_TEST
// Test: render with cursor in middle
START_TEST(test_render_input_buffer_with_cursor) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_render_ctx_t *render = NULL;

    res_t res = ik_render_create(ctx, 24, 80, 1, &render);
    ck_assert(is_ok(&res));

    mock_write_reset();
    const char *text = "hello world";
    res = ik_render_input_buffer(render, text, 11, 5);

    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(mock_write_buffer);

    // Cursor at byte 5 -> screen position (1,6) -> "\x1b[1;6H"
    ck_assert(strstr(mock_write_buffer, "\x1b[1;6H") != NULL);

    mock_write_reset();
    talloc_free(ctx);
}

END_TEST
// Test: render empty text
START_TEST(test_render_input_buffer_empty_text) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_render_ctx_t *render = NULL;

    res_t res = ik_render_create(ctx, 24, 80, 1, &render);
    ck_assert(is_ok(&res));

    mock_write_reset();
    res = ik_render_input_buffer(render, "", 0, 0);

    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(mock_write_buffer);

    // Should contain clear screen, home escape and cursor at (1,1)
    ck_assert(strstr(mock_write_buffer, "\x1b[2J") != NULL);
    ck_assert(strstr(mock_write_buffer, "\x1b[H") != NULL);
    ck_assert(strstr(mock_write_buffer, "\x1b[1;1H") != NULL);

    mock_write_reset();
    talloc_free(ctx);
}

END_TEST
// Test: render NULL text with length 0
START_TEST(test_render_input_buffer_null_text) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_render_ctx_t *render = NULL;

    res_t res = ik_render_create(ctx, 24, 80, 1, &render);
    ck_assert(is_ok(&res));

    mock_write_reset();
    res = ik_render_input_buffer(render, NULL, 0, 0);

    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(mock_write_buffer);

    // Should contain clear screen, home escape and cursor at (1,1)
    ck_assert(strstr(mock_write_buffer, "\x1b[2J") != NULL);
    ck_assert(strstr(mock_write_buffer, "\x1b[H") != NULL);
    ck_assert(strstr(mock_write_buffer, "\x1b[1;1H") != NULL);

    mock_write_reset();
    talloc_free(ctx);
}

END_TEST
// Test: render UTF-8 text with emoji
START_TEST(test_render_input_buffer_utf8_text) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_render_ctx_t *render = NULL;

    res_t res = ik_render_create(ctx, 24, 80, 1, &render);
    ck_assert(is_ok(&res));

    mock_write_reset();
    // "hello 😀" - emoji is 4 bytes, 2 cells width
    const char *text = "hello \xf0\x9f\x98\x80";
    res = ik_render_input_buffer(render, text, 10, 10);

    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(mock_write_buffer);

    // Text should be present
    ck_assert(strstr(mock_write_buffer, "hello") != NULL);

    // Cursor at end: "hello " (6 cells) + emoji (2 cells) = 8 cells -> position (1,9)
    ck_assert(strstr(mock_write_buffer, "\x1b[1;9H") != NULL);

    mock_write_reset();
    talloc_free(ctx);
}

END_TEST
// Test: render wrapping text
START_TEST(test_render_input_buffer_wrapping_text) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_render_ctx_t *render = NULL;

    res_t res = ik_render_create(ctx, 24, 10, 1, &render);
    ck_assert(is_ok(&res));

    mock_write_reset();
    // 15 chars, terminal width 10 -> wraps
    const char *text = "abcdefghijklmno";
    res = ik_render_input_buffer(render, text, 15, 15);

    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(mock_write_buffer);

    // Text should be present
    ck_assert(strstr(mock_write_buffer, "abcdefghijklmno") != NULL);

    // Cursor at byte 15: wraps to row 2, col 6 -> position (2,6)
    ck_assert(strstr(mock_write_buffer, "\x1b[2;6H") != NULL);

    mock_write_reset();
    talloc_free(ctx);
}

END_TEST
// Test: render text with newlines
START_TEST(test_render_input_buffer_with_newlines) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_render_ctx_t *render = NULL;

    res_t res = ik_render_create(ctx, 24, 80, 1, &render);
    ck_assert(is_ok(&res));

    mock_write_reset();
    const char *text = "hello\nworld";
    res = ik_render_input_buffer(render, text, 11, 8);

    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(mock_write_buffer);

    // Text should be present
    ck_assert(strstr(mock_write_buffer, "hello") != NULL);
    ck_assert(strstr(mock_write_buffer, "world") != NULL);

    // Cursor at byte 8: "hello\nwo|rld" -> row 2, col 3 -> position (2,3)
    ck_assert(strstr(mock_write_buffer, "\x1b[2;3H") != NULL);

    mock_write_reset();
    talloc_free(ctx);
}

END_TEST
// Test: render cursor after wrap boundary
START_TEST(test_render_input_buffer_cursor_after_wrap) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_render_ctx_t *render = NULL;

    res_t res = ik_render_create(ctx, 24, 8, 1, &render);
    ck_assert(is_ok(&res));

    mock_write_reset();
    // Exactly 8 chars, cursor at end
    const char *text = "abcdefgh";
    res = ik_render_input_buffer(render, text, 8, 8);

    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(mock_write_buffer);

    // Cursor at wrap boundary -> row 2, col 1 -> position (2,1)
    ck_assert(strstr(mock_write_buffer, "\x1b[2;1H") != NULL);

    mock_write_reset();
    talloc_free(ctx);
}

END_TEST
// Test: write failure
START_TEST(test_render_input_buffer_write_failure) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_render_ctx_t *render = NULL;

    res_t res = ik_render_create(ctx, 24, 80, 1, &render);
    ck_assert(is_ok(&res));

    mock_write_reset();
    mock_write_should_fail = true;

    const char *text = "hello";
    res = ik_render_input_buffer(render, text, 5, 5);

    ck_assert(is_err(&res));
    ck_assert_int_eq(error_code(res.err), ERR_IO);

    mock_write_reset();
    talloc_free(ctx);
}

END_TEST
// Test: invalid UTF-8 handling
START_TEST(test_render_input_buffer_invalid_utf8) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_render_ctx_t *render = NULL;

    res_t res = ik_render_create(ctx, 24, 80, 1, &render);
    ck_assert(is_ok(&res));

    mock_write_reset();
    // Invalid UTF-8 sequence
    const char *text = "hello\xff\xfe";
    res = ik_render_input_buffer(render, text, 7, 7);

    // Should return error
    ck_assert(is_err(&res));
    ck_assert_int_eq(error_code(res.err), ERR_INVALID_ARG);

    mock_write_reset();
    talloc_free(ctx);
}

END_TEST

#if !defined(NDEBUG) && !defined(SKIP_SIGNAL_TESTS)
// Test: NULL ctx asserts
START_TEST(test_render_input_buffer_null_ctx_asserts) {
    const char *text = "hello";
    ik_render_input_buffer(NULL, text, 5, 5);
}

END_TEST
// Test: NULL text with non-zero length asserts
START_TEST(test_render_input_buffer_null_text_asserts) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_render_ctx_t *render = NULL;

    res_t res = ik_render_create(ctx, 24, 80, 1, &render);
    ck_assert(is_ok(&res));

    ik_render_input_buffer(render, NULL, 5, 0);

    talloc_free(ctx);
}

END_TEST
#endif

// Test suite
static Suite *input_buffer_suite(void)
{
    Suite *s = suite_create("RenderDirect_InputBuffer");
    TCase *tc_core = tcase_create("Core");
    tcase_set_timeout(tc_core, IK_TEST_TIMEOUT);

    tcase_add_test(tc_core, test_render_input_buffer_simple_text);
    tcase_add_test(tc_core, test_render_input_buffer_with_cursor);
    tcase_add_test(tc_core, test_render_input_buffer_empty_text);
    tcase_add_test(tc_core, test_render_input_buffer_null_text);
    tcase_add_test(tc_core, test_render_input_buffer_utf8_text);
    tcase_add_test(tc_core, test_render_input_buffer_wrapping_text);
    tcase_add_test(tc_core, test_render_input_buffer_with_newlines);
    tcase_add_test(tc_core, test_render_input_buffer_cursor_after_wrap);
    tcase_add_test(tc_core, test_render_input_buffer_write_failure);
    tcase_add_test(tc_core, test_render_input_buffer_invalid_utf8);

#if !defined(NDEBUG) && !defined(SKIP_SIGNAL_TESTS)
    tcase_add_test_raise_signal(tc_core, test_render_input_buffer_null_ctx_asserts, SIGABRT);
    tcase_add_test_raise_signal(tc_core, test_render_input_buffer_null_text_asserts, SIGABRT);
#endif

    suite_add_tcase(s, tc_core);
    return s;
}

int main(void)
{
    Suite *s = input_buffer_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/apps/ikigai/render/input_buffer_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    ik_test_reset_terminal();

    return (number_failed == 0) ? 0 : 1;
}
