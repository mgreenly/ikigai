/**
 * @file repl_init_test.c
 * @brief Unit tests for REPL initialization error handling
 */

#include <check.h>
#include <talloc.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>
#include "../../../src/repl.h"
#include "../../test_utils.h"

// Mock state for controlling ik_open_wrapper failures
static bool mock_open_should_fail = false;

// Mock state for controlling ik_ioctl_wrapper failures
static bool mock_ioctl_should_fail = false;

// Forward declarations for wrapper functions
int ik_open_wrapper(const char *pathname, int flags);
int ik_ioctl_wrapper(int fd, unsigned long request, void *argp);
int ik_close_wrapper(int fd);
int ik_tcgetattr_wrapper(int fd, struct termios *termios_p);
int ik_tcsetattr_wrapper(int fd, int optional_actions, const struct termios *termios_p);
int ik_tcflush_wrapper(int fd, int queue_selector);
ssize_t ik_write_wrapper(int fd, const void *buf, size_t count);
ssize_t ik_read_wrapper(int fd, void *buf, size_t count);

// Forward declaration for suite function
static Suite *repl_init_suite(void);

// Mock ik_open_wrapper to test terminal open failure
int ik_open_wrapper(const char *pathname, int flags)
{
    (void)pathname;
    (void)flags;

    if (mock_open_should_fail) {
        return -1;  // Simulate failure to open /dev/tty
    }

    // Return a dummy fd (not actually used in this test)
    return 99;
}

// Mock ik_ioctl_wrapper to test invalid terminal dimensions
int ik_ioctl_wrapper(int fd, unsigned long request, void *argp)
{
    (void)fd;
    (void)request;

    if (mock_ioctl_should_fail) {
        struct winsize *ws = (struct winsize *)argp;
        ws->ws_row = 0;  // Invalid: zero rows
        ws->ws_col = 0;  // Invalid: zero cols
        return 0;  // ioctl succeeds but returns invalid dimensions
    }

    // Return valid dimensions
    struct winsize *ws = (struct winsize *)argp;
    ws->ws_row = 24;
    ws->ws_col = 80;
    return 0;
}

// Other required wrappers (pass-through to avoid link errors)
int ik_close_wrapper(int fd)
{
    (void)fd;
    return 0;
}

int ik_tcgetattr_wrapper(int fd, struct termios *termios_p)
{
    (void)fd;
    (void)termios_p;
    return 0;
}

int ik_tcsetattr_wrapper(int fd, int optional_actions, const struct termios *termios_p)
{
    (void)fd;
    (void)optional_actions;
    (void)termios_p;
    return 0;
}

int ik_tcflush_wrapper(int fd, int queue_selector)
{
    (void)fd;
    (void)queue_selector;
    return 0;
}

ssize_t ik_write_wrapper(int fd, const void *buf, size_t count)
{
    (void)fd;
    (void)buf;
    return (ssize_t)count;
}

ssize_t ik_read_wrapper(int fd, void *buf, size_t count)
{
    (void)fd;
    (void)buf;
    (void)count;
    return 0;
}

/* Test: Terminal init failure (cannot open /dev/tty) */
START_TEST(test_repl_init_terminal_open_failure) {
    void *ctx = talloc_new(NULL);
    ik_repl_ctx_t *repl = NULL;

    // Enable mock failure
    mock_open_should_fail = true;

    // Attempt to initialize REPL - should fail
    res_t res = ik_repl_init(ctx, &repl);

    // Verify failure
    ck_assert(is_err(&res));
    ck_assert_ptr_null(repl);

    // Cleanup mock state
    mock_open_should_fail = false;

    talloc_free(ctx);
}
END_TEST
/* Test: Render creation failure (invalid terminal dimensions) */
START_TEST(test_repl_init_render_invalid_dimensions)
{
    void *ctx = talloc_new(NULL);
    ik_repl_ctx_t *repl = NULL;

    // Enable mock failure for ioctl
    mock_ioctl_should_fail = true;

    // Attempt to initialize REPL - should fail when creating render
    res_t res = ik_repl_init(ctx, &repl);

    // Verify failure
    ck_assert(is_err(&res));
    ck_assert_ptr_null(repl);

    // Cleanup mock state
    mock_ioctl_should_fail = false;

    talloc_free(ctx);
}

END_TEST

static Suite *repl_init_suite(void)
{
    Suite *s = suite_create("REPL Initialization");

    TCase *tc_term = tcase_create("Terminal Init Failures");
    tcase_set_timeout(tc_term, 30);
    tcase_add_test(tc_term, test_repl_init_terminal_open_failure);
    tcase_add_test(tc_term, test_repl_init_render_invalid_dimensions);
    suite_add_tcase(s, tc_term);

    return s;
}

int main(void)
{
    int number_failed;
    Suite *s = repl_init_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
