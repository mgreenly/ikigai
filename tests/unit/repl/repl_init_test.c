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
#include <signal.h>
#include "../../../src/repl.h"
#include "../../test_utils.h"

// Mock state for controlling posix_open_ failures
static bool mock_open_should_fail = false;

// Mock state for controlling posix_ioctl_ failures
static bool mock_ioctl_should_fail = false;

// Mock state for controlling posix_sigaction_ failures
static bool mock_sigaction_should_fail = false;

// Forward declarations for wrapper functions
int posix_open_(const char *pathname, int flags);
int posix_ioctl_(int fd, unsigned long request, void *argp);
int posix_close_(int fd);
int posix_tcgetattr_(int fd, struct termios *termios_p);
int posix_tcsetattr_(int fd, int optional_actions, const struct termios *termios_p);
int posix_tcflush_(int fd, int queue_selector);
ssize_t posix_write_(int fd, const void *buf, size_t count);
ssize_t posix_read_(int fd, void *buf, size_t count);
int posix_sigaction_(int signum, const struct sigaction *act, struct sigaction *oldact);

// Forward declaration for suite function
static Suite *repl_init_suite(void);

// Mock posix_open_ to test terminal open failure
int posix_open_(const char *pathname, int flags)
{
    (void)pathname;
    (void)flags;

    if (mock_open_should_fail) {
        return -1;  // Simulate failure to open /dev/tty
    }

    // Return a dummy fd (not actually used in this test)
    return 99;
}

// Mock posix_ioctl_ to test invalid terminal dimensions
int posix_ioctl_(int fd, unsigned long request, void *argp)
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

int posix_sigaction_(int signum, const struct sigaction *act, struct sigaction *oldact)
{
    (void)signum;
    (void)act;
    (void)oldact;

    if (mock_sigaction_should_fail) {
        return -1;  // Simulate sigaction failure
    }

    return 0;  // Success
}

/* Test: Terminal init failure (cannot open /dev/tty) */
START_TEST(test_repl_init_terminal_open_failure) {
    void *ctx = talloc_new(NULL);
    ik_repl_ctx_t *repl = NULL;

    // Enable mock failure
    mock_open_should_fail = true;

    // Attempt to initialize REPL - should fail
    ik_cfg_t *cfg = ik_test_create_config(ctx);
    res_t res = ik_repl_init(ctx, cfg, &repl);

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
    ik_cfg_t *cfg = ik_test_create_config(ctx);
    res_t res = ik_repl_init(ctx, cfg, &repl);

    // Verify failure
    ck_assert(is_err(&res));
    ck_assert_ptr_null(repl);

    // Cleanup mock state
    mock_ioctl_should_fail = false;

    talloc_free(ctx);
}

END_TEST
/* Test: Signal handler setup failure */
START_TEST(test_repl_init_signal_handler_failure)
{
    void *ctx = talloc_new(NULL);
    ik_repl_ctx_t *repl = NULL;

    // Enable mock failure for sigaction
    mock_sigaction_should_fail = true;

    // Attempt to initialize REPL - should fail when setting up signal handler
    ik_cfg_t *cfg = ik_test_create_config(ctx);
    res_t res = ik_repl_init(ctx, cfg, &repl);

    // Verify failure
    ck_assert(is_err(&res));
    ck_assert_ptr_null(repl);

    // Cleanup mock state
    mock_sigaction_should_fail = false;

    talloc_free(ctx);
}

END_TEST
/* Test: Successful initialization verifies debug manager creation */
START_TEST(test_repl_init_success_debug_manager)
{
    void *ctx = talloc_new(NULL);
    ik_repl_ctx_t *repl = NULL;

    // Initialize REPL - should succeed
    ik_cfg_t *cfg = ik_test_create_config(ctx);
    res_t res = ik_repl_init(ctx, cfg, &repl);

    // Verify success
    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(repl);

    // Verify debug manager is created
    ck_assert_ptr_nonnull(repl->debug_mgr);

    // Verify debug is disabled by default
    ck_assert(!repl->debug_enabled);

    ik_repl_cleanup(repl);
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
    tcase_add_test(tc_term, test_repl_init_signal_handler_failure);
    suite_add_tcase(s, tc_term);

    TCase *tc_success = tcase_create("Successful Init");
    tcase_set_timeout(tc_success, 30);
    tcase_add_test(tc_success, test_repl_init_success_debug_manager);
    suite_add_tcase(s, tc_success);

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
