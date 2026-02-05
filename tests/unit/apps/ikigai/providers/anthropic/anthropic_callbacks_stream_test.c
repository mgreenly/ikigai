#include "tests/test_constants.h"
/**
 * @file anthropic_callbacks_stream_test.c
 * @brief Coverage tests for anthropic.c stream callbacks
 */

#include <check.h>
#include <talloc.h>
#include <string.h>
#include "apps/ikigai/providers/anthropic/anthropic.h"
#include "apps/ikigai/providers/anthropic/anthropic_internal.h"
#include "apps/ikigai/providers/common/http_multi.h"
#include "apps/ikigai/providers/common/sse_parser.h"
#include "shared/wrapper.h"
#include "shared/wrapper_internal.h"
#include "shared/logger.h"
#include "shared/error.h"

static TALLOC_CTX *test_ctx;

static void setup(void)
{
    test_ctx = talloc_new(NULL);
}

static void teardown(void)
{
    talloc_free(test_ctx);
}

/* Stream Write Callback Tests */

START_TEST(test_stream_write_cb_with_null_context) {
    const char *data = "test data";
    size_t result = ik_anthropic_stream_write_cb(data, 9, NULL);

    // Should return len even with NULL context
    ck_assert_uint_eq(result, 9);
}
END_TEST

START_TEST(test_stream_write_cb_with_null_sse_parser) {
    ik_anthropic_active_stream_t *stream = talloc_zero_(test_ctx, sizeof(ik_anthropic_active_stream_t));
    stream->sse_parser = NULL;

    const char *data = "test data";
    size_t result = ik_anthropic_stream_write_cb(data, 9, stream);

    // Should return len even with NULL sse_parser
    ck_assert_uint_eq(result, 9);
}

END_TEST

START_TEST(test_stream_write_cb_with_valid_context) {
    ik_anthropic_active_stream_t *stream = talloc_zero_(test_ctx, sizeof(ik_anthropic_active_stream_t));
    stream->sse_parser = ik_sse_parser_create(stream);
    stream->stream_ctx = talloc_zero_(stream, 1); // Mock - won't be dereferenced with no events

    const char *data = "partial";  // Incomplete SSE won't trigger event processing
    size_t result = ik_anthropic_stream_write_cb(data, strlen(data), stream);

    // Should accept and process the data
    ck_assert_uint_eq(result, strlen(data));
}

END_TEST
/* Stream Completion Callback Tests */

START_TEST(test_stream_completion_cb_with_null_context) {
    ik_http_completion_t completion = {
        .http_code = 200,
        .curl_code = 0
    };

    // Should not crash with NULL context
    ik_anthropic_stream_completion_cb(&completion, NULL);
}

END_TEST

START_TEST(test_stream_completion_cb_with_valid_context) {
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

/* Provider Creation Tests */

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

START_TEST(test_anthropic_create_http_multi_failure) {
    g_http_multi_create_should_fail = true;

    ik_provider_t *provider = NULL;
    res_t r = ik_anthropic_create(test_ctx, "test-api-key", &provider);

    ck_assert(is_err(&r));
    ck_assert_ptr_null(provider);

    g_http_multi_create_should_fail = false;
}

END_TEST

/* Stream Write Callback - Event Processing Tests */

// Dummy stream callback
static res_t dummy_stream_cb(const ik_stream_event_t *event, void *ctx)
{
    (void)event;
    (void)ctx;
    return OK(NULL);
}

START_TEST(test_stream_write_cb_with_complete_event) {
    ik_anthropic_active_stream_t *stream = talloc_zero_(test_ctx, sizeof(ik_anthropic_active_stream_t));
    stream->sse_parser = ik_sse_parser_create(stream);

    // Create a proper streaming context
    res_t r = ik_anthropic_stream_ctx_create(stream, dummy_stream_cb, NULL, &stream->stream_ctx);
    ck_assert(is_ok(&r));

    // Feed complete SSE event to trigger event processing loop
    const char *sse_data = "event: message_start\ndata: {\"type\":\"message_start\"}\n\n";
    size_t result = ik_anthropic_stream_write_cb(sse_data, strlen(sse_data), stream);

    ck_assert_uint_eq(result, strlen(sse_data));
}

END_TEST

START_TEST(test_stream_write_cb_with_null_event_fields) {
    ik_anthropic_active_stream_t *stream = talloc_zero_(test_ctx, sizeof(ik_anthropic_active_stream_t));
    stream->sse_parser = ik_sse_parser_create(stream);

    res_t r = ik_anthropic_stream_ctx_create(stream, dummy_stream_cb, NULL, &stream->stream_ctx);
    ck_assert(is_ok(&r));

    // SSE comment line creates event with NULL fields
    const char *sse_data = ":\n\n";
    size_t result = ik_anthropic_stream_write_cb(sse_data, strlen(sse_data), stream);

    ck_assert_uint_eq(result, strlen(sse_data));
}

END_TEST

/* Test Suite Setup */

static Suite *anthropic_callbacks_stream_suite(void)
{
    Suite *s = suite_create("Anthropic Callbacks - Stream");

    TCase *tc_write = tcase_create("Stream Write Callback");
    tcase_set_timeout(tc_write, IK_TEST_TIMEOUT);
    tcase_add_unchecked_fixture(tc_write, setup, teardown);
    tcase_add_test(tc_write, test_stream_write_cb_with_null_context);
    tcase_add_test(tc_write, test_stream_write_cb_with_null_sse_parser);
    tcase_add_test(tc_write, test_stream_write_cb_with_valid_context);
    tcase_add_test(tc_write, test_stream_write_cb_with_complete_event);
    tcase_add_test(tc_write, test_stream_write_cb_with_null_event_fields);
    suite_add_tcase(s, tc_write);

    TCase *tc_completion = tcase_create("Stream Completion Callback");
    tcase_set_timeout(tc_completion, IK_TEST_TIMEOUT);
    tcase_add_unchecked_fixture(tc_completion, setup, teardown);
    tcase_add_test(tc_completion, test_stream_completion_cb_with_null_context);
    tcase_add_test(tc_completion, test_stream_completion_cb_with_valid_context);
    suite_add_tcase(s, tc_completion);

    TCase *tc_creation = tcase_create("Provider Creation");
    tcase_set_timeout(tc_creation, IK_TEST_TIMEOUT);
    tcase_add_unchecked_fixture(tc_creation, setup, teardown);
    tcase_add_test(tc_creation, test_anthropic_create_http_multi_failure);
    suite_add_tcase(s, tc_creation);

    return s;
}

int main(void)
{
    Suite *s = anthropic_callbacks_stream_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/apps/ikigai/providers/anthropic/anthropic_callbacks_stream_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
