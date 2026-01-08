#include "../../../test_constants.h"
/**
 * @file google_test.c
 * @brief Unit tests for Google provider factory and vtable methods
 */

#include <check.h>
#include <talloc.h>
#include <sys/select.h>
#include <string.h>
#include "providers/google/google.h"
#include "providers/provider.h"
#include "providers/request.h"
#include "logger.h"

static TALLOC_CTX *test_ctx;

static void setup(void)
{
    test_ctx = talloc_new(NULL);
}

static void teardown(void)
{
    talloc_free(test_ctx);
}

/* ================================================================
 * Provider Creation Tests
 * ================================================================ */

START_TEST(test_google_create_success) {
    ik_provider_t *provider = NULL;
    res_t result = ik_google_create(test_ctx, "test-api-key", &provider);

    ck_assert(!is_err(&result));
    ck_assert_ptr_nonnull(provider);
    ck_assert_str_eq(provider->name, "google");
    ck_assert_ptr_nonnull(provider->vt);
    ck_assert_ptr_nonnull(provider->ctx);
}
END_TEST

START_TEST(test_google_create_has_vtable) {
    ik_provider_t *provider = NULL;
    res_t result = ik_google_create(test_ctx, "test-api-key", &provider);

    ck_assert(!is_err(&result));
    ck_assert_ptr_nonnull(provider->vt->fdset);
    ck_assert_ptr_nonnull(provider->vt->perform);
    ck_assert_ptr_nonnull(provider->vt->timeout);
    ck_assert_ptr_nonnull(provider->vt->info_read);
    ck_assert_ptr_nonnull(provider->vt->start_request);
    ck_assert_ptr_nonnull(provider->vt->start_stream);
    ck_assert_ptr_nonnull(provider->vt->cleanup);
    ck_assert_ptr_nonnull(provider->vt->cancel);
}

END_TEST
/* ================================================================
 * Vtable Method Tests
 * ================================================================ */

START_TEST(test_google_cleanup) {
    ik_provider_t *provider = NULL;
    res_t result = ik_google_create(test_ctx, "test-api-key", &provider);
    ck_assert(!is_err(&result));

    /* Call cleanup - should not crash */
    provider->vt->cleanup(provider->ctx);

    /* Provider should still be valid after cleanup */
    ck_assert_ptr_nonnull(provider->ctx);
}

END_TEST

START_TEST(test_google_cancel) {
    ik_provider_t *provider = NULL;
    res_t result = ik_google_create(test_ctx, "test-api-key", &provider);
    ck_assert(!is_err(&result));

    /* Call cancel - should not crash */
    provider->vt->cancel(provider->ctx);

    /* Provider should still be valid after cancel */
    ck_assert_ptr_nonnull(provider->ctx);
}

END_TEST

START_TEST(test_google_fdset) {
    ik_provider_t *provider = NULL;
    res_t result = ik_google_create(test_ctx, "test-api-key", &provider);
    ck_assert(!is_err(&result));

    fd_set read_fds, write_fds, exc_fds;
    int max_fd = 0;

    FD_ZERO(&read_fds);
    FD_ZERO(&write_fds);
    FD_ZERO(&exc_fds);

    /* Call fdset - should not crash */
    res_t r = provider->vt->fdset(provider->ctx, &read_fds, &write_fds, &exc_fds, &max_fd);
    ck_assert(!is_err(&r));
}

END_TEST

START_TEST(test_google_perform) {
    ik_provider_t *provider = NULL;
    res_t result = ik_google_create(test_ctx, "test-api-key", &provider);
    ck_assert(!is_err(&result));

    int running_handles = 0;

    /* Call perform - should not crash */
    res_t r = provider->vt->perform(provider->ctx, &running_handles);
    ck_assert(!is_err(&r));
}

END_TEST

START_TEST(test_google_timeout) {
    ik_provider_t *provider = NULL;
    res_t result = ik_google_create(test_ctx, "test-api-key", &provider);
    ck_assert(!is_err(&result));

    long timeout_ms = 0;

    /* Call timeout - should not crash */
    res_t r = provider->vt->timeout(provider->ctx, &timeout_ms);
    ck_assert(!is_err(&r));
}

END_TEST

START_TEST(test_google_info_read) {
    ik_provider_t *provider = NULL;
    res_t result = ik_google_create(test_ctx, "test-api-key", &provider);
    ck_assert(!is_err(&result));

    ik_logger_t *logger = ik_logger_create(test_ctx, "/tmp");

    /* Call info_read - should not crash */
    provider->vt->info_read(provider->ctx, logger);
}

END_TEST

/* ================================================================
 * Test Suite Setup
 * ================================================================ */

static Suite *google_provider_suite(void)
{
    Suite *s = suite_create("Google Provider");

    TCase *tc_create = tcase_create("Provider Creation");
    tcase_set_timeout(tc_create, IK_TEST_TIMEOUT);
    tcase_add_unchecked_fixture(tc_create, setup, teardown);
    tcase_add_test(tc_create, test_google_create_success);
    tcase_add_test(tc_create, test_google_create_has_vtable);
    suite_add_tcase(s, tc_create);

    TCase *tc_vtable = tcase_create("Vtable Methods");
    tcase_set_timeout(tc_vtable, IK_TEST_TIMEOUT);
    tcase_add_unchecked_fixture(tc_vtable, setup, teardown);
    tcase_add_test(tc_vtable, test_google_cleanup);
    tcase_add_test(tc_vtable, test_google_cancel);
    tcase_add_test(tc_vtable, test_google_fdset);
    tcase_add_test(tc_vtable, test_google_perform);
    tcase_add_test(tc_vtable, test_google_timeout);
    tcase_add_test(tc_vtable, test_google_info_read);
    suite_add_tcase(s, tc_vtable);

    return s;
}

int main(void)
{
    Suite *s = google_provider_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
