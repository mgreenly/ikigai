#include "tests/test_constants.h"
/**
 * @file google_streaming_error_coverage_test.c
 * @brief Coverage tests for Google streaming error handling paths
 *
 * Tests error categorization and error processing edge cases.
 */

#include <check.h>
#include <talloc.h>
#include <string.h>
#include "apps/ikigai/providers/google/streaming.h"
#include "apps/ikigai/providers/google/response.h"
#include "apps/ikigai/providers/provider.h"

/* Test context */
static TALLOC_CTX *test_ctx;

/* Captured stream events */
#define MAX_EVENTS 50
#define MAX_STRING_LEN 512
static ik_stream_event_t captured_events[MAX_EVENTS];
static char captured_strings1[MAX_EVENTS][MAX_STRING_LEN];
static size_t captured_count;

/* ================================================================
 * Test Callbacks
 * ================================================================ */

static res_t test_stream_cb(const ik_stream_event_t *event, void *ctx)
{
    (void)ctx;

    if (captured_count < MAX_EVENTS) {
        captured_events[captured_count] = *event;

        if (event->type == IK_STREAM_ERROR) {
            if (event->data.error.message) {
                strncpy(captured_strings1[captured_count], event->data.error.message, MAX_STRING_LEN - 1);
                captured_strings1[captured_count][MAX_STRING_LEN - 1] = '\0';
                captured_events[captured_count].data.error.message = captured_strings1[captured_count];
            }
        }

        captured_count++;
    }

    return OK(NULL);
}

/* ================================================================
 * Test Fixtures
 * ================================================================ */

static void setup(void)
{
    test_ctx = talloc_new(NULL);
    captured_count = 0;
    memset(captured_events, 0, sizeof(captured_events));
}

static void teardown(void)
{
    talloc_free(test_ctx);
}

/* ================================================================
 * Helper Functions
 * ================================================================ */

static void process_chunk(ik_google_stream_ctx_t *sctx, const char *chunk)
{
    ik_google_stream_process_data(sctx, chunk);
}

static const ik_stream_event_t *find_event(ik_stream_event_type_t type)
{
    for (size_t i = 0; i < captured_count; i++) {
        if (captured_events[i].type == type) {
            return &captured_events[i];
        }
    }
    return NULL;
}

/* ================================================================
 * Error Categorization Tests
 * ================================================================ */

START_TEST(test_error_unauthenticated_status) {
    ik_google_stream_ctx_t *sctx = NULL;
    res_t r = ik_google_stream_ctx_create(test_ctx, test_stream_cb, NULL, &sctx);
    ck_assert(!is_err(&r));

    /* Process error with UNAUTHENTICATED status */
    const char *chunk = "{\"error\":{\"message\":\"Invalid API key\",\"status\":\"UNAUTHENTICATED\"}}";
    process_chunk(sctx, chunk);

    /* Verify ERROR event with AUTH category */
    const ik_stream_event_t *event = find_event(IK_STREAM_ERROR);
    ck_assert_ptr_nonnull(event);
    ck_assert_int_eq(event->data.error.category, IK_ERR_CAT_AUTH);
    ck_assert_str_eq(event->data.error.message, "Invalid API key");
}

END_TEST

START_TEST(test_error_resource_exhausted_status) {
    ik_google_stream_ctx_t *sctx = NULL;
    res_t r = ik_google_stream_ctx_create(test_ctx, test_stream_cb, NULL, &sctx);
    ck_assert(!is_err(&r));

    /* Process error with RESOURCE_EXHAUSTED status - covers line 65 */
    const char *chunk = "{\"error\":{\"message\":\"Rate limit exceeded\",\"status\":\"RESOURCE_EXHAUSTED\"}}";
    process_chunk(sctx, chunk);

    /* Verify ERROR event with RATE_LIMIT category */
    const ik_stream_event_t *event = find_event(IK_STREAM_ERROR);
    ck_assert_ptr_nonnull(event);
    ck_assert_int_eq(event->data.error.category, IK_ERR_CAT_RATE_LIMIT);
    ck_assert_str_eq(event->data.error.message, "Rate limit exceeded");
}

END_TEST

START_TEST(test_error_invalid_argument_status) {
    ik_google_stream_ctx_t *sctx = NULL;
    res_t r = ik_google_stream_ctx_create(test_ctx, test_stream_cb, NULL, &sctx);
    ck_assert(!is_err(&r));

    /* Process error with INVALID_ARGUMENT status - covers line 67 */
    const char *chunk = "{\"error\":{\"message\":\"Invalid request parameters\",\"status\":\"INVALID_ARGUMENT\"}}";
    process_chunk(sctx, chunk);

    /* Verify ERROR event with INVALID_ARG category */
    const ik_stream_event_t *event = find_event(IK_STREAM_ERROR);
    ck_assert_ptr_nonnull(event);
    ck_assert_int_eq(event->data.error.category, IK_ERR_CAT_INVALID_ARG);
    ck_assert_str_eq(event->data.error.message, "Invalid request parameters");
}

END_TEST

START_TEST(test_error_unknown_status) {
    ik_google_stream_ctx_t *sctx = NULL;
    res_t r = ik_google_stream_ctx_create(test_ctx, test_stream_cb, NULL, &sctx);
    ck_assert(!is_err(&r));

    /* Process error with unknown status - covers line 59 false branch */
    const char *chunk = "{\"error\":{\"message\":\"Internal server error\",\"status\":\"INTERNAL\"}}";
    process_chunk(sctx, chunk);

    /* Verify ERROR event with UNKNOWN category */
    const ik_stream_event_t *event = find_event(IK_STREAM_ERROR);
    ck_assert_ptr_nonnull(event);
    ck_assert_int_eq(event->data.error.category, IK_ERR_CAT_UNKNOWN);
    ck_assert_str_eq(event->data.error.message, "Internal server error");
}

END_TEST

START_TEST(test_error_without_message) {
    ik_google_stream_ctx_t *sctx = NULL;
    res_t r = ik_google_stream_ctx_create(test_ctx, test_stream_cb, NULL, &sctx);
    ck_assert(!is_err(&r));

    /* Process error without message field - covers lines 83, 85, 87 */
    const char *chunk = "{\"error\":{\"status\":\"UNAUTHENTICATED\"}}";
    process_chunk(sctx, chunk);

    /* Verify ERROR event with default message */
    const ik_stream_event_t *event = find_event(IK_STREAM_ERROR);
    ck_assert_ptr_nonnull(event);
    ck_assert_int_eq(event->data.error.category, IK_ERR_CAT_AUTH);
    ck_assert_str_eq(event->data.error.message, "Unknown error");
}

END_TEST

START_TEST(test_error_without_status) {
    ik_google_stream_ctx_t *sctx = NULL;
    res_t r = ik_google_stream_ctx_create(test_ctx, test_stream_cb, NULL, &sctx);
    ck_assert(!is_err(&r));

    /* Process error without status field - covers line 93 */
    const char *chunk = "{\"error\":{\"message\":\"Something went wrong\"}}";
    process_chunk(sctx, chunk);

    /* Verify ERROR event with UNKNOWN category */
    const ik_stream_event_t *event = find_event(IK_STREAM_ERROR);
    ck_assert_ptr_nonnull(event);
    ck_assert_int_eq(event->data.error.category, IK_ERR_CAT_UNKNOWN);
    ck_assert_str_eq(event->data.error.message, "Something went wrong");
}

END_TEST

START_TEST(test_error_with_null_message_value) {
    ik_google_stream_ctx_t *sctx = NULL;
    res_t r = ik_google_stream_ctx_create(test_ctx, test_stream_cb, NULL, &sctx);
    ck_assert(!is_err(&r));

    /* Process error with null message value - covers line 87 */
    const char *chunk = "{\"error\":{\"message\":null,\"status\":\"INTERNAL\"}}";
    process_chunk(sctx, chunk);

    /* Verify ERROR event with default message */
    const ik_stream_event_t *event = find_event(IK_STREAM_ERROR);
    ck_assert_ptr_nonnull(event);
    ck_assert_int_eq(event->data.error.category, IK_ERR_CAT_UNKNOWN);
    ck_assert_str_eq(event->data.error.message, "Unknown error");
}

END_TEST

START_TEST(test_error_minimal) {
    ik_google_stream_ctx_t *sctx = NULL;
    res_t r = ik_google_stream_ctx_create(test_ctx, test_stream_cb, NULL, &sctx);
    ck_assert(!is_err(&r));

    /* Process minimal error object - covers all NULL branches */
    const char *chunk = "{\"error\":{}}";
    process_chunk(sctx, chunk);

    /* Verify ERROR event with defaults */
    const ik_stream_event_t *event = find_event(IK_STREAM_ERROR);
    ck_assert_ptr_nonnull(event);
    ck_assert_int_eq(event->data.error.category, IK_ERR_CAT_UNKNOWN);
    ck_assert_str_eq(event->data.error.message, "Unknown error");
}

END_TEST

START_TEST(test_error_with_non_string_message) {
    ik_google_stream_ctx_t *sctx = NULL;
    res_t r = ik_google_stream_ctx_create(test_ctx, test_stream_cb, NULL, &sctx);
    ck_assert(!is_err(&r));

    /* Process error with non-string message - covers line 83 true, 87 true */
    const char *chunk = "{\"error\":{\"message\":123,\"status\":\"UNAUTHENTICATED\"}}";
    process_chunk(sctx, chunk);

    /* Verify ERROR event with default message */
    const ik_stream_event_t *event = find_event(IK_STREAM_ERROR);
    ck_assert_ptr_nonnull(event);
    ck_assert_int_eq(event->data.error.category, IK_ERR_CAT_AUTH);
    ck_assert_str_eq(event->data.error.message, "Unknown error");
}

END_TEST

START_TEST(test_error_with_non_string_status) {
    ik_google_stream_ctx_t *sctx = NULL;
    res_t r = ik_google_stream_ctx_create(test_ctx, test_stream_cb, NULL, &sctx);
    ck_assert(!is_err(&r));

    /* Process error with non-string status - covers line 93 true, 59 false */
    const char *chunk = "{\"error\":{\"message\":\"Error occurred\",\"status\":404}}";
    process_chunk(sctx, chunk);

    /* Verify ERROR event with UNKNOWN category */
    const ik_stream_event_t *event = find_event(IK_STREAM_ERROR);
    ck_assert_ptr_nonnull(event);
    ck_assert_int_eq(event->data.error.category, IK_ERR_CAT_UNKNOWN);
    ck_assert_str_eq(event->data.error.message, "Error occurred");
}

END_TEST

/* ================================================================
 * Test Suite Setup
 * ================================================================ */

static Suite *google_streaming_error_coverage_suite(void)
{
    Suite *s = suite_create("Google Streaming - Error Coverage");

    TCase *tc_error = tcase_create("Error Categorization");
    tcase_set_timeout(tc_error, IK_TEST_TIMEOUT);
    tcase_add_checked_fixture(tc_error, setup, teardown);
    tcase_add_test(tc_error, test_error_unauthenticated_status);
    tcase_add_test(tc_error, test_error_resource_exhausted_status);
    tcase_add_test(tc_error, test_error_invalid_argument_status);
    tcase_add_test(tc_error, test_error_unknown_status);
    tcase_add_test(tc_error, test_error_without_message);
    tcase_add_test(tc_error, test_error_without_status);
    tcase_add_test(tc_error, test_error_with_null_message_value);
    tcase_add_test(tc_error, test_error_minimal);
    tcase_add_test(tc_error, test_error_with_non_string_message);
    tcase_add_test(tc_error, test_error_with_non_string_status);
    suite_add_tcase(s, tc_error);

    return s;
}

int main(void)
{
    Suite *s = google_streaming_error_coverage_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/apps/ikigai/providers/google/google_streaming_error_coverage_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
