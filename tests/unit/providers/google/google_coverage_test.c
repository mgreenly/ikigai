/**
 * @file google_coverage_test.c
 * @brief Coverage tests for Google provider google.c
 */

#include <check.h>
#include <talloc.h>
#include <sys/select.h>
#include <string.h>

#include "error.h"
#include "logger.h"
#include "providers/common/http_multi.h"
#include "providers/common/sse_parser.h"
#include "providers/google/google.h"
#include "providers/google/google_internal.h"
#include "providers/google/streaming.h"
#include "providers/provider.h"
#include "providers/request.h"
#include "wrapper_internal.h"

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
 * Coverage Tests
 * ================================================================ */

// Test line 113 branches 1 and 2: NULL stream and NULL sse_parser
START_TEST(test_google_stream_write_cb_null_stream)
{
    // Test with NULL stream context
    const char *test_data = "test data";
    size_t result = ik_google_stream_write_cb(test_data, 10, NULL);

    // Should return len without crashing
    ck_assert_uint_eq(result, 10);
}
END_TEST

START_TEST(test_google_stream_write_cb_null_sse_parser)
{
    // Create a stream context but with NULL sse_parser
    ik_google_active_stream_t stream = {0};
    stream.stream_ctx = (void*)1; // Non-null but sse_parser is NULL
    stream.sse_parser = NULL;

    const char *test_data = "test data";
    size_t result = ik_google_stream_write_cb(test_data, 10, &stream);

    // Should return len without crashing
    ck_assert_uint_eq(result, 10);
}
END_TEST


// Test line 166: NULL stream in completion callback
START_TEST(test_google_stream_completion_cb_null_stream)
{
    ik_http_completion_t completion = {0};
    completion.http_code = 200;

    // Should not crash with NULL ctx
    ik_google_stream_completion_cb(&completion, NULL);
}
END_TEST

// Test line 219 branch 3: NULL active_stream in google_info_read
START_TEST(test_google_info_read_null_active_stream)
{
    ik_provider_t *provider = NULL;
    res_t result = ik_google_create(test_ctx, "test-api-key", &provider);
    ck_assert(!is_err(&result));

    ik_logger_t *logger = ik_logger_create(test_ctx, "/tmp");

    // Call info_read with no active stream (NULL)
    provider->vt->info_read(provider->ctx, logger);

    // Should not crash
}
END_TEST

// Helper globals for tracking callback invocations
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

// Test line 225 branch 1: Non-2xx HTTP status (error path)
START_TEST(test_google_info_read_error_status)
{
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
START_TEST(test_google_info_read_auth_error_401)
{
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

START_TEST(test_google_info_read_auth_error_403)
{
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
START_TEST(test_google_info_read_rate_limit_error)
{
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
START_TEST(test_google_info_read_server_error)
{
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

// Test line 253: NULL completion_cb
START_TEST(test_google_info_read_null_completion_cb)
{
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

// Test line 373-374: NULL active_stream in cancel
START_TEST(test_google_cancel_null_active_stream)
{
    ik_provider_t *provider = NULL;
    res_t result = ik_google_create(test_ctx, "test-api-key", &provider);
    ck_assert(!is_err(&result));

    // Call cancel with no active stream
    provider->vt->cancel(provider->ctx);

    // Should not crash
}
END_TEST

// Test line 373-374: Non-NULL active_stream in cancel
START_TEST(test_google_cancel_with_active_stream)
{
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

// Helper callback for testing streaming with valid data
static int stream_cb_called = 0;
static res_t test_stream_callback(const ik_stream_event_t *event, void *ctx)
{
    (void)event;
    (void)ctx;
    stream_cb_called++;
    return OK(NULL);
}

// Test lines 118-134: Normal streaming path with valid data
START_TEST(test_google_stream_write_cb_with_valid_data)
{
    // Create a minimal stream context with valid parser
    ik_google_active_stream_t *stream = talloc_zero(test_ctx, ik_google_active_stream_t);

    // Create streaming context
    res_t r = ik_google_stream_ctx_create(stream, test_stream_callback, NULL, &stream->stream_ctx);
    ck_assert(!is_err(&r));

    // Create SSE parser
    stream->sse_parser = ik_sse_parser_create(stream);
    ck_assert(stream->sse_parser != NULL);

    // Feed some SSE data
    const char *test_data = "data: {\"test\": \"data\"}\n\n";
    size_t result = ik_google_stream_write_cb(test_data, strlen(test_data), stream);

    // Should return the full length
    ck_assert_uint_eq(result, strlen(test_data));

    // Clean up
    talloc_free(stream);
}
END_TEST

// Test lines 149-150: Stream completion with non-NULL stream
START_TEST(test_google_stream_completion_cb_with_valid_stream)
{
    // Create a stream context
    ik_google_active_stream_t *stream = talloc_zero(test_ctx, ik_google_active_stream_t);
    stream->completed = false;
    stream->http_status = 0;

    // Create completion info
    ik_http_completion_t completion = {0};
    completion.http_code = 200;

    // Call completion callback
    ik_google_stream_completion_cb(&completion, stream);

    // Should have set completed and status
    ck_assert(stream->completed);
    ck_assert_int_eq(stream->http_status, 200);

    // Clean up
    talloc_free(stream);
}
END_TEST

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

// Test line 204: success path (200-299 status) in info_read
START_TEST(test_google_info_read_success_status)
{
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
START_TEST(test_google_info_read_error_message_cleanup)
{
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
 * Test Suite Setup
 * ================================================================ */

static Suite *google_coverage_suite(void)
{
    Suite *s = suite_create("Google Coverage");

    TCase *tc_coverage = tcase_create("Coverage Tests");
    tcase_set_timeout(tc_coverage, 30);
    tcase_add_unchecked_fixture(tc_coverage, setup, teardown);

    // Line 113: NULL checks in stream_write_cb
    tcase_add_test(tc_coverage, test_google_stream_write_cb_null_stream);
    tcase_add_test(tc_coverage, test_google_stream_write_cb_null_sse_parser);

    // Line 166: NULL stream in completion callback
    tcase_add_test(tc_coverage, test_google_stream_completion_cb_null_stream);

    // Line 219: NULL active_stream in info_read
    tcase_add_test(tc_coverage, test_google_info_read_null_active_stream);

    // Line 225: Error status path
    tcase_add_test(tc_coverage, test_google_info_read_error_status);

    // Line 238-245: Error category mapping
    tcase_add_test(tc_coverage, test_google_info_read_auth_error_401);
    tcase_add_test(tc_coverage, test_google_info_read_auth_error_403);
    tcase_add_test(tc_coverage, test_google_info_read_rate_limit_error);
    tcase_add_test(tc_coverage, test_google_info_read_server_error);

    // Line 253: NULL completion_cb
    tcase_add_test(tc_coverage, test_google_info_read_null_completion_cb);

    // Line 373-374: cancel with/without active_stream
    tcase_add_test(tc_coverage, test_google_cancel_null_active_stream);
    tcase_add_test(tc_coverage, test_google_cancel_with_active_stream);

    // Vtable methods
    tcase_add_test(tc_coverage, test_google_fdset);
    tcase_add_test(tc_coverage, test_google_perform);
    tcase_add_test(tc_coverage, test_google_timeout);
    tcase_add_test(tc_coverage, test_google_cleanup);

    // Success path and error message cleanup
    tcase_add_test(tc_coverage, test_google_info_read_success_status);
    tcase_add_test(tc_coverage, test_google_info_read_error_message_cleanup);

    // Streaming with valid data (lines 118-134, 149-150)
    tcase_add_test(tc_coverage, test_google_stream_write_cb_with_valid_data);
    tcase_add_test(tc_coverage, test_google_stream_completion_cb_with_valid_stream);

    suite_add_tcase(s, tc_coverage);

    return s;
}

int main(void)
{
    Suite *s = google_coverage_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
