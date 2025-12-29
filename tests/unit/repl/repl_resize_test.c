/**
 * @file repl_resize_test.c
 * @brief Tests for REPL terminal resize handling
 */

#include <check.h>
#include "../../../src/agent.h"
#include <talloc.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <signal.h>
#include <string.h>
#include "../../../src/repl.h"
#include "../../../src/shared.h"
#include "../../../src/terminal.h"
#include "../../../src/wrapper.h"
#include "../../test_utils.h"
#include "../../../src/logger.h"

// Forward declaration for suite function
static Suite *repl_resize_suite(void);

// Suite-level setup: Set log directory
static void suite_setup(void)
{
    ik_test_set_log_dir(__FILE__);
}

// Mock state for terminal operations
static int mock_screen_rows = 24;
static int mock_screen_cols = 80;
static bool mock_ioctl_should_fail = false;

// Forward declarations for wrapper functions
int posix_open_(const char *pathname, int flags);
int posix_close_(int fd);
int posix_tcgetattr_(int fd, struct termios *termios_p);
int posix_tcsetattr_(int fd, int optional_actions, const struct termios *termios_p);
int posix_tcflush_(int fd, int queue_selector);
int posix_ioctl_(int fd, unsigned long request, void *argp);
ssize_t posix_write_(int fd, const void *buf, size_t count);
ssize_t posix_read_(int fd, void *buf, size_t count);

// Mock wrapper functions for terminal operations
int posix_open_(const char *pathname, int flags)
{
    (void)pathname;
    (void)flags;
    return 3;  // Return valid fd
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

int posix_ioctl_(int fd, unsigned long request, void *argp)
{
    (void)fd;
    (void)request;

    if (mock_ioctl_should_fail) {
        return -1;
    }

    struct winsize *ws = (struct winsize *)argp;
    ws->ws_row = (unsigned short)mock_screen_rows;
    ws->ws_col = (unsigned short)mock_screen_cols;
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
    return 0;  // EOF
}

// Test: ik_repl_handle_resize updates terminal dimensions
START_TEST(test_resize_updates_terminal_dimensions) {
    void *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    // Create REPL context
    ik_repl_ctx_t *repl = NULL;
    ik_config_t *cfg = ik_test_create_config(ctx);
    // Create shared context
    ik_shared_ctx_t *shared = NULL;
    // Create logger before calling init
    ik_logger_t *logger = ik_logger_create(ctx, "/tmp");
    res_t result = ik_shared_ctx_init(ctx, cfg, "/tmp", ".ikigai", logger, &shared);
    ck_assert(is_ok(&result));

    // Create REPL context
    result = ik_repl_init(ctx, shared, &repl);
    ck_assert(is_ok(&result));
    ck_assert_ptr_nonnull(repl);

    // Initial size should be 24x80
    ck_assert_int_eq(repl->shared->term->screen_rows, 24);
    ck_assert_int_eq(repl->shared->term->screen_cols, 80);

    // Change mock terminal size
    mock_screen_rows = 40;
    mock_screen_cols = 120;

    // Handle resize
    result = ik_repl_handle_resize(repl);
    ck_assert(is_ok(&result));

    // Terminal dimensions should be updated
    ck_assert_int_eq(repl->shared->term->screen_rows, 40);
    ck_assert_int_eq(repl->shared->term->screen_cols, 120);

    talloc_free(ctx);
}
END_TEST
// Test: ik_repl_handle_resize invalidates scrollback layout cache
START_TEST(test_resize_invalidates_scrollback_layout)
{
    void *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    // Create REPL context
    ik_repl_ctx_t *repl = NULL;
    ik_config_t *cfg = ik_test_create_config(ctx);
    // Create shared context
    ik_shared_ctx_t *shared = NULL;
    // Create logger before calling init
    ik_logger_t *logger = ik_logger_create(ctx, "/tmp");
    res_t result = ik_shared_ctx_init(ctx, cfg, "/tmp", ".ikigai", logger, &shared);
    ck_assert(is_ok(&result));

    // Create REPL context
    result = ik_repl_init(ctx, shared, &repl);
    ck_assert(is_ok(&result));

    // Add a long line that will wrap differently at different widths
    // 200 character line: at 80 cols = 3 lines, at 120 cols = 2 lines
    const char *line1 =
        "This is a very long line that will definitely wrap differently at different terminal widths and needs to be reflowed when the terminal is resized to a different width than what it was originally laid out at";
    result = ik_scrollback_append_line(repl->current->scrollback, line1, strlen(line1));
    ck_assert(is_ok(&result));

    // Ensure layout at 80 cols
    ik_scrollback_ensure_layout(repl->current->scrollback, 80);
    size_t physical_lines_80 = ik_scrollback_get_total_physical_lines(repl->current->scrollback);

    // Change to 120 cols and handle resize
    mock_screen_cols = 120;
    result = ik_repl_handle_resize(repl);
    ck_assert(is_ok(&result));

    // Layout should be recalculated (fewer physical lines at wider width)
    size_t physical_lines_120 = ik_scrollback_get_total_physical_lines(repl->current->scrollback);
    ck_assert_uint_lt(physical_lines_120, physical_lines_80);

    // Verify cached_width was updated
    ck_assert_int_eq(repl->current->scrollback->cached_width, 120);

    talloc_free(ctx);
}

END_TEST
// Test: ik_repl_handle_resize handles ioctl failure gracefully
START_TEST(test_resize_handles_ioctl_failure)
{
    void *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    // Create REPL context
    ik_repl_ctx_t *repl = NULL;
    ik_config_t *cfg = ik_test_create_config(ctx);
    // Create shared context
    ik_shared_ctx_t *shared = NULL;
    // Create logger before calling init
    ik_logger_t *logger = ik_logger_create(ctx, "/tmp");
    res_t result = ik_shared_ctx_init(ctx, cfg, "/tmp", ".ikigai", logger, &shared);
    ck_assert(is_ok(&result));

    // Create REPL context
    result = ik_repl_init(ctx, shared, &repl);
    ck_assert(is_ok(&result));

    // Make ioctl fail
    mock_ioctl_should_fail = true;

    // Handle resize should return error
    result = ik_repl_handle_resize(repl);
    ck_assert(is_err(&result));

    // Reset mock state
    mock_ioctl_should_fail = false;

    talloc_free(ctx);
}

END_TEST
// Test: SIGWINCH signal handler is installed
START_TEST(test_sigwinch_handler_installed)
{
    void *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    // Create REPL context (which installs SIGWINCH handler)
    ik_repl_ctx_t *repl = NULL;
    ik_config_t *cfg = ik_test_create_config(ctx);
    // Create shared context
    ik_shared_ctx_t *shared = NULL;
    // Create logger before calling init
    ik_logger_t *logger = ik_logger_create(ctx, "/tmp");
    res_t result = ik_shared_ctx_init(ctx, cfg, "/tmp", ".ikigai", logger, &shared);
    ck_assert(is_ok(&result));

    // Create REPL context
    result = ik_repl_init(ctx, shared, &repl);
    ck_assert(is_ok(&result));

    // Get current SIGWINCH handler
    struct sigaction sa;
    int ret = sigaction(SIGWINCH, NULL, &sa);
    ck_assert_int_eq(ret, 0);

    // Verify handler is not SIG_DFL or SIG_IGN
    ck_assert(sa.sa_handler != SIG_DFL);
    ck_assert(sa.sa_handler != SIG_IGN);

    talloc_free(ctx);
}

END_TEST

static Suite *repl_resize_suite(void)
{
    Suite *s = suite_create("REPL Resize");

    TCase *tc_core = tcase_create("Core");
    tcase_set_timeout(tc_core, 30);
    tcase_set_timeout(tc_core, 30);
    tcase_set_timeout(tc_core, 30);
    tcase_set_timeout(tc_core, 30);
    tcase_set_timeout(tc_core, 30);
    tcase_add_unchecked_fixture(tc_core, suite_setup, NULL);
    tcase_add_test(tc_core, test_resize_updates_terminal_dimensions);
    tcase_add_test(tc_core, test_resize_invalidates_scrollback_layout);
    tcase_add_test(tc_core, test_resize_handles_ioctl_failure);
    tcase_add_test(tc_core, test_sigwinch_handler_installed);
    suite_add_tcase(s, tc_core);

    return s;
}

int main(void)
{
    Suite *s = repl_resize_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
