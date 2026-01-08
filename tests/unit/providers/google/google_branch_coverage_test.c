/**
 * @file google_branch_coverage_test.c
 * @brief Additional branch coverage tests for Google provider google.c
 */

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-qual"
#pragma GCC diagnostic ignored "-Wmissing-prototypes"

#include <check.h>
#include <curl/curl.h>
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
 * Branch Coverage Tests
 * ================================================================ */

// Helper for stream tests
static res_t noop_stream_cb(const ik_stream_event_t *e, void *c)
{
    (void)e; (void)c; return OK(NULL);
}

static res_t noop_completion_cb(const ik_provider_completion_t *c, void *ctx)
{
    (void)c; (void)ctx; return OK(NULL);
}

/* ================================================================
 * Mock for curl_easy_init to test http_multi_add_request failure
 * ================================================================ */

static bool g_curl_easy_init_should_fail = false;

CURL *curl_easy_init_(void)
{
    if (g_curl_easy_init_should_fail) {
        return NULL;
    }
    return curl_easy_init();
}

// Test line 125: NULL data field in event (branch 1)
START_TEST(test_google_stream_write_cb_null_event_data) {
    ik_google_active_stream_t *stream = talloc_zero(test_ctx, ik_google_active_stream_t);
    res_t r = ik_google_stream_ctx_create(stream, noop_stream_cb, NULL, &stream->stream_ctx);
    ck_assert(!is_err(&r));
    stream->sse_parser = ik_sse_parser_create(stream);
    // Feed data that will produce an event with NULL data field
    // An event with no data lines at all - just an event type
    const char *data = "event: test\n\n";
    ck_assert_uint_eq(ik_google_stream_write_cb(data, strlen(data), stream), strlen(data));
    talloc_free(stream);
}
END_TEST

// Test line 198 branch 3: active_stream exists but not completed
START_TEST(test_google_info_read_active_stream_not_completed) {
    ik_provider_t *provider = NULL;
    res_t result = ik_google_create(test_ctx, "test-api-key", &provider);
    ck_assert(!is_err(&result));

    ik_google_ctx_t *impl_ctx = (ik_google_ctx_t *)provider->ctx;

    // Create an active stream that is NOT completed
    ik_google_active_stream_t *stream = talloc_zero(impl_ctx, ik_google_active_stream_t);
    stream->completed = false;  // Not completed yet
    stream->http_status = 0;
    impl_ctx->active_stream = stream;

    ik_logger_t *logger = ik_logger_create(test_ctx, "/tmp");
    provider->vt->info_read(provider->ctx, logger);

    // Stream should still exist (not cleaned up)
    ck_assert(impl_ctx->active_stream != NULL);
    ck_assert(!impl_ctx->active_stream->completed);

    // Clean up manually
    talloc_free(stream);
    impl_ctx->active_stream = NULL;
}
END_TEST

// Test line 328-331: ik_http_multi_add_request failure in google_start_stream
START_TEST(test_google_start_stream_http_multi_add_request_failure) {
    ik_provider_t *provider = NULL;
    res_t result = ik_google_create(test_ctx, "test-api-key", &provider);
    ck_assert(!is_err(&result));

    // Create a minimal valid request
    ik_content_block_t block = {
        .type = IK_CONTENT_TEXT,
        .data.text.text = (char *)"test message"
    };
    ik_message_t message = {
        .role = IK_ROLE_USER,
        .content_count = 1,
        .content_blocks = &block
    };
    ik_request_t req = {
        .model = (char *)"gemini-2.0-flash",
        .message_count = 1,
        .messages = &message
    };

    // Make curl_easy_init fail - this will cause http_multi_add_request to fail
    g_curl_easy_init_should_fail = true;

    // Attempt to start stream - should fail and clean up properly
    res_t r = provider->vt->start_stream(provider->ctx, &req, noop_stream_cb, NULL, noop_completion_cb, NULL);
    ck_assert(is_err(&r));

    // Verify no active stream remains (it should have been cleaned up)
    ik_google_ctx_t *impl_ctx = (ik_google_ctx_t *)provider->ctx;
    ck_assert(impl_ctx->active_stream == NULL);

    // Reset mock
    g_curl_easy_init_should_fail = false;
}
END_TEST

/* ================================================================
 * Test Suite Setup
 * ================================================================ */

static Suite *google_branch_coverage_suite(void)
{
    Suite *s = suite_create("Google Branch Coverage");

    TCase *tc_branch = tcase_create("Branch Coverage Tests");
    tcase_set_timeout(tc_branch, IK_TEST_TIMEOUT);
    tcase_add_unchecked_fixture(tc_branch, setup, teardown);

    // Line 125: NULL data in event
    tcase_add_test(tc_branch, test_google_stream_write_cb_null_event_data);

    // Line 198 branch 3: active_stream not completed
    tcase_add_test(tc_branch, test_google_info_read_active_stream_not_completed);

    // Line 328-331: http_multi_add_request failure in start_stream
    tcase_add_test(tc_branch, test_google_start_stream_http_multi_add_request_failure);

    suite_add_tcase(s, tc_branch);

    return s;
}

int main(void)
{
    Suite *s = google_branch_coverage_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
