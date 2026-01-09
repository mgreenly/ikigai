#include "../../../test_constants.h"
/**
 * @file google_coverage_info_read_test.c
 * @brief Coverage tests for Google provider info_read and cancel functionality
 */

#include <check.h>
#include <talloc.h>

#include "error.h"
#include "logger.h"
#include "providers/google/google.h"
#include "providers/google/google_internal.h"
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
 * Helper Globals for Completion Callback Tracking
 * ================================================================ */

static int completion_cb_called = 0;
static bool completion_success = false;
static int completion_http_status = 0;
static ik_error_category_t completion_error_category = IK_ERR_CAT_UNKNOWN;

static res_t test_completion_cb(const ik_provider_completion_t *completion, void *ctx)
{
    (void)ctx; // Unused
    completion_cb_called++;
    completion_success = completion->success;
    completion_http_status = completion->http_status;
    completion_error_category = completion->error_category;
    return OK(NULL);
}

/* ================================================================
 * Info Read Tests
 * ================================================================ */

// Test line 219 branch 3: NULL active_stream in google_info_read
START_TEST(test_google_info_read_null_active_stream) {
    ik_provider_t *provider = NULL;
    res_t result = ik_google_create(test_ctx, "test-api-key", &provider);
    ck_assert(!is_err(&result));

    ik_logger_t *logger = ik_logger_create(test_ctx, "/tmp");

    // Call info_read with no active stream (NULL)
    provider->vt->info_read(provider->ctx, logger);

    // Should not crash
}
END_TEST

// Test line 198 branch 3: active_stream exists but not completed
START_TEST(test_google_info_read_active_stream_not_completed) {
    ik_provider_t *provider = NULL;
    res_t result = ik_google_create(test_ctx, "test-api-key", &provider);
    ck_assert(!is_err(&result));

    ik_google_ctx_t *impl_ctx = (ik_google_ctx_t *)provider->ctx;

    // Create a fake active stream that is NOT completed
    ik_google_active_stream_t *stream = talloc_zero(impl_ctx, ik_google_active_stream_t);
    stream->completed = false; // NOT completed
    stream->http_status = 200;
    stream->completion_cb = test_completion_cb;
    stream->completion_ctx = NULL;

    impl_ctx->active_stream = stream;

    completion_cb_called = 0;

    ik_logger_t *logger = ik_logger_create(test_ctx, "/tmp");
    provider->vt->info_read(provider->ctx, logger);

    // Completion callback should NOT be called (stream not complete yet)
    ck_assert_int_eq(completion_cb_called, 0);
}
END_TEST

// Test line 225 branch 1: Non-2xx HTTP status (error path)
START_TEST(test_google_info_read_error_status) {
    ik_provider_t *provider = NULL;
    res_t result = ik_google_create(test_ctx, "test-api-key", &provider);
    ck_assert(!is_err(&result));

    ik_google_ctx_t *impl_ctx = (ik_google_ctx_t *)provider->ctx;

    // Create a fake active stream with error status
    ik_google_active_stream_t *stream = talloc_zero(impl_ctx, ik_google_active_stream_t);
    stream->completed = true;
    stream->http_status = 400;
    stream->completion_cb = test_completion_cb;
    stream->completion_ctx = NULL;

    impl_ctx->active_stream = stream;

    completion_cb_called = 0;

    ik_logger_t *logger = ik_logger_create(test_ctx, "/tmp");
    provider->vt->info_read(provider->ctx, logger);

    // Completion callback should be called with error
    ck_assert_int_eq(completion_cb_called, 1);
    ck_assert(!completion_success);
    ck_assert_int_eq(completion_http_status, 400);
}
END_TEST

// Test line 238: 401/403 auth error
START_TEST(test_google_info_read_auth_error_401) {
    ik_provider_t *provider = NULL;
    res_t result = ik_google_create(test_ctx, "test-api-key", &provider);
    ck_assert(!is_err(&result));

    ik_google_ctx_t *impl_ctx = (ik_google_ctx_t *)provider->ctx;

    ik_google_active_stream_t *stream = talloc_zero(impl_ctx, ik_google_active_stream_t);
    stream->completed = true;
    stream->http_status = 401;
    stream->completion_cb = test_completion_cb;
    stream->completion_ctx = NULL;

    impl_ctx->active_stream = stream;

    completion_cb_called = 0;

    ik_logger_t *logger = ik_logger_create(test_ctx, "/tmp");
    provider->vt->info_read(provider->ctx, logger);

    ck_assert_int_eq(completion_cb_called, 1);
    ck_assert(!completion_success);
    ck_assert_int_eq(completion_error_category, IK_ERR_CAT_AUTH);
}
END_TEST

START_TEST(test_google_info_read_auth_error_403) {
    ik_provider_t *provider = NULL;
    res_t result = ik_google_create(test_ctx, "test-api-key", &provider);
    ck_assert(!is_err(&result));

    ik_google_ctx_t *impl_ctx = (ik_google_ctx_t *)provider->ctx;

    ik_google_active_stream_t *stream = talloc_zero(impl_ctx, ik_google_active_stream_t);
    stream->completed = true;
    stream->http_status = 403;
    stream->completion_cb = test_completion_cb;
    stream->completion_ctx = NULL;

    impl_ctx->active_stream = stream;

    completion_cb_called = 0;

    ik_logger_t *logger = ik_logger_create(test_ctx, "/tmp");
    provider->vt->info_read(provider->ctx, logger);

    ck_assert_int_eq(completion_cb_called, 1);
    ck_assert(!completion_success);
    ck_assert_int_eq(completion_error_category, IK_ERR_CAT_AUTH);
}
END_TEST

// Test line 240: 429 rate limit error
START_TEST(test_google_info_read_rate_limit_error) {
    ik_provider_t *provider = NULL;
    res_t result = ik_google_create(test_ctx, "test-api-key", &provider);
    ck_assert(!is_err(&result));

    ik_google_ctx_t *impl_ctx = (ik_google_ctx_t *)provider->ctx;

    ik_google_active_stream_t *stream = talloc_zero(impl_ctx, ik_google_active_stream_t);
    stream->completed = true;
    stream->http_status = 429;
    stream->completion_cb = test_completion_cb;
    stream->completion_ctx = NULL;

    impl_ctx->active_stream = stream;

    completion_cb_called = 0;

    ik_logger_t *logger = ik_logger_create(test_ctx, "/tmp");
    provider->vt->info_read(provider->ctx, logger);

    ck_assert_int_eq(completion_cb_called, 1);
    ck_assert(!completion_success);
    ck_assert_int_eq(completion_error_category, IK_ERR_CAT_RATE_LIMIT);
}
END_TEST

// Test line 242: 5xx server error
START_TEST(test_google_info_read_server_error) {
    ik_provider_t *provider = NULL;
    res_t result = ik_google_create(test_ctx, "test-api-key", &provider);
    ck_assert(!is_err(&result));

    ik_google_ctx_t *impl_ctx = (ik_google_ctx_t *)provider->ctx;

    ik_google_active_stream_t *stream = talloc_zero(impl_ctx, ik_google_active_stream_t);
    stream->completed = true;
    stream->http_status = 500;
    stream->completion_cb = test_completion_cb;
    stream->completion_ctx = NULL;

    impl_ctx->active_stream = stream;

    completion_cb_called = 0;

    ik_logger_t *logger = ik_logger_create(test_ctx, "/tmp");
    provider->vt->info_read(provider->ctx, logger);

    ck_assert_int_eq(completion_cb_called, 1);
    ck_assert(!completion_success);
    ck_assert_int_eq(completion_error_category, IK_ERR_CAT_SERVER);
}
END_TEST

// Test line 204 branch: status < 200 (informational/redirect)
START_TEST(test_google_info_read_status_below_200) {
    ik_provider_t *provider = NULL;
    res_t result = ik_google_create(test_ctx, "test-api-key", &provider);
    ck_assert(!is_err(&result));

    ik_google_ctx_t *impl_ctx = (ik_google_ctx_t *)provider->ctx;

    ik_google_active_stream_t *stream = talloc_zero(impl_ctx, ik_google_active_stream_t);
    stream->completed = true;
    stream->http_status = 100; // Informational status
    stream->completion_cb = test_completion_cb;
    stream->completion_ctx = NULL;

    impl_ctx->active_stream = stream;

    completion_cb_called = 0;

    ik_logger_t *logger = ik_logger_create(test_ctx, "/tmp");
    provider->vt->info_read(provider->ctx, logger);

    ck_assert_int_eq(completion_cb_called, 1);
    ck_assert(!completion_success); // Not a success
    ck_assert_int_eq(completion_http_status, 100);
}
END_TEST

// Test line 253: NULL completion_cb
START_TEST(test_google_info_read_null_completion_cb) {
    ik_provider_t *provider = NULL;
    res_t result = ik_google_create(test_ctx, "test-api-key", &provider);
    ck_assert(!is_err(&result));

    ik_google_ctx_t *impl_ctx = (ik_google_ctx_t *)provider->ctx;

    ik_google_active_stream_t *stream = talloc_zero(impl_ctx, ik_google_active_stream_t);
    stream->completed = true;
    stream->http_status = 200;
    stream->completion_cb = NULL; // NULL callback
    stream->completion_ctx = NULL;

    impl_ctx->active_stream = stream;

    ik_logger_t *logger = ik_logger_create(test_ctx, "/tmp");

    // Should not crash with NULL completion callback
    provider->vt->info_read(provider->ctx, logger);
}
END_TEST

// Test line 204: success path (200-299 status) in info_read
START_TEST(test_google_info_read_success_status) {
    ik_provider_t *provider = NULL;
    res_t result = ik_google_create(test_ctx, "test-api-key", &provider);
    ck_assert(!is_err(&result));

    ik_google_ctx_t *impl_ctx = (ik_google_ctx_t *)provider->ctx;

    ik_google_active_stream_t *stream = talloc_zero(impl_ctx, ik_google_active_stream_t);
    stream->completed = true;
    stream->http_status = 200;
    stream->completion_cb = test_completion_cb;
    stream->completion_ctx = NULL;

    impl_ctx->active_stream = stream;

    completion_cb_called = 0;

    ik_logger_t *logger = ik_logger_create(test_ctx, "/tmp");
    provider->vt->info_read(provider->ctx, logger);

    // Completion callback should be called with success
    ck_assert_int_eq(completion_cb_called, 1);
    ck_assert(completion_success);
    ck_assert_int_eq(completion_http_status, 200);
}
END_TEST

// Test line 237: non-NULL error_message path
START_TEST(test_google_info_read_error_message_cleanup) {
    ik_provider_t *provider = NULL;
    res_t result = ik_google_create(test_ctx, "test-api-key", &provider);
    ck_assert(!is_err(&result));

    ik_google_ctx_t *impl_ctx = (ik_google_ctx_t *)provider->ctx;

    ik_google_active_stream_t *stream = talloc_zero(impl_ctx, ik_google_active_stream_t);
    stream->completed = true;
    stream->http_status = 404;
    stream->completion_cb = test_completion_cb;
    stream->completion_ctx = NULL;

    impl_ctx->active_stream = stream;

    completion_cb_called = 0;

    ik_logger_t *logger = ik_logger_create(test_ctx, "/tmp");
    provider->vt->info_read(provider->ctx, logger);

    // Should have cleaned up error message
    ck_assert_int_eq(completion_cb_called, 1);
    ck_assert(!completion_success);
}
END_TEST

/* ================================================================
 * Cancel Tests
 * ================================================================ */

// Test line 373-374: NULL active_stream in cancel
START_TEST(test_google_cancel_null_active_stream) {
    ik_provider_t *provider = NULL;
    res_t result = ik_google_create(test_ctx, "test-api-key", &provider);
    ck_assert(!is_err(&result));

    // Call cancel with no active stream
    provider->vt->cancel(provider->ctx);

    // Should not crash
}
END_TEST

// Test line 373-374: Non-NULL active_stream in cancel
START_TEST(test_google_cancel_with_active_stream) {
    ik_provider_t *provider = NULL;
    res_t result = ik_google_create(test_ctx, "test-api-key", &provider);
    ck_assert(!is_err(&result));

    ik_google_ctx_t *impl_ctx = (ik_google_ctx_t *)provider->ctx;

    // Create a fake active stream
    ik_google_active_stream_t *stream = talloc_zero(impl_ctx, ik_google_active_stream_t);
    stream->completed = false;
    impl_ctx->active_stream = stream;

    // Call cancel - should mark as completed
    provider->vt->cancel(provider->ctx);

    ck_assert(stream->completed);
}
END_TEST

/* ================================================================
 * Test Suite Setup
 * ================================================================ */

static Suite *google_coverage_info_read_suite(void)
{
    Suite *s = suite_create("Google Coverage - Info Read");

    TCase *tc_info_read = tcase_create("Info Read Tests");
    tcase_set_timeout(tc_info_read, IK_TEST_TIMEOUT);
    tcase_add_unchecked_fixture(tc_info_read, setup, teardown);

    // Line 219: NULL active_stream in info_read
    tcase_add_test(tc_info_read, test_google_info_read_null_active_stream);

    // Line 198 branch 3: active_stream not completed
    tcase_add_test(tc_info_read, test_google_info_read_active_stream_not_completed);

    // Line 225: Error status path
    tcase_add_test(tc_info_read, test_google_info_read_error_status);

    // Line 238-245: Error category mapping
    tcase_add_test(tc_info_read, test_google_info_read_auth_error_401);
    tcase_add_test(tc_info_read, test_google_info_read_auth_error_403);
    tcase_add_test(tc_info_read, test_google_info_read_rate_limit_error);
    tcase_add_test(tc_info_read, test_google_info_read_server_error);

    // Line 204: status < 200
    tcase_add_test(tc_info_read, test_google_info_read_status_below_200);

    // Line 253: NULL completion_cb
    tcase_add_test(tc_info_read, test_google_info_read_null_completion_cb);

    // Success path and error message cleanup
    tcase_add_test(tc_info_read, test_google_info_read_success_status);
    tcase_add_test(tc_info_read, test_google_info_read_error_message_cleanup);

    // Line 373-374: cancel with/without active_stream
    tcase_add_test(tc_info_read, test_google_cancel_null_active_stream);
    tcase_add_test(tc_info_read, test_google_cancel_with_active_stream);

    suite_add_tcase(s, tc_info_read);

    return s;
}

int main(void)
{
    Suite *s = google_coverage_info_read_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
