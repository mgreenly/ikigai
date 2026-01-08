/**
 * @file google_coverage_stream_test.c
 * @brief Coverage tests for Google provider streaming functionality
 */

#include <check.h>
#include <talloc.h>
#include <string.h>

#include "error.h"
#include "providers/common/sse_parser.h"
#include "providers/google/google_internal.h"
#include "providers/google/streaming.h"

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
 * Stream Callback Tests
 * ================================================================ */

// Test NULL stream and NULL sse_parser (lines 113-114)
START_TEST(test_google_stream_write_cb_null_stream) {
    ck_assert_uint_eq(ik_google_stream_write_cb("data", 4, NULL), 4);
}
END_TEST

START_TEST(test_google_stream_write_cb_null_sse_parser) {
    ik_google_active_stream_t stream = {.stream_ctx = (void *)1, .sse_parser = NULL};
    ck_assert_uint_eq(ik_google_stream_write_cb("data", 4, &stream), 4);
}
END_TEST

// Test NULL stream in completion callback (line 145)
START_TEST(test_google_stream_completion_cb_null_stream) {
    ik_http_completion_t completion = {.http_code = 200};
    ik_google_stream_completion_cb(&completion, NULL);
}
END_TEST

// Helper for stream tests
static res_t noop_stream_cb(const ik_stream_event_t *e, void *c)
{
    (void)e; (void)c; return OK(NULL);
}

// Test lines 118-134: Normal streaming path with valid data
START_TEST(test_google_stream_write_cb_with_valid_data) {
    ik_google_active_stream_t *stream = talloc_zero(test_ctx, ik_google_active_stream_t);
    res_t r = ik_google_stream_ctx_create(stream, noop_stream_cb, NULL, &stream->stream_ctx);
    ck_assert(!is_err(&r));
    stream->sse_parser = ik_sse_parser_create(stream);
    const char *data = "data: {\"test\": \"data\"}\n\n";
    ck_assert_uint_eq(ik_google_stream_write_cb(data, strlen(data), stream), strlen(data));
    talloc_free(stream);
}
END_TEST

// Test line 125: NULL event->data branch (empty data field)
START_TEST(test_google_stream_write_cb_null_event_data) {
    ik_google_active_stream_t *stream = talloc_zero(test_ctx, ik_google_active_stream_t);
    res_t r = ik_google_stream_ctx_create(stream, noop_stream_cb, NULL, &stream->stream_ctx);
    ck_assert(!is_err(&r));
    stream->sse_parser = ik_sse_parser_create(stream);
    // Send SSE event without data field - parser will create event with NULL data
    const char *data = ": comment\n\n";
    ck_assert_uint_eq(ik_google_stream_write_cb(data, strlen(data), stream), strlen(data));
    talloc_free(stream);
}
END_TEST

// Test lines 149-150: Stream completion with non-NULL stream
START_TEST(test_google_stream_completion_cb_with_valid_stream) {
    ik_google_active_stream_t *stream = talloc_zero(test_ctx, ik_google_active_stream_t);
    stream->completed = false;
    ik_http_completion_t completion = {.http_code = 200};
    ik_google_stream_completion_cb(&completion, stream);
    ck_assert(stream->completed);
    ck_assert_int_eq(stream->http_status, 200);
    talloc_free(stream);
}
END_TEST

/* ================================================================
 * Test Suite Setup
 * ================================================================ */

static Suite *google_coverage_stream_suite(void)
{
    Suite *s = suite_create("Google Coverage - Stream");

    TCase *tc_stream = tcase_create("Stream Tests");
    tcase_set_timeout(tc_stream, IK_TEST_TIMEOUT);
    tcase_add_unchecked_fixture(tc_stream, setup, teardown);

    // Line 113: NULL checks in stream_write_cb
    tcase_add_test(tc_stream, test_google_stream_write_cb_null_stream);
    tcase_add_test(tc_stream, test_google_stream_write_cb_null_sse_parser);

    // Line 166: NULL stream in completion callback
    tcase_add_test(tc_stream, test_google_stream_completion_cb_null_stream);

    // Streaming with valid data (lines 118-134, 149-150)
    tcase_add_test(tc_stream, test_google_stream_write_cb_with_valid_data);
    tcase_add_test(tc_stream, test_google_stream_write_cb_null_event_data);
    tcase_add_test(tc_stream, test_google_stream_completion_cb_with_valid_stream);

    suite_add_tcase(s, tc_stream);

    return s;
}

int main(void)
{
    Suite *s = google_coverage_stream_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
