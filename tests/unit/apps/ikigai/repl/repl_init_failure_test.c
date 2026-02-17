/**
 * @file repl_init_failure_test.c
 * @brief Unit tests for REPL initialization failure scenarios
 */

#include <check.h>
#include <talloc.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <termios.h>
#include <unistd.h>
#include "shared/logger.h"

#include "apps/ikigai/repl.h"
#include "apps/ikigai/shared.h"
#include "apps/ikigai/paths.h"
#include "shared/credentials.h"
#include "tests/helpers/test_utils_helper.h"
#include "shared/logger.h"

// Mock state for controlling posix_open_ failures
static bool mock_open_should_fail = false;

// Mock state for controlling posix_ioctl_ failures
static bool mock_ioctl_should_fail = false;

// Mock state for controlling posix_sigaction_ failures
static bool mock_sigaction_should_fail = false;

// Mock state for controlling posix_stat_ failures (for history)
static bool mock_stat_should_fail = false;

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
int posix_stat_(const char *pathname, struct stat *statbuf);
int posix_mkdir_(const char *pathname, mode_t mode);

// Forward declaration for suite function
static Suite *repl_init_failure_suite(void);

// Suite-level setup: Set log directory
static void suite_setup(void)
{
    ik_test_set_log_dir(__FILE__);
}

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

int posix_stat_(const char *pathname, struct stat *statbuf)
{
    if (mock_stat_should_fail) {
        errno = EACCES;  // Permission denied
        return -1;
    }

    // For logger directories in /tmp, call real stat
    // The test uses /tmp as the working directory
    if (strncmp(pathname, "/tmp", 4) == 0) {
        return stat(pathname, statbuf);
    }

    // For history file (non-directory), simulate not exists
    errno = ENOENT;  // File doesn't exist (normal case for history file)
    return -1;
}

int posix_mkdir_(const char *pathname, mode_t mode)
{
    if (mock_stat_should_fail) {
        errno = EACCES;  // Permission denied
        return -1;
    }

    // For logger directories in /tmp, call real mkdir
    if (strncmp(pathname, "/tmp", 4) == 0) {
        return mkdir(pathname, mode);
    }

    return 0;  // Success
}

/* Test: Terminal init failure (cannot open /dev/tty) */
START_TEST(test_repl_init_terminal_open_failure) {
    void *ctx = talloc_new(NULL);

    // Enable mock failure
    mock_open_should_fail = true;

    // Attempt to initialize shared context - should fail during terminal init
    ik_config_t *cfg = ik_test_create_config(ctx);
    ik_credentials_t *creds = talloc_zero_(ctx, sizeof(ik_credentials_t));
    if (creds == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
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

    res_t res = ik_shared_ctx_init(ctx, cfg, creds, paths, logger, false, &shared);

    // Verify failure (terminal init failed)
    ck_assert(is_err(&res));
    ck_assert_ptr_null(shared);

    // Cleanup mock state
    mock_open_should_fail = false;

    talloc_free(ctx);
}
END_TEST

/* Test: Render creation failure (invalid terminal dimensions) */
START_TEST(test_repl_init_render_invalid_dimensions) {
    void *ctx = talloc_new(NULL);

    // Enable mock failure for ioctl
    mock_ioctl_should_fail = true;

    // Attempt to initialize shared context - should fail when creating render
    ik_config_t *cfg = ik_test_create_config(ctx);
    ik_credentials_t *creds = talloc_zero_(ctx, sizeof(ik_credentials_t));
    if (creds == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
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

    res_t res = ik_shared_ctx_init(ctx, cfg, creds, paths, logger, false, &shared);

    // Verify failure (render init failed)
    ck_assert(is_err(&res));
    ck_assert_ptr_null(shared);

    // Cleanup mock state
    mock_ioctl_should_fail = false;

    talloc_free(ctx);
}

END_TEST

/* Test: Signal handler setup failure */
START_TEST(test_repl_init_signal_handler_failure) {
    void *ctx = talloc_new(NULL);
    ik_repl_ctx_t *repl = NULL;

    // Enable mock failure for sigaction
    mock_sigaction_should_fail = true;

    // Attempt to initialize REPL - should fail when setting up signal handler
    ik_config_t *cfg = ik_test_create_config(ctx);
    ik_credentials_t *creds = talloc_zero_(ctx, sizeof(ik_credentials_t));
    if (creds == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
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

    res_t res = ik_shared_ctx_init(ctx, cfg, creds, paths, logger, false, &shared);
    ck_assert(is_ok(&res));

    // Create REPL context
    res = ik_repl_init(ctx, shared, &repl);

    // Verify failure
    ck_assert(is_err(&res));
    ck_assert_ptr_null(repl);

    // Cleanup mock state
    mock_sigaction_should_fail = false;

    talloc_free(ctx);
}

END_TEST

/* Test: History load failure (graceful degradation) */
START_TEST(test_repl_init_history_load_failure) {
    void *ctx = talloc_new(NULL);
    ik_repl_ctx_t *repl = NULL;

    // Initialize REPL - should succeed even with history failure
    ik_config_t *cfg = ik_test_create_config(ctx);
    ik_credentials_t *creds = talloc_zero_(ctx, sizeof(ik_credentials_t));
    if (creds == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
    // Create shared context (logger needs to initialize first)
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

    res_t res = ik_shared_ctx_init(ctx, cfg, creds, paths, logger, false, &shared);
    ck_assert(is_ok(&res));

    // Enable mock failure for stat/mkdir (history directory creation)
    // Must be set AFTER shared context init because logger also uses stat
    mock_stat_should_fail = true;

    // Create REPL context
    res = ik_repl_init(ctx, shared, &repl);

    // Verify success (graceful degradation)
    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(repl);

    // History should be created but empty
    ck_assert_ptr_nonnull(repl->shared->history);
    ck_assert_uint_eq(repl->shared->history->count, 0);

    // Cleanup mock state
    mock_stat_should_fail = false;

    ik_repl_cleanup(repl);
    talloc_free(ctx);
}

END_TEST

static Suite *repl_init_failure_suite(void)
{
    Suite *s = suite_create("REPL Init Failures");

    TCase *tc_failures = tcase_create("Initialization Failures");
    tcase_set_timeout(tc_failures, IK_TEST_TIMEOUT);
    tcase_add_unchecked_fixture(tc_failures, suite_setup, NULL);
    tcase_add_test(tc_failures, test_repl_init_terminal_open_failure);
    tcase_add_test(tc_failures, test_repl_init_render_invalid_dimensions);
    tcase_add_test(tc_failures, test_repl_init_signal_handler_failure);
    tcase_add_test(tc_failures, test_repl_init_history_load_failure);
    suite_add_tcase(s, tc_failures);

    return s;
}

int main(void)
{
    int number_failed;
    Suite *s = repl_init_failure_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/apps/ikigai/repl/repl_init_failure_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
