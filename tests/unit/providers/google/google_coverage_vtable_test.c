/**
 * @file google_coverage_vtable_test.c
 * @brief Coverage tests for Google provider vtable methods
 */

#include <check.h>
#include <talloc.h>
#include <sys/select.h>

#include "error.h"
#include "providers/google/google.h"
#include "providers/provider.h"
#include "providers/request.h"

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
 * Helper for Completion Callback
 * ================================================================ */

static res_t test_completion_cb(const ik_provider_completion_t *completion, void *ctx)
{
    (void)completion;
    (void)ctx;
    return OK(NULL);
}

static res_t noop_stream_cb(const ik_stream_event_t *e, void *c)
{
    (void)e;
    (void)c;
    return OK(NULL);
}

/* ================================================================
 * Vtable Method Tests
 * ================================================================ */

// Test google_fdset vtable method
START_TEST(test_google_fdset)
{
    ik_provider_t *provider = NULL;
    res_t result = ik_google_create(test_ctx, "test-api-key", &provider);
    ck_assert(!is_err(&result));

    fd_set read_fds, write_fds, exc_fds;
    int max_fd = 0;
    FD_ZERO(&read_fds);
    FD_ZERO(&write_fds);
    FD_ZERO(&exc_fds);

    res_t r = provider->vt->fdset(provider->ctx, &read_fds, &write_fds, &exc_fds, &max_fd);

    // Should succeed (delegates to http_multi)
    ck_assert(!is_err(&r));
}
END_TEST

// Test google_perform vtable method
START_TEST(test_google_perform)
{
    ik_provider_t *provider = NULL;
    res_t result = ik_google_create(test_ctx, "test-api-key", &provider);
    ck_assert(!is_err(&result));

    int running_handles = 0;
    res_t r = provider->vt->perform(provider->ctx, &running_handles);

    // Should succeed (delegates to http_multi)
    ck_assert(!is_err(&r));
}
END_TEST

// Test google_timeout vtable method
START_TEST(test_google_timeout)
{
    ik_provider_t *provider = NULL;
    res_t result = ik_google_create(test_ctx, "test-api-key", &provider);
    ck_assert(!is_err(&result));

    long timeout_ms = 0;
    res_t r = provider->vt->timeout(provider->ctx, &timeout_ms);

    // Should succeed (delegates to http_multi)
    ck_assert(!is_err(&r));
}
END_TEST

// Test google_cleanup vtable method
START_TEST(test_google_cleanup)
{
    ik_provider_t *provider = NULL;
    res_t result = ik_google_create(test_ctx, "test-api-key", &provider);
    ck_assert(!is_err(&result));

    // Call cleanup - should not crash
    provider->vt->cleanup(provider->ctx);
}
END_TEST

// Test google_start_request vtable method (wrapper - line 246-255)
START_TEST(test_google_start_request)
{
    ik_provider_t *provider = NULL;
    res_t result = ik_google_create(test_ctx, "test-api-key", &provider);
    ck_assert(!is_err(&result));

    // Create minimal request
    ik_request_t req = {0};
    req.model = talloc_strdup(test_ctx, "gemini-2.5-flash");

    // Call start_request - delegates to ik_google_start_request (which is a stub)
    res_t r = provider->vt->start_request(provider->ctx, &req, test_completion_cb, NULL);

    // Should succeed (stub returns OK)
    ck_assert(!is_err(&r));
}
END_TEST

// Test google_start_stream vtable method (wrapper - lines 258-334)
START_TEST(test_google_start_stream)
{
    ik_provider_t *provider = NULL;
    res_t result = ik_google_create(test_ctx, "test-api-key", &provider);
    ck_assert(!is_err(&result));

    // Create minimal request
    ik_request_t req = {0};
    req.model = talloc_strdup(test_ctx, "gemini-2.5-flash");

    // Call start_stream
    res_t r = provider->vt->start_stream(provider->ctx, &req, noop_stream_cb, NULL,
                                          test_completion_cb, NULL);

    // Should succeed
    ck_assert(!is_err(&r));
}
END_TEST

/* ================================================================
 * Test Suite Setup
 * ================================================================ */

static Suite *google_coverage_vtable_suite(void)
{
    Suite *s = suite_create("Google Coverage - Vtable");

    TCase *tc_vtable = tcase_create("Vtable Tests");
    tcase_set_timeout(tc_vtable, 30);
    tcase_add_unchecked_fixture(tc_vtable, setup, teardown);

    // Vtable methods
    tcase_add_test(tc_vtable, test_google_fdset);
    tcase_add_test(tc_vtable, test_google_perform);
    tcase_add_test(tc_vtable, test_google_timeout);
    tcase_add_test(tc_vtable, test_google_cleanup);
    tcase_add_test(tc_vtable, test_google_start_request);
    tcase_add_test(tc_vtable, test_google_start_stream);

    suite_add_tcase(s, tc_vtable);

    return s;
}

int main(void)
{
    Suite *s = google_coverage_vtable_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
