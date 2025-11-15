#include <check.h>
#include <fcntl.h>
#include <inttypes.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <talloc.h>
#include <termios.h>
#include <unistd.h>
#include "../../src/repl.h"
#include "../test_utils.h"

// Mock terminal file descriptor
static int mock_tty_fd = 100;

// Mock control flags
static int mock_open_fail = 0;
static int mock_tcgetattr_fail = 0;
static int mock_tcsetattr_fail = 0;
static int mock_tcflush_fail = 0;
static int mock_write_fail = 0;
static int mock_ioctl_fail = 0;

// Mock function prototypes
int ik_open_wrapper(const char *pathname, int flags);
int ik_tcgetattr_wrapper(int fd, struct termios *termios_p);
int ik_tcsetattr_wrapper(int fd, int optional_actions, const struct termios *termios_p);
int ik_tcflush_wrapper(int fd, int queue_selector);
ssize_t ik_write_wrapper(int fd, const void *buf, size_t count);
int ik_ioctl_wrapper(int fd, unsigned long request, void *argp);
int ik_close_wrapper(int fd);

// Mock functions for terminal operations
int ik_open_wrapper(const char *pathname, int flags)
{
    (void)pathname;
    (void)flags;
    if (mock_open_fail) {
        return -1;
    }
    return mock_tty_fd;
}

int ik_tcgetattr_wrapper(int fd, struct termios *termios_p)
{
    (void)fd;
    if (mock_tcgetattr_fail) {
        return -1;
    }
    // Initialize with some default values
    termios_p->c_iflag = ICRNL | IXON;
    termios_p->c_oflag = OPOST;
    termios_p->c_cflag = CS8;
    termios_p->c_lflag = ECHO | ICANON | IEXTEN | ISIG;
    termios_p->c_cc[VMIN] = 0;
    termios_p->c_cc[VTIME] = 0;
    return 0;
}

int ik_tcsetattr_wrapper(int fd, int optional_actions, const struct termios *termios_p)
{
    (void)fd;
    (void)optional_actions;
    (void)termios_p;
    if (mock_tcsetattr_fail) {
        return -1;
    }
    return 0;
}

int ik_tcflush_wrapper(int fd, int queue_selector)
{
    (void)fd;
    (void)queue_selector;
    if (mock_tcflush_fail) {
        return -1;
    }
    return 0;
}

ssize_t ik_write_wrapper(int fd, const void *buf, size_t count)
{
    (void)fd;
    (void)buf;
    if (mock_write_fail) {
        return -1;
    }
    return (ssize_t)count;
}

int ik_ioctl_wrapper(int fd, unsigned long request, void *argp)
{
    (void)fd;
    (void)request;
    if (mock_ioctl_fail) {
        return -1;
    }
    struct winsize *ws = (struct winsize *)argp;
    ws->ws_row = 24;
    ws->ws_col = 80;
    return 0;
}

int ik_close_wrapper(int fd)
{
    (void)fd;
    return 0;
}

// Helper to reset mocks
static void reset_mocks(void)
{
    mock_open_fail = 0;
    mock_tcgetattr_fail = 0;
    mock_tcsetattr_fail = 0;
    mock_tcflush_fail = 0;
    mock_write_fail = 0;
    mock_ioctl_fail = 0;
}

// Test: REPL initialization creates all components
START_TEST(test_repl_init) {
    reset_mocks();
    void *ctx = talloc_new(NULL);
    ik_repl_ctx_t *repl = NULL;

    // Initialize REPL
    res_t result = ik_repl_init(ctx, &repl);

    // Verify successful initialization
    ck_assert(is_ok(&result));
    ck_assert_ptr_nonnull(repl);
    ck_assert_ptr_nonnull(repl->term);
    ck_assert_ptr_nonnull(repl->render);
    ck_assert_ptr_nonnull(repl->input_buffer);
    ck_assert_ptr_nonnull(repl->input_parser);
    ck_assert(!repl->quit);

    // Cleanup
    ik_repl_cleanup(repl);
    talloc_free(ctx);
}
END_TEST
// Test: REPL initialization with NULL parent
START_TEST(test_repl_init_null_parent)
{
    ik_repl_ctx_t *repl = NULL;
    (void)ik_repl_init(NULL, &repl);
}

END_TEST
// Test: REPL initialization with NULL out pointer
START_TEST(test_repl_init_null_out)
{
    void *ctx = talloc_new(NULL);
    (void)ik_repl_init(ctx, NULL);
    talloc_free(ctx);
}

END_TEST
// Test: ik_repl_cleanup with NULL
START_TEST(test_repl_cleanup_null)
{
    // Should not crash
    ik_repl_cleanup(NULL);
}

END_TEST
// Test: ik_repl_cleanup with NULL term field
START_TEST(test_repl_cleanup_null_term)
{
    void *ctx = talloc_new(NULL);

    // Create a REPL context with NULL term field
    // This simulates a partially initialized REPL (e.g., if term init failed
    // but we still need to cleanup the repl structure)
    ik_repl_ctx_t *repl = talloc_zero(ctx, ik_repl_ctx_t);
    ck_assert_ptr_nonnull(repl);

    // Explicitly ensure term is NULL (talloc_zero does this, but being explicit)
    repl->term = NULL;

    // Should not crash - cleanup should handle NULL term gracefully
    ik_repl_cleanup(repl);

    talloc_free(ctx);
}

END_TEST
// Test: ik_repl_run
START_TEST(test_repl_run)
{
    reset_mocks();
    void *ctx = talloc_new(NULL);
    ik_repl_ctx_t *repl = NULL;

    res_t result = ik_repl_init(ctx, &repl);
    ck_assert(is_ok(&result));

    // Run should return OK (even though it's not implemented yet)
    res_t run_result = ik_repl_run(repl);
    ck_assert(is_ok(&run_result));

    ik_repl_cleanup(repl);
    talloc_free(ctx);
}

END_TEST

static Suite *repl_suite(void)
{
    Suite *s = suite_create("REPL");

    TCase *tc_core = tcase_create("Core");
    tcase_set_timeout(tc_core, 30);
    tcase_add_test(tc_core, test_repl_init);
    tcase_add_test(tc_core, test_repl_cleanup_null);
    tcase_add_test(tc_core, test_repl_cleanup_null_term);
    tcase_add_test(tc_core, test_repl_run);
    suite_add_tcase(s, tc_core);

    TCase *tc_assertions = tcase_create("Assertions");
    tcase_set_timeout(tc_assertions, 30); // Longer timeout for valgrind
    tcase_add_test_raise_signal(tc_assertions, test_repl_init_null_parent, SIGABRT);
    tcase_add_test_raise_signal(tc_assertions, test_repl_init_null_out, SIGABRT);
    suite_add_tcase(s, tc_assertions);

    return s;
}

int32_t main(void)
{
    Suite *s = repl_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    int32_t number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
