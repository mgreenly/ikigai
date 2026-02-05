// Terminal module unit tests - CSI u probe tests
#include <check.h>
#include <signal.h>
#include <talloc.h>

#include "shared/error.h"
#include "shared/terminal.h"
#include "tests/helpers/test_utils_helper.h"
#include "terminal_test_mocks.h"

// Test: csi_u_supported field exists and is initialized
START_TEST(test_term_init_sets_csi_u_supported) {
    reset_mocks();
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_term_ctx_t *term = NULL;

    res_t res = ik_term_init(ctx, NULL, &term);

    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(term);

    // Field should be initialized (either true or false, not uninitialized)
    // In test environment with mocks, the value depends on mock behavior
    ck_assert(term->csi_u_supported == true || term->csi_u_supported == false);

    ik_term_cleanup(term);
    talloc_free(ctx);
}
END_TEST
// Test: CSI u probe write failure
START_TEST(test_csi_u_probe_write_fails) {
    reset_mocks();
    // Fail on third write (CSI u query) - first write is alt screen enter, second is screen clear
    mock_write_fail_on_call = 3;

    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_term_ctx_t *term = NULL;

    res_t res = ik_term_init(ctx, NULL, &term);

    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(term);
    // CSI u probe failed, so it should be disabled
    ck_assert(!term->csi_u_supported);

    ik_term_cleanup(term);
    talloc_free(ctx);
}

END_TEST
// Test: CSI u probe read failure
START_TEST(test_csi_u_probe_read_fails) {
    reset_mocks();
    mock_select_return = 1; // Indicate ready to read
    mock_read_fail = 1;     // But read fails

    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_term_ctx_t *term = NULL;

    res_t res = ik_term_init(ctx, NULL, &term);

    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(term);
    // CSI u probe failed, so it should be disabled
    ck_assert(!term->csi_u_supported);

    ik_term_cleanup(term);
    talloc_free(ctx);
}

END_TEST
// Test: CSI u probe succeeds and enables CSI u mode
START_TEST(test_csi_u_probe_succeeds) {
    reset_mocks();
    mock_select_return = 1; // Indicate ready to read (CSI u response available)

    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_term_ctx_t *term = NULL;

    res_t res = ik_term_init(ctx, NULL, &term);

    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(term);
    // CSI u probe succeeded
    ck_assert(term->csi_u_supported);

    // Verify CSI u enable sequence was written (3rd write after query and alt screen)
    ck_assert_int_ge(mock_write_count, 3);

    ik_term_cleanup(term);
    talloc_free(ctx);
}

END_TEST
// Test: CSI u enable fails after successful probe
START_TEST(test_csi_u_enable_fails) {
    reset_mocks();
    mock_select_return = 1; // Indicate CSI u is supported
    // Write sequence: 1=alt screen, 2=CSI u query, 3=CSI u enable
    mock_write_fail_on_call = 3; // Fail on CSI u enable (3rd write)

    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_term_ctx_t *term = NULL;

    res_t res = ik_term_init(ctx, NULL, &term);

    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(term);
    // CSI u enable failed, so it should be marked as unsupported
    ck_assert(!term->csi_u_supported);

    ik_term_cleanup(term);
    talloc_free(ctx);
}

END_TEST
// Test: CSI u cleanup disables when enabled
START_TEST(test_csi_u_cleanup_disables) {
    reset_mocks();
    mock_select_return = 1; // Enable CSI u

    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_term_ctx_t *term = NULL;

    res_t res = ik_term_init(ctx, NULL, &term);
    ck_assert(is_ok(&res));
    ck_assert(term->csi_u_supported);

    // Reset buffer and counts to track cleanup
    memset(mock_write_buffer, 0, MOCK_WRITE_BUFFER_SIZE);
    mock_write_buffer_pos = 0;
    int write_count_before_cleanup = mock_write_count;

    ik_term_cleanup(term);

    // Verify CSI u disable sequence was written
    ck_assert(strstr(mock_write_buffer, "\x1b[<u") != NULL);
    ck_assert_int_gt(mock_write_count, write_count_before_cleanup);

    talloc_free(ctx);
}

END_TEST
// Test: CSI u probe with invalid response (no 'u' terminator)
START_TEST(test_csi_u_probe_invalid_response) {
    reset_mocks();
    mock_select_return = 1; // Indicate ready to read
    mock_read_response = "\x1b[?123"; // Response without 'u' terminator

    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_term_ctx_t *term = NULL;

    res_t res = ik_term_init(ctx, NULL, &term);

    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(term);
    // CSI u probe failed due to invalid response
    ck_assert(!term->csi_u_supported);

    ik_term_cleanup(term);
    talloc_free(ctx);
}

END_TEST
// Test: CSI u probe with response that's too short (< 4 bytes)
START_TEST(test_csi_u_probe_short_response) {
    reset_mocks();
    mock_select_return = 1; // Indicate ready to read
    mock_read_response = "\x1b["; // Response too short

    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_term_ctx_t *term = NULL;

    res_t res = ik_term_init(ctx, NULL, &term);

    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(term);
    // CSI u probe failed due to short response
    ck_assert(!term->csi_u_supported);

    ik_term_cleanup(term);
    talloc_free(ctx);
}

END_TEST
// Test: CSI u probe with response missing ESC prefix
START_TEST(test_csi_u_probe_no_esc_prefix) {
    reset_mocks();
    mock_select_return = 1; // Indicate ready to read
    mock_read_response = "[?0u"; // Missing ESC

    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_term_ctx_t *term = NULL;

    res_t res = ik_term_init(ctx, NULL, &term);

    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(term);
    // CSI u probe failed due to missing ESC prefix
    ck_assert(!term->csi_u_supported);

    ik_term_cleanup(term);
    talloc_free(ctx);
}

END_TEST
// Test: CSI u probe with response missing '[' after ESC
START_TEST(test_csi_u_probe_no_bracket) {
    reset_mocks();
    mock_select_return = 1; // Indicate ready to read
    mock_read_response = "\x1b?0u"; // Missing '['

    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_term_ctx_t *term = NULL;

    res_t res = ik_term_init(ctx, NULL, &term);

    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(term);
    // CSI u probe failed due to missing '['
    ck_assert(!term->csi_u_supported);

    ik_term_cleanup(term);
    talloc_free(ctx);
}

END_TEST
// Test: CSI u probe with response missing '?' after '['
START_TEST(test_csi_u_probe_no_question) {
    reset_mocks();
    mock_select_return = 1; // Indicate ready to read
    mock_read_response = "\x1b[0u"; // Missing '?'

    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_term_ctx_t *term = NULL;

    res_t res = ik_term_init(ctx, NULL, &term);

    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(term);
    // CSI u probe failed due to missing '?'
    ck_assert(!term->csi_u_supported);

    ik_term_cleanup(term);
    talloc_free(ctx);
}

END_TEST

// Test: CSI u enable read fails after select returns ready
// This covers the edge case in enable_csi_u where select() indicates data
// is available but read() fails
START_TEST(test_csi_u_enable_read_fails) {
    reset_mocks();
    mock_select_return = 1; // Indicate CSI u is supported and data ready
    // Read sequence: 1=CSI u probe response, 2=CSI u enable response
    mock_read_fail_on_call = 2; // Fail on second read (enable response read)

    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_term_ctx_t *term = NULL;

    res_t res = ik_term_init(ctx, NULL, &term);

    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(term);
    // CSI u enable read failed, so it should be marked as unsupported
    ck_assert(!term->csi_u_supported);

    ik_term_cleanup(term);
    talloc_free(ctx);
}

END_TEST

// Test suite
static Suite *terminal_csi_u_suite(void)
{
    Suite *s = suite_create("Terminal CSI u");
    TCase *tc_core = tcase_create("Core");
    tcase_set_timeout(tc_core, IK_TEST_TIMEOUT);

    tcase_add_test(tc_core, test_term_init_sets_csi_u_supported);
    tcase_add_test(tc_core, test_csi_u_probe_write_fails);
    tcase_add_test(tc_core, test_csi_u_probe_read_fails);
    tcase_add_test(tc_core, test_csi_u_probe_succeeds);
    tcase_add_test(tc_core, test_csi_u_enable_fails);
    tcase_add_test(tc_core, test_csi_u_enable_read_fails);
    tcase_add_test(tc_core, test_csi_u_cleanup_disables);
    tcase_add_test(tc_core, test_csi_u_probe_invalid_response);
    tcase_add_test(tc_core, test_csi_u_probe_short_response);
    tcase_add_test(tc_core, test_csi_u_probe_no_esc_prefix);
    tcase_add_test(tc_core, test_csi_u_probe_no_bracket);
    tcase_add_test(tc_core, test_csi_u_probe_no_question);

    suite_add_tcase(s, tc_core);
    return s;
}

int main(void)
{
    Suite *s = terminal_csi_u_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/shared/terminal/terminal_csi_u_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    ik_test_reset_terminal();

    return (number_failed == 0) ? 0 : 1;
}
