/**
 * @file create_test.c
 * @brief Unit tests for debug pipe creation
 */

#include <check.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <talloc.h>
#include <unistd.h>
#include "../../../src/debug_pipe.h"
#include "../../../src/error.h"
#include "../../../src/wrapper.h"
#include "../../test_utils_helper.h"

/* Test: Create debug pipe with prefix */
START_TEST(test_debug_pipe_create_with_prefix) {
    void *ctx = talloc_new(NULL);
    const char *prefix = "[test]";

    /* Create debug pipe */
    res_t res = ik_debug_pipe_create(ctx, prefix);
    ck_assert(is_ok(&res));

    ik_debug_pipe_t *pipe = res.ok;
    ck_assert_ptr_nonnull(pipe);

    /* Verify FILE* write end is valid */
    ck_assert_ptr_nonnull(pipe->write_end);

    /* Verify read_fd is valid (non-negative) */
    ck_assert_int_ge(pipe->read_fd, 0);

    /* Verify read_fd is non-blocking */
    int flags = fcntl(pipe->read_fd, F_GETFL);
    ck_assert_int_ge(flags, 0);
    ck_assert_int_eq(flags & O_NONBLOCK, O_NONBLOCK);

    /* Verify prefix is stored */
    ck_assert_ptr_nonnull(pipe->prefix);
    ck_assert_str_eq(pipe->prefix, prefix);

    /* Verify line buffer is allocated */
    ck_assert_ptr_nonnull(pipe->line_buffer);
    ck_assert_uint_gt(pipe->buffer_capacity, 0);
    ck_assert_uint_eq(pipe->buffer_pos, 0);

    talloc_free(ctx);
}
END_TEST
/* Test: Create debug pipe without prefix */
START_TEST(test_debug_pipe_create_no_prefix) {
    void *ctx = talloc_new(NULL);

    /* Create debug pipe with NULL prefix */
    res_t res = ik_debug_pipe_create(ctx, NULL);
    ck_assert(is_ok(&res));

    ik_debug_pipe_t *pipe = res.ok;
    ck_assert_ptr_nonnull(pipe);

    /* Verify prefix is NULL */
    ck_assert_ptr_null(pipe->prefix);

    /* Other fields should still be valid */
    ck_assert_ptr_nonnull(pipe->write_end);
    ck_assert_int_ge(pipe->read_fd, 0);
    ck_assert_ptr_nonnull(pipe->line_buffer);

    talloc_free(ctx);
}

END_TEST
/* Test: Pipe write/read connectivity */
START_TEST(test_debug_pipe_connectivity) {
    void *ctx = talloc_new(NULL);

    res_t res = ik_debug_pipe_create(ctx, NULL);
    ck_assert(is_ok(&res));

    ik_debug_pipe_t *pipe = res.ok;

    /* Write to FILE* end */
    const char *test_data = "hello";
    size_t written = fwrite(test_data, 1, strlen(test_data), pipe->write_end);
    ck_assert_uint_eq(written, strlen(test_data));
    fflush(pipe->write_end);

    /* Read from fd end */
    char buffer[64] = {0};
    ssize_t nread = read(pipe->read_fd, buffer, sizeof(buffer) - 1);
    ck_assert_int_eq(nread, (ssize_t)strlen(test_data));
    ck_assert_str_eq(buffer, test_data);

    talloc_free(ctx);
}

END_TEST

/* Error injection tests */

/* Override posix_pipe_ to inject failure */
static int fail_pipe = 0;
int posix_pipe_(int pipefd[2])
{
    if (fail_pipe) {
        errno = EMFILE;  // Too many open files
        return -1;
    }
    return pipe(pipefd);
}

START_TEST(test_debug_pipe_create_pipe_failure) {
    void *ctx = talloc_new(NULL);

    /* Enable pipe() failure */
    fail_pipe = 1;

    /* Create debug pipe - should fail */
    res_t res = ik_debug_pipe_create(ctx, NULL);
    ck_assert(is_err(&res));
    ck_assert_int_eq(res.err->code, ERR_IO);

    /* Disable failure for cleanup */
    fail_pipe = 0;

    talloc_free(ctx);
}

END_TEST

/* Override posix_fcntl_ to inject failure */
static int fail_fcntl = 0;
static int fcntl_fail_mode = 0;  // 0=F_GETFL, 1=F_SETFL
int posix_fcntl_(int fd, int cmd, int arg)
{
    if (fail_fcntl) {
        if ((fcntl_fail_mode == 0 && cmd == F_GETFL) ||
            (fcntl_fail_mode == 1 && cmd == F_SETFL)) {
            errno = EBADF;  // Bad file descriptor
            return -1;
        }
    }
    return fcntl(fd, cmd, arg);
}

START_TEST(test_debug_pipe_create_fcntl_getfl_failure) {
    void *ctx = talloc_new(NULL);

    /* Enable fcntl(F_GETFL) failure */
    fail_fcntl = 1;
    fcntl_fail_mode = 0;

    /* Create debug pipe - should fail */
    res_t res = ik_debug_pipe_create(ctx, NULL);
    ck_assert(is_err(&res));
    ck_assert_int_eq(res.err->code, ERR_IO);

    /* Disable failure for cleanup */
    fail_fcntl = 0;

    talloc_free(ctx);
}

END_TEST

START_TEST(test_debug_pipe_create_fcntl_setfl_failure) {
    void *ctx = talloc_new(NULL);

    /* Enable fcntl(F_SETFL) failure */
    fail_fcntl = 1;
    fcntl_fail_mode = 1;

    /* Create debug pipe - should fail */
    res_t res = ik_debug_pipe_create(ctx, NULL);
    ck_assert(is_err(&res));
    ck_assert_int_eq(res.err->code, ERR_IO);

    /* Disable failure for cleanup */
    fail_fcntl = 0;

    talloc_free(ctx);
}

END_TEST

/* Override posix_fdopen_ to inject failure */
static int fail_fdopen = 0;
FILE *posix_fdopen_(int fd, const char *mode)
{
    if (fail_fdopen) {
        errno = EMFILE;  // Too many open files
        return NULL;
    }
    return fdopen(fd, mode);
}

START_TEST(test_debug_pipe_create_fdopen_failure) {
    void *ctx = talloc_new(NULL);

    /* Enable fdopen() failure */
    fail_fdopen = 1;

    /* Create debug pipe - should fail */
    res_t res = ik_debug_pipe_create(ctx, NULL);
    ck_assert(is_err(&res));
    ck_assert_int_eq(res.err->code, ERR_IO);

    /* Disable failure for cleanup */
    fail_fdopen = 0;

    talloc_free(ctx);
}

END_TEST

static Suite *debug_pipe_create_suite(void)
{
    Suite *s = suite_create("Debug Pipe Create");
    TCase *tc_core = tcase_create("Core");
    tcase_set_timeout(tc_core, IK_TEST_TIMEOUT);

    /* Normal tests */
    tcase_add_test(tc_core, test_debug_pipe_create_with_prefix);
    tcase_add_test(tc_core, test_debug_pipe_create_no_prefix);
    tcase_add_test(tc_core, test_debug_pipe_connectivity);

    /* Error injection tests */
    tcase_add_test(tc_core, test_debug_pipe_create_pipe_failure);
    tcase_add_test(tc_core, test_debug_pipe_create_fcntl_getfl_failure);
    tcase_add_test(tc_core, test_debug_pipe_create_fcntl_setfl_failure);
    tcase_add_test(tc_core, test_debug_pipe_create_fdopen_failure);

    suite_add_tcase(s, tc_core);

    return s;
}

int main(void)
{
    int number_failed;
    Suite *s = debug_pipe_create_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
