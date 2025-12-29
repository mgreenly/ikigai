/**
 * @file test_openai_adapter.c
 * @brief Unit tests for OpenAI provider adapter vtable implementation
 */

#include <check.h>
#include <talloc.h>
#include <sys/select.h>
#include "providers/openai/openai.h"
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

START_TEST(test_create_adapter_with_valid_credentials) {
    ik_provider_t *provider = NULL;
    res_t result = ik_openai_create(test_ctx, "sk-test-api-key-12345", &provider);

    ck_assert(!is_err(&result));
    ck_assert_ptr_nonnull(provider);
    ck_assert_str_eq(provider->name, "openai");
    ck_assert_ptr_nonnull(provider->vt);
    ck_assert_ptr_nonnull(provider->ctx);
}
END_TEST START_TEST(test_destroy_adapter_cleans_up_resources)
{
    ik_provider_t *provider = NULL;
    res_t result = ik_openai_create(test_ctx, "sk-test-api-key-12345", &provider);

    ck_assert(!is_err(&result));

    // Cleanup via talloc should work without leaks
    // (Valgrind will catch any leaks)
    talloc_free(provider);
}

END_TEST START_TEST(test_create_adapter_with_empty_api_key_returns_error)
{
    ik_provider_t *provider = NULL;
    res_t result = ik_openai_create(test_ctx, "", &provider);

    ck_assert(is_err(&result));
    ck_assert_ptr_null(provider);
}

END_TEST START_TEST(test_vtable_async_methods_non_null)
{
    ik_provider_t *provider = NULL;
    res_t result = ik_openai_create(test_ctx, "sk-test-api-key-12345", &provider);

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

END_TEST START_TEST(test_fdset_returns_ok)
{
    ik_provider_t *provider = NULL;
    res_t result = ik_openai_create(test_ctx, "sk-test-api-key-12345", &provider);
    ck_assert(!is_err(&result));

    fd_set read_fds, write_fds, exc_fds;
    int max_fd = 0;
    FD_ZERO(&read_fds);
    FD_ZERO(&write_fds);
    FD_ZERO(&exc_fds);

    res_t r = provider->vt->fdset(provider->ctx, &read_fds, &write_fds, &exc_fds, &max_fd);
    ck_assert(!is_err(&r));
}

END_TEST START_TEST(test_perform_returns_ok)
{
    ik_provider_t *provider = NULL;
    res_t result = ik_openai_create(test_ctx, "sk-test-api-key-12345", &provider);
    ck_assert(!is_err(&result));

    int running = 0;
    res_t r = provider->vt->perform(provider->ctx, &running);
    ck_assert(!is_err(&r));
    ck_assert_int_eq(running, 0); // No requests started yet
}

END_TEST

/* ================================================================
 * Test Suite Setup
 * ================================================================ */

static Suite *openai_adapter_suite(void)
{
    Suite *s = suite_create("OpenAI Adapter");

    TCase *tc_create = tcase_create("Provider Creation");
    tcase_set_timeout(tc_create, 30);
    tcase_add_unchecked_fixture(tc_create, setup, teardown);
    tcase_add_test(tc_create, test_create_adapter_with_valid_credentials);
    tcase_add_test(tc_create, test_create_adapter_with_empty_api_key_returns_error);
    tcase_add_test(tc_create, test_destroy_adapter_cleans_up_resources);
    tcase_add_test(tc_create, test_vtable_async_methods_non_null);
    tcase_add_test(tc_create, test_fdset_returns_ok);
    tcase_add_test(tc_create, test_perform_returns_ok);
    suite_add_tcase(s, tc_create);

    return s;
}

int main(void)
{
    Suite *s = openai_adapter_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
