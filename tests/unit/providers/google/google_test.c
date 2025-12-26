/**
 * @file google_test.c
 * @brief Unit tests for Google provider factory
 */

#include <check.h>
#include <talloc.h>
#include "providers/google/google.h"
#include "providers/provider.h"

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
END_TEST START_TEST(test_google_create_has_vtable)
{
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
 * Test Suite Setup
 * ================================================================ */

static Suite *google_provider_suite(void)
{
    Suite *s = suite_create("Google Provider");

    TCase *tc_create = tcase_create("Provider Creation");
    tcase_add_unchecked_fixture(tc_create, setup, teardown);
    tcase_add_test(tc_create, test_google_create_success);
    tcase_add_test(tc_create, test_google_create_has_vtable);
    suite_add_tcase(s, tc_create);

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
