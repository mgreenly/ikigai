/**
 * @file manager_test.c
 * @brief Unit tests for debug pipe manager
 */

#include <check.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/select.h>
#include <talloc.h>
#include <unistd.h>
#include "../../../src/debug_pipe.h"
#include "../../../src/scrollback.h"
#include "../../../src/wrapper.h"
#include "../../test_utils.h"

/* Test: Create debug pipe manager */
START_TEST(test_debug_mgr_create) {
    void *ctx = talloc_new(NULL);

    /* Create manager */
    res_t res = ik_debug_manager_create(ctx);
    ck_assert(is_ok(&res));

    ik_debug_pipe_manager_t *mgr = res.ok;
    ck_assert_ptr_nonnull(mgr);

    /* Verify initial state */
    ck_assert_ptr_nonnull(mgr->pipes);
    ck_assert_uint_eq(mgr->count, 0);
    ck_assert_uint_eq(mgr->capacity, 4);

    talloc_free(ctx);
}
END_TEST
/* Test: Add pipe to manager */
START_TEST(test_debug_mgr_add_pipe) {
    void *ctx = talloc_new(NULL);

    /* Create manager */
    res_t mgr_res = ik_debug_manager_create(ctx);
    ck_assert(is_ok(&mgr_res));
    ik_debug_pipe_manager_t *mgr = mgr_res.ok;

    /* Add pipe with prefix */
    res_t pipe_res = ik_debug_manager_add_pipe(mgr, "[test1]");
    ck_assert(is_ok(&pipe_res));
    ik_debug_pipe_t *pipe = pipe_res.ok;

    /* Verify pipe is valid */
    ck_assert_ptr_nonnull(pipe);
    ck_assert_ptr_nonnull(pipe->write_end);
    ck_assert_int_ge(pipe->read_fd, 0);
    ck_assert_str_eq(pipe->prefix, "[test1]");

    /* Verify manager state */
    ck_assert_uint_eq(mgr->count, 1);
    ck_assert_ptr_eq(mgr->pipes[0], pipe);

    talloc_free(ctx);
}

END_TEST
/* Test: Add multiple pipes (verify array growth) */
START_TEST(test_debug_mgr_add_multiple_pipes) {
    void *ctx = talloc_new(NULL);

    /* Create manager */
    res_t mgr_res = ik_debug_manager_create(ctx);
    ck_assert(is_ok(&mgr_res));
    ik_debug_pipe_manager_t *mgr = mgr_res.ok;

    /* Initial capacity is 4 */
    ck_assert_uint_eq(mgr->capacity, 4);

    /* Add 10 pipes to trigger array growth */
    ik_debug_pipe_t *pipes[10];
    for (int i = 0; i < 10; i++) {
        char prefix[32];
        snprintf(prefix, sizeof(prefix), "[pipe%d]", i);
        res_t pipe_res = ik_debug_manager_add_pipe(mgr, prefix);
        ck_assert(is_ok(&pipe_res));
        pipes[i] = pipe_res.ok;
        ck_assert_ptr_nonnull(pipes[i]);
    }

    /* Verify all pipes are accessible */
    ck_assert_uint_eq(mgr->count, 10);
    for (int i = 0; i < 10; i++) {
        ck_assert_ptr_eq(mgr->pipes[i], pipes[i]);
    }

    /* Verify array grew (capacity should be at least 10) */
    ck_assert_uint_ge(mgr->capacity, 10);

    talloc_free(ctx);
}

END_TEST
/* Test: Add pipes to fd_set */
START_TEST(test_debug_mgr_add_to_fdset) {
    void *ctx = talloc_new(NULL);

    /* Create manager and add 3 pipes */
    res_t mgr_res = ik_debug_manager_create(ctx);
    ck_assert(is_ok(&mgr_res));
    ik_debug_pipe_manager_t *mgr = mgr_res.ok;

    ik_debug_pipe_t *pipe1, *pipe2, *pipe3;
    res_t res1 = ik_debug_manager_add_pipe(mgr, "[pipe1]");
    ck_assert(is_ok(&res1));
    pipe1 = res1.ok;

    res_t res2 = ik_debug_manager_add_pipe(mgr, "[pipe2]");
    ck_assert(is_ok(&res2));
    pipe2 = res2.ok;

    res_t res3 = ik_debug_manager_add_pipe(mgr, "[pipe3]");
    ck_assert(is_ok(&res3));
    pipe3 = res3.ok;

    /* Initialize fd_set and max_fd */
    fd_set read_fds;
    FD_ZERO(&read_fds);
    int max_fd = 0;

    /* Add all pipes to fd_set */
    ik_debug_manager_add_to_fdset(mgr, &read_fds, &max_fd);

    /* Verify all pipe read_fds are in set */
    ck_assert(FD_ISSET(pipe1->read_fd, &read_fds));
    ck_assert(FD_ISSET(pipe2->read_fd, &read_fds));
    ck_assert(FD_ISSET(pipe3->read_fd, &read_fds));

    /* Verify max_fd is updated correctly */
    int expected_max = pipe1->read_fd;
    if (pipe2->read_fd > expected_max) expected_max = pipe2->read_fd;
    if (pipe3->read_fd > expected_max) expected_max = pipe3->read_fd;
    ck_assert_int_eq(max_fd, expected_max);

    talloc_free(ctx);
}

END_TEST
/* Test: Handle ready pipes with debug_enabled=true */
START_TEST(test_debug_mgr_handle_ready_enabled) {
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
    ck_assert_ptr_nonnull(scrollback);

    /* Write test data to pipe */
    const char *test_data = "hello world\n";
    size_t written = fwrite(test_data, 1, strlen(test_data), pipe->write_end);
    ck_assert_uint_eq(written, strlen(test_data));
    fflush(pipe->write_end);

    /* Set up fd_set with pipe marked as ready */
    fd_set read_fds;
    FD_ZERO(&read_fds);
    FD_SET(pipe->read_fd, &read_fds);

    /* Handle ready pipes with debug enabled */
    res_t handle_res = ik_debug_manager_handle_ready(mgr, &read_fds, scrollback, true);
    ck_assert(is_ok(&handle_res));

    /* Verify output was appended to scrollback with blank line after */
    ck_assert_uint_eq(ik_scrollback_get_line_count(scrollback), 2);

    const char *line_text = NULL;
    size_t line_length = 0;
    res_t get_res = ik_scrollback_get_line_text(scrollback, 0, &line_text, &line_length);
    ck_assert(is_ok(&get_res));
    ck_assert_ptr_nonnull(line_text);

    /* Should have prefix + space + content */
    ck_assert_str_eq(line_text, "[test] hello world");

    /* Second line should be blank */
    get_res = ik_scrollback_get_line_text(scrollback, 1, &line_text, &line_length);
    ck_assert(is_ok(&get_res));
    ck_assert_str_eq(line_text, "");

    talloc_free(ctx);
}

END_TEST
/* Test: Handle ready pipes with debug_enabled=false */
START_TEST(test_debug_mgr_handle_ready_disabled) {
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
    ck_assert_ptr_nonnull(scrollback);

    /* Write test data to pipe */
    const char *test_data = "should be discarded\n";
    size_t written = fwrite(test_data, 1, strlen(test_data), pipe->write_end);
    ck_assert_uint_eq(written, strlen(test_data));
    fflush(pipe->write_end);

    /* Set up fd_set with pipe marked as ready */
    fd_set read_fds;
    FD_ZERO(&read_fds);
    FD_SET(pipe->read_fd, &read_fds);

    /* Handle ready pipes with debug disabled */
    res_t handle_res = ik_debug_manager_handle_ready(mgr, &read_fds, scrollback, false);
    ck_assert(is_ok(&handle_res));

    /* Verify scrollback was NOT modified */
    ck_assert_uint_eq(ik_scrollback_get_line_count(scrollback), 0);

    /* Verify pipe was drained - write more data and it should not block */
    const char *more_data = "second write\n";
    written = fwrite(more_data, 1, strlen(more_data), pipe->write_end);
    ck_assert_uint_eq(written, strlen(more_data));
    fflush(pipe->write_end);

    talloc_free(ctx);
}

END_TEST
/* Test: Handle multiple pipes but only some are ready */
START_TEST(test_debug_mgr_handle_ready_partial) {
    void *ctx = talloc_new(NULL);

    /* Create manager and add 3 pipes */
    res_t mgr_res = ik_debug_manager_create(ctx);
    ck_assert(is_ok(&mgr_res));
    ik_debug_pipe_manager_t *mgr = mgr_res.ok;

    res_t pipe1_res = ik_debug_manager_add_pipe(mgr, "[pipe1]");
    ck_assert(is_ok(&pipe1_res));
    ik_debug_pipe_t *pipe1 = pipe1_res.ok;

    res_t pipe2_res = ik_debug_manager_add_pipe(mgr, "[pipe2]");
    ck_assert(is_ok(&pipe2_res));
    ik_debug_pipe_t *pipe2 = pipe2_res.ok;
    (void)pipe2;  /* pipe2 intentionally not used - tests continue path */

    res_t pipe3_res = ik_debug_manager_add_pipe(mgr, "[pipe3]");
    ck_assert(is_ok(&pipe3_res));
    ik_debug_pipe_t *pipe3 = pipe3_res.ok;

    /* Create scrollback */
    ik_scrollback_t *scrollback = ik_scrollback_create(ctx, 80);

    /* Write to pipe1 and pipe3 only */
    fwrite("from pipe1\n", 1, 11, pipe1->write_end);
    fflush(pipe1->write_end);

    fwrite("from pipe3\n", 1, 11, pipe3->write_end);
    fflush(pipe3->write_end);

    /* Set up fd_set with only pipe1 and pipe3 (not pipe2) */
    fd_set read_fds;
    FD_ZERO(&read_fds);
    FD_SET(pipe1->read_fd, &read_fds);
    FD_SET(pipe3->read_fd, &read_fds);
    /* pipe2 NOT set - tests continue path */

    /* Handle ready pipes with debug enabled */
    res_t handle_res = ik_debug_manager_handle_ready(mgr, &read_fds, scrollback, true);
    ck_assert(is_ok(&handle_res));

    /* Verify we got 4 lines: pipe1 line + blank, pipe3 line + blank (but not pipe2) */
    ck_assert_uint_eq(ik_scrollback_get_line_count(scrollback), 4);

    talloc_free(ctx);
}

END_TEST

/* Error injection tests */

/* Override posix_pipe_ to inject failure */
static int fail_pipe = 0;
int posix_pipe_(int pipefd[2])
{
    if (fail_pipe) {
        errno = EMFILE;
        return -1;
    }
    return pipe(pipefd);
}

/* Test: Add pipe fails when pipe creation fails */
START_TEST(test_debug_mgr_add_pipe_creation_failure) {
    void *ctx = talloc_new(NULL);

    /* Create manager */
    res_t mgr_res = ik_debug_manager_create(ctx);
    ck_assert(is_ok(&mgr_res));
    ik_debug_pipe_manager_t *mgr = mgr_res.ok;

    /* Enable pipe() failure */
    fail_pipe = 1;

    /* Try to add pipe - should fail */
    res_t pipe_res = ik_debug_manager_add_pipe(mgr, "[test]");
    ck_assert(is_err(&pipe_res));

    /* Manager should still be valid but empty */
    ck_assert_uint_eq(mgr->count, 0);

    /* Disable failure for cleanup */
    fail_pipe = 0;

    talloc_free(ctx);
}

END_TEST
/* Test: add_to_fdset when max_fd is already larger than pipe fds */
START_TEST(test_debug_mgr_add_to_fdset_max_fd_large) {
    void *ctx = talloc_new(NULL);

    /* Create manager and add pipe */
    res_t mgr_res = ik_debug_manager_create(ctx);
    ck_assert(is_ok(&mgr_res));
    ik_debug_pipe_manager_t *mgr = mgr_res.ok;

    res_t pipe_res = ik_debug_manager_add_pipe(mgr, "[test]");
    ck_assert(is_ok(&pipe_res));
    ik_debug_pipe_t *pipe = pipe_res.ok;

    /* Initialize fd_set and set max_fd to a value larger than pipe->read_fd */
    fd_set read_fds;
    FD_ZERO(&read_fds);
    int max_fd = pipe->read_fd + 100;  /* Much larger than any pipe fd */
    int original_max_fd = max_fd;

    /* Add pipe to fd_set */
    ik_debug_manager_add_to_fdset(mgr, &read_fds, &max_fd);

    /* Verify pipe is in set */
    ck_assert(FD_ISSET(pipe->read_fd, &read_fds));

    /* max_fd should remain unchanged since pipe->read_fd < original max_fd */
    ck_assert_int_eq(max_fd, original_max_fd);

    talloc_free(ctx);
}

END_TEST
/* Test: handle_ready when pipe has data but no complete line */
START_TEST(test_debug_mgr_handle_ready_no_newline) {
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

    /* Write data WITHOUT newline */
    const char *test_data = "incomplete line";
    size_t written = fwrite(test_data, 1, strlen(test_data), pipe->write_end);
    ck_assert_uint_eq(written, strlen(test_data));
    fflush(pipe->write_end);

    /* Set up fd_set with pipe marked as ready */
    fd_set read_fds;
    FD_ZERO(&read_fds);
    FD_SET(pipe->read_fd, &read_fds);

    /* Handle ready pipes with debug enabled */
    res_t handle_res = ik_debug_manager_handle_ready(mgr, &read_fds, scrollback, true);
    ck_assert(is_ok(&handle_res));

    /* Since no newline, no lines should be added to scrollback (count == 0) */
    ck_assert_uint_eq(ik_scrollback_get_line_count(scrollback), 0);

    /* The data should be buffered in the pipe for next read */
    ck_assert_uint_eq(pipe->buffer_pos, strlen(test_data));

    talloc_free(ctx);
}

END_TEST
/* Test: handle_ready when pipe has no data (tests lines==NULL branch) */
START_TEST(test_debug_mgr_handle_ready_no_data) {
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

    /* Don't write anything to pipe - it will have no data */

    /* Set up fd_set with pipe marked as ready */
    fd_set read_fds;
    FD_ZERO(&read_fds);
    FD_SET(pipe->read_fd, &read_fds);

    /* Handle ready pipes with debug enabled */
    res_t handle_res = ik_debug_manager_handle_ready(mgr, &read_fds, scrollback, true);
    ck_assert(is_ok(&handle_res));

    /* No lines should be added since there was no data */
    ck_assert_uint_eq(ik_scrollback_get_line_count(scrollback), 0);

    talloc_free(ctx);
}

END_TEST
/* Test: Destructor properly cleans up read_fd */
START_TEST(test_debug_pipe_destructor) {
    void *ctx = talloc_new(NULL);

    /* Create a pipe */
    res_t res = ik_debug_pipe_create(ctx, "[destructor_test]");
    ck_assert(is_ok(&res));
    ik_debug_pipe_t *pipe = res.ok;

    /* Store the read_fd for verification */
    int read_fd = pipe->read_fd;
    ck_assert_int_ge(read_fd, 0);

    /* Free the pipe - this triggers the destructor */
    talloc_free(pipe);

    /* After destruction, the read_fd should be closed */
    /* We can't directly verify it's closed, but at least we tested the destructor path */

    talloc_free(ctx);
}

END_TEST

static Suite *debug_pipe_manager_suite(void)
{
    Suite *s = suite_create("Debug Pipe Manager");
    TCase *tc_core = tcase_create("Core");
    tcase_set_timeout(tc_core, IK_TEST_TIMEOUT);

    /* Normal tests */
    tcase_add_test(tc_core, test_debug_mgr_create);
    tcase_add_test(tc_core, test_debug_mgr_add_pipe);
    tcase_add_test(tc_core, test_debug_mgr_add_multiple_pipes);
    tcase_add_test(tc_core, test_debug_mgr_add_to_fdset);
    tcase_add_test(tc_core, test_debug_mgr_handle_ready_enabled);
    tcase_add_test(tc_core, test_debug_mgr_handle_ready_disabled);
    tcase_add_test(tc_core, test_debug_mgr_handle_ready_partial);
    tcase_add_test(tc_core, test_debug_mgr_add_pipe_creation_failure);
    tcase_add_test(tc_core, test_debug_mgr_add_to_fdset_max_fd_large);
    tcase_add_test(tc_core, test_debug_mgr_handle_ready_no_newline);
    tcase_add_test(tc_core, test_debug_mgr_handle_ready_no_data);
    tcase_add_test(tc_core, test_debug_pipe_destructor);

    suite_add_tcase(s, tc_core);

    return s;
}

int main(void)
{
    int number_failed;
    Suite *s = debug_pipe_manager_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
