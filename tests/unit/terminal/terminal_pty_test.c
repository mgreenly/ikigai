// Terminal module PTY-based tests - basic tests using real pseudo-terminals
// Tests basic terminal initialization, size detection, and cleanup
//
// Unlike mock-based tests, these use actual PTY pairs to simulate terminal behavior.
// This provides more realistic testing of the basic terminal functionality.

#include "terminal_pty_helpers.h"

#include <check.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <talloc.h>
#include <unistd.h>

#include "../../../src/error.h"
#include "../../../src/terminal.h"
#include "../../test_utils.h"

// ============================================================================
// Test: Basic PTY terminal initialization succeeds
// ============================================================================
START_TEST(test_pty_init_success)
{
    pty_pair_t pty;
    ck_assert_int_eq(create_pty_pair(&pty), 0);

    // Set a reasonable terminal size
    ck_assert_int_eq(pty_set_size(&pty, 24, 80), 0);

    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    ik_term_ctx_t *term = NULL;

    // No simulator thread - CSI u probe will timeout
    // Initialize terminal with PTY slave
    res_t res = ik_term_init_with_fd(ctx, NULL, pty.slave_fd, &term);

    // Should succeed (CSI u probe will timeout, but init continues)
    ck_assert_msg(is_ok(&res), "Expected success, got error");
    ck_assert_ptr_nonnull(term);

    // Verify terminal size was detected
    ck_assert_int_eq(term->screen_rows, 24);
    ck_assert_int_eq(term->screen_cols, 80);

    // CSI u should not be supported (no response sent)
    ck_assert(!term->csi_u_supported);

    // Cleanup
    ik_term_cleanup(term);
    talloc_free(ctx);
    close_pty_pair(&pty);
}
END_TEST

// ============================================================================
// Test: Terminal get_size works with PTY
// ============================================================================
START_TEST(test_pty_get_size)
{
    pty_pair_t pty;
    ck_assert_int_eq(create_pty_pair(&pty), 0);
    ck_assert_int_eq(pty_set_size(&pty, 40, 120), 0);

    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    ik_term_ctx_t *term = NULL;
    res_t res = ik_term_init_with_fd(ctx, NULL, pty.slave_fd, &term);

    ck_assert_msg(is_ok(&res), "Expected success");
    ck_assert_ptr_nonnull(term);

    // Verify initial size
    ck_assert_int_eq(term->screen_rows, 40);
    ck_assert_int_eq(term->screen_cols, 120);

    // Change size
    ck_assert_int_eq(pty_set_size(&pty, 50, 200), 0);

    // Get updated size
    int rows, cols;
    res_t size_res = ik_term_get_size(term, &rows, &cols);

    ck_assert_msg(is_ok(&size_res), "Expected success");
    ck_assert_int_eq(rows, 50);
    ck_assert_int_eq(cols, 200);
    ck_assert_int_eq(term->screen_rows, 50);
    ck_assert_int_eq(term->screen_cols, 200);

    ik_term_cleanup(term);
    talloc_free(ctx);
    close_pty_pair(&pty);
}
END_TEST

// ============================================================================
// Test: Cleanup with NULL is safe
// ============================================================================
START_TEST(test_pty_cleanup_null_safe)
{
    // Should not crash
    ik_term_cleanup(NULL);
}
END_TEST

// ============================================================================
// Test: Terminal cleanup without CSI u enabled (no disable sequence)
// ============================================================================
START_TEST(test_pty_cleanup_no_csi_u)
{
    pty_pair_t pty;
    ck_assert_int_eq(create_pty_pair(&pty), 0);
    ck_assert_int_eq(pty_set_size(&pty, 24, 80), 0);

    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    ik_term_ctx_t *term = NULL;

    // No simulator - probe will timeout, CSI u won't be enabled
    res_t res = ik_term_init_with_fd(ctx, NULL, pty.slave_fd, &term);

    ck_assert_msg(is_ok(&res), "Expected success");
    ck_assert_ptr_nonnull(term);
    ck_assert_msg(!term->csi_u_supported, "CSI u should not be supported");

    // Cleanup without CSI u - should skip disable sequence
    ik_term_cleanup(term);

    talloc_free(ctx);
    close_pty_pair(&pty);
}
END_TEST

// ============================================================================
// Test: Terminal cleanup with CSI u enabled writes disable sequence
// ============================================================================
START_TEST(test_pty_cleanup_csi_u_disable)
{
    pty_pair_t pty;
    ck_assert_int_eq(create_pty_pair(&pty), 0);
    ck_assert_int_eq(pty_set_size(&pty, 24, 80), 0);

    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    term_sim_config_t cfg = {
        .master_fd = pty.master_fd,
        .probe_response = "\x1b[?1u",
        .enable_response = "\x1b[?9u",
        .done = 0
    };

    pthread_t sim_thread;
    ck_assert_int_eq(pthread_create(&sim_thread, NULL, term_simulator_thread, &cfg), 0);

    ik_term_ctx_t *term = NULL;
    res_t res = ik_term_init_with_fd(ctx, NULL, pty.slave_fd, &term);

    cfg.done = 1;
    pthread_join(sim_thread, NULL);

    ck_assert_msg(is_ok(&res), "Expected success");
    ck_assert_ptr_nonnull(term);
    ck_assert_msg(term->csi_u_supported, "CSI u should be supported");

    // Cleanup should write CSI u disable sequence
    ik_term_cleanup(term);

    // Read what cleanup wrote to master
    char buf[256];
    usleep(10000);  // Small delay for data to be available
    ssize_t n = read(pty.master_fd, buf, sizeof(buf) - 1);
    if (n > 0) {
        buf[n] = '\0';
        // Should contain CSI u disable sequence (ESC[<u)
        ck_assert_msg(strstr(buf, "\x1b[<u") != NULL, "Cleanup should write CSI u disable sequence");
    }

    talloc_free(ctx);
    close_pty_pair(&pty);
}
END_TEST

// ============================================================================
// Test suite setup
// ============================================================================
static Suite *terminal_pty_suite(void)
{
    Suite *s = suite_create("Terminal PTY");

    TCase *tc_basic = tcase_create("Basic");
    tcase_set_timeout(tc_basic, IK_TEST_TIMEOUT);
    tcase_add_test(tc_basic, test_pty_init_success);
    tcase_add_test(tc_basic, test_pty_get_size);
    tcase_add_test(tc_basic, test_pty_cleanup_null_safe);
    tcase_add_test(tc_basic, test_pty_cleanup_no_csi_u);
    suite_add_tcase(s, tc_basic);

    TCase *tc_cleanup = tcase_create("Cleanup");
    tcase_set_timeout(tc_cleanup, IK_TEST_TIMEOUT);
    tcase_add_test(tc_cleanup, test_pty_cleanup_csi_u_disable);
    suite_add_tcase(s, tc_cleanup);

    return s;
}

int main(void)
{
    Suite *s = terminal_pty_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    ik_test_reset_terminal();

    return (number_failed == 0) ? 0 : 1;
}