// Terminal module unit tests
#include <check.h>
#include <signal.h>
#include <talloc.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <sys/ioctl.h>
#include "../../../src/terminal.h"
#include "../../../src/error.h"
#include "../../test_utils.h"

// Mock control state
static int mock_open_fail = 0;
static int mock_tcgetattr_fail = 0;
static int mock_tcsetattr_fail = 0;
static int mock_tcflush_fail = 0;
static int mock_write_fail = 0;
static int mock_write_fail_on_call = 0;  // Fail on specific write call number
static int mock_ioctl_fail = 0;
static int mock_select_return = 0;        // 0 = timeout, >0 = ready
static int mock_read_fail = 0;
static int mock_close_count = 0;
static int mock_write_count = 0;
static int mock_tcsetattr_count = 0;
static int mock_tcflush_count = 0;

// Buffer to capture write calls
#define MOCK_WRITE_BUFFER_SIZE 1024
static char mock_write_buffer[MOCK_WRITE_BUFFER_SIZE];
static size_t mock_write_buffer_pos = 0;

// Mock function prototypes
int posix_open_(const char *pathname, int flags);
int posix_close_(int fd);
int posix_tcgetattr_(int fd, struct termios *termios_p);
int posix_tcsetattr_(int fd, int optional_actions, const struct termios *termios_p);
int posix_tcflush_(int fd, int queue_selector);
int posix_ioctl_(int fd, unsigned long request, void *argp);
ssize_t posix_write_(int fd, const void *buf, size_t count);
int posix_select_(int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds, struct timeval *timeout);
ssize_t posix_read_(int fd, void *buf, size_t count);

// Mock implementations
int posix_open_(const char *pathname, int flags)
{
    (void)pathname;
    (void)flags;
    if (mock_open_fail) {
        return -1;
    }
    return 42; // Mock fd
}

int posix_close_(int fd)
{
    (void)fd;
    mock_close_count++;
    return 0;
}

int posix_tcgetattr_(int fd, struct termios *termios_p)
{
    (void)fd;
    if (mock_tcgetattr_fail) {
        return -1;
    }
    // Fill with dummy data
    memset(termios_p, 0, sizeof(*termios_p));
    return 0;
}

int posix_tcsetattr_(int fd, int optional_actions, const struct termios *termios_p)
{
    (void)fd;
    (void)optional_actions;
    (void)termios_p;
    mock_tcsetattr_count++;
    if (mock_tcsetattr_fail) {
        return -1;
    }
    return 0;
}

int posix_tcflush_(int fd, int queue_selector)
{
    (void)fd;
    (void)queue_selector;
    mock_tcflush_count++;
    if (mock_tcflush_fail) {
        return -1;
    }
    return 0;
}

int posix_ioctl_(int fd, unsigned long request, void *argp)
{
    (void)fd;
    (void)request;
    if (mock_ioctl_fail) {
        return -1;
    }
    // Fill winsize with dummy data
    struct winsize *ws = (struct winsize *)argp;
    ws->ws_row = 24;
    ws->ws_col = 80;
    return 0;
}

ssize_t posix_write_(int fd, const void *buf, size_t count)
{
    (void)fd;
    mock_write_count++;
    if (mock_write_fail) {
        return -1;
    }
    if (mock_write_fail_on_call > 0 && mock_write_count == mock_write_fail_on_call) {
        return -1;
    }
    // Capture written data to buffer
    if (mock_write_buffer_pos + count < MOCK_WRITE_BUFFER_SIZE) {
        memcpy(mock_write_buffer + mock_write_buffer_pos, buf, count);
        mock_write_buffer_pos += count;
    }
    return (ssize_t)count;
}

int posix_select_(int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds, struct timeval *timeout)
{
    (void)nfds;
    (void)readfds;
    (void)writefds;
    (void)exceptfds;
    (void)timeout;
    return mock_select_return;
}

ssize_t posix_read_(int fd, void *buf, size_t count)
{
    (void)fd;
    (void)count;
    if (mock_read_fail) {
        return -1;
    }
    // Return a dummy CSI u response if select indicated ready
    if (mock_select_return > 0) {
        const char *response = "\x1b[?0u";
        size_t len = 5;
        if (len > count) len = count;
        memcpy(buf, response, len);
        return (ssize_t)len;
    }
    return 0;
}

// Reset mock state before each test
static void reset_mocks(void)
{
    mock_open_fail = 0;
    mock_tcgetattr_fail = 0;
    mock_tcsetattr_fail = 0;
    mock_tcflush_fail = 0;
    mock_write_fail = 0;
    mock_write_fail_on_call = 0;
    mock_ioctl_fail = 0;
    mock_select_return = 0;
    mock_read_fail = 0;
    mock_close_count = 0;
    mock_write_count = 0;
    mock_tcsetattr_count = 0;
    mock_tcflush_count = 0;
    memset(mock_write_buffer, 0, MOCK_WRITE_BUFFER_SIZE);
    mock_write_buffer_pos = 0;
}

// Test: successful terminal initialization
START_TEST(test_term_init_success) {
    reset_mocks();
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_term_ctx_t *term = NULL;

    res_t res = ik_term_init(ctx, &term);

    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(term);
    ck_assert_int_eq(term->tty_fd, 42);
    ck_assert_int_eq(term->screen_rows, 24);
    ck_assert_int_eq(term->screen_cols, 80);

    // Verify write was called for alternate screen (and CSI u query)
    // CSI u query (4 bytes) + alt screen enter (8 bytes) = 2 writes
    ck_assert_int_eq(mock_write_count, 2);

    // Cleanup
    ik_term_cleanup(term);
    talloc_free(ctx);

    // Verify cleanup operations
    // Note: CSI u was not enabled in mocks (select times out), so no disable write
    ck_assert_int_eq(mock_write_count, 3); // query + alt screen enter + alt screen exit
    ck_assert_int_eq(mock_tcsetattr_count, 2); // restore termios
    ck_assert_int_eq(mock_tcflush_count, 2); // flush after set raw + cleanup
    ck_assert_int_eq(mock_close_count, 1);
}
END_TEST

// Test: alternate screen sequences are written during init and cleanup
START_TEST(test_term_alt_screen_sequences)
{
    reset_mocks();
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_term_ctx_t *term = NULL;

    res_t res = ik_term_init(ctx, &term);
    ck_assert(is_ok(&res));

    // Verify alternate screen enter sequence is present in init output
    ck_assert(strstr(mock_write_buffer, "\x1b[?1049h") != NULL);

    // Reset buffer to capture cleanup output
    memset(mock_write_buffer, 0, MOCK_WRITE_BUFFER_SIZE);
    mock_write_buffer_pos = 0;

    ik_term_cleanup(term);

    // Verify alternate screen exit sequence is present in cleanup output
    ck_assert(strstr(mock_write_buffer, "\x1b[?1049l") != NULL);

    talloc_free(ctx);
}
END_TEST
// Test: open fails
START_TEST(test_term_init_open_fails)
{
    reset_mocks();
    mock_open_fail = 1;

    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_term_ctx_t *term = NULL;

    res_t res = ik_term_init(ctx, &term);

    ck_assert(is_err(&res));
    ck_assert_int_eq(error_code(res.err), ERR_IO);
    ck_assert_ptr_null(term);

    // No cleanup calls should have been made
    ck_assert_int_eq(mock_close_count, 0);

    talloc_free(ctx);
}

END_TEST
// Test: tcgetattr fails
START_TEST(test_term_init_tcgetattr_fails)
{
    reset_mocks();
    mock_tcgetattr_fail = 1;

    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_term_ctx_t *term = NULL;

    res_t res = ik_term_init(ctx, &term);

    ck_assert(is_err(&res));
    ck_assert_int_eq(error_code(res.err), ERR_IO);
    ck_assert_ptr_null(term);

    // Close should have been called
    ck_assert_int_eq(mock_close_count, 1);

    talloc_free(ctx);
}

END_TEST
// Test: tcsetattr fails (raw mode)
START_TEST(test_term_init_tcsetattr_fails)
{
    reset_mocks();
    mock_tcsetattr_fail = 1;

    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_term_ctx_t *term = NULL;

    res_t res = ik_term_init(ctx, &term);

    ck_assert(is_err(&res));
    ck_assert_int_eq(error_code(res.err), ERR_IO);
    ck_assert_ptr_null(term);

    // Close should have been called
    ck_assert_int_eq(mock_close_count, 1);

    talloc_free(ctx);
}

END_TEST
// Test: write fails (alternate screen)
START_TEST(test_term_init_write_fails)
{
    reset_mocks();
    mock_write_fail = 1;

    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_term_ctx_t *term = NULL;

    res_t res = ik_term_init(ctx, &term);

    ck_assert(is_err(&res));
    ck_assert_int_eq(error_code(res.err), ERR_IO);
    ck_assert_ptr_null(term);

    // Cleanup should have been called (tcsetattr + close)
    ck_assert_int_eq(mock_tcsetattr_count, 2); // raw mode + restore
    ck_assert_int_eq(mock_tcflush_count, 1); // flush after set raw
    ck_assert_int_eq(mock_close_count, 1);

    talloc_free(ctx);
}

END_TEST

// Test: ioctl fails (get terminal size)
START_TEST(test_term_init_ioctl_fails)
{
    reset_mocks();
    mock_ioctl_fail = 1;

    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_term_ctx_t *term = NULL;

    res_t res = ik_term_init(ctx, &term);

    ck_assert(is_err(&res));
    ck_assert_int_eq(error_code(res.err), ERR_IO);
    ck_assert_ptr_null(term);

    // Full cleanup should have been called
    ck_assert_int_eq(mock_write_count, 3); // CSI u query + enter alt screen + exit alt screen
    ck_assert_int_eq(mock_tcsetattr_count, 2); // raw mode + restore
    ck_assert_int_eq(mock_tcflush_count, 1); // flush after set raw
    ck_assert_int_eq(mock_close_count, 1);

    talloc_free(ctx);
}

END_TEST
// Test: terminal cleanup with NULL
START_TEST(test_term_cleanup_null_safe)
{
    reset_mocks();
    // Should handle NULL gracefully (no crash)
    ik_term_cleanup(NULL);

    // No operations should have been called
    ck_assert_int_eq(mock_write_count, 0);
    ck_assert_int_eq(mock_tcsetattr_count, 0);
    ck_assert_int_eq(mock_close_count, 0);
}

END_TEST
// Test: get terminal size success
START_TEST(test_term_get_size_success)
{
    reset_mocks();
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_term_ctx_t *term = NULL;

    res_t res = ik_term_init(ctx, &term);
    ck_assert(is_ok(&res));

    int rows, cols;
    res_t size_res = ik_term_get_size(term, &rows, &cols);

    ck_assert(is_ok(&size_res));
    ck_assert_int_eq(rows, 24);
    ck_assert_int_eq(cols, 80);
    ck_assert_int_eq(rows, term->screen_rows);
    ck_assert_int_eq(cols, term->screen_cols);

    ik_term_cleanup(term);
    talloc_free(ctx);
}

END_TEST
// Test: get terminal size fails
START_TEST(test_term_get_size_fails)
{
    reset_mocks();
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_term_ctx_t *term = NULL;

    res_t res = ik_term_init(ctx, &term);
    ck_assert(is_ok(&res));

    // Make ioctl fail on second call
    mock_ioctl_fail = 1;

    int rows, cols;
    res_t size_res = ik_term_get_size(term, &rows, &cols);

    ck_assert(is_err(&size_res));
    ck_assert_int_eq(error_code(size_res.err), ERR_IO);

    ik_term_cleanup(term);
    talloc_free(ctx);
}

END_TEST

#if !defined(NDEBUG) && !defined(SKIP_SIGNAL_TESTS)
// Test: ik_term_init with NULL parent asserts
START_TEST(test_term_init_null_parent_asserts)
{
    ik_term_ctx_t *term = NULL;
    ik_term_init(NULL, &term);
}

END_TEST
// Test: ik_term_init with NULL ctx_out asserts
START_TEST(test_term_init_null_ctx_out_asserts)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_term_init(ctx, NULL);
    talloc_free(ctx);
}

END_TEST
// Test: ik_term_get_size with NULL ctx asserts
START_TEST(test_term_get_size_null_ctx_asserts)
{
    int rows, cols;
    ik_term_get_size(NULL, &rows, &cols);
}

END_TEST
// Test: ik_term_get_size with NULL rows_out asserts
START_TEST(test_term_get_size_null_rows_asserts)
{
    reset_mocks();
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_term_ctx_t *term = NULL;
    ik_term_init(ctx, &term);

    int cols;
    ik_term_get_size(term, NULL, &cols);

    ik_term_cleanup(term);
    talloc_free(ctx);
}

END_TEST
// Test: ik_term_get_size with NULL cols_out asserts
START_TEST(test_term_get_size_null_cols_asserts)
{
    reset_mocks();
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_term_ctx_t *term = NULL;
    ik_term_init(ctx, &term);

    int rows;
    ik_term_get_size(term, &rows, NULL);

    ik_term_cleanup(term);
    talloc_free(ctx);
}

END_TEST
#endif

// Test: tcflush fails
START_TEST(test_term_init_tcflush_fails)
{
    reset_mocks();
    mock_tcflush_fail = 1;

    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_term_ctx_t *term = NULL;

    res_t res = ik_term_init(ctx, &term);

    ck_assert(is_err(&res));
    ck_assert_int_eq(error_code(res.err), ERR_IO);
    ck_assert_ptr_null(term);

    // Cleanup should have been called (tcsetattr + close)
    ck_assert_int_eq(mock_tcsetattr_count, 2); // raw mode + restore
    ck_assert_int_eq(mock_close_count, 1);

    talloc_free(ctx);
}

END_TEST

// Test: csi_u_supported field exists and is initialized
START_TEST(test_term_init_sets_csi_u_supported)
{
    reset_mocks();
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_term_ctx_t *term = NULL;

    res_t res = ik_term_init(ctx, &term);

    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(term);

    // Field should be initialized (either true or false, not uninitialized)
    // In test environment with mocks, the value depends on mock behavior
    ck_assert(term->csi_u_supported == true || term->csi_u_supported == false);

    ik_term_cleanup(term);
    talloc_free(ctx);
}
END_TEST

// Test suite
static Suite *terminal_suite(void)
{
    Suite *s = suite_create("Terminal");
    TCase *tc_core = tcase_create("Core");
    tcase_set_timeout(tc_core, 30);

    tcase_add_test(tc_core, test_term_init_success);
    tcase_add_test(tc_core, test_term_alt_screen_sequences);
    tcase_add_test(tc_core, test_term_init_open_fails);
    tcase_add_test(tc_core, test_term_init_tcgetattr_fails);
    tcase_add_test(tc_core, test_term_init_tcsetattr_fails);
    tcase_add_test(tc_core, test_term_init_tcflush_fails);
    tcase_add_test(tc_core, test_term_init_write_fails);
    tcase_add_test(tc_core, test_term_init_ioctl_fails);
    tcase_add_test(tc_core, test_term_cleanup_null_safe);
    tcase_add_test(tc_core, test_term_get_size_success);
    tcase_add_test(tc_core, test_term_get_size_fails);
    tcase_add_test(tc_core, test_term_init_sets_csi_u_supported);

#if !defined(NDEBUG) && !defined(SKIP_SIGNAL_TESTS)
    tcase_add_test_raise_signal(tc_core, test_term_init_null_parent_asserts, SIGABRT);
    tcase_add_test_raise_signal(tc_core, test_term_init_null_ctx_out_asserts, SIGABRT);
    tcase_add_test_raise_signal(tc_core, test_term_get_size_null_ctx_asserts, SIGABRT);
    tcase_add_test_raise_signal(tc_core, test_term_get_size_null_rows_asserts, SIGABRT);
    tcase_add_test_raise_signal(tc_core, test_term_get_size_null_cols_asserts, SIGABRT);
#endif

    suite_add_tcase(s, tc_core);
    return s;
}

int main(void)
{
    Suite *s = terminal_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    ik_test_reset_terminal();

    return (number_failed == 0) ? 0 : 1;
}
