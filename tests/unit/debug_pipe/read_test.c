/**
 * @file read_test.c
 * @brief Unit tests for debug pipe reading and line parsing
 */

#include <check.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <talloc.h>
#include <unistd.h>
#include "../../../src/debug_pipe.h"
#include "../../../src/error.h"
#include "../../../src/wrapper.h"
#include "../../test_utils_helper.h"

/* Test: Read single complete line */
START_TEST(test_debug_pipe_read_single_line) {
    void *ctx = talloc_new(NULL);
    const char *prefix = "[test]";

    /* Create debug pipe */
    res_t res = ik_debug_pipe_create(ctx, prefix);
    ck_assert(is_ok(&res));
    ik_debug_pipe_t *pipe = res.ok;

    /* Write a complete line */
    const char *test_line = "hello world\n";
    fwrite(test_line, 1, strlen(test_line), pipe->write_end);
    fflush(pipe->write_end);

    /* Read lines */
    char **lines = NULL;
    size_t count = 0;
    res = ik_debug_pipe_read(pipe, &lines, &count);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(count, 1);
    ck_assert_ptr_nonnull(lines);
    ck_assert_str_eq(lines[0], "[test] hello world");

    talloc_free(ctx);
}
END_TEST
/* Test: Read single line without prefix */
START_TEST(test_debug_pipe_read_no_prefix) {
    void *ctx = talloc_new(NULL);

    /* Create debug pipe without prefix */
    res_t res = ik_debug_pipe_create(ctx, NULL);
    ck_assert(is_ok(&res));
    ik_debug_pipe_t *pipe = res.ok;

    /* Write a complete line */
    const char *test_line = "no prefix\n";
    fwrite(test_line, 1, strlen(test_line), pipe->write_end);
    fflush(pipe->write_end);

    /* Read lines */
    char **lines = NULL;
    size_t count = 0;
    res = ik_debug_pipe_read(pipe, &lines, &count);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(count, 1);
    ck_assert_ptr_nonnull(lines);
    ck_assert_str_eq(lines[0], "no prefix");

    talloc_free(ctx);
}

END_TEST
/* Test: Read partial line (no newline) */
START_TEST(test_debug_pipe_read_partial_line) {
    void *ctx = talloc_new(NULL);

    res_t res = ik_debug_pipe_create(ctx, NULL);
    ck_assert(is_ok(&res));
    ik_debug_pipe_t *pipe = res.ok;

    /* Write partial line (no newline) */
    const char *partial = "incomplete";
    fwrite(partial, 1, strlen(partial), pipe->write_end);
    fflush(pipe->write_end);

    /* Read should return no complete lines */
    char **lines = NULL;
    size_t count = 0;
    res = ik_debug_pipe_read(pipe, &lines, &count);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(count, 0);

    /* Write the rest of the line */
    const char *rest = " line\n";
    fwrite(rest, 1, strlen(rest), pipe->write_end);
    fflush(pipe->write_end);

    /* Now should get the complete line */
    res = ik_debug_pipe_read(pipe, &lines, &count);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(count, 1);
    ck_assert_str_eq(lines[0], "incomplete line");

    talloc_free(ctx);
}

END_TEST
/* Test: Read multiple lines in single read */
START_TEST(test_debug_pipe_read_multiple_lines) {
    void *ctx = talloc_new(NULL);
    const char *prefix = "[multi]";

    res_t res = ik_debug_pipe_create(ctx, prefix);
    ck_assert(is_ok(&res));
    ik_debug_pipe_t *pipe = res.ok;

    /* Write multiple lines at once */
    const char *multiline = "line1\nline2\nline3\n";
    fwrite(multiline, 1, strlen(multiline), pipe->write_end);
    fflush(pipe->write_end);

    /* Read should return all three lines */
    char **lines = NULL;
    size_t count = 0;
    res = ik_debug_pipe_read(pipe, &lines, &count);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(count, 3);
    ck_assert_str_eq(lines[0], "[multi] line1");
    ck_assert_str_eq(lines[1], "[multi] line2");
    ck_assert_str_eq(lines[2], "[multi] line3");

    talloc_free(ctx);
}

END_TEST
/* Test: Empty line handling */
START_TEST(test_debug_pipe_read_empty_lines) {
    void *ctx = talloc_new(NULL);

    res_t res = ik_debug_pipe_create(ctx, NULL);
    ck_assert(is_ok(&res));
    ik_debug_pipe_t *pipe = res.ok;

    /* Write lines with empty line in middle */
    const char *with_empty = "first\n\nlast\n";
    fwrite(with_empty, 1, strlen(with_empty), pipe->write_end);
    fflush(pipe->write_end);

    /* Read should return all three lines (including empty) */
    char **lines = NULL;
    size_t count = 0;
    res = ik_debug_pipe_read(pipe, &lines, &count);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(count, 3);
    ck_assert_str_eq(lines[0], "first");
    ck_assert_str_eq(lines[1], "");
    ck_assert_str_eq(lines[2], "last");

    talloc_free(ctx);
}

END_TEST
/* Test: No data available (non-blocking) */
START_TEST(test_debug_pipe_read_no_data) {
    void *ctx = talloc_new(NULL);

    res_t res = ik_debug_pipe_create(ctx, NULL);
    ck_assert(is_ok(&res));
    ik_debug_pipe_t *pipe = res.ok;

    /* Read without writing anything */
    char **lines = NULL;
    size_t count = 0;
    res = ik_debug_pipe_read(pipe, &lines, &count);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(count, 0);

    talloc_free(ctx);
}

END_TEST
/* Test: Read from closed pipe (EOF) */
START_TEST(test_debug_pipe_read_eof) {
    void *ctx = talloc_new(NULL);

    res_t res = ik_debug_pipe_create(ctx, "[test]");
    ck_assert(is_ok(&res));

    ik_debug_pipe_t *pipe = res.ok;

    /* Close write end to trigger EOF */
    fclose(pipe->write_end);
    pipe->write_end = NULL;

    /* Read should return OK with 0 lines */
    char **lines = NULL;
    size_t count = 0;
    res_t read_res = ik_debug_pipe_read(pipe, &lines, &count);
    ck_assert(is_ok(&read_res));
    ck_assert_uint_eq(count, 0);

    talloc_free(ctx);
}

END_TEST
/* Test: Read many lines (>16 to trigger array growth) */
START_TEST(test_debug_pipe_read_many_lines) {
    void *ctx = talloc_new(NULL);

    res_t res = ik_debug_pipe_create(ctx, "[test]");
    ck_assert(is_ok(&res));

    ik_debug_pipe_t *pipe = res.ok;

    /* Write 20 lines */
    for (int i = 0; i < 20; i++) {
        fprintf(pipe->write_end, "line %d\n", i);
    }
    fflush(pipe->write_end);

    /* Read all lines */
    char **lines = NULL;
    size_t count = 0;
    res_t read_res = ik_debug_pipe_read(pipe, &lines, &count);
    ck_assert(is_ok(&read_res));
    ck_assert_uint_eq(count, 20);

    /* Verify lines */
    for (int i = 0; i < 20; i++) {
        char expected[64];
        snprintf(expected, sizeof(expected), "[test] line %d", i);
        ck_assert_str_eq(lines[i], expected);
    }

    talloc_free(ctx);
}

END_TEST
/* Test: Read very long line (>1024 chars to trigger buffer growth) */
START_TEST(test_debug_pipe_read_long_line) {
    void *ctx = talloc_new(NULL);

    res_t res = ik_debug_pipe_create(ctx, "[test]");
    ck_assert(is_ok(&res));

    ik_debug_pipe_t *pipe = res.ok;

    /* Create a line with 2000 'a' characters */
    char long_line[2048];
    memset(long_line, 'a', 2000);
    long_line[2000] = '\0';

    /* Write the long line */
    fprintf(pipe->write_end, "%s\n", long_line);
    fflush(pipe->write_end);

    /* Read the line */
    char **lines = NULL;
    size_t count = 0;
    res_t read_res = ik_debug_pipe_read(pipe, &lines, &count);
    ck_assert(is_ok(&read_res));
    ck_assert_uint_eq(count, 1);

    /* Verify the line has prefix + long content */
    ck_assert_uint_eq(strlen(lines[0]), 7 + 2000);  /* "[test] " + 2000 chars */
    ck_assert(strncmp(lines[0], "[test] ", 7) == 0);
    ck_assert(strncmp(lines[0] + 7, long_line, 2000) == 0);

    talloc_free(ctx);
}

END_TEST

/* Error injection tests */

/* Override posix_read_ to inject read() failure */
static int fail_read = 0;
static int read_errno_value = 0;
ssize_t posix_read_(int fd, void *buf, size_t count)
{
    if (fail_read) {
        errno = read_errno_value;
        return -1;
    }
    return read(fd, buf, count);
}

START_TEST(test_debug_pipe_read_eagain) {
    void *ctx = talloc_new(NULL);

    res_t res = ik_debug_pipe_create(ctx, NULL);
    ck_assert(is_ok(&res));
    ik_debug_pipe_t *pipe = res.ok;

    /* Enable read() failure with EAGAIN */
    fail_read = 1;
    read_errno_value = EAGAIN;

    /* Read should return OK with 0 lines (EAGAIN is not an error) */
    char **lines = NULL;
    size_t count = 0;
    res_t read_res = ik_debug_pipe_read(pipe, &lines, &count);
    ck_assert(is_ok(&read_res));
    ck_assert_uint_eq(count, 0);

    /* Disable failure for cleanup */
    fail_read = 0;

    talloc_free(ctx);
}

END_TEST

START_TEST(test_debug_pipe_read_ewouldblock) {
    void *ctx = talloc_new(NULL);

    res_t res = ik_debug_pipe_create(ctx, NULL);
    ck_assert(is_ok(&res));
    ik_debug_pipe_t *pipe = res.ok;

    /* Enable read() failure with EWOULDBLOCK */
    fail_read = 1;
    read_errno_value = EWOULDBLOCK;

    /* Read should return OK with 0 lines (EWOULDBLOCK is not an error) */
    char **lines = NULL;
    size_t count = 0;
    res_t read_res = ik_debug_pipe_read(pipe, &lines, &count);
    ck_assert(is_ok(&read_res));
    ck_assert_uint_eq(count, 0);

    /* Disable failure for cleanup */
    fail_read = 0;

    talloc_free(ctx);
}

END_TEST

START_TEST(test_debug_pipe_read_error) {
    void *ctx = talloc_new(NULL);

    res_t res = ik_debug_pipe_create(ctx, NULL);
    ck_assert(is_ok(&res));
    ik_debug_pipe_t *pipe = res.ok;

    /* Enable read() failure with EIO (real error) */
    fail_read = 1;
    read_errno_value = EIO;

    /* Read should return error */
    char **lines = NULL;
    size_t count = 0;
    res_t read_res = ik_debug_pipe_read(pipe, &lines, &count);
    ck_assert(is_err(&read_res));
    ck_assert_int_eq(read_res.err->code, ERR_IO);

    /* Disable failure for cleanup */
    fail_read = 0;

    talloc_free(ctx);
}

END_TEST

static Suite *debug_pipe_read_suite(void)
{
    Suite *s = suite_create("Debug Pipe Read");
    TCase *tc_core = tcase_create("Core");
    tcase_set_timeout(tc_core, IK_TEST_TIMEOUT);

    /* Normal tests */
    tcase_add_test(tc_core, test_debug_pipe_read_single_line);
    tcase_add_test(tc_core, test_debug_pipe_read_no_prefix);
    tcase_add_test(tc_core, test_debug_pipe_read_partial_line);
    tcase_add_test(tc_core, test_debug_pipe_read_multiple_lines);
    tcase_add_test(tc_core, test_debug_pipe_read_empty_lines);
    tcase_add_test(tc_core, test_debug_pipe_read_no_data);
    tcase_add_test(tc_core, test_debug_pipe_read_eof);
    tcase_add_test(tc_core, test_debug_pipe_read_many_lines);
    tcase_add_test(tc_core, test_debug_pipe_read_long_line);

    /* Error injection tests */
    tcase_add_test(tc_core, test_debug_pipe_read_eagain);
    tcase_add_test(tc_core, test_debug_pipe_read_ewouldblock);
    tcase_add_test(tc_core, test_debug_pipe_read_error);

    suite_add_tcase(s, tc_core);

    return s;
}

int main(void)
{
    int number_failed;
    Suite *s = debug_pipe_read_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/debug_pipe/read_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
