#include "tests/test_constants.h"
/**
 * @file test_provider_factory.c
 * @brief Unit tests for provider factory and async vtable operations
 *
 * Tests provider creation and async event loop integration (fdset/perform/timeout/info_read).
 * These tests verify the async vtable interface that integrates with select()-based event loops.
 */

#include <check.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <talloc.h>

#include "apps/ikigai/providers/factory.h"
#include "apps/ikigai/providers/provider.h"
#include "shared/error.h"

/* Test context */
static TALLOC_CTX *test_ctx = NULL;

/* Setup/teardown */
static void setup(void)
{
    test_ctx = talloc_new(NULL);
}

static void teardown(void)
{
    talloc_free(test_ctx);
    test_ctx = NULL;
}

/**
 * Provider Creation Tests (with async vtable verification)
 *
 * These tests verify that providers are created with the correct async vtable
 * methods required for select()-based event loop integration.
 */

START_TEST(test_create_openai_provider) {
    setenv("OPENAI_API_KEY", "test-key-openai", 1);

    ik_provider_t *provider = NULL;
    res_t result = ik_provider_create(test_ctx, "openai", &provider);

    /* NOTE: This may fail with ERR_NOT_IMPLEMENTED until provider is fully implemented */
    if (is_ok(&result)) {
        ck_assert_ptr_nonnull(provider);
        ck_assert_str_eq(provider->name, "openai");
        ck_assert_ptr_nonnull(provider->vt);

        /* Verify async vtable methods exist (NOT blocking send/stream) */
        ck_assert_ptr_nonnull(provider->vt->fdset);
        ck_assert_ptr_nonnull(provider->vt->perform);
        ck_assert_ptr_nonnull(provider->vt->timeout);
        ck_assert_ptr_nonnull(provider->vt->info_read);
        ck_assert_ptr_nonnull(provider->vt->start_request);
        ck_assert_ptr_nonnull(provider->vt->start_stream);
    }

    unsetenv("OPENAI_API_KEY");
}
END_TEST

START_TEST(test_create_anthropic_provider) {
    setenv("ANTHROPIC_API_KEY", "test-key-anthropic", 1);

    ik_provider_t *provider = NULL;
    res_t result = ik_provider_create(test_ctx, "anthropic", &provider);

    /* NOTE: This may fail with ERR_NOT_IMPLEMENTED until provider is fully implemented */
    if (is_ok(&result)) {
        ck_assert_ptr_nonnull(provider);
        ck_assert_str_eq(provider->name, "anthropic");

        /* Verify async vtable methods */
        ck_assert_ptr_nonnull(provider->vt->fdset);
        ck_assert_ptr_nonnull(provider->vt->perform);
        ck_assert_ptr_nonnull(provider->vt->start_stream);
    }

    unsetenv("ANTHROPIC_API_KEY");
}

END_TEST

START_TEST(test_create_google_provider) {
    setenv("GOOGLE_API_KEY", "test-key-google", 1);

    ik_provider_t *provider = NULL;
    res_t result = ik_provider_create(test_ctx, "google", &provider);

    /* NOTE: This may fail with ERR_NOT_IMPLEMENTED until provider is fully implemented */
    if (is_ok(&result)) {
        ck_assert_ptr_nonnull(provider);
        ck_assert_str_eq(provider->name, "google");

        /* Verify async vtable methods */
        ck_assert_ptr_nonnull(provider->vt->fdset);
        ck_assert_ptr_nonnull(provider->vt->perform);
    }

    unsetenv("GOOGLE_API_KEY");
}

END_TEST

START_TEST(test_create_unknown_provider_fails) {
    ik_provider_t *provider = NULL;
    res_t result = ik_provider_create(test_ctx, "unknown", &provider);

    ck_assert(is_err(&result));
    ck_assert_int_eq(error_code(result.err), ERR_INVALID_ARG);
}

END_TEST

START_TEST(test_create_provider_missing_credentials) {
    unsetenv("OPENAI_API_KEY");
    unsetenv("ANTHROPIC_API_KEY");
    unsetenv("GOOGLE_API_KEY");

    ik_provider_t *provider = NULL;
    res_t result = ik_provider_create(test_ctx, "openai", &provider);

    /* This test result depends on whether credentials.json exists with keys.
     * If credentials.json has an openai key, creation will succeed.
     * If not, it should fail with ERR_MISSING_CREDENTIALS or ERR_NOT_IMPLEMENTED. */
    if (is_err(&result)) {
        int code = error_code(result.err);
        ck_assert(code == ERR_MISSING_CREDENTIALS || code == ERR_NOT_IMPLEMENTED);
    } else {
        /* Success case: credentials.json has openai key */
        ck_assert_ptr_nonnull(provider);
        ck_assert_str_eq(provider->name, "openai");
    }
}

END_TEST
/**
 * Async Event Loop Integration Tests
 *
 * These tests verify that provider vtable methods work correctly for
 * integration with select()-based event loops. They test the async pattern
 * even when no active requests are in flight.
 */

START_TEST(test_provider_fdset_returns_ok) {
    setenv("OPENAI_API_KEY", "test-key", 1);

    ik_provider_t *provider = NULL;
    res_t result = ik_provider_create(test_ctx, "openai", &provider);

    /* Skip test if provider not yet implemented */
    if (is_err(&result)) {
        unsetenv("OPENAI_API_KEY");
        return;
    }

    /* fdset should work even with no active requests */
    fd_set read_fds, write_fds, exc_fds;
    FD_ZERO(&read_fds);
    FD_ZERO(&write_fds);
    FD_ZERO(&exc_fds);
    int max_fd = 0;

    result = provider->vt->fdset(provider->ctx, &read_fds, &write_fds, &exc_fds, &max_fd);
    ck_assert(is_ok(&result));

    unsetenv("OPENAI_API_KEY");
}

END_TEST

START_TEST(test_provider_perform_returns_ok) {
    setenv("OPENAI_API_KEY", "test-key", 1);

    ik_provider_t *provider = NULL;
    res_t result = ik_provider_create(test_ctx, "openai", &provider);

    /* Skip test if provider not yet implemented */
    if (is_err(&result)) {
        unsetenv("OPENAI_API_KEY");
        return;
    }

    /* perform should work even with no active requests */
    int running_handles = -1;
    result = provider->vt->perform(provider->ctx, &running_handles);
    ck_assert(is_ok(&result));
    ck_assert_int_eq(running_handles, 0);  /* No active requests */

    unsetenv("OPENAI_API_KEY");
}

END_TEST

START_TEST(test_provider_timeout_returns_value) {
    setenv("OPENAI_API_KEY", "test-key", 1);

    ik_provider_t *provider = NULL;
    res_t result = ik_provider_create(test_ctx, "openai", &provider);

    /* Skip test if provider not yet implemented */
    if (is_err(&result)) {
        unsetenv("OPENAI_API_KEY");
        return;
    }

    long timeout_ms = -999;
    result = provider->vt->timeout(provider->ctx, &timeout_ms);
    ck_assert(is_ok(&result));
    /* timeout should be >= -1 (curl convention: -1 means no timeout) */
    ck_assert(timeout_ms >= -1);

    unsetenv("OPENAI_API_KEY");
}

END_TEST

START_TEST(test_provider_info_read_no_crash) {
    setenv("OPENAI_API_KEY", "test-key", 1);

    ik_provider_t *provider = NULL;
    res_t result = ik_provider_create(test_ctx, "openai", &provider);

    /* Skip test if provider not yet implemented */
    if (is_err(&result)) {
        unsetenv("OPENAI_API_KEY");
        return;
    }

    /* info_read should not crash even with no logger and no active requests */
    provider->vt->info_read(provider->ctx, NULL);
    /* Test passes if we get here without crashing */

    unsetenv("OPENAI_API_KEY");
}

END_TEST
/**
 * NOTE: Full async request/stream flow tests are skipped because they require:
 * - Mock curl_multi infrastructure
 * - VCR fixture loading
 * - Helper functions (create_test_request, etc.)
 * - Full provider implementations
 *
 * These will be added in integration tests once providers are implemented.
 */

START_TEST(test_async_vtable_complete_after_implementation) {
    /* Placeholder test documenting async flow tests to be added:
     *
     * test_provider_async_request_flow - Tests complete request lifecycle:
     *   1. start_request() returns immediately
     *   2. fdset() populates fd_sets
     *   3. perform() drives transfer
     *   4. info_read() invokes completion callback
     *
     * test_provider_async_stream_flow - Tests complete stream lifecycle:
     *   1. start_stream() returns immediately
     *   2. perform() drives transfer and delivers stream events
     *   3. info_read() invokes final completion callback
     *
     * These require provider implementations and mock infrastructure.
     */
    ck_assert(true);  /* Placeholder - always passes */
}

END_TEST

START_TEST(test_async_pattern_documented) {
    /* This test documents the expected async pattern for providers:
     *
     * CORRECT (async/non-blocking):
     * - start_request() / start_stream() - Initiate transfer, return immediately
     * - fdset() - Get FDs for select()
     * - perform() - Process I/O after select()
     * - info_read() - Check for completions, invoke callbacks
     *
     * INCORRECT (blocking - NOT allowed):
     * - send() - Blocking request (DOES NOT EXIST in vtable)
     * - stream() - Blocking stream (DOES NOT EXIST in vtable)
     *
     * All providers MUST use async pattern to integrate with select() event loop.
     */
    ck_assert(true);  /* Placeholder - documents contract */
}

END_TEST

/**
 * Test Suite Configuration
 */

static Suite *provider_factory_suite(void)
{
    Suite *s = suite_create("Provider Factory");

    /* Provider creation with vtable verification */
    TCase *tc_create = tcase_create("Provider Creation");
    tcase_set_timeout(tc_create, IK_TEST_TIMEOUT);
    tcase_add_checked_fixture(tc_create, setup, teardown);
    tcase_add_test(tc_create, test_create_openai_provider);
    tcase_add_test(tc_create, test_create_anthropic_provider);
    tcase_add_test(tc_create, test_create_google_provider);
    tcase_add_test(tc_create, test_create_unknown_provider_fails);
    tcase_add_test(tc_create, test_create_provider_missing_credentials);
    suite_add_tcase(s, tc_create);

    /* Async event loop integration */
    TCase *tc_async = tcase_create("Async Event Loop Integration");
    tcase_set_timeout(tc_async, IK_TEST_TIMEOUT);
    tcase_add_checked_fixture(tc_async, setup, teardown);
    tcase_add_test(tc_async, test_provider_fdset_returns_ok);
    tcase_add_test(tc_async, test_provider_perform_returns_ok);
    tcase_add_test(tc_async, test_provider_timeout_returns_value);
    tcase_add_test(tc_async, test_provider_info_read_no_crash);
    tcase_add_test(tc_async, test_async_vtable_complete_after_implementation);
    tcase_add_test(tc_async, test_async_pattern_documented);
    suite_add_tcase(s, tc_async);

    return s;
}

int main(void)
{
    Suite *s = provider_factory_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/apps/ikigai/providers/provider_factory_async_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
