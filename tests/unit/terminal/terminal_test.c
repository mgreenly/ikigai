// Terminal module unit tests
#include <check.h>
#include <signal.h>
#include <talloc.h>

#include "../../../src/error.h"
#include "../../../src/terminal.h"
#include "../../test_utils.h"
#include "terminal_test_mocks.h"

// Test: successful terminal initialization
START_TEST(test_term_init_success) {
    reset_mocks();
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_term_ctx_t *term = NULL;

    res_t res = ik_term_init(ctx, &term);

    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(term);
    ck_assert_int_eq(term->tty_fd, 42);
    ck_assert_int_eq(term->screen_rows, 24);
    ck_assert_int_eq(term->screen_cols, 80);

    // Verify write was called for alternate screen (and CSI u query)
    // CSI u query (4 bytes) + alt screen enter (8 bytes) = 2 writes
    ck_assert_int_eq(mock_write_count, 2);

    // Cleanup
    ik_term_cleanup(term);
    talloc_free(ctx);

    // Verify cleanup operations
    // Note: CSI u was not enabled in mocks (select times out), so no disable write
    ck_assert_int_eq(mock_write_count, 3); // query + alt screen enter + alt screen exit
    ck_assert_int_eq(mock_tcsetattr_count, 2); // restore termios
    ck_assert_int_eq(mock_tcflush_count, 2); // flush after set raw + cleanup
    ck_assert_int_eq(mock_close_count, 1);
}
END_TEST
// Test: alternate screen sequences are written during init and cleanup
START_TEST(test_term_alt_screen_sequences)
{
    reset_mocks();
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_term_ctx_t *term = NULL;

    res_t res = ik_term_init(ctx, &term);
    ck_assert(is_ok(&res));

    // Verify alternate screen enter sequence is present in init output
    ck_assert(strstr(mock_write_buffer, "\x1b[?1049h") != NULL);

    // Reset buffer to capture cleanup output
    memset(mock_write_buffer, 0, MOCK_WRITE_BUFFER_SIZE);
    mock_write_buffer_pos = 0;

    ik_term_cleanup(term);

    // Verify alternate screen exit sequence is present in cleanup output
    ck_assert(strstr(mock_write_buffer, "\x1b[?1049l") != NULL);

    talloc_free(ctx);
}

END_TEST
// Test: open fails
START_TEST(test_term_init_open_fails)
{
    reset_mocks();
    mock_open_fail = 1;

    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_term_ctx_t *term = NULL;

    res_t res = ik_term_init(ctx, &term);

    ck_assert(is_err(&res));
    ck_assert_int_eq(error_code(res.err), ERR_IO);
    ck_assert_ptr_null(term);

    // No cleanup calls should have been made
    ck_assert_int_eq(mock_close_count, 0);

    talloc_free(ctx);
}

END_TEST
// Test: tcgetattr fails
START_TEST(test_term_init_tcgetattr_fails)
{
    reset_mocks();
    mock_tcgetattr_fail = 1;

    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_term_ctx_t *term = NULL;

    res_t res = ik_term_init(ctx, &term);

    ck_assert(is_err(&res));
    ck_assert_int_eq(error_code(res.err), ERR_IO);
    ck_assert_ptr_null(term);

    // Close should have been called
    ck_assert_int_eq(mock_close_count, 1);

    talloc_free(ctx);
}

END_TEST
// Test: tcsetattr fails (raw mode)
START_TEST(test_term_init_tcsetattr_fails)
{
    reset_mocks();
    mock_tcsetattr_fail = 1;

    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_term_ctx_t *term = NULL;

    res_t res = ik_term_init(ctx, &term);

    ck_assert(is_err(&res));
    ck_assert_int_eq(error_code(res.err), ERR_IO);
    ck_assert_ptr_null(term);

    // Close should have been called
    ck_assert_int_eq(mock_close_count, 1);

    talloc_free(ctx);
}

END_TEST
// Test: write fails (alternate screen)
START_TEST(test_term_init_write_fails)
{
    reset_mocks();
    mock_write_fail = 1;

    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_term_ctx_t *term = NULL;

    res_t res = ik_term_init(ctx, &term);

    ck_assert(is_err(&res));
    ck_assert_int_eq(error_code(res.err), ERR_IO);
    ck_assert_ptr_null(term);

    // Cleanup should have been called (tcsetattr + close)
    ck_assert_int_eq(mock_tcsetattr_count, 2); // raw mode + restore
    ck_assert_int_eq(mock_tcflush_count, 1); // flush after set raw
    ck_assert_int_eq(mock_close_count, 1);

    talloc_free(ctx);
}

END_TEST
// Test: ioctl fails (get terminal size)
START_TEST(test_term_init_ioctl_fails)
{
    reset_mocks();
    mock_ioctl_fail = 1;

    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_term_ctx_t *term = NULL;

    res_t res = ik_term_init(ctx, &term);

    ck_assert(is_err(&res));
    ck_assert_int_eq(error_code(res.err), ERR_IO);
    ck_assert_ptr_null(term);

    // Full cleanup should have been called
    ck_assert_int_eq(mock_write_count, 3); // CSI u query + enter alt screen + exit alt screen
    ck_assert_int_eq(mock_tcsetattr_count, 2); // raw mode + restore
    ck_assert_int_eq(mock_tcflush_count, 1); // flush after set raw
    ck_assert_int_eq(mock_close_count, 1);

    talloc_free(ctx);
}

END_TEST
// Test: terminal cleanup with NULL
START_TEST(test_term_cleanup_null_safe)
{
    reset_mocks();
    // Should handle NULL gracefully (no crash)
    ik_term_cleanup(NULL);

    // No operations should have been called
    ck_assert_int_eq(mock_write_count, 0);
    ck_assert_int_eq(mock_tcsetattr_count, 0);
    ck_assert_int_eq(mock_close_count, 0);
}

END_TEST
// Test: get terminal size success
START_TEST(test_term_get_size_success)
{
    reset_mocks();
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_term_ctx_t *term = NULL;

    res_t res = ik_term_init(ctx, &term);
    ck_assert(is_ok(&res));

    int rows, cols;
    res_t size_res = ik_term_get_size(term, &rows, &cols);

    ck_assert(is_ok(&size_res));
    ck_assert_int_eq(rows, 24);
    ck_assert_int_eq(cols, 80);
    ck_assert_int_eq(rows, term->screen_rows);
    ck_assert_int_eq(cols, term->screen_cols);

    ik_term_cleanup(term);
    talloc_free(ctx);
}

END_TEST
// Test: get terminal size fails
START_TEST(test_term_get_size_fails)
{
    reset_mocks();
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_term_ctx_t *term = NULL;

    res_t res = ik_term_init(ctx, &term);
    ck_assert(is_ok(&res));

    // Make ioctl fail on second call
    mock_ioctl_fail = 1;

    int rows, cols;
    res_t size_res = ik_term_get_size(term, &rows, &cols);

    ck_assert(is_err(&size_res));
    ck_assert_int_eq(error_code(size_res.err), ERR_IO);

    ik_term_cleanup(term);
    talloc_free(ctx);
}

END_TEST

#if !defined(NDEBUG) && !defined(SKIP_SIGNAL_TESTS)
// Test: ik_term_init with NULL parent asserts
START_TEST(test_term_init_null_parent_asserts)
{
    ik_term_ctx_t *term = NULL;
    ik_term_init(NULL, &term);
}

END_TEST
// Test: ik_term_init with NULL ctx_out asserts
START_TEST(test_term_init_null_ctx_out_asserts)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_term_init(ctx, NULL);
    talloc_free(ctx);
}

END_TEST
// Test: ik_term_get_size with NULL ctx asserts
START_TEST(test_term_get_size_null_ctx_asserts)
{
    int rows, cols;
    ik_term_get_size(NULL, &rows, &cols);
}

END_TEST
// Test: ik_term_get_size with NULL rows_out asserts
START_TEST(test_term_get_size_null_rows_asserts)
{
    reset_mocks();
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_term_ctx_t *term = NULL;
    ik_term_init(ctx, &term);

    int cols;
    ik_term_get_size(term, NULL, &cols);

    ik_term_cleanup(term);
    talloc_free(ctx);
}

END_TEST
// Test: ik_term_get_size with NULL cols_out asserts
START_TEST(test_term_get_size_null_cols_asserts)
{
    reset_mocks();
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_term_ctx_t *term = NULL;
    ik_term_init(ctx, &term);

    int rows;
    ik_term_get_size(term, &rows, NULL);

    ik_term_cleanup(term);
    talloc_free(ctx);
}

END_TEST
#endif

// Test: tcflush fails
START_TEST(test_term_init_tcflush_fails)
{
    reset_mocks();
    mock_tcflush_fail = 1;

    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_term_ctx_t *term = NULL;

    res_t res = ik_term_init(ctx, &term);

    ck_assert(is_err(&res));
    ck_assert_int_eq(error_code(res.err), ERR_IO);
    ck_assert_ptr_null(term);

    // Cleanup should have been called (tcsetattr + close)
    ck_assert_int_eq(mock_tcsetattr_count, 2); // raw mode + restore
    ck_assert_int_eq(mock_close_count, 1);

    talloc_free(ctx);
}

END_TEST

// Test suite
static Suite *terminal_suite(void)
{
    Suite *s = suite_create("Terminal");
    TCase *tc_core = tcase_create("Core");
    tcase_set_timeout(tc_core, 30);

    tcase_add_test(tc_core, test_term_init_success);
    tcase_add_test(tc_core, test_term_alt_screen_sequences);
    tcase_add_test(tc_core, test_term_init_open_fails);
    tcase_add_test(tc_core, test_term_init_tcgetattr_fails);
    tcase_add_test(tc_core, test_term_init_tcsetattr_fails);
    tcase_add_test(tc_core, test_term_init_tcflush_fails);
    tcase_add_test(tc_core, test_term_init_write_fails);
    tcase_add_test(tc_core, test_term_init_ioctl_fails);
    tcase_add_test(tc_core, test_term_cleanup_null_safe);
    tcase_add_test(tc_core, test_term_get_size_success);
    tcase_add_test(tc_core, test_term_get_size_fails);

#if !defined(NDEBUG) && !defined(SKIP_SIGNAL_TESTS)
    tcase_add_test_raise_signal(tc_core, test_term_init_null_parent_asserts, SIGABRT);
    tcase_add_test_raise_signal(tc_core, test_term_init_null_ctx_out_asserts, SIGABRT);
    tcase_add_test_raise_signal(tc_core, test_term_get_size_null_ctx_asserts, SIGABRT);
    tcase_add_test_raise_signal(tc_core, test_term_get_size_null_rows_asserts, SIGABRT);
    tcase_add_test_raise_signal(tc_core, test_term_get_size_null_cols_asserts, SIGABRT);
#endif

    suite_add_tcase(s, tc_core);
    return s;
}

int main(void)
{
    Suite *s = terminal_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    ik_test_reset_terminal();

    return (number_failed == 0) ? 0 : 1;
}
