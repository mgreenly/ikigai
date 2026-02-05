// Terminal module PTY-based CSI u probe tests
// Tests CSI u probe functionality using real pseudo-terminals

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
// Test: CSI u probe with valid response - terminal supports CSI u
// ============================================================================
START_TEST(test_pty_csi_u_probe_valid_response) {
    pty_pair_t pty;
    ck_assert_int_eq(create_pty_pair(&pty), 0);
    ck_assert_int_eq(pty_set_size(&pty, 24, 80), 0);

    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    // Configure and start terminal simulator
    term_sim_config_t cfg = {
        .master_fd = pty.master_fd,
        .probe_response = "\x1b[?1u",    // Valid probe response
        .enable_response = "\x1b[?9u",   // Valid enable response
        .probe_delay_ms = 0,
        .enable_delay_ms = 0,
        .done = 0
    };

    pthread_t sim_thread;
    ck_assert_int_eq(pthread_create(&sim_thread, NULL, term_simulator_thread, &cfg), 0);

    ik_term_ctx_t *term = NULL;
    res_t res = ik_term_init_with_fd(ctx, NULL, pty.slave_fd, &term);

    cfg.done = 1;
    pthread_join(sim_thread, NULL);

    ck_assert_msg(is_ok(&res), "Expected success, got error");
    ck_assert_ptr_nonnull(term);

    // CSI u should be detected as supported
    ck_assert_msg(term->csi_u_supported, "CSI u should be detected as supported");

    ik_term_cleanup(term);
    talloc_free(ctx);
    close_pty_pair(&pty);
}
END_TEST

// ============================================================================
// Test: CSI u probe with invalid response format (no 'u' terminator)
// ============================================================================
START_TEST(test_pty_csi_u_probe_invalid_no_terminator) {
    pty_pair_t pty;
    ck_assert_int_eq(create_pty_pair(&pty), 0);
    ck_assert_int_eq(pty_set_size(&pty, 24, 80), 0);

    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    term_sim_config_t cfg = {
        .master_fd = pty.master_fd,
        .probe_response = "\x1b[?123",   // Missing 'u' terminator
        .enable_response = NULL,
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

    // CSI u should NOT be supported due to invalid response
    ck_assert_msg(!term->csi_u_supported, "CSI u should not be supported with invalid response");

    ik_term_cleanup(term);
    talloc_free(ctx);
    close_pty_pair(&pty);
}
END_TEST

// ============================================================================
// Test: CSI u probe with too short response (< 4 bytes)
// ============================================================================
START_TEST(test_pty_csi_u_probe_short_response) {
    pty_pair_t pty;
    ck_assert_int_eq(create_pty_pair(&pty), 0);
    ck_assert_int_eq(pty_set_size(&pty, 24, 80), 0);

    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    term_sim_config_t cfg = {
        .master_fd = pty.master_fd,
        .probe_response = "\x1b[",   // Too short
        .enable_response = NULL,
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
    ck_assert_msg(!term->csi_u_supported, "CSI u should not be supported with short response");

    ik_term_cleanup(term);
    talloc_free(ctx);
    close_pty_pair(&pty);
}
END_TEST

// ============================================================================
// Test: CSI u probe with response missing ESC prefix
// ============================================================================
START_TEST(test_pty_csi_u_probe_missing_esc) {
    pty_pair_t pty;
    ck_assert_int_eq(create_pty_pair(&pty), 0);
    ck_assert_int_eq(pty_set_size(&pty, 24, 80), 0);

    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    term_sim_config_t cfg = {
        .master_fd = pty.master_fd,
        .probe_response = "[?0u",   // Missing ESC
        .enable_response = NULL,
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
    ck_assert_msg(!term->csi_u_supported, "CSI u should not be supported without ESC prefix");

    ik_term_cleanup(term);
    talloc_free(ctx);
    close_pty_pair(&pty);
}
END_TEST

// ============================================================================
// Test: CSI u probe with response missing '[' after ESC
// ============================================================================
START_TEST(test_pty_csi_u_probe_missing_bracket) {
    pty_pair_t pty;
    ck_assert_int_eq(create_pty_pair(&pty), 0);
    ck_assert_int_eq(pty_set_size(&pty, 24, 80), 0);

    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    term_sim_config_t cfg = {
        .master_fd = pty.master_fd,
        .probe_response = "\x1b?0u",   // Missing '['
        .enable_response = NULL,
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
    ck_assert_msg(!term->csi_u_supported, "CSI u should not be supported without bracket");

    ik_term_cleanup(term);
    talloc_free(ctx);
    close_pty_pair(&pty);
}
END_TEST

// ============================================================================
// Test: CSI u probe with response missing '?' after '['
// ============================================================================
START_TEST(test_pty_csi_u_probe_missing_question) {
    pty_pair_t pty;
    ck_assert_int_eq(create_pty_pair(&pty), 0);
    ck_assert_int_eq(pty_set_size(&pty, 24, 80), 0);

    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    term_sim_config_t cfg = {
        .master_fd = pty.master_fd,
        .probe_response = "\x1b[0u",   // Missing '?'
        .enable_response = NULL,
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
    ck_assert_msg(!term->csi_u_supported, "CSI u should not be supported without question mark");

    ik_term_cleanup(term);
    talloc_free(ctx);
    close_pty_pair(&pty);
}
END_TEST

// ============================================================================
// Test: CSI u probe select timeout (no response at all)
// ============================================================================
START_TEST(test_pty_csi_u_probe_timeout) {
    pty_pair_t pty;
    ck_assert_int_eq(create_pty_pair(&pty), 0);
    ck_assert_int_eq(pty_set_size(&pty, 24, 80), 0);

    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    // No simulator thread - probe will timeout
    ik_term_ctx_t *term = NULL;
    res_t res = ik_term_init_with_fd(ctx, NULL, pty.slave_fd, &term);

    // Should succeed even with probe timeout
    ck_assert_msg(is_ok(&res), "Expected success");
    ck_assert_ptr_nonnull(term);

    // CSI u should not be supported (probe timed out)
    ck_assert_msg(!term->csi_u_supported, "CSI u should not be supported after timeout");

    ik_term_cleanup(term);
    talloc_free(ctx);
    close_pty_pair(&pty);
}
END_TEST

// ============================================================================
// Test: CSI u probe with multi-digit flags
// ============================================================================
START_TEST(test_pty_csi_u_probe_multi_digit_flags) {
    pty_pair_t pty;
    ck_assert_int_eq(create_pty_pair(&pty), 0);
    ck_assert_int_eq(pty_set_size(&pty, 24, 80), 0);

    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    ik_test_set_log_dir(__FILE__);
    ik_logger_t *logger = ik_logger_create(ctx, "/tmp");

    term_sim_config_t cfg = {
        .master_fd = pty.master_fd,
        .probe_response = "\x1b[?15u",   // Multi-digit flags
        .enable_response = "\x1b[?123u", // Three-digit flags
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
    ck_assert_msg(term->csi_u_supported, "CSI u should be supported with multi-digit flags");

    ik_term_cleanup(term);
    talloc_free(ctx);
    close_pty_pair(&pty);
}
END_TEST

// ============================================================================
// Test suite setup
// ============================================================================
static Suite *terminal_pty_probe_suite(void)
{
    Suite *s = suite_create("Terminal PTY CSI u Probe");

    TCase *tc_probe = tcase_create("CSI u Probe");
    tcase_set_timeout(tc_probe, IK_TEST_TIMEOUT);
    tcase_add_test(tc_probe, test_pty_csi_u_probe_valid_response);
    tcase_add_test(tc_probe, test_pty_csi_u_probe_invalid_no_terminator);
    tcase_add_test(tc_probe, test_pty_csi_u_probe_short_response);
    tcase_add_test(tc_probe, test_pty_csi_u_probe_missing_esc);
    tcase_add_test(tc_probe, test_pty_csi_u_probe_missing_bracket);
    tcase_add_test(tc_probe, test_pty_csi_u_probe_missing_question);
    tcase_add_test(tc_probe, test_pty_csi_u_probe_timeout);
    tcase_add_test(tc_probe, test_pty_csi_u_probe_multi_digit_flags);
    suite_add_tcase(s, tc_probe);

    return s;
}

int main(void)
{
    Suite *s = terminal_pty_probe_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/shared/terminal/terminal_pty_probe_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    ik_test_reset_terminal();

    return (number_failed == 0) ? 0 : 1;
}
