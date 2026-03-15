/* bg_process_io_test.c — stdin write, output append, output read */
#include <check.h>
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <talloc.h>
#include <unistd.h>

#include "apps/ikigai/bg_line_index.h"
#include "apps/ikigai/bg_process.h"
#include "apps/ikigai/bg_process_io.h"
#include "shared/error.h"

/* ================================================================
 * Mock state
 * ================================================================ */

static bool    g_write_passthrough = false; /* true → call real write() */
static ssize_t g_write_return      = 0;     /* <0 → error; 0 → return count */
static int     g_write_errno       = 0;
static char    g_write_last[256];
static size_t  g_write_last_len    = 0;
static int     g_write_call_count  = 0;

static void reset_mocks(void)
{
    g_write_passthrough = false;
    g_write_return      = 0;
    g_write_errno       = 0;
    g_write_last_len    = 0;
    g_write_call_count  = 0;
    memset(g_write_last, 0, sizeof(g_write_last));
}

/* ================================================================
 * Weak symbol overrides
 * ================================================================ */

ssize_t posix_write_(int fd, const void *buf, size_t count);
ssize_t posix_pread_(int fd, void *buf, size_t count, off_t offset);
int     posix_close_(int fd);

ssize_t posix_write_(int fd, const void *buf, size_t count)
{
    g_write_call_count++;
    if (count < sizeof(g_write_last)) {
        memcpy(g_write_last, buf, count);
        g_write_last_len = count;
    }
    if (g_write_passthrough) {
        return write(fd, buf, count);
    }
    if (g_write_return < 0) {
        errno = g_write_errno;
        return -1;
    }
    return (ssize_t)count;
}

ssize_t posix_pread_(int fd, void *buf, size_t count, off_t offset)
{
    return pread(fd, buf, count, offset);
}

int posix_close_(int fd)
{
    return close(fd);
}

/* ================================================================
 * Helpers
 * ================================================================ */

/* Build a minimal bg_process_t without using bg_process_start. */
static bg_process_t *make_proc(TALLOC_CTX *ctx, int output_fd)
{
    bg_process_t *proc  = talloc_zero(ctx, bg_process_t);
    proc->master_fd     = -1;
    proc->pidfd         = -1;
    proc->output_fd     = output_fd;
    proc->stdin_open    = true;
    proc->total_bytes   = 0;
    proc->cursor        = 0;
    proc->line_index    = bg_line_index_create(proc);
    return proc;
}

/* Create an anonymous temp file and return its fd. */
static int make_tmpfile(void)
{
    char path[] = "/tmp/bg_io_test_XXXXXX";
    int  fd     = mkstemp(path);
    if (fd < 0) ck_abort_msg("mkstemp failed");
    unlink(path);
    return fd;
}

/* ================================================================
 * bg_process_write_stdin tests
 * ================================================================ */

START_TEST(test_write_stdin_succeeds)
{
    reset_mocks();
    TALLOC_CTX   *ctx  = talloc_new(NULL);
    bg_process_t *proc = make_proc(ctx, -1);
    proc->master_fd    = 5;

    res_t r = bg_process_write_stdin(proc, "hello", 5, false);
    ck_assert(is_ok(&r));
    ck_assert_int_eq(g_write_call_count, 1);
    ck_assert_int_eq((int)g_write_last_len, 5);
    ck_assert(memcmp(g_write_last, "hello", 5) == 0);
    talloc_free(ctx);
}
END_TEST

START_TEST(test_write_stdin_appends_newline)
{
    reset_mocks();
    TALLOC_CTX   *ctx  = talloc_new(NULL);
    bg_process_t *proc = make_proc(ctx, -1);
    proc->master_fd    = 5;

    res_t r = bg_process_write_stdin(proc, "hi", 2, true);
    ck_assert(is_ok(&r));
    ck_assert_int_eq(g_write_call_count, 2); /* data write + newline write */
    talloc_free(ctx);
}
END_TEST

START_TEST(test_write_stdin_io_error_returns_err)
{
    reset_mocks();
    g_write_return = -1;
    g_write_errno  = EIO;
    TALLOC_CTX   *ctx  = talloc_new(NULL);
    bg_process_t *proc = make_proc(ctx, -1);
    proc->master_fd    = 5;

    res_t r = bg_process_write_stdin(proc, "x", 1, false);
    ck_assert(is_err(&r));
    ck_assert_int_eq(r.err->code, ERR_IO);
    talloc_free(ctx);
}
END_TEST

START_TEST(test_write_stdin_after_close_returns_error)
{
    reset_mocks();
    TALLOC_CTX   *ctx  = talloc_new(NULL);
    bg_process_t *proc = make_proc(ctx, -1);
    proc->master_fd    = 5;
    proc->stdin_open   = false;

    res_t r = bg_process_write_stdin(proc, "x", 1, false);
    ck_assert(is_err(&r));
    ck_assert_int_eq(r.err->code, ERR_INVALID_ARG);
    ck_assert_int_eq(g_write_call_count, 0); /* no write attempted */
    talloc_free(ctx);
}
END_TEST

START_TEST(test_write_stdin_null_data_returns_error)
{
    reset_mocks();
    TALLOC_CTX   *ctx  = talloc_new(NULL);
    bg_process_t *proc = make_proc(ctx, -1);
    proc->master_fd    = 5;

    res_t r = bg_process_write_stdin(proc, NULL, 0, false);
    ck_assert(is_err(&r));
    ck_assert_int_eq(r.err->code, ERR_INVALID_ARG);
    ck_assert_int_eq(g_write_call_count, 0);
    talloc_free(ctx);
}
END_TEST

/* ================================================================
 * bg_process_close_stdin tests
 * ================================================================ */

START_TEST(test_close_stdin_sets_flag)
{
    reset_mocks();
    TALLOC_CTX   *ctx  = talloc_new(NULL);
    bg_process_t *proc = make_proc(ctx, -1);
    proc->master_fd    = 5;

    ck_assert(proc->stdin_open);
    res_t r = bg_process_close_stdin(proc);
    ck_assert(is_ok(&r));
    ck_assert(!proc->stdin_open);
    ck_assert_int_eq(g_write_call_count, 1);           /* wrote Ctrl-D */
    ck_assert_int_eq((int)g_write_last_len, 1);
    ck_assert_int_eq((unsigned char)g_write_last[0], 0x04);
    talloc_free(ctx);
}
END_TEST

START_TEST(test_close_stdin_already_closed_is_noop)
{
    reset_mocks();
    TALLOC_CTX   *ctx  = talloc_new(NULL);
    bg_process_t *proc = make_proc(ctx, -1);
    proc->master_fd    = 5;
    proc->stdin_open   = false;

    res_t r = bg_process_close_stdin(proc);
    ck_assert(is_ok(&r));
    ck_assert_int_eq(g_write_call_count, 0); /* no Ctrl-D written */
    talloc_free(ctx);
}
END_TEST

START_TEST(test_close_stdin_io_error)
{
    reset_mocks();
    g_write_return = -1;
    g_write_errno  = EIO;
    TALLOC_CTX   *ctx  = talloc_new(NULL);
    bg_process_t *proc = make_proc(ctx, -1);
    proc->master_fd    = 5;

    res_t r = bg_process_close_stdin(proc);
    ck_assert(is_err(&r));
    ck_assert_int_eq(r.err->code, ERR_IO);
    ck_assert(proc->stdin_open); /* flag not changed on error */
    talloc_free(ctx);
}
END_TEST

/* ================================================================
 * bg_process_append_output tests
 * ================================================================ */

START_TEST(test_append_output_updates_line_index)
{
    reset_mocks();
    g_write_passthrough = true;
    int output_fd = make_tmpfile();

    TALLOC_CTX   *ctx  = talloc_new(NULL);
    bg_process_t *proc = make_proc(ctx, output_fd);

    const uint8_t data[] = "hello\nworld\n";
    res_t r = bg_process_append_output(proc, data, sizeof(data) - 1);
    ck_assert(is_ok(&r));
    ck_assert_int_eq(bg_line_index_count(proc->line_index), 2);
    ck_assert_int_eq(proc->total_bytes, (int64_t)(sizeof(data) - 1));

    talloc_free(ctx);
    close(output_fd);
}
END_TEST

START_TEST(test_append_output_null_data_returns_error)
{
    TALLOC_CTX   *ctx  = talloc_new(NULL);
    bg_process_t *proc = make_proc(ctx, -1);

    res_t r = bg_process_append_output(proc, NULL, 5);
    ck_assert(is_err(&r));
    ck_assert_int_eq(r.err->code, ERR_INVALID_ARG);
    talloc_free(ctx);
}
END_TEST

START_TEST(test_append_output_zero_len_returns_error)
{
    TALLOC_CTX    *ctx  = talloc_new(NULL);
    bg_process_t  *proc = make_proc(ctx, -1);
    const uint8_t  data[] = "x";

    res_t r = bg_process_append_output(proc, data, 0);
    ck_assert(is_err(&r));
    ck_assert_int_eq(r.err->code, ERR_INVALID_ARG);
    talloc_free(ctx);
}
END_TEST

/* ================================================================
 * bg_process_read_output tests
 * ================================================================ */

START_TEST(test_read_since_last_empty_output)
{
    reset_mocks();
    g_write_passthrough = true;
    int output_fd = make_tmpfile();

    TALLOC_CTX   *ctx  = talloc_new(NULL);
    bg_process_t *proc = make_proc(ctx, output_fd);

    uint8_t *buf = NULL;
    size_t   len = 0;
    res_t r = bg_process_read_output(proc, ctx, BG_READ_SINCE_LAST,
                                     0, 0, 0, &buf, &len);
    ck_assert(is_ok(&r));
    ck_assert_ptr_null(buf);
    ck_assert_int_eq((int)len, 0);

    talloc_free(ctx);
    close(output_fd);
}
END_TEST

START_TEST(test_read_since_last_advances_cursor)
{
    reset_mocks();
    g_write_passthrough = true;
    int output_fd = make_tmpfile();

    TALLOC_CTX   *ctx  = talloc_new(NULL);
    bg_process_t *proc = make_proc(ctx, output_fd);

    const uint8_t data[] = "line1\nline2\n";
    bg_process_append_output(proc, data, sizeof(data) - 1);

    uint8_t *buf = NULL;
    size_t   len = 0;
    res_t r = bg_process_read_output(proc, ctx, BG_READ_SINCE_LAST,
                                     0, 0, 0, &buf, &len);
    ck_assert(is_ok(&r));
    ck_assert_ptr_nonnull(buf);
    ck_assert_int_eq((int)len, (int)(sizeof(data) - 1));
    ck_assert(memcmp(buf, data, len) == 0);
    ck_assert_int_eq(proc->cursor, (int64_t)(sizeof(data) - 1));

    talloc_free(ctx);
    close(output_fd);
}
END_TEST

START_TEST(test_read_since_last_no_new_output)
{
    reset_mocks();
    g_write_passthrough = true;
    int output_fd = make_tmpfile();

    TALLOC_CTX   *ctx  = talloc_new(NULL);
    bg_process_t *proc = make_proc(ctx, output_fd);

    const uint8_t data[] = "hello\n";
    bg_process_append_output(proc, data, sizeof(data) - 1);

    /* First read drains all output */
    uint8_t *buf1 = NULL;
    size_t   len1 = 0;
    bg_process_read_output(proc, ctx, BG_READ_SINCE_LAST,
                           0, 0, 0, &buf1, &len1);
    ck_assert_int_eq(proc->cursor, proc->total_bytes);

    /* Second read: no new output */
    uint8_t *buf2 = NULL;
    size_t   len2 = 0;
    res_t r = bg_process_read_output(proc, ctx, BG_READ_SINCE_LAST,
                                     0, 0, 0, &buf2, &len2);
    ck_assert(is_ok(&r));
    ck_assert_ptr_null(buf2);
    ck_assert_int_eq((int)len2, 0);

    talloc_free(ctx);
    close(output_fd);
}
END_TEST

START_TEST(test_read_tail_mode)
{
    reset_mocks();
    g_write_passthrough = true;
    int output_fd = make_tmpfile();

    TALLOC_CTX   *ctx  = talloc_new(NULL);
    bg_process_t *proc = make_proc(ctx, output_fd);

    const uint8_t data[] = "line1\nline2\nline3\n";
    bg_process_append_output(proc, data, sizeof(data) - 1);

    uint8_t *buf = NULL;
    size_t   len = 0;
    res_t r = bg_process_read_output(proc, ctx, BG_READ_TAIL,
                                     2, 0, 0, &buf, &len);
    ck_assert(is_ok(&r));
    ck_assert_ptr_nonnull(buf);
    ck_assert(memcmp(buf, "line2\nline3\n", 12) == 0);
    ck_assert_int_eq((int)len, 12);

    talloc_free(ctx);
    close(output_fd);
}
END_TEST

START_TEST(test_read_tail_mode_clamped_to_available)
{
    reset_mocks();
    g_write_passthrough = true;
    int output_fd = make_tmpfile();

    TALLOC_CTX   *ctx  = talloc_new(NULL);
    bg_process_t *proc = make_proc(ctx, output_fd);

    const uint8_t data[] = "line1\nline2\n";
    bg_process_append_output(proc, data, sizeof(data) - 1);

    uint8_t *buf = NULL;
    size_t   len = 0;
    res_t r = bg_process_read_output(proc, ctx, BG_READ_TAIL,
                                     10, 0, 0, &buf, &len); /* request 10 but only 2 exist */
    ck_assert(is_ok(&r));
    ck_assert_ptr_nonnull(buf);
    ck_assert_int_eq((int)len, (int)(sizeof(data) - 1));

    talloc_free(ctx);
    close(output_fd);
}
END_TEST

START_TEST(test_read_tail_mode_empty_output)
{
    reset_mocks();
    g_write_passthrough = true;
    int output_fd = make_tmpfile();

    TALLOC_CTX   *ctx  = talloc_new(NULL);
    bg_process_t *proc = make_proc(ctx, output_fd);

    uint8_t *buf = NULL;
    size_t   len = 0;
    res_t r = bg_process_read_output(proc, ctx, BG_READ_TAIL,
                                     5, 0, 0, &buf, &len);
    ck_assert(is_ok(&r));
    ck_assert_ptr_null(buf);
    ck_assert_int_eq((int)len, 0);

    talloc_free(ctx);
    close(output_fd);
}
END_TEST

START_TEST(test_read_range_mode)
{
    reset_mocks();
    g_write_passthrough = true;
    int output_fd = make_tmpfile();

    TALLOC_CTX   *ctx  = talloc_new(NULL);
    bg_process_t *proc = make_proc(ctx, output_fd);

    const uint8_t data[] = "line1\nline2\nline3\n";
    bg_process_append_output(proc, data, sizeof(data) - 1);

    uint8_t *buf = NULL;
    size_t   len = 0;
    res_t r = bg_process_read_output(proc, ctx, BG_READ_RANGE,
                                     0, 2, 3, &buf, &len);
    ck_assert(is_ok(&r));
    ck_assert_ptr_nonnull(buf);
    ck_assert(memcmp(buf, "line2\nline3\n", 12) == 0);
    ck_assert_int_eq((int)len, 12);

    talloc_free(ctx);
    close(output_fd);
}
END_TEST

START_TEST(test_read_null_ctx_returns_error)
{
    TALLOC_CTX   *ctx  = talloc_new(NULL);
    bg_process_t *proc = make_proc(ctx, -1);

    uint8_t *buf = NULL;
    size_t   len = 0;
    res_t r = bg_process_read_output(proc, NULL, BG_READ_SINCE_LAST,
                                     0, 0, 0, &buf, &len);
    ck_assert(is_err(&r));
    ck_assert_int_eq(r.err->code, ERR_INVALID_ARG);
    talloc_free(ctx);
}
END_TEST

START_TEST(test_read_tail_zero_lines_returns_error)
{
    TALLOC_CTX   *ctx  = talloc_new(NULL);
    bg_process_t *proc = make_proc(ctx, -1);
    proc->total_bytes  = 10; /* pretend there's output */

    uint8_t *buf = NULL;
    size_t   len = 0;
    res_t r = bg_process_read_output(proc, ctx, BG_READ_TAIL,
                                     0, 0, 0, &buf, &len);
    ck_assert(is_err(&r));
    ck_assert_int_eq(r.err->code, ERR_INVALID_ARG);
    talloc_free(ctx);
}
END_TEST

/* ================================================================
 * Suite
 * ================================================================ */

static Suite *bg_process_io_suite(void)
{
    Suite *s = suite_create("bg_process_io");

    TCase *tc_stdin = tcase_create("WriteStdin");
    tcase_add_test(tc_stdin, test_write_stdin_succeeds);
    tcase_add_test(tc_stdin, test_write_stdin_appends_newline);
    tcase_add_test(tc_stdin, test_write_stdin_io_error_returns_err);
    tcase_add_test(tc_stdin, test_write_stdin_after_close_returns_error);
    tcase_add_test(tc_stdin, test_write_stdin_null_data_returns_error);
    suite_add_tcase(s, tc_stdin);

    TCase *tc_close = tcase_create("CloseStdin");
    tcase_add_test(tc_close, test_close_stdin_sets_flag);
    tcase_add_test(tc_close, test_close_stdin_already_closed_is_noop);
    tcase_add_test(tc_close, test_close_stdin_io_error);
    suite_add_tcase(s, tc_close);

    TCase *tc_append = tcase_create("AppendOutput");
    tcase_add_test(tc_append, test_append_output_updates_line_index);
    tcase_add_test(tc_append, test_append_output_null_data_returns_error);
    tcase_add_test(tc_append, test_append_output_zero_len_returns_error);
    suite_add_tcase(s, tc_append);

    TCase *tc_read = tcase_create("ReadOutput");
    tcase_add_test(tc_read, test_read_since_last_empty_output);
    tcase_add_test(tc_read, test_read_since_last_advances_cursor);
    tcase_add_test(tc_read, test_read_since_last_no_new_output);
    tcase_add_test(tc_read, test_read_tail_mode);
    tcase_add_test(tc_read, test_read_tail_mode_clamped_to_available);
    tcase_add_test(tc_read, test_read_tail_mode_empty_output);
    tcase_add_test(tc_read, test_read_range_mode);
    tcase_add_test(tc_read, test_read_null_ctx_returns_error);
    tcase_add_test(tc_read, test_read_tail_zero_lines_returns_error);
    suite_add_tcase(s, tc_read);

    return s;
}

int32_t main(void)
{
    Suite   *s  = bg_process_io_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/bg_process_io_test.xml");
    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);
    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
