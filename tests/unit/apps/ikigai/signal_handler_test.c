/**
 * @file signal_handler_test.c
 * @brief Unit tests for signal_handler initialization failure paths
 */

#include <check.h>
#include <errno.h>
#include <signal.h>
#include <talloc.h>

#include "apps/ikigai/signal_handler.h"
#include "shared/error.h"
#include "tests/helpers/test_utils_helper.h"

// Mock state for controlling posix_sigaction_ failures
static int mock_sigaction_fail_on_call = 0;
static int mock_sigaction_call_count = 0;

// Forward declarations
int posix_sigaction_(int signum, const struct sigaction *act, struct sigaction *oldact);

static void suite_setup(void)
{
    ik_test_set_log_dir(__FILE__);
}

int posix_sigaction_(int signum, const struct sigaction *act, struct sigaction *oldact)
{
    (void)signum;
    (void)act;
    (void)oldact;

    mock_sigaction_call_count++;
    if (mock_sigaction_call_count == mock_sigaction_fail_on_call) {
        errno = EINVAL;
        return -1;
    }
    return 0;
}

// Test: SIGWINCH failure (first sigaction call fails)
START_TEST(test_sigwinch_failure) {
    void *ctx = talloc_new(NULL);

    mock_sigaction_fail_on_call = 1;
    mock_sigaction_call_count = 0;

    res_t res = ik_signal_handler_init(ctx);
    ck_assert(is_err(&res));
    talloc_free(res.err);

    mock_sigaction_fail_on_call = 0;
    talloc_free(ctx);
}

END_TEST

// Test: SIGINT failure (second sigaction call fails)
START_TEST(test_sigint_failure) {
    void *ctx = talloc_new(NULL);

    mock_sigaction_fail_on_call = 2;
    mock_sigaction_call_count = 0;

    res_t res = ik_signal_handler_init(ctx);
    ck_assert(is_err(&res));
    talloc_free(res.err);

    mock_sigaction_fail_on_call = 0;
    talloc_free(ctx);
}

END_TEST

// Test: SIGTERM failure (third sigaction call fails)
START_TEST(test_sigterm_failure) {
    void *ctx = talloc_new(NULL);

    mock_sigaction_fail_on_call = 3;
    mock_sigaction_call_count = 0;

    res_t res = ik_signal_handler_init(ctx);
    ck_assert(is_err(&res));
    talloc_free(res.err);

    mock_sigaction_fail_on_call = 0;
    talloc_free(ctx);
}

END_TEST

// Test: All sigaction calls succeed
START_TEST(test_success) {
    void *ctx = talloc_new(NULL);

    mock_sigaction_fail_on_call = 0;
    mock_sigaction_call_count = 0;

    res_t res = ik_signal_handler_init(ctx);
    ck_assert(is_ok(&res));

    talloc_free(ctx);
}

END_TEST

static Suite *signal_handler_suite(void)
{
    Suite *s = suite_create("SignalHandler");

    TCase *tc_init = tcase_create("Init");
    tcase_add_unchecked_fixture(tc_init, suite_setup, NULL);
    tcase_add_test(tc_init, test_sigwinch_failure);
    tcase_add_test(tc_init, test_sigint_failure);
    tcase_add_test(tc_init, test_sigterm_failure);
    tcase_add_test(tc_init, test_success);
    suite_add_tcase(s, tc_init);

    return s;
}

int main(void)
{
    int number_failed;
    Suite *s = signal_handler_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/apps/ikigai/signal_handler_test.xml");
    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);
    return (number_failed == 0) ? 0 : 1;
}
