/**
 * @file repl_submit_line_error_test.c
 * @brief Tests for ik_repl_submit_line error handling
 *
 * Tests the error path when event rendering fails during line submission.
 */

#include "../../../src/agent.h"
#include "../../../src/repl.h"
#include "../../../src/repl_actions.h"
#include "../../../src/scrollback.h"
#include "../../../src/shared.h"
#include "../../../src/paths.h"
#include "../../../src/wrapper.h"
#include "../../test_utils_helper.h"
#include "../../../src/logger.h"

#include <check.h>
#include <fcntl.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <talloc.h>
#include <termios.h>
#include <unistd.h>
#include <errno.h>
#include "../../../src/logger.h"

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
int posix_stat_(const char *pathname, struct stat *statbuf);
int posix_mkdir_(const char *pathname, mode_t mode);
int posix_rename_(const char *oldpath, const char *newpath);
FILE *fopen_(const char *pathname, const char *mode);
int fclose_(FILE *stream);

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

int posix_stat_(const char *pathname, struct stat *statbuf)
{
    // Use real stat
    return stat(pathname, statbuf);
}

int posix_mkdir_(const char *pathname, mode_t mode)
{
    // Use real mkdir
    return mkdir(pathname, mode);
}

int posix_rename_(const char *oldpath, const char *newpath)
{
    // Use real rename
    return rename(oldpath, newpath);
}

FILE *fopen_(const char *pathname, const char *mode)
{
    // Use real fopen
    return fopen(pathname, mode);
}

int fclose_(FILE *stream)
{
    // Use real fclose
    return fclose(stream);
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

// Suite-level setup: Set log directory
static void suite_setup(void)
{
    ik_test_set_log_dir(__FILE__);
}

/* Test: Submit line fails when event render fails */
START_TEST(test_submit_line_event_render_fails) {
    void *ctx = talloc_new(NULL);
    reset_mocks();

    // Setup REPL
    ik_repl_ctx_t *repl = NULL;
    ik_config_t *cfg = ik_test_create_config(ctx);
    // Create shared context
    ik_shared_ctx_t *shared = NULL;
    // Create logger before calling init
    ik_logger_t *logger = ik_logger_create(ctx, "/tmp");
    // Setup test paths
    test_paths_setup_env();
    ik_paths_t *paths = NULL;
    {
        res_t paths_res = ik_paths_init(ctx, &paths);
        ck_assert(is_ok(&paths_res));
    }

    res_t res = ik_shared_ctx_init(ctx, cfg, paths, logger, &shared);
    ck_assert(is_ok(&res));

    // Create REPL context
    res = ik_repl_init(ctx, shared, &repl);
    ck_assert(is_ok(&res));

    // Add some text to input buffer
    const char *test_text = "Hello, world!";
    for (size_t i = 0; test_text[i] != '\0'; i++) {
        ik_input_action_t action = {.type = IK_INPUT_CHAR, .codepoint = (uint32_t)test_text[i]};
        res = ik_repl_process_action(repl, &action);
        ck_assert(is_ok(&res));
    }

    // Verify input buffer has content
    size_t ws_len = ik_byte_array_size(repl->current->input_buffer->text);
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
    tcase_set_timeout(tc_error, IK_TEST_TIMEOUT);
    tcase_set_timeout(tc_error, IK_TEST_TIMEOUT);
    tcase_set_timeout(tc_error, IK_TEST_TIMEOUT);
    tcase_set_timeout(tc_error, IK_TEST_TIMEOUT);
    tcase_add_unchecked_fixture(tc_error, suite_setup, reset_mocks);
    tcase_set_timeout(tc_error, IK_TEST_TIMEOUT);
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
