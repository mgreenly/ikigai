/**
 * @file repl_debug_pipe_integration_test.c
 * @brief Integration test for debug pipe system in REPL event loop
 */

#include <check.h>
#include <talloc.h>
#include <string.h>
#include <unistd.h>
#include <sys/select.h>
#include "../../../src/repl.h"
#include "../../../src/debug_pipe.h"
#include "../../../src/scrollback.h"
#include "../../test_utils.h"

/**
 * Test: Debug pipe manager integration with debug_enabled=true
 *
 * Verifies that when debug is enabled, output written to a debug pipe
 * appears in the scrollback buffer.
 */
START_TEST(test_debug_pipe_enabled) {
    void *ctx = talloc_new(NULL);

    // Create debug manager
    res_t res = ik_debug_manager_create(ctx);
    ck_assert(is_ok(&res));
    ik_debug_pipe_manager_t *mgr = (ik_debug_pipe_manager_t *)res.ok;
    ck_assert_ptr_nonnull(mgr);

    // Create scrollback
    ik_scrollback_t *scrollback = ik_scrollback_create(ctx, 80);

    // Enable debug output
    bool debug_enabled = true;

    // Add a debug pipe
    res = ik_debug_manager_add_pipe(mgr, "[test]");
    ck_assert(is_ok(&res));
    ik_debug_pipe_t *pipe = (ik_debug_pipe_t *)res.ok;
    ck_assert_ptr_nonnull(pipe);
    ck_assert_ptr_nonnull(pipe->write_end);

    // Write test data to the pipe
    fprintf(pipe->write_end, "debug line 1\n");
    fprintf(pipe->write_end, "debug line 2\n");
    fflush(pipe->write_end);

    // Set up fd_set and add debug pipes
    fd_set read_fds;
    FD_ZERO(&read_fds);
    int max_fd = 0;
    ik_debug_manager_add_to_fdset(mgr, &read_fds, &max_fd);

    // Verify pipe fd is in the set
    ck_assert(FD_ISSET(pipe->read_fd, &read_fds));
    ck_assert_int_ge(max_fd, pipe->read_fd);

    // Simulate select() readiness by calling handle_ready
    ik_debug_manager_handle_ready(mgr, &read_fds, scrollback, debug_enabled);

    // Verify output appeared in scrollback
    size_t line_count = ik_scrollback_get_line_count(scrollback);
    ck_assert_uint_ge(line_count, 2);

    // Find lines with [test] prefix
    bool found_line1 = false;
    bool found_line2 = false;
    for (size_t i = 0; i < line_count; i++) {
        const char *line_text = NULL;
        size_t line_len = 0;
        res = ik_scrollback_get_line_text(scrollback, i, &line_text, &line_len);
        ck_assert(is_ok(&res));
        if (strstr(line_text, "[test] debug line 1") != NULL) found_line1 = true;
        if (strstr(line_text, "[test] debug line 2") != NULL) found_line2 = true;
    }
    ck_assert(found_line1);
    ck_assert(found_line2);

    talloc_free(ctx);
}
END_TEST
/**
 * Test: Debug pipe manager integration with debug_enabled=false
 *
 * Verifies that when debug is disabled, output written to a debug pipe
 * is drained but does NOT appear in scrollback.
 */
START_TEST(test_debug_pipe_disabled)
{
    void *ctx = talloc_new(NULL);

    // Create debug manager
    res_t res = ik_debug_manager_create(ctx);
    ck_assert(is_ok(&res));
    ik_debug_pipe_manager_t *mgr = (ik_debug_pipe_manager_t *)res.ok;
    ck_assert_ptr_nonnull(mgr);

    // Create scrollback
    ik_scrollback_t *scrollback = ik_scrollback_create(ctx, 80);

    // Debug is disabled
    bool debug_enabled = false;

    // Add a debug pipe
    res = ik_debug_manager_add_pipe(mgr, "[test]");
    ck_assert(is_ok(&res));
    ik_debug_pipe_t *pipe = (ik_debug_pipe_t *)res.ok;
    ck_assert_ptr_nonnull(pipe);

    // Record initial scrollback line count
    size_t initial_line_count = ik_scrollback_get_line_count(scrollback);

    // Write test data to the pipe
    fprintf(pipe->write_end, "should not appear\n");
    fflush(pipe->write_end);

    // Set up fd_set and add debug pipes
    fd_set read_fds;
    FD_ZERO(&read_fds);
    int max_fd = 0;
    ik_debug_manager_add_to_fdset(mgr, &read_fds, &max_fd);

    // Handle ready (should drain but not add to scrollback)
    ik_debug_manager_handle_ready(mgr, &read_fds, scrollback, debug_enabled);

    // Verify scrollback line count unchanged
    size_t final_line_count = ik_scrollback_get_line_count(scrollback);
    ck_assert_uint_eq(final_line_count, initial_line_count);

    talloc_free(ctx);
}

END_TEST

static Suite *debug_pipe_integration_suite(void)
{
    Suite *s = suite_create("Debug Pipe Integration");

    TCase *tc_integration = tcase_create("Event Loop Integration");
    tcase_set_timeout(tc_integration, 30);
    tcase_add_test(tc_integration, test_debug_pipe_enabled);
    tcase_add_test(tc_integration, test_debug_pipe_disabled);
    suite_add_tcase(s, tc_integration);

    return s;
}

int main(void)
{
    int number_failed;
    Suite *s = debug_pipe_integration_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
