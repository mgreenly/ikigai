// Terminal module PTY-based CSI u enable basic tests
// Tests basic CSI u enable functionality using real pseudo-terminals

#include "terminal_pty_helper.h"

#include <check.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <talloc.h>
#include <unistd.h>

#include "../../../src/error.h"
#include "../../../src/logger.h"
#include "../../../src/terminal.h"
#include "../../test_utils_helper.h"

// ============================================================================
// Test: CSI u enable with no response (normal for some terminals)
// ============================================================================
START_TEST(test_pty_csi_u_enable_no_response) {
    pty_pair_t pty;
    ck_assert_int_eq(create_pty_pair(&pty), 0);
    ck_assert_int_eq(pty_set_size(&pty, 24, 80), 0);

    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    term_sim_config_t cfg = {
        .master_fd = pty.master_fd,
        .probe_response = "\x1b[?1u",    // Valid probe response
        .enable_response = NULL,         // No enable response (normal for some terminals)
        .done = 0
    };

    pthread_t sim_thread;
    ck_assert_int_eq(pthread_create(&sim_thread, NULL, term_simulator_thread, &cfg), 0);

    ik_term_ctx_t *term = NULL;
    res_t res = ik_term_init_with_fd(ctx, NULL, pty.slave_fd, &term);

    atomic_store(&cfg.done, 1);
    pthread_join(sim_thread, NULL);

    ck_assert_msg(is_ok(&res), "Expected success");
    ck_assert_ptr_nonnull(term);

    // CSI u should still be marked as supported (enable returned true on timeout)
    ck_assert_msg(term->csi_u_supported, "CSI u should be supported even without enable response");

    ik_term_cleanup(term);
    talloc_free(ctx);
    close_pty_pair(&pty);
}
END_TEST

// ============================================================================
// Test: CSI u enable with unexpected response format (still succeeds)
// ============================================================================
START_TEST(test_pty_csi_u_enable_unexpected_response) {
    pty_pair_t pty;
    ck_assert_int_eq(create_pty_pair(&pty), 0);
    ck_assert_int_eq(pty_set_size(&pty, 24, 80), 0);

    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    // Create a logger for this test to cover the logging path
    ik_test_set_log_dir(__FILE__);
    ik_logger_t *logger = ik_logger_create(ctx, "/tmp");

    term_sim_config_t cfg = {
        .master_fd = pty.master_fd,
        .probe_response = "\x1b[?1u",    // Valid probe response
        .enable_response = "UNEXPECTED",  // Garbage response
        .done = 0
    };

    pthread_t sim_thread;
    ck_assert_int_eq(pthread_create(&sim_thread, NULL, term_simulator_thread, &cfg), 0);

    ik_term_ctx_t *term = NULL;
    res_t res = ik_term_init_with_fd(ctx, logger, pty.slave_fd, &term);

    atomic_store(&cfg.done, 1);
    pthread_join(sim_thread, NULL);

    ck_assert_msg(is_ok(&res), "Expected success");
    ck_assert_ptr_nonnull(term);

    // CSI u should still be supported (unexpected response still returns true)
    ck_assert_msg(term->csi_u_supported, "CSI u should be supported with unexpected response");

    ik_term_cleanup(term);
    talloc_free(ctx);
    close_pty_pair(&pty);
}
END_TEST

// ============================================================================
// Test: CSI u enable with valid response and flags parsing
// ============================================================================
START_TEST(test_pty_csi_u_enable_valid_flags) {
    pty_pair_t pty;
    ck_assert_int_eq(create_pty_pair(&pty), 0);
    ck_assert_int_eq(pty_set_size(&pty, 24, 80), 0);

    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    // Create logger to test JSON logging path
    ik_test_set_log_dir(__FILE__);
    ik_logger_t *logger = ik_logger_create(ctx, "/tmp");

    term_sim_config_t cfg = {
        .master_fd = pty.master_fd,
        .probe_response = "\x1b[?1u",    // Valid probe response
        .enable_response = "\x1b[?9u",   // Valid enable response with flags=9
        .done = 0
    };

    pthread_t sim_thread;
    ck_assert_int_eq(pthread_create(&sim_thread, NULL, term_simulator_thread, &cfg), 0);

    ik_term_ctx_t *term = NULL;
    res_t res = ik_term_init_with_fd(ctx, logger, pty.slave_fd, &term);

    atomic_store(&cfg.done, 1);
    pthread_join(sim_thread, NULL);

    ck_assert_msg(is_ok(&res), "Expected success");
    ck_assert_ptr_nonnull(term);
    ck_assert_msg(term->csi_u_supported, "CSI u should be supported with valid flags response");

    ik_term_cleanup(term);
    talloc_free(ctx);
    close_pty_pair(&pty);
}
END_TEST

// ============================================================================
// Test: CSI u enable with unexpected response and NO logger (covers line 142)
// ============================================================================
START_TEST(test_pty_csi_u_enable_unexpected_no_logger) {
    pty_pair_t pty;
    ck_assert_int_eq(create_pty_pair(&pty), 0);
    ck_assert_int_eq(pty_set_size(&pty, 24, 80), 0);

    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    // No logger - tests the logger == NULL branch in unexpected response path
    term_sim_config_t cfg = {
        .master_fd = pty.master_fd,
        .probe_response = "\x1b[?1u",    // Valid probe response
        .enable_response = "GARBAGE",     // Unexpected format triggers line 142
        .done = 0
    };

    pthread_t sim_thread;
    ck_assert_int_eq(pthread_create(&sim_thread, NULL, term_simulator_thread, &cfg), 0);

    ik_term_ctx_t *term = NULL;
    res_t res = ik_term_init_with_fd(ctx, NULL, pty.slave_fd, &term);  // NULL logger

    atomic_store(&cfg.done, 1);
    pthread_join(sim_thread, NULL);

    ck_assert_msg(is_ok(&res), "Expected success");
    ck_assert_ptr_nonnull(term);
    ck_assert_msg(term->csi_u_supported, "CSI u should be supported with unexpected response");

    ik_term_cleanup(term);
    talloc_free(ctx);
    close_pty_pair(&pty);
}
END_TEST

// ============================================================================
// Test suite setup
// ============================================================================
static Suite *terminal_pty_enable_basic_suite(void)
{
    Suite *s = suite_create("Terminal PTY CSI u Enable Basic");

    TCase *tc_enable_basic = tcase_create("CSI u Enable Basic");
    tcase_set_timeout(tc_enable_basic, IK_TEST_TIMEOUT);
    tcase_add_test(tc_enable_basic, test_pty_csi_u_enable_no_response);
    tcase_add_test(tc_enable_basic, test_pty_csi_u_enable_unexpected_response);
    tcase_add_test(tc_enable_basic, test_pty_csi_u_enable_valid_flags);
    tcase_add_test(tc_enable_basic, test_pty_csi_u_enable_unexpected_no_logger);
    suite_add_tcase(s, tc_enable_basic);

    return s;
}

int main(void)
{
    Suite *s = terminal_pty_enable_basic_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/terminal/terminal_pty_enable_basic_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    ik_test_reset_terminal();

    return (number_failed == 0) ? 0 : 1;
}
