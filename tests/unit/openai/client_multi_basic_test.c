/* Unit tests for OpenAI multi-handle manager - Basic operations */

#include "client_multi_test_common.h"

/*
 * Multi-handle creation tests
 */

START_TEST(test_multi_create_success) {
    res_t res = ik_openai_multi_create(ctx);
    ck_assert(!res.is_err);
    ck_assert(res.ok != NULL);
}

END_TEST START_TEST(test_multi_create_curl_init_failure)
{
    fail_curl_multi_init = true;

    res_t res = ik_openai_multi_create(ctx);
    ck_assert(res.is_err);
    ck_assert_int_eq(res.err->code, ERR_IO);

    fail_curl_multi_init = false;
}

END_TEST
/*
 * Perform tests
 */

START_TEST(test_multi_perform_no_requests)
{
    res_t multi_res = ik_openai_multi_create(ctx);
    ck_assert(!multi_res.is_err);
    ik_openai_multi_t *multi = multi_res.ok;

    int still_running = -1;
    res_t perform_res = ik_openai_multi_perform(multi, &still_running);
    ck_assert(!perform_res.is_err);
    ck_assert_int_eq(still_running, 0);
}

END_TEST START_TEST(test_multi_perform_failure)
{
    res_t multi_res = ik_openai_multi_create(ctx);
    ck_assert(!multi_res.is_err);
    ik_openai_multi_t *multi = multi_res.ok;

    /* Mock curl_multi_perform to fail */
    fail_curl_multi_perform = true;

    int still_running = -1;
    res_t perform_res = ik_openai_multi_perform(multi, &still_running);
    ck_assert(perform_res.is_err);
    ck_assert_int_eq(perform_res.err->code, ERR_IO);

    fail_curl_multi_perform = false;
}

END_TEST
/*
 * FD set tests
 */

START_TEST(test_multi_fdset_no_requests)
{
    res_t multi_res = ik_openai_multi_create(ctx);
    ck_assert(!multi_res.is_err);
    ik_openai_multi_t *multi = multi_res.ok;

    fd_set read_fds, write_fds, exc_fds;
    int max_fd = -1;

    FD_ZERO(&read_fds);
    FD_ZERO(&write_fds);
    FD_ZERO(&exc_fds);

    res_t fdset_res = ik_openai_multi_fdset(multi, &read_fds, &write_fds, &exc_fds, &max_fd);
    ck_assert(!fdset_res.is_err);
    ck_assert_int_eq(max_fd, -1);  /* No FDs when no requests active */
}

END_TEST START_TEST(test_multi_fdset_failure)
{
    res_t multi_res = ik_openai_multi_create(ctx);
    ck_assert(!multi_res.is_err);
    ik_openai_multi_t *multi = multi_res.ok;

    fd_set read_fds, write_fds, exc_fds;
    int max_fd = -1;

    FD_ZERO(&read_fds);
    FD_ZERO(&write_fds);
    FD_ZERO(&exc_fds);

    /* Mock curl_multi_fdset to fail */
    fail_curl_multi_fdset = true;

    res_t fdset_res = ik_openai_multi_fdset(multi, &read_fds, &write_fds, &exc_fds, &max_fd);
    ck_assert(fdset_res.is_err);
    ck_assert_int_eq(fdset_res.err->code, ERR_IO);

    fail_curl_multi_fdset = false;
}

END_TEST
/*
 * Timeout tests
 */

START_TEST(test_multi_timeout_no_requests)
{
    res_t multi_res = ik_openai_multi_create(ctx);
    ck_assert(!multi_res.is_err);
    ik_openai_multi_t *multi = multi_res.ok;

    long timeout_ms = 0;
    res_t timeout_res = ik_openai_multi_timeout(multi, &timeout_ms);
    ck_assert(!timeout_res.is_err);
    ck_assert_int_eq(timeout_ms, -1);  /* No timeout when no requests */
}

END_TEST START_TEST(test_multi_timeout_failure)
{
    res_t multi_res = ik_openai_multi_create(ctx);
    ck_assert(!multi_res.is_err);
    ik_openai_multi_t *multi = multi_res.ok;

    /* Mock curl_multi_timeout to fail */
    fail_curl_multi_timeout = true;

    long timeout_ms = 0;
    res_t timeout_res = ik_openai_multi_timeout(multi, &timeout_ms);
    ck_assert(timeout_res.is_err);
    ck_assert_int_eq(timeout_res.err->code, ERR_IO);

    fail_curl_multi_timeout = false;
}

END_TEST

/*
 * Test suite
 */

static Suite *client_multi_basic_suite(void)
{
    Suite *s = suite_create("openai_client_multi_basic");

    TCase *tc_create = tcase_create("create");
    tcase_add_checked_fixture(tc_create, setup, teardown);
    tcase_add_test(tc_create, test_multi_create_success);
    tcase_add_test(tc_create, test_multi_create_curl_init_failure);
    suite_add_tcase(s, tc_create);

    TCase *tc_perform = tcase_create("perform");
    tcase_add_checked_fixture(tc_perform, setup, teardown);
    tcase_add_test(tc_perform, test_multi_perform_no_requests);
    tcase_add_test(tc_perform, test_multi_perform_failure);
    suite_add_tcase(s, tc_perform);

    TCase *tc_fdset = tcase_create("fdset");
    tcase_add_checked_fixture(tc_fdset, setup, teardown);
    tcase_add_test(tc_fdset, test_multi_fdset_no_requests);
    tcase_add_test(tc_fdset, test_multi_fdset_failure);
    suite_add_tcase(s, tc_fdset);

    TCase *tc_timeout = tcase_create("timeout");
    tcase_add_checked_fixture(tc_timeout, setup, teardown);
    tcase_add_test(tc_timeout, test_multi_timeout_no_requests);
    tcase_add_test(tc_timeout, test_multi_timeout_failure);
    suite_add_tcase(s, tc_timeout);

    return s;
}

int main(void)
{
    Suite *s = client_multi_basic_suite();
    SRunner *sr = srunner_create(s);
    srunner_run_all(sr, CK_VERBOSE);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);
    return (number_failed == 0) ? 0 : 1;
}
