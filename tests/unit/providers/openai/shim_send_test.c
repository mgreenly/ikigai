#include "providers/openai/shim.h"
#include "providers/provider.h"
#include "providers/request.h"
#include "error.h"

#include <check.h>
#include <stdlib.h>
#include <string.h>
#include <talloc.h>

/* Test fixture */
static TALLOC_CTX *test_ctx;
static ik_provider_t *provider;

static void setup(void)
{
    test_ctx = talloc_new(NULL);

    /* Create provider with test API key */
    res_t result = ik_openai_shim_create(test_ctx, "test-api-key", &provider);
    ck_assert(!is_err(&result));
    ck_assert_ptr_nonnull(provider);
}

static void teardown(void)
{
    talloc_free(test_ctx);
}

/* ================================================================
 * Provider Creation Tests
 * ================================================================ */

START_TEST(test_create_provider_success)
{
    ik_provider_t *test_provider = NULL;
    res_t result = ik_openai_shim_create(test_ctx, "valid-api-key", &test_provider);

    ck_assert(!is_err(&result));
    ck_assert_ptr_nonnull(test_provider);
    ck_assert_ptr_nonnull(test_provider->vt);
    ck_assert_ptr_nonnull(test_provider->ctx);
}
END_TEST

START_TEST(test_create_provider_missing_credentials)
{
    ik_provider_t *test_provider = NULL;
    res_t result = ik_openai_shim_create(test_ctx, NULL, &test_provider);

    ck_assert(is_err(&result));
    ck_assert_int_eq(result.err->code, ERR_MISSING_CREDENTIALS);
}
END_TEST

START_TEST(test_create_provider_empty_credentials)
{
    ik_provider_t *test_provider = NULL;
    res_t result = ik_openai_shim_create(test_ctx, "", &test_provider);

    ck_assert(is_err(&result));
    ck_assert_int_eq(result.err->code, ERR_MISSING_CREDENTIALS);
}
END_TEST

/* ================================================================
 * Request Validation Tests
 * ================================================================ */

/* Dummy completion callback for tests */
static res_t dummy_completion_cb(const ik_provider_completion_t *completion, void *ctx)
{
    (void)completion;
    (void)ctx;
    return OK(NULL);
}

/* Dummy stream callback for tests */
static res_t dummy_stream_cb(const ik_stream_event_t *event, void *ctx)
{
    (void)event;
    (void)ctx;
    return OK(NULL);
}

START_TEST(test_start_request_empty_messages)
{
    /* Create request with no messages */
    ik_request_t *req = NULL;
    res_t create_res = ik_request_create(test_ctx, "gpt-5-mini", &req);
    ck_assert(!is_err(&create_res));

    /* Try to start request - should fail */
    res_t result = provider->vt->start_request(
        provider->ctx,
        req,
        dummy_completion_cb,
        NULL   /* completion_ctx */
    );

    ck_assert(is_err(&result));
    ck_assert_int_eq(result.err->code, ERR_INVALID_ARG);
}
END_TEST

/* ================================================================
 * Vtable Integration Tests
 * ================================================================ */

START_TEST(test_vtable_methods_exist)
{
    ck_assert_ptr_nonnull(provider->vt);
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

START_TEST(test_fdset_basic)
{
    fd_set read_fds, write_fds, exc_fds;
    int max_fd = 0;

    FD_ZERO(&read_fds);
    FD_ZERO(&write_fds);
    FD_ZERO(&exc_fds);

    res_t result = provider->vt->fdset(
        provider->ctx,
        &read_fds,
        &write_fds,
        &exc_fds,
        &max_fd
    );

    /* Should succeed even with no pending requests */
    ck_assert(!is_err(&result));
}
END_TEST

START_TEST(test_perform_basic)
{
    int running_handles = 0;

    res_t result = provider->vt->perform(
        provider->ctx,
        &running_handles
    );

    /* Should succeed even with no pending requests */
    ck_assert(!is_err(&result));
    ck_assert_int_eq(running_handles, 0);
}
END_TEST

START_TEST(test_timeout_basic)
{
    long timeout_ms = 0;

    res_t result = provider->vt->timeout(
        provider->ctx,
        &timeout_ms
    );

    /* Should succeed */
    ck_assert(!is_err(&result));
}
END_TEST

START_TEST(test_info_read_basic)
{
    /* info_read should not crash with NULL logger */
    provider->vt->info_read(provider->ctx, NULL);

    /* If we get here, it didn't crash */
    ck_assert(1);
}
END_TEST

START_TEST(test_start_stream_requires_callbacks)
{
    /* Create minimal request */
    ik_request_t *req = NULL;
    res_t create_res = ik_request_create(test_ctx, "gpt-5-mini", &req);
    ck_assert(!is_err(&create_res));

    ik_request_add_message(req, IK_ROLE_USER, "test");

    /* start_stream with valid callbacks should succeed (won't actually make HTTP call) */
    res_t result = provider->vt->start_stream(
        provider->ctx,
        req,
        dummy_stream_cb,  /* stream_cb */
        NULL,  /* stream_ctx */
        dummy_completion_cb,  /* completion_cb */
        NULL   /* completion_ctx */
    );

    /* Should succeed - request is queued but won't actually execute without perform() */
    ck_assert(!is_err(&result));
}
END_TEST

START_TEST(test_cleanup_does_not_crash)
{
    /* cleanup should be safe to call */
    provider->vt->cleanup(provider->ctx);

    /* If we get here, it didn't crash */
    ck_assert(1);
}
END_TEST

START_TEST(test_cancel_does_not_crash)
{
    /* cancel should be safe to call */
    provider->vt->cancel(provider->ctx);

    /* If we get here, it didn't crash */
    ck_assert(1);
}
END_TEST

/* ================================================================
 * Test Suite Definition
 * ================================================================ */

static Suite *shim_send_suite(void)
{
    Suite *s = suite_create("OpenAI Shim Send Integration");

    /* Provider creation tests */
    TCase *tc_create = tcase_create("Provider Creation");
    tcase_add_checked_fixture(tc_create, setup, teardown);
    tcase_add_test(tc_create, test_create_provider_success);
    tcase_add_test(tc_create, test_create_provider_missing_credentials);
    tcase_add_test(tc_create, test_create_provider_empty_credentials);
    suite_add_tcase(s, tc_create);

    /* Request validation tests */
    TCase *tc_validate = tcase_create("Request Validation");
    tcase_add_checked_fixture(tc_validate, setup, teardown);
    tcase_add_test(tc_validate, test_start_request_empty_messages);
    suite_add_tcase(s, tc_validate);

    /* Vtable integration tests */
    TCase *tc_vtable = tcase_create("Vtable Integration");
    tcase_add_checked_fixture(tc_vtable, setup, teardown);
    tcase_add_test(tc_vtable, test_vtable_methods_exist);
    tcase_add_test(tc_vtable, test_fdset_basic);
    tcase_add_test(tc_vtable, test_perform_basic);
    tcase_add_test(tc_vtable, test_timeout_basic);
    tcase_add_test(tc_vtable, test_info_read_basic);
    tcase_add_test(tc_vtable, test_start_stream_requires_callbacks);
    tcase_add_test(tc_vtable, test_cleanup_does_not_crash);
    tcase_add_test(tc_vtable, test_cancel_does_not_crash);
    suite_add_tcase(s, tc_vtable);

    return s;
}

int main(void)
{
    int failed;
    Suite *s = shim_send_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
