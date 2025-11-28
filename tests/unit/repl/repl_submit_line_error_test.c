/**
 * @file repl_submit_line_error_test.c
 * @brief Tests for ik_repl_submit_line error handling
 *
 * Tests the error path when event rendering fails during line submission.
 */

#include <check.h>
#include <talloc.h>
#include <string.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>
#include "../../../src/repl.h"
#include "../../../src/repl_actions.h"
#include "../../../src/scrollback.h"
#include "../../../src/wrapper.h"
#include "../../test_utils.h"

// Mock state for ik_scrollback_append_line_
static bool mock_scrollback_append_should_fail = false;
static TALLOC_CTX *mock_err_ctx = NULL;

// Cleanup function registered with atexit to prevent leaks in forked test processes
static void cleanup_mock_err_ctx(void)
{
    if (mock_err_ctx) {
        talloc_free(mock_err_ctx);
        mock_err_ctx = NULL;
    }
}

// Forward declarations for wrapper functions
int posix_open_(const char *pathname, int flags);
int posix_ioctl_(int fd, unsigned long request, void *argp);
int posix_close_(int fd);
int posix_tcgetattr_(int fd, struct termios *termios_p);
int posix_tcsetattr_(int fd, int optional_actions, const struct termios *termios_p);
int posix_tcflush_(int fd, int queue_selector);
ssize_t posix_write_(int fd, const void *buf, size_t count);
ssize_t posix_read_(int fd, void *buf, size_t count);

// Mock wrapper functions for terminal operations (required for ik_repl_init)
int posix_open_(const char *pathname, int flags)
{
    (void)pathname;
    (void)flags;
    return 99;  // Dummy fd
}

int posix_ioctl_(int fd, unsigned long request, void *argp)
{
    (void)fd;
    (void)request;
    struct winsize *ws = (struct winsize *)argp;
    ws->ws_row = 24;  // Standard terminal size
    ws->ws_col = 80;
    return 0;
}

int posix_close_(int fd)
{
    (void)fd;
    return 0;
}

int posix_tcgetattr_(int fd, struct termios *termios_p)
{
    (void)fd;
    (void)termios_p;
    return 0;
}

int posix_tcsetattr_(int fd, int optional_actions, const struct termios *termios_p)
{
    (void)fd;
    (void)optional_actions;
    (void)termios_p;
    return 0;
}

int posix_tcflush_(int fd, int queue_selector)
{
    (void)fd;
    (void)queue_selector;
    return 0;
}

ssize_t posix_write_(int fd, const void *buf, size_t count)
{
    (void)fd;
    (void)buf;
    return (ssize_t)count;
}

ssize_t posix_read_(int fd, void *buf, size_t count)
{
    (void)fd;
    (void)buf;
    (void)count;
    return 0;
}

// Mock ik_scrollback_append_line_ - needs weak attribute for override
res_t ik_scrollback_append_line_(void *scrollback, const char *text, size_t length)
{
    (void)scrollback;
    (void)text;
    (void)length;

    if (mock_scrollback_append_should_fail) {
        if (mock_err_ctx == NULL) {
            mock_err_ctx = talloc_new(NULL);
            atexit(cleanup_mock_err_ctx);
        }
        return ERR(mock_err_ctx, IO, "Mock scrollback append failure");
    }

    return OK(NULL);
}

static void reset_mocks(void)
{
    mock_scrollback_append_should_fail = false;

    // Clean up error context
    if (mock_err_ctx) {
        talloc_free(mock_err_ctx);
        mock_err_ctx = NULL;
    }
}

/* Test: Submit line fails when event render fails */
START_TEST(test_submit_line_event_render_fails) {
    void *ctx = talloc_new(NULL);
    reset_mocks();

    // Setup REPL
    ik_repl_ctx_t *repl = NULL;
    ik_cfg_t *cfg = ik_test_create_config(ctx);
    res_t res = ik_repl_init(ctx, cfg, &repl);
    ck_assert(is_ok(&res));

    // Add some text to input buffer
    const char *test_text = "Hello, world!";
    for (size_t i = 0; test_text[i] != '\0'; i++) {
        ik_input_action_t action = {.type = IK_INPUT_CHAR, .codepoint = (uint32_t)test_text[i]};
        res = ik_repl_process_action(repl, &action);
        ck_assert(is_ok(&res));
    }

    // Verify input buffer has content
    size_t ws_len = ik_byte_array_size(repl->input_buffer->text);
    ck_assert_uint_gt(ws_len, 0);

    // Make scrollback append fail (which is called by event_render)
    mock_scrollback_append_should_fail = true;

    // Submit line - should fail
    res = ik_repl_submit_line(repl);
    ck_assert(is_err(&res));
    ck_assert_int_eq(res.err->code, ERR_IO);

    // Cleanup
    talloc_free(ctx);
}

END_TEST

/* Create test suite */
static Suite *repl_submit_line_error_suite(void)
{
    Suite *s = suite_create("REPL Submit Line Error Handling");

    TCase *tc_error = tcase_create("Error Handling");
    tcase_set_timeout(tc_error, 30);
    tcase_add_unchecked_fixture(tc_error, NULL, reset_mocks);
    tcase_add_test(tc_error, test_submit_line_event_render_fails);
    suite_add_tcase(s, tc_error);

    return s;
}

int main(void)
{
    Suite *s = repl_submit_line_error_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
