/**
 * @file create_test.c
 * @brief Unit tests for debug pipe creation
 */

#include <check.h>
#include <fcntl.h>
#include <signal.h>
#include <talloc.h>
#include <unistd.h>
#include "../../../src/debug_pipe.h"
#include "../../test_utils.h"

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
START_TEST(test_debug_pipe_create_no_prefix)
{
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
START_TEST(test_debug_pipe_connectivity)
{
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

static Suite *debug_pipe_create_suite(void)
{
    Suite *s = suite_create("Debug Pipe Create");
    TCase *tc_core = tcase_create("Core");

    /* Normal tests */
    tcase_add_test(tc_core, test_debug_pipe_create_with_prefix);
    tcase_add_test(tc_core, test_debug_pipe_create_no_prefix);
    tcase_add_test(tc_core, test_debug_pipe_connectivity);

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
