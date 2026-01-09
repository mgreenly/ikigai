#include "../../../test_constants.h"
/**
 * @file google_streaming_errors_coverage_test.c
 * @brief Branch coverage tests for Google streaming - Error handling
 *
 * Tests error processing, data validation, and usage metadata edge cases.
 */

#include <check.h>
#include <talloc.h>
#include <string.h>
#include "providers/google/streaming.h"
#include "providers/google/response.h"
#include "providers/provider.h"

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

        switch (event->type) {
            case IK_STREAM_START:
                if (event->data.start.model) {
                    strncpy(captured_strings1[captured_count], event->data.start.model, MAX_STRING_LEN - 1);
                    captured_strings1[captured_count][MAX_STRING_LEN - 1] = '\0';
                    captured_events[captured_count].data.start.model = captured_strings1[captured_count];
                }
                break;
            default:
                break;
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

static size_t count_events(ik_stream_event_type_t type)
{
    size_t count = 0;
    for (size_t i = 0; i < captured_count; i++) {
        if (captured_events[i].type == type) {
            count++;
        }
    }
    return count;
}

/* ================================================================
 * Process Error Edge Cases
 * ================================================================ */

START_TEST(test_error_without_message) {
    ik_google_stream_ctx_t *sctx = NULL;
    res_t r = ik_google_stream_ctx_create(test_ctx, test_stream_cb, NULL, &sctx);
    ck_assert(!is_err(&r));

    /* Process error without message field - covers line 85 false branch */
    const char *chunk = "{\"error\":{\"status\":\"UNAUTHENTICATED\"}}";
    process_chunk(sctx, chunk);

    /* Verify ERROR event with default message */
    const ik_stream_event_t *event = find_event(IK_STREAM_ERROR);
    ck_assert_ptr_nonnull(event);
    ck_assert_str_eq(event->data.error.message, "Unknown error");
    ck_assert_int_eq(event->data.error.category, IK_ERR_CAT_AUTH);
}
END_TEST

START_TEST(test_error_with_null_message) {
    ik_google_stream_ctx_t *sctx = NULL;
    res_t r = ik_google_stream_ctx_create(test_ctx, test_stream_cb, NULL, &sctx);
    ck_assert(!is_err(&r));

    /* Process error with null message - covers line 87 false branch */
    const char *chunk = "{\"error\":{\"message\":null,\"status\":\"RESOURCE_EXHAUSTED\"}}";
    process_chunk(sctx, chunk);

    /* Verify ERROR event with default message */
    const ik_stream_event_t *event = find_event(IK_STREAM_ERROR);
    ck_assert_ptr_nonnull(event);
    ck_assert_str_eq(event->data.error.message, "Unknown error");
    ck_assert_int_eq(event->data.error.category, IK_ERR_CAT_RATE_LIMIT);
}
END_TEST

START_TEST(test_error_with_non_string_message) {
    ik_google_stream_ctx_t *sctx = NULL;
    res_t r = ik_google_stream_ctx_create(test_ctx, test_stream_cb, NULL, &sctx);
    ck_assert(!is_err(&r));

    /* Process error with non-string message - covers line 87 false branch */
    const char *chunk = "{\"error\":{\"message\":12345,\"status\":\"INVALID_ARGUMENT\"}}";
    process_chunk(sctx, chunk);

    /* Verify ERROR event with default message */
    const ik_stream_event_t *event = find_event(IK_STREAM_ERROR);
    ck_assert_ptr_nonnull(event);
    ck_assert_str_eq(event->data.error.message, "Unknown error");
    ck_assert_int_eq(event->data.error.category, IK_ERR_CAT_INVALID_ARG);
}
END_TEST

START_TEST(test_error_without_status) {
    ik_google_stream_ctx_t *sctx = NULL;
    res_t r = ik_google_stream_ctx_create(test_ctx, test_stream_cb, NULL, &sctx);
    ck_assert(!is_err(&r));

    /* Process error without status field - covers line 95 false branch */
    const char *chunk = "{\"error\":{\"message\":\"Something went wrong\"}}";
    process_chunk(sctx, chunk);

    /* Verify ERROR event with UNKNOWN category */
    const ik_stream_event_t *event = find_event(IK_STREAM_ERROR);
    ck_assert_ptr_nonnull(event);
    ck_assert_str_eq(event->data.error.message, "Something went wrong");
    ck_assert_int_eq(event->data.error.category, IK_ERR_CAT_UNKNOWN);
}
END_TEST

START_TEST(test_error_with_null_status) {
    ik_google_stream_ctx_t *sctx = NULL;
    res_t r = ik_google_stream_ctx_create(test_ctx, test_stream_cb, NULL, &sctx);
    ck_assert(!is_err(&r));

    /* Process error with null status - covers line 59 true branch (status == NULL) */
    const char *chunk = "{\"error\":{\"message\":\"Error\",\"status\":null}}";
    process_chunk(sctx, chunk);

    /* Verify ERROR event with UNKNOWN category */
    const ik_stream_event_t *event = find_event(IK_STREAM_ERROR);
    ck_assert_ptr_nonnull(event);
    ck_assert_int_eq(event->data.error.category, IK_ERR_CAT_UNKNOWN);
}
END_TEST

START_TEST(test_error_with_unknown_status) {
    ik_google_stream_ctx_t *sctx = NULL;
    res_t r = ik_google_stream_ctx_create(test_ctx, test_stream_cb, NULL, &sctx);
    ck_assert(!is_err(&r));

    /* Process error with unrecognized status - covers line 67 false branch */
    const char *chunk = "{\"error\":{\"message\":\"Error\",\"status\":\"SOME_OTHER_ERROR\"}}";
    process_chunk(sctx, chunk);

    /* Verify ERROR event with UNKNOWN category (default case) */
    const ik_stream_event_t *event = find_event(IK_STREAM_ERROR);
    ck_assert_ptr_nonnull(event);
    ck_assert_int_eq(event->data.error.category, IK_ERR_CAT_UNKNOWN);
}
END_TEST

/* ================================================================
 * Process Data Edge Cases
 * ================================================================ */

START_TEST(test_process_null_data) {
    ik_google_stream_ctx_t *sctx = NULL;
    res_t r = ik_google_stream_ctx_create(test_ctx, test_stream_cb, NULL, &sctx);
    ck_assert(!is_err(&r));

    /* Process NULL data - covers line 352 branch 1 */
    ik_google_stream_process_data(sctx, NULL);

    /* Verify no events emitted */
    ck_assert_int_eq((int)captured_count, 0);
}
END_TEST

START_TEST(test_process_empty_data) {
    ik_google_stream_ctx_t *sctx = NULL;
    res_t r = ik_google_stream_ctx_create(test_ctx, test_stream_cb, NULL, &sctx);
    ck_assert(!is_err(&r));

    /* Process empty data - covers line 352 branch 2 */
    ik_google_stream_process_data(sctx, "");

    /* Verify no events emitted */
    ck_assert_int_eq((int)captured_count, 0);
}
END_TEST

START_TEST(test_process_malformed_json) {
    ik_google_stream_ctx_t *sctx = NULL;
    res_t r = ik_google_stream_ctx_create(test_ctx, test_stream_cb, NULL, &sctx);
    ck_assert(!is_err(&r));

    /* Process malformed JSON - covers line 361 true branch */
    ik_google_stream_process_data(sctx, "{invalid json");

    /* Verify no events emitted (silently ignored) */
    ck_assert_int_eq((int)captured_count, 0);
}
END_TEST

START_TEST(test_process_non_object_root) {
    ik_google_stream_ctx_t *sctx = NULL;
    res_t r = ik_google_stream_ctx_create(test_ctx, test_stream_cb, NULL, &sctx);
    ck_assert(!is_err(&r));

    /* Process JSON array instead of object - covers line 367 true branch */
    ik_google_stream_process_data(sctx, "[1,2,3]");

    /* Verify no events emitted */
    ck_assert_int_eq((int)captured_count, 0);
}
END_TEST

START_TEST(test_process_error_only_chunk) {
    ik_google_stream_ctx_t *sctx = NULL;
    res_t r = ik_google_stream_ctx_create(test_ctx, test_stream_cb, NULL, &sctx);
    ck_assert(!is_err(&r));

    /* Process chunk with error - covers line 374 true branch and early return */
    const char *chunk = "{\"error\":{\"message\":\"API error\",\"status\":\"UNAUTHENTICATED\"}}";
    process_chunk(sctx, chunk);

    /* Verify ERROR event emitted and processing stopped */
    ck_assert_int_eq((int)count_events(IK_STREAM_ERROR), 1);
    ck_assert_int_eq((int)count_events(IK_STREAM_START), 0);
}
END_TEST

/* ================================================================
 * Usage Metadata Edge Cases
 * ================================================================ */

START_TEST(test_usage_with_missing_fields) {
    ik_google_stream_ctx_t *sctx = NULL;
    res_t r = ik_google_stream_ctx_create(test_ctx, test_stream_cb, NULL, &sctx);
    ck_assert(!is_err(&r));

    /* Process START first */
    process_chunk(sctx, "{\"modelVersion\":\"gemini-2.5-flash\"}");

    /* Process usage with missing fields - covers lines 264-276 NULL branches */
    const char *chunk = "{\"usageMetadata\":{}}";
    process_chunk(sctx, chunk);

    /* Verify DONE event with zero usage */
    const ik_stream_event_t *event = find_event(IK_STREAM_DONE);
    ck_assert_ptr_nonnull(event);
    ck_assert_int_eq(event->data.done.usage.input_tokens, 0);
    ck_assert_int_eq(event->data.done.usage.output_tokens, 0);
    ck_assert_int_eq(event->data.done.usage.thinking_tokens, 0);
    ck_assert_int_eq(event->data.done.usage.total_tokens, 0);
}
END_TEST

START_TEST(test_usage_with_thoughts) {
    ik_google_stream_ctx_t *sctx = NULL;
    res_t r = ik_google_stream_ctx_create(test_ctx, test_stream_cb, NULL, &sctx);
    ck_assert(!is_err(&r));

    /* Process START first */
    process_chunk(sctx, "{\"modelVersion\":\"gemini-2.5-flash\"}");

    /* Process usage with thoughts - covers thinking token handling */
    const char *chunk =
        "{\"usageMetadata\":{\"promptTokenCount\":10,\"candidatesTokenCount\":30,\"thoughtsTokenCount\":5,\"totalTokenCount\":40}}";
    process_chunk(sctx, chunk);

    /* Verify usage correctly excludes thoughts from output */
    const ik_stream_event_t *event = find_event(IK_STREAM_DONE);
    ck_assert_ptr_nonnull(event);
    ck_assert_int_eq(event->data.done.usage.input_tokens, 10);
    ck_assert_int_eq(event->data.done.usage.thinking_tokens, 5);
    ck_assert_int_eq(event->data.done.usage.output_tokens, 25); /* 30 - 5 */
    ck_assert_int_eq(event->data.done.usage.total_tokens, 40);
}
END_TEST

/* ================================================================
 * Test Suite Setup
 * ================================================================ */

static Suite *google_streaming_errors_coverage_suite(void)
{
    Suite *s = suite_create("Google Streaming - Error Coverage");

    TCase *tc_errors = tcase_create("Process Error Edge Cases");
    tcase_set_timeout(tc_errors, IK_TEST_TIMEOUT);
    tcase_add_checked_fixture(tc_errors, setup, teardown);
    tcase_add_test(tc_errors, test_error_without_message);
    tcase_add_test(tc_errors, test_error_with_null_message);
    tcase_add_test(tc_errors, test_error_with_non_string_message);
    tcase_add_test(tc_errors, test_error_without_status);
    tcase_add_test(tc_errors, test_error_with_null_status);
    tcase_add_test(tc_errors, test_error_with_unknown_status);
    suite_add_tcase(s, tc_errors);

    TCase *tc_data = tcase_create("Process Data Edge Cases");
    tcase_set_timeout(tc_data, IK_TEST_TIMEOUT);
    tcase_add_checked_fixture(tc_data, setup, teardown);
    tcase_add_test(tc_data, test_process_null_data);
    tcase_add_test(tc_data, test_process_empty_data);
    tcase_add_test(tc_data, test_process_malformed_json);
    tcase_add_test(tc_data, test_process_non_object_root);
    tcase_add_test(tc_data, test_process_error_only_chunk);
    suite_add_tcase(s, tc_data);

    TCase *tc_usage = tcase_create("Usage Metadata Edge Cases");
    tcase_set_timeout(tc_usage, IK_TEST_TIMEOUT);
    tcase_add_checked_fixture(tc_usage, setup, teardown);
    tcase_add_test(tc_usage, test_usage_with_missing_fields);
    tcase_add_test(tc_usage, test_usage_with_thoughts);
    suite_add_tcase(s, tc_usage);

    return s;
}

int main(void)
{
    Suite *s = google_streaming_errors_coverage_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
