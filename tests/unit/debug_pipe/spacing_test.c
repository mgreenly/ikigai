/**
 * @file spacing_test.c
 * @brief Unit tests for debug pipe blank line spacing
 */

#include <check.h>
#include <errno.h>
#include <stdio.h>
#include <sys/select.h>
#include <talloc.h>
#include <unistd.h>
#include "../../../src/debug_pipe.h"
#include "../../../src/scrollback.h"
#include "../../../src/wrapper.h"

/* Test: handle_ready adds blank lines after each debug line when enabled */
START_TEST(test_debug_mgr_handle_ready_adds_blank_lines) {
    void *ctx = talloc_new(NULL);

    /* Create manager */
    res_t mgr_res = ik_debug_manager_create(ctx);
    ck_assert(is_ok(&mgr_res));
    ik_debug_pipe_manager_t *mgr = mgr_res.ok;

    /* Add pipe with prefix */
    res_t pipe_res = ik_debug_manager_add_pipe(mgr, "[test]");
    ck_assert(is_ok(&pipe_res));
    ik_debug_pipe_t *pipe = pipe_res.ok;

    /* Create scrollback */
    ik_scrollback_t *scrollback = ik_scrollback_create(ctx, 80);
    ck_assert_ptr_nonnull(scrollback);

    /* Write test lines to pipe */
    fprintf(pipe->write_end, "line1\n");
    fprintf(pipe->write_end, "line2\n");
    fflush(pipe->write_end);

    /* Set up fd_set */
    fd_set read_fds;
    FD_ZERO(&read_fds);
    FD_SET(pipe->read_fd, &read_fds);

    /* Handle ready pipes with debug enabled */
    res_t handle_res = ik_debug_manager_handle_ready(mgr, &read_fds, scrollback, true);
    ck_assert(is_ok(&handle_res));

    /* Should have 4 lines: line1, blank, line2, blank */
    ck_assert_uint_eq(ik_scrollback_get_line_count(scrollback), 4);

    /* Verify line contents */
    const char *line_text = NULL;
    size_t line_length = 0;
    res_t get_res = ik_scrollback_get_line_text(scrollback, 0, &line_text, &line_length);
    ck_assert(is_ok(&get_res));
    ck_assert_str_eq(line_text, "[test] line1");

    get_res = ik_scrollback_get_line_text(scrollback, 1, &line_text, &line_length);
    ck_assert(is_ok(&get_res));
    ck_assert_str_eq(line_text, "");

    get_res = ik_scrollback_get_line_text(scrollback, 2, &line_text, &line_length);
    ck_assert(is_ok(&get_res));
    ck_assert_str_eq(line_text, "[test] line2");

    get_res = ik_scrollback_get_line_text(scrollback, 3, &line_text, &line_length);
    ck_assert(is_ok(&get_res));
    ck_assert_str_eq(line_text, "");

    talloc_free(ctx);
}

END_TEST
/* Test: handle_ready with debug disabled reads but discards (no blank lines) */
START_TEST(test_debug_mgr_handle_ready_disabled_no_blank_lines) {
    void *ctx = talloc_new(NULL);

    /* Create manager */
    res_t mgr_res = ik_debug_manager_create(ctx);
    ck_assert(is_ok(&mgr_res));
    ik_debug_pipe_manager_t *mgr = mgr_res.ok;

    /* Add pipe */
    res_t pipe_res = ik_debug_manager_add_pipe(mgr, "[test]");
    ck_assert(is_ok(&pipe_res));
    ik_debug_pipe_t *pipe = pipe_res.ok;

    /* Create scrollback */
    ik_scrollback_t *scrollback = ik_scrollback_create(ctx, 80);
    ck_assert_ptr_nonnull(scrollback);

    /* Write test line */
    fprintf(pipe->write_end, "should not appear\n");
    fflush(pipe->write_end);

    /* Set up fd_set */
    fd_set read_fds;
    FD_ZERO(&read_fds);
    FD_SET(pipe->read_fd, &read_fds);

    /* Handle ready pipes with debug DISABLED */
    res_t handle_res = ik_debug_manager_handle_ready(mgr, &read_fds, scrollback, false);
    ck_assert(is_ok(&handle_res));

    /* Should have 0 lines (debug disabled) */
    ck_assert_uint_eq(ik_scrollback_get_line_count(scrollback), 0);

    talloc_free(ctx);
}

END_TEST

/* Override posix_read_ to inject failure */
static int fail_read = 0;
ssize_t posix_read_(int fd, void *buf, size_t count)
{
    if (fail_read) {
        errno = EIO;
        return -1;
    }
    return read(fd, buf, count);
}

/* Test: handle_ready when read fails with error */
START_TEST(test_debug_mgr_handle_ready_read_error) {
    void *ctx = talloc_new(NULL);

    /* Create manager and add pipe */
    res_t mgr_res = ik_debug_manager_create(ctx);
    ck_assert(is_ok(&mgr_res));
    ik_debug_pipe_manager_t *mgr = mgr_res.ok;

    res_t pipe_res = ik_debug_manager_add_pipe(mgr, "[test]");
    ck_assert(is_ok(&pipe_res));
    ik_debug_pipe_t *pipe = pipe_res.ok;

    /* Create scrollback */
    ik_scrollback_t *scrollback = ik_scrollback_create(ctx, 80);

    /* Set up fd_set with pipe marked as ready */
    fd_set read_fds;
    FD_ZERO(&read_fds);
    FD_SET(pipe->read_fd, &read_fds);

    /* Enable read() failure */
    fail_read = 1;

    /* Handle ready pipes - should fail with read error */
    res_t handle_res = ik_debug_manager_handle_ready(mgr, &read_fds, scrollback, true);
    ck_assert(is_err(&handle_res));

    /* Disable failure for cleanup */
    fail_read = 0;

    talloc_free(ctx);
}

END_TEST

static Suite *debug_pipe_spacing_suite(void)
{
    Suite *s = suite_create("Debug Pipe Spacing");
    TCase *tc_core = tcase_create("Core");
    tcase_set_timeout(tc_core, IK_TEST_TIMEOUT);

    tcase_add_test(tc_core, test_debug_mgr_handle_ready_adds_blank_lines);
    tcase_add_test(tc_core, test_debug_mgr_handle_ready_disabled_no_blank_lines);
    tcase_add_test(tc_core, test_debug_mgr_handle_ready_read_error);

    suite_add_tcase(s, tc_core);

    return s;
}

int main(void)
{
    int number_failed;
    Suite *s = debug_pipe_spacing_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
