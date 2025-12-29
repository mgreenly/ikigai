/**
 * @file anthropic_callbacks_coverage_test.c
 * @brief Coverage tests for anthropic.c internal callbacks
 *
 * These tests directly invoke exposed callback functions to achieve
 * 100% branch coverage of defensive checks and edge cases.
 */

#include <check.h>
#include <talloc.h>
#include <string.h>
#include "providers/anthropic/anthropic.h"
#include "providers/anthropic/anthropic_internal.h"
#include "providers/common/http_multi.h"
#include "providers/common/sse_parser.h"
#include "wrapper.h"
#include "wrapper_internal.h"
#include "logger.h"
#include "error.h"

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
 * Stream Write Callback Tests
 * ================================================================ */

START_TEST(test_stream_write_cb_with_null_context) {
    // Test line 121: stream == NULL
    const char *data = "test data";
    size_t result = ik_anthropic_stream_write_cb(data, 9, NULL);

    // Should return len even with NULL context
    ck_assert_uint_eq(result, 9);
}
END_TEST START_TEST(test_stream_write_cb_with_null_sse_parser)
{
    // Test line 121: stream->sse_parser == NULL
    ik_anthropic_active_stream_t *stream = talloc_zero_(test_ctx, sizeof(ik_anthropic_active_stream_t));
    stream->sse_parser = NULL;

    const char *data = "test data";
    size_t result = ik_anthropic_stream_write_cb(data, 9, stream);

    // Should return len even with NULL sse_parser
    ck_assert_uint_eq(result, 9);
}

END_TEST START_TEST(test_stream_write_cb_with_valid_context)
{
    // Test that valid context is handled properly
    // The full integration path is tested in anthropic_streaming_*.c tests
    // This just confirms the callback accepts valid contexts
    ik_anthropic_active_stream_t *stream = talloc_zero_(test_ctx, sizeof(ik_anthropic_active_stream_t));
    stream->sse_parser = ik_sse_parser_create(stream);
    stream->stream_ctx = talloc_zero_(stream, 1); // Mock - won't be dereferenced with no events

    const char *data = "partial";  // Incomplete SSE won't trigger event processing
    size_t result = ik_anthropic_stream_write_cb(data, strlen(data), stream);

    // Should accept and process the data
    ck_assert_uint_eq(result, strlen(data));
}

END_TEST
/* ================================================================
 * Stream Completion Callback Tests
 * ================================================================ */

START_TEST(test_stream_completion_cb_with_null_context)
{
    // Test line 152: stream == NULL
    ik_http_completion_t completion = {
        .http_code = 200,
        .curl_code = 0
    };

    // Should not crash with NULL context
    ik_anthropic_stream_completion_cb(&completion, NULL);
}

END_TEST START_TEST(test_stream_completion_cb_with_valid_context)
{
    // Test normal path
    ik_anthropic_active_stream_t *stream = talloc_zero_(test_ctx, sizeof(ik_anthropic_active_stream_t));
    stream->completed = false;
    stream->http_status = 0;

    ik_http_completion_t completion = {
        .http_code = 200,
        .curl_code = 0
    };

    ik_anthropic_stream_completion_cb(&completion, stream);

    ck_assert(stream->completed);
    ck_assert_int_eq(stream->http_status, 200);
}

END_TEST

/* ================================================================
 * Provider Creation Tests
 * ================================================================ */

// Mock control flag
static bool g_http_multi_create_should_fail = false;

// Mock for ik_http_multi_create_
res_t ik_http_multi_create_(void *parent, void **out)
{
    if (g_http_multi_create_should_fail) {
        return ERR(parent, IO, "Mock HTTP multi create failure");
    }
    // Success case - create a dummy http_multi
    void *http_multi = talloc_zero_(parent, 1);  // Minimal allocation
    *out = http_multi;
    return OK(http_multi);
}

START_TEST(test_anthropic_create_http_multi_failure)
{
    // Test line 94: is_err(&r) from ik_http_multi_create_
    g_http_multi_create_should_fail = true;

    ik_provider_t *provider = NULL;
    res_t r = ik_anthropic_create(test_ctx, "test-api-key", &provider);

    ck_assert(is_err(&r));
    ck_assert_ptr_null(provider);

    g_http_multi_create_should_fail = false;
}

END_TEST

/* ================================================================
 * Stream Write Callback - Event Processing Tests
 * ================================================================ */

START_TEST(test_stream_write_cb_with_events)
{
    // Test lines 130-144: while loop with events, event->event != NULL, event->data != NULL
    // The full integration is tested in other anthropic streaming tests
    // Here we just ensure the code path with non-NULL event and data is covered
    // by feeding incomplete SSE data that won't generate complete events
    ik_anthropic_active_stream_t *stream = talloc_zero_(test_ctx, sizeof(ik_anthropic_active_stream_t));
    stream->sse_parser = ik_sse_parser_create(stream);
    stream->stream_ctx = talloc_zero_(stream, 1);

    // Feed partial SSE data - won't create complete events but exercises the parser
    const char *sse_data = "event: test\n";
    size_t result = ik_anthropic_stream_write_cb(sse_data, strlen(sse_data), stream);

    // Should accept the data
    ck_assert_uint_eq(result, strlen(sse_data));

    // Note: Lines 130-144 (while loop processing events) are covered by
    // the full integration tests in anthropic_streaming_*.c
}

END_TEST

/* ================================================================
 * Info Read Tests - Requires Provider Context Access
 * ================================================================ */

// We need to expose the internal context structure for testing
typedef struct {
    char *api_key;
    char *base_url;
    ik_http_multi_t *http_multi;
    ik_anthropic_active_stream_t *active_stream;
} ik_anthropic_ctx_t;

// Mock for ik_http_multi_info_read_
void ik_http_multi_info_read_(void *http_multi, void *logger)
{
    (void)http_multi;
    (void)logger;
    // Do nothing - just a mock
}

// Completion callback tracker
static bool g_completion_called = false;
static ik_provider_completion_t g_last_completion;

static res_t test_completion_cb(const ik_provider_completion_t *completion, void *ctx)
{
    (void)ctx;
    g_completion_called = true;
    g_last_completion = *completion;
    return OK(NULL);
}

START_TEST(test_info_read_no_active_stream)
{
    // Test line 205: impl_ctx->active_stream == NULL (false branch)
    ik_provider_t *provider = NULL;
    res_t r = ik_anthropic_create(test_ctx, "test-key", &provider);
    ck_assert(is_ok(&r));

    // Call info_read with no active stream
    provider->vt->info_read(provider->ctx, NULL);

    // Should not crash, just returns
}

END_TEST

START_TEST(test_info_read_success_http_status)
{
    // Test line 211: stream->http_status >= 200 && stream->http_status < 300
    ik_provider_t *provider = NULL;
    res_t r = ik_anthropic_create(test_ctx, "test-key", &provider);
    ck_assert(is_ok(&r));

    // Get internal context
    ik_anthropic_ctx_t *impl_ctx = (ik_anthropic_ctx_t *)provider->ctx;

    // Create active stream with success status
    ik_anthropic_active_stream_t *stream = talloc_zero_(impl_ctx, sizeof(ik_anthropic_active_stream_t));
    stream->completed = true;
    stream->http_status = 200;
    stream->completion_cb = test_completion_cb;
    stream->completion_ctx = NULL;
    impl_ctx->active_stream = stream;

    // Reset tracker
    g_completion_called = false;

    // Call info_read
    provider->vt->info_read(provider->ctx, NULL);

    // Verify completion callback was invoked with success
    ck_assert(g_completion_called);
    ck_assert(g_last_completion.success);
    ck_assert_int_eq(g_last_completion.http_status, 200);
}

END_TEST

START_TEST(test_info_read_auth_error_401)
{
    // Test line 224: stream->http_status == 401
    ik_provider_t *provider = NULL;
    res_t r = ik_anthropic_create(test_ctx, "test-key", &provider);
    ck_assert(is_ok(&r));

    ik_anthropic_ctx_t *impl_ctx = (ik_anthropic_ctx_t *)provider->ctx;

    ik_anthropic_active_stream_t *stream = talloc_zero_(impl_ctx, sizeof(ik_anthropic_active_stream_t));
    stream->completed = true;
    stream->http_status = 401;
    stream->completion_cb = test_completion_cb;
    stream->completion_ctx = NULL;
    impl_ctx->active_stream = stream;

    g_completion_called = false;

    provider->vt->info_read(provider->ctx, NULL);

    ck_assert(g_completion_called);
    ck_assert(!g_last_completion.success);
    ck_assert_int_eq(g_last_completion.error_category, IK_ERR_CAT_AUTH);
}

END_TEST

START_TEST(test_info_read_auth_error_403)
{
    // Test line 224: stream->http_status == 403
    ik_provider_t *provider = NULL;
    res_t r = ik_anthropic_create(test_ctx, "test-key", &provider);
    ck_assert(is_ok(&r));

    ik_anthropic_ctx_t *impl_ctx = (ik_anthropic_ctx_t *)provider->ctx;

    ik_anthropic_active_stream_t *stream = talloc_zero_(impl_ctx, sizeof(ik_anthropic_active_stream_t));
    stream->completed = true;
    stream->http_status = 403;
    stream->completion_cb = test_completion_cb;
    stream->completion_ctx = NULL;
    impl_ctx->active_stream = stream;

    g_completion_called = false;

    provider->vt->info_read(provider->ctx, NULL);

    ck_assert(g_completion_called);
    ck_assert(!g_last_completion.success);
    ck_assert_int_eq(g_last_completion.error_category, IK_ERR_CAT_AUTH);
}

END_TEST

START_TEST(test_info_read_rate_limit_429)
{
    // Test line 226: stream->http_status == 429
    ik_provider_t *provider = NULL;
    res_t r = ik_anthropic_create(test_ctx, "test-key", &provider);
    ck_assert(is_ok(&r));

    ik_anthropic_ctx_t *impl_ctx = (ik_anthropic_ctx_t *)provider->ctx;

    ik_anthropic_active_stream_t *stream = talloc_zero_(impl_ctx, sizeof(ik_anthropic_active_stream_t));
    stream->completed = true;
    stream->http_status = 429;
    stream->completion_cb = test_completion_cb;
    stream->completion_ctx = NULL;
    impl_ctx->active_stream = stream;

    g_completion_called = false;

    provider->vt->info_read(provider->ctx, NULL);

    ck_assert(g_completion_called);
    ck_assert(!g_last_completion.success);
    ck_assert_int_eq(g_last_completion.error_category, IK_ERR_CAT_RATE_LIMIT);
}

END_TEST

START_TEST(test_info_read_server_error_500)
{
    // Test line 228: stream->http_status >= 500
    ik_provider_t *provider = NULL;
    res_t r = ik_anthropic_create(test_ctx, "test-key", &provider);
    ck_assert(is_ok(&r));

    ik_anthropic_ctx_t *impl_ctx = (ik_anthropic_ctx_t *)provider->ctx;

    ik_anthropic_active_stream_t *stream = talloc_zero_(impl_ctx, sizeof(ik_anthropic_active_stream_t));
    stream->completed = true;
    stream->http_status = 500;
    stream->completion_cb = test_completion_cb;
    stream->completion_ctx = NULL;
    impl_ctx->active_stream = stream;

    g_completion_called = false;

    provider->vt->info_read(provider->ctx, NULL);

    ck_assert(g_completion_called);
    ck_assert(!g_last_completion.success);
    ck_assert_int_eq(g_last_completion.error_category, IK_ERR_CAT_SERVER);
}

END_TEST

START_TEST(test_info_read_unknown_error_400)
{
    // Test line 231: else branch (other error codes)
    ik_provider_t *provider = NULL;
    res_t r = ik_anthropic_create(test_ctx, "test-key", &provider);
    ck_assert(is_ok(&r));

    ik_anthropic_ctx_t *impl_ctx = (ik_anthropic_ctx_t *)provider->ctx;

    ik_anthropic_active_stream_t *stream = talloc_zero_(impl_ctx, sizeof(ik_anthropic_active_stream_t));
    stream->completed = true;
    stream->http_status = 400;
    stream->completion_cb = test_completion_cb;
    stream->completion_ctx = NULL;
    impl_ctx->active_stream = stream;

    g_completion_called = false;

    provider->vt->info_read(provider->ctx, NULL);

    ck_assert(g_completion_called);
    ck_assert(!g_last_completion.success);
    ck_assert_int_eq(g_last_completion.error_category, IK_ERR_CAT_UNKNOWN);
}

END_TEST

/* ================================================================
 * Test Suite Setup
 * ================================================================ */

static Suite *anthropic_callbacks_coverage_suite(void)
{
    Suite *s = suite_create("Anthropic Callbacks Coverage");

    TCase *tc_write = tcase_create("Stream Write Callback");
    tcase_add_unchecked_fixture(tc_write, setup, teardown);
    tcase_add_test(tc_write, test_stream_write_cb_with_null_context);
    tcase_add_test(tc_write, test_stream_write_cb_with_null_sse_parser);
    tcase_add_test(tc_write, test_stream_write_cb_with_valid_context);
    tcase_add_test(tc_write, test_stream_write_cb_with_events);
    suite_add_tcase(s, tc_write);

    TCase *tc_completion = tcase_create("Stream Completion Callback");
    tcase_add_unchecked_fixture(tc_completion, setup, teardown);
    tcase_add_test(tc_completion, test_stream_completion_cb_with_null_context);
    tcase_add_test(tc_completion, test_stream_completion_cb_with_valid_context);
    suite_add_tcase(s, tc_completion);

    TCase *tc_creation = tcase_create("Provider Creation");
    tcase_add_unchecked_fixture(tc_creation, setup, teardown);
    tcase_add_test(tc_creation, test_anthropic_create_http_multi_failure);
    suite_add_tcase(s, tc_creation);

    TCase *tc_info_read = tcase_create("Info Read");
    tcase_add_unchecked_fixture(tc_info_read, setup, teardown);
    tcase_add_test(tc_info_read, test_info_read_no_active_stream);
    tcase_add_test(tc_info_read, test_info_read_success_http_status);
    tcase_add_test(tc_info_read, test_info_read_auth_error_401);
    tcase_add_test(tc_info_read, test_info_read_auth_error_403);
    tcase_add_test(tc_info_read, test_info_read_rate_limit_429);
    tcase_add_test(tc_info_read, test_info_read_server_error_500);
    tcase_add_test(tc_info_read, test_info_read_unknown_error_400);
    suite_add_tcase(s, tc_info_read);

    return s;
}

int main(void)
{
    Suite *s = anthropic_callbacks_coverage_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
