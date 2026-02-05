// Terminal module PTY-based CSI u enable edge case tests
// Tests edge cases and boundary conditions for CSI u enable using real pseudo-terminals

#include "terminal_pty_helper.h"

#include <check.h>
#include <pthread.h>
#include <stdlib.h>
#include <talloc.h>
#include <unistd.h>

#include "shared/error.h"
#include "shared/logger.h"
#include "shared/terminal.h"
#include "tests/helpers/test_utils_helper.h"

// ============================================================================
// Test: CSI u enable response missing ESC prefix (covers line 116 short-circuit)
// ============================================================================
START_TEST(test_pty_csi_u_enable_missing_esc) {
    pty_pair_t pty;
    ck_assert_int_eq(create_pty_pair(&pty), 0);
    ck_assert_int_eq(pty_set_size(&pty, 24, 80), 0);

    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    ik_test_set_log_dir(__FILE__);
    ik_logger_t *logger = ik_logger_create(ctx, "/tmp");

    term_sim_config_t cfg = {
        .master_fd = pty.master_fd,
        .probe_response = "\x1b[?1u",    // Valid probe response
        .enable_response = "[?9u",       // Missing ESC - covers buf[0] != '\x1b'
        .done = 0
    };

    pthread_t sim_thread;
    ck_assert_int_eq(pthread_create(&sim_thread, NULL, term_simulator_thread, &cfg), 0);

    ik_term_ctx_t *term = NULL;
    res_t res = ik_term_init_with_fd(ctx, logger, pty.slave_fd, &term);

    cfg.done = 1;
    pthread_join(sim_thread, NULL);

    ck_assert_msg(is_ok(&res), "Expected success");
    ck_assert_ptr_nonnull(term);
    // Still returns true (unexpected response handled gracefully)
    ck_assert_msg(term->csi_u_supported, "CSI u should be supported with unexpected response");

    ik_term_cleanup(term);
    talloc_free(ctx);
    close_pty_pair(&pty);
}
END_TEST

// ============================================================================
// Test: CSI u enable response missing '[' (covers line 116 short-circuit)
// ============================================================================
START_TEST(test_pty_csi_u_enable_missing_bracket) {
    pty_pair_t pty;
    ck_assert_int_eq(create_pty_pair(&pty), 0);
    ck_assert_int_eq(pty_set_size(&pty, 24, 80), 0);

    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    ik_test_set_log_dir(__FILE__);
    ik_logger_t *logger = ik_logger_create(ctx, "/tmp");

    term_sim_config_t cfg = {
        .master_fd = pty.master_fd,
        .probe_response = "\x1b[?1u",    // Valid probe response
        .enable_response = "\x1b?9u",    // Missing '[' - covers buf[1] != '['
        .done = 0
    };

    pthread_t sim_thread;
    ck_assert_int_eq(pthread_create(&sim_thread, NULL, term_simulator_thread, &cfg), 0);

    ik_term_ctx_t *term = NULL;
    res_t res = ik_term_init_with_fd(ctx, logger, pty.slave_fd, &term);

    cfg.done = 1;
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
// Test: CSI u enable response missing '?' (covers line 116 short-circuit)
// ============================================================================
START_TEST(test_pty_csi_u_enable_missing_question) {
    pty_pair_t pty;
    ck_assert_int_eq(create_pty_pair(&pty), 0);
    ck_assert_int_eq(pty_set_size(&pty, 24, 80), 0);

    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    ik_test_set_log_dir(__FILE__);
    ik_logger_t *logger = ik_logger_create(ctx, "/tmp");

    term_sim_config_t cfg = {
        .master_fd = pty.master_fd,
        .probe_response = "\x1b[?1u",    // Valid probe response
        .enable_response = "\x1b[9u",    // Missing '?' - covers buf[2] != '?'
        .done = 0
    };

    pthread_t sim_thread;
    ck_assert_int_eq(pthread_create(&sim_thread, NULL, term_simulator_thread, &cfg), 0);

    ik_term_ctx_t *term = NULL;
    res_t res = ik_term_init_with_fd(ctx, logger, pty.slave_fd, &term);

    cfg.done = 1;
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
// Test: CSI u enable response with non-digit character in flags (covers line 133)
// ============================================================================
START_TEST(test_pty_csi_u_enable_non_digit_in_flags) {
    pty_pair_t pty;
    ck_assert_int_eq(create_pty_pair(&pty), 0);
    ck_assert_int_eq(pty_set_size(&pty, 24, 80), 0);

    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    ik_test_set_log_dir(__FILE__);
    ik_logger_t *logger = ik_logger_create(ctx, "/tmp");

    term_sim_config_t cfg = {
        .master_fd = pty.master_fd,
        .probe_response = "\x1b[?1u",    // Valid probe response
        .enable_response = "\x1b[?9xu",  // 'x' is non-digit before 'u' - covers else branch
        .done = 0
    };

    pthread_t sim_thread;
    ck_assert_int_eq(pthread_create(&sim_thread, NULL, term_simulator_thread, &cfg), 0);

    ik_term_ctx_t *term = NULL;
    res_t res = ik_term_init_with_fd(ctx, logger, pty.slave_fd, &term);

    cfg.done = 1;
    pthread_join(sim_thread, NULL);

    ck_assert_msg(is_ok(&res), "Expected success");
    ck_assert_ptr_nonnull(term);
    ck_assert_msg(term->csi_u_supported, "CSI u should be supported");

    ik_term_cleanup(term);
    talloc_free(ctx);
    close_pty_pair(&pty);
}
END_TEST

// ============================================================================
// Test: CSI u enable with too short response (< 4 bytes) - covers n >= 4 branch
// ============================================================================
START_TEST(test_pty_csi_u_enable_short_response) {
    pty_pair_t pty;
    ck_assert_int_eq(create_pty_pair(&pty), 0);
    ck_assert_int_eq(pty_set_size(&pty, 24, 80), 0);

    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    ik_test_set_log_dir(__FILE__);
    ik_logger_t *logger = ik_logger_create(ctx, "/tmp");

    term_sim_config_t cfg = {
        .master_fd = pty.master_fd,
        .probe_response = "\x1b[?1u",    // Valid probe response
        .enable_response = "\x1b[",      // Too short (< 4 bytes) - covers n >= 4 false
        .done = 0
    };

    pthread_t sim_thread;
    ck_assert_int_eq(pthread_create(&sim_thread, NULL, term_simulator_thread, &cfg), 0);

    ik_term_ctx_t *term = NULL;
    res_t res = ik_term_init_with_fd(ctx, logger, pty.slave_fd, &term);

    cfg.done = 1;
    pthread_join(sim_thread, NULL);

    ck_assert_msg(is_ok(&res), "Expected success");
    ck_assert_ptr_nonnull(term);
    // Still returns true (unexpected response handled gracefully)
    ck_assert_msg(term->csi_u_supported, "CSI u should be supported with short response");

    ik_term_cleanup(term);
    talloc_free(ctx);
    close_pty_pair(&pty);
}
END_TEST

// ============================================================================
// Test: CSI u enable response with no 'u' terminator (covers line 133 loop exit)
// ============================================================================
START_TEST(test_pty_csi_u_enable_no_terminator) {
    pty_pair_t pty;
    ck_assert_int_eq(create_pty_pair(&pty), 0);
    ck_assert_int_eq(pty_set_size(&pty, 24, 80), 0);

    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    ik_test_set_log_dir(__FILE__);
    ik_logger_t *logger = ik_logger_create(ctx, "/tmp");

    term_sim_config_t cfg = {
        .master_fd = pty.master_fd,
        .probe_response = "\x1b[?1u",    // Valid probe response
        .enable_response = "\x1b[?123",  // Missing 'u' terminator
        .done = 0
    };

    pthread_t sim_thread;
    ck_assert_int_eq(pthread_create(&sim_thread, NULL, term_simulator_thread, &cfg), 0);

    ik_term_ctx_t *term = NULL;
    res_t res = ik_term_init_with_fd(ctx, logger, pty.slave_fd, &term);

    cfg.done = 1;
    pthread_join(sim_thread, NULL);

    ck_assert_msg(is_ok(&res), "Expected success");
    ck_assert_ptr_nonnull(term);
    ck_assert_msg(term->csi_u_supported, "CSI u should be supported");

    ik_term_cleanup(term);
    talloc_free(ctx);
    close_pty_pair(&pty);
}
END_TEST

// ============================================================================
// Test: CSI u enable with long unexpected response (>32 bytes) - covers line 149 loop
// ============================================================================
START_TEST(test_pty_csi_u_enable_long_unexpected_response) {
    pty_pair_t pty;
    ck_assert_int_eq(create_pty_pair(&pty), 0);
    ck_assert_int_eq(pty_set_size(&pty, 24, 80), 0);

    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    ik_test_set_log_dir(__FILE__);
    ik_logger_t *logger = ik_logger_create(ctx, "/tmp");

    // Response longer than 32 bytes to hit the i < 32 loop bound
    term_sim_config_t cfg = {
        .master_fd = pty.master_fd,
        .probe_response = "\x1b[?1u",    // Valid probe response
        .enable_response = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789abcd",  // 40 bytes
        .done = 0
    };

    pthread_t sim_thread;
    ck_assert_int_eq(pthread_create(&sim_thread, NULL, term_simulator_thread, &cfg), 0);

    ik_term_ctx_t *term = NULL;
    res_t res = ik_term_init_with_fd(ctx, logger, pty.slave_fd, &term);

    cfg.done = 1;
    pthread_join(sim_thread, NULL);

    ck_assert_msg(is_ok(&res), "Expected success");
    ck_assert_ptr_nonnull(term);
    ck_assert_msg(term->csi_u_supported, "CSI u should be supported");

    ik_term_cleanup(term);
    talloc_free(ctx);
    close_pty_pair(&pty);
}
END_TEST

// ============================================================================
// Test: CSI u enable response with character > '9' in flags (covers line 133)
// ============================================================================
START_TEST(test_pty_csi_u_enable_char_above_nine) {
    pty_pair_t pty;
    ck_assert_int_eq(create_pty_pair(&pty), 0);
    ck_assert_int_eq(pty_set_size(&pty, 24, 80), 0);

    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    ik_test_set_log_dir(__FILE__);
    ik_logger_t *logger = ik_logger_create(ctx, "/tmp");

    // ':' is ASCII 58, which is > '9' (ASCII 57) but also > '0' (ASCII 48)
    // This tests the buf[i] <= '9' false branch specifically
    term_sim_config_t cfg = {
        .master_fd = pty.master_fd,
        .probe_response = "\x1b[?1u",    // Valid probe response
        .enable_response = "\x1b[?9:u",  // ':' is > '9', tests the <= '9' branch
        .done = 0
    };

    pthread_t sim_thread;
    ck_assert_int_eq(pthread_create(&sim_thread, NULL, term_simulator_thread, &cfg), 0);

    ik_term_ctx_t *term = NULL;
    res_t res = ik_term_init_with_fd(ctx, logger, pty.slave_fd, &term);

    cfg.done = 1;
    pthread_join(sim_thread, NULL);

    ck_assert_msg(is_ok(&res), "Expected success");
    ck_assert_ptr_nonnull(term);
    ck_assert_msg(term->csi_u_supported, "CSI u should be supported");

    ik_term_cleanup(term);
    talloc_free(ctx);
    close_pty_pair(&pty);
}
END_TEST

// ============================================================================
// Test: CSI u enable response with character < '0' in flags (covers line 133)
// ============================================================================
START_TEST(test_pty_csi_u_enable_char_below_zero) {
    pty_pair_t pty;
    ck_assert_int_eq(create_pty_pair(&pty), 0);
    ck_assert_int_eq(pty_set_size(&pty, 24, 80), 0);

    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    ik_test_set_log_dir(__FILE__);
    ik_logger_t *logger = ik_logger_create(ctx, "/tmp");

    // Space (ASCII 32) is < '0' (ASCII 48)
    // This tests the buf[i] >= '0' false branch specifically
    term_sim_config_t cfg = {
        .master_fd = pty.master_fd,
        .probe_response = "\x1b[?1u",    // Valid probe response
        .enable_response = "\x1b[?9 u",  // Space is < '0', tests the >= '0' branch
        .done = 0
    };

    pthread_t sim_thread;
    ck_assert_int_eq(pthread_create(&sim_thread, NULL, term_simulator_thread, &cfg), 0);

    ik_term_ctx_t *term = NULL;
    res_t res = ik_term_init_with_fd(ctx, logger, pty.slave_fd, &term);

    cfg.done = 1;
    pthread_join(sim_thread, NULL);

    ck_assert_msg(is_ok(&res), "Expected success");
    ck_assert_ptr_nonnull(term);
    ck_assert_msg(term->csi_u_supported, "CSI u should be supported");

    ik_term_cleanup(term);
    talloc_free(ctx);
    close_pty_pair(&pty);
}
END_TEST

// ============================================================================
// Test suite setup
// ============================================================================
static Suite *terminal_pty_enable_edge_suite(void)
{
    Suite *s = suite_create("Terminal PTY CSI u Enable Edge Cases");

    TCase *tc_enable_edge = tcase_create("CSI u Enable Edge Cases");
    tcase_set_timeout(tc_enable_edge, IK_TEST_TIMEOUT);
    tcase_add_test(tc_enable_edge, test_pty_csi_u_enable_missing_esc);
    tcase_add_test(tc_enable_edge, test_pty_csi_u_enable_missing_bracket);
    tcase_add_test(tc_enable_edge, test_pty_csi_u_enable_missing_question);
    tcase_add_test(tc_enable_edge, test_pty_csi_u_enable_non_digit_in_flags);
    tcase_add_test(tc_enable_edge, test_pty_csi_u_enable_short_response);
    tcase_add_test(tc_enable_edge, test_pty_csi_u_enable_no_terminator);
    tcase_add_test(tc_enable_edge, test_pty_csi_u_enable_long_unexpected_response);
    tcase_add_test(tc_enable_edge, test_pty_csi_u_enable_char_above_nine);
    tcase_add_test(tc_enable_edge, test_pty_csi_u_enable_char_below_zero);
    suite_add_tcase(s, tc_enable_edge);

    return s;
}

int main(void)
{
    Suite *s = terminal_pty_enable_edge_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/shared/terminal/terminal_pty_enable_edge_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    ik_test_reset_terminal();

    return (number_failed == 0) ? 0 : 1;
}
