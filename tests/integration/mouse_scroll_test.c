// Integration test for complete mouse scroll flow

#include <check.h>
#include <fcntl.h>
#include <inttypes.h>
#include <sys/ioctl.h>
#include <talloc.h>
#include <termios.h>
#include <unistd.h>
#include "../../src/repl.h"
#include "../../src/repl_actions.h"
#include "../../src/input.h"
#include "../../src/terminal.h"
#include "../test_utils.h"

// Mock terminal file descriptor
static int32_t mock_tty_fd = 100;

// Track alt screen enter/exit calls
static int32_t alt_screen_enter_count = 0;
static int32_t alt_screen_exit_count = 0;

// Mock control flags
static int32_t mock_open_fail = 0;
static int32_t mock_tcgetattr_fail = 0;
static int32_t mock_tcsetattr_fail = 0;
static int32_t mock_tcflush_fail = 0;
static int32_t mock_write_fail = 0;
static int32_t mock_ioctl_fail = 0;

// Buffer to track write calls
#define MAX_WRITE_CALLS 10
static char write_buffers[MAX_WRITE_CALLS][32];
static size_t write_lengths[MAX_WRITE_CALLS];
static int32_t write_call_count = 0;

// Mock function prototypes
int posix_open_(const char *pathname, int flags);
int posix_tcgetattr_(int fd, struct termios *termios_p);
int posix_tcsetattr_(int fd, int optional_actions, const struct termios *termios_p);
int posix_tcflush_(int fd, int queue_selector);
ssize_t posix_write_(int fd, const void *buf, size_t count);
int posix_ioctl_(int fd, unsigned long request, void *argp);
int posix_close_(int fd);

// Mock functions for terminal operations
int posix_open_(const char *pathname, int flags)
{
    (void)pathname;
    (void)flags;
    if (mock_open_fail) {
        return -1;
    }
    return mock_tty_fd;
}

int posix_tcgetattr_(int fd, struct termios *termios_p)
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

int posix_tcsetattr_(int fd, int optional_actions, const struct termios *termios_p)
{
    (void)fd;
    (void)optional_actions;
    (void)termios_p;
    if (mock_tcsetattr_fail) {
        return -1;
    }
    return 0;
}

int posix_tcflush_(int fd, int queue_selector)
{
    (void)fd;
    (void)queue_selector;
    if (mock_tcflush_fail) {
        return -1;
    }
    return 0;
}

ssize_t posix_write_(int fd, const void *buf, size_t count)
{
    (void)fd;
    if (mock_write_fail) {
        return -1;
    }

    if (write_call_count < MAX_WRITE_CALLS && count < sizeof(write_buffers[0])) {
        memcpy(write_buffers[write_call_count], buf, count);
        write_lengths[write_call_count] = count;

        if (count >= 8) {
            const char alt_screen_enter[] = "\x1b[?1049h";
            if (memcmp(buf, alt_screen_enter, sizeof(alt_screen_enter) - 1) == 0) {
                alt_screen_enter_count++;
            }
            const char alt_screen_exit[] = "\x1b[?1049l";
            if (memcmp(buf, alt_screen_exit, sizeof(alt_screen_exit) - 1) == 0) {
                alt_screen_exit_count++;
            }
        }
        write_call_count++;
    }

    return (ssize_t)count;
}

int posix_ioctl_(int fd, unsigned long request, void *argp)
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

int posix_close_(int fd)
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
    alt_screen_enter_count = 0;
    alt_screen_exit_count = 0;
    write_call_count = 0;
    memset(write_buffers, 0, sizeof(write_buffers));
    memset(write_lengths, 0, sizeof(write_lengths));
}

START_TEST(test_terminal_init_enters_alt_screen) {
    reset_mocks();
    void *ctx = talloc_new(NULL);
    ik_term_ctx_t *term = NULL;
    res_t result = ik_term_init(ctx, NULL, &term);
    ck_assert(is_ok(&result));
    ck_assert_ptr_nonnull(term);
    ck_assert_int_eq(alt_screen_enter_count, 1);
    ik_term_cleanup(term);
    talloc_free(ctx);
}
END_TEST

START_TEST(test_terminal_cleanup_exits_alt_screen) {
    reset_mocks();
    void *ctx = talloc_new(NULL);
    ik_term_ctx_t *term = NULL;
    res_t result = ik_term_init(ctx, NULL, &term);
    ck_assert(is_ok(&result));
    ck_assert_ptr_nonnull(term);
    alt_screen_exit_count = 0;
    ik_term_cleanup(term);
    ck_assert_int_eq(alt_screen_exit_count, 1);
    talloc_free(ctx);
}

END_TEST

// Create test suite
static Suite *mouse_scroll_suite(void)
{
    Suite *s = suite_create("Mouse Scroll Integration");

    TCase *tc_terminal = tcase_create("Terminal");
    tcase_set_timeout(tc_terminal, IK_TEST_TIMEOUT);
    tcase_add_test(tc_terminal, test_terminal_init_enters_alt_screen);
    tcase_add_test(tc_terminal, test_terminal_cleanup_exits_alt_screen);
    suite_add_tcase(s, tc_terminal);

    TCase *tc_integration = tcase_create("Integration");
    tcase_set_timeout(tc_integration, IK_TEST_TIMEOUT);
    suite_add_tcase(s, tc_integration);

    return s;
}

int32_t main(void)
{
    Suite *s = mouse_scroll_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    int32_t number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    ik_test_reset_terminal();

    return (number_failed == 0) ? 0 : 1;
}
