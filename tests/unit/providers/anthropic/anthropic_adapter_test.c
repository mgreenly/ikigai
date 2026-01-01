/**
 * @file test_anthropic_adapter.c
 * @brief Unit tests for Anthropic provider adapter vtable implementation
 */

#include <check.h>
#include <talloc.h>
#include <sys/select.h>
#include "providers/anthropic/anthropic.h"
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

START_TEST(test_create_adapter_with_valid_credentials) {
    ik_provider_t *provider = NULL;
    res_t result = ik_anthropic_create(test_ctx, "test-api-key", &provider);

    ck_assert(!is_err(&result));
    ck_assert_ptr_nonnull(provider);
    ck_assert_str_eq(provider->name, "anthropic");
    ck_assert_ptr_nonnull(provider->vt);
    ck_assert_ptr_nonnull(provider->ctx);
}
END_TEST

START_TEST(test_destroy_adapter_cleans_up_resources) {
    ik_provider_t *provider = NULL;
    res_t result = ik_anthropic_create(test_ctx, "test-api-key", &provider);

    ck_assert(!is_err(&result));

    // Cleanup via talloc should work without leaks
    // (Valgrind will catch any leaks)
    talloc_free(provider);
}

END_TEST

START_TEST(test_all_vtable_functions_non_null) {
    ik_provider_t *provider = NULL;
    res_t result = ik_anthropic_create(test_ctx, "test-api-key", &provider);

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
 * Async Pattern Tests
 * ================================================================ */

START_TEST(test_fdset_returns_ok) {
    ik_provider_t *provider = NULL;
    res_t result = ik_anthropic_create(test_ctx, "test-api-key", &provider);
    ck_assert(!is_err(&result));

    fd_set read_fds, write_fds, exc_fds;
    int max_fd = 0;
    FD_ZERO(&read_fds);
    FD_ZERO(&write_fds);
    FD_ZERO(&exc_fds);

    res_t r = provider->vt->fdset(provider->ctx, &read_fds, &write_fds, &exc_fds, &max_fd);
    ck_assert(!is_err(&r));
}

END_TEST

START_TEST(test_perform_returns_ok) {
    ik_provider_t *provider = NULL;
    res_t result = ik_anthropic_create(test_ctx, "test-api-key", &provider);
    ck_assert(!is_err(&result));

    int running = 0;
    res_t r = provider->vt->perform(provider->ctx, &running);
    ck_assert(!is_err(&r));
    ck_assert_int_eq(running, 0); // No requests started yet
}

END_TEST

START_TEST(test_timeout_returns_ok) {
    ik_provider_t *provider = NULL;
    res_t result = ik_anthropic_create(test_ctx, "test-api-key", &provider);
    ck_assert(!is_err(&result));

    long timeout_ms = 0;
    res_t r = provider->vt->timeout(provider->ctx, &timeout_ms);
    ck_assert(!is_err(&r));
}

END_TEST

START_TEST(test_cleanup_does_not_crash) {
    ik_provider_t *provider = NULL;
    res_t result = ik_anthropic_create(test_ctx, "test-api-key", &provider);
    ck_assert(!is_err(&result));

    // Call cleanup vtable function
    provider->vt->cleanup(provider->ctx);

    // Should complete without crashing
}

END_TEST

START_TEST(test_cancel_does_not_crash) {
    ik_provider_t *provider = NULL;
    res_t result = ik_anthropic_create(test_ctx, "test-api-key", &provider);
    ck_assert(!is_err(&result));

    // Call cancel vtable function (no active stream)
    provider->vt->cancel(provider->ctx);

    // Should complete without crashing
}

END_TEST

START_TEST(test_info_read_without_active_stream) {
    ik_provider_t *provider = NULL;
    res_t result = ik_anthropic_create(test_ctx, "test-api-key", &provider);
    ck_assert(!is_err(&result));

    // Create logger
    ik_logger_t *logger = ik_logger_create(test_ctx, "/tmp");
    ck_assert_ptr_nonnull(logger);

    // Call info_read with no active stream
    provider->vt->info_read(provider->ctx, logger);

    // Should complete without crashing
}

END_TEST

/* ================================================================
 * Non-streaming Request Tests
 * ================================================================ */

static bool completion_called = false;
static ik_provider_completion_t captured_completion;

static res_t test_completion_cb(const ik_provider_completion_t *completion, void *ctx)
{
    (void)ctx;
    captured_completion = *completion;
    completion_called = true;
    return OK(NULL);
}

START_TEST(test_start_request_delegates_to_response_module) {
    ik_provider_t *provider = NULL;
    res_t result = ik_anthropic_create(test_ctx, "test-api-key", &provider);
    ck_assert(!is_err(&result));

    // Create a minimal request
    ik_request_t *request = NULL;
    res_t req_res = ik_request_create(test_ctx, "claude-3-5-sonnet-20241022", &request);
    ck_assert(!is_err(&req_res));

    req_res = ik_request_add_message(request, IK_ROLE_USER, "test");
    ck_assert(!is_err(&req_res));

    // Call start_request (non-streaming)
    completion_called = false;
    res_t start_res = provider->vt->start_request(provider->ctx, request,
                                                  test_completion_cb, NULL);

    // The function should return OK - actual network call will happen async
    ck_assert(!is_err(&start_res));
}
END_TEST

/* ================================================================
 * Test Suite Setup
 * ================================================================ */

static Suite *anthropic_adapter_suite(void)
{
    Suite *s = suite_create("Anthropic Adapter");

    TCase *tc_create = tcase_create("Provider Creation");
    tcase_set_timeout(tc_create, 30);
    tcase_add_unchecked_fixture(tc_create, setup, teardown);
    tcase_add_test(tc_create, test_create_adapter_with_valid_credentials);
    tcase_add_test(tc_create, test_destroy_adapter_cleans_up_resources);
    tcase_add_test(tc_create, test_all_vtable_functions_non_null);
    suite_add_tcase(s, tc_create);

    TCase *tc_async = tcase_create("Async Pattern");
    tcase_set_timeout(tc_async, 30);
    tcase_add_unchecked_fixture(tc_async, setup, teardown);
    tcase_add_test(tc_async, test_fdset_returns_ok);
    tcase_add_test(tc_async, test_perform_returns_ok);
    tcase_add_test(tc_async, test_timeout_returns_ok);
    tcase_add_test(tc_async, test_cleanup_does_not_crash);
    tcase_add_test(tc_async, test_cancel_does_not_crash);
    tcase_add_test(tc_async, test_info_read_without_active_stream);
    suite_add_tcase(s, tc_async);

    TCase *tc_request = tcase_create("Non-streaming Request");
    tcase_set_timeout(tc_request, 30);
    tcase_add_unchecked_fixture(tc_request, setup, teardown);
    tcase_add_test(tc_request, test_start_request_delegates_to_response_module);
    suite_add_tcase(s, tc_request);

    return s;
}

int main(void)
{
    Suite *s = anthropic_adapter_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
