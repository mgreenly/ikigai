/**
 * @file google_streaming_edge_cases_test.c
 * @brief Edge case tests for Google streaming parser
 *
 * Tests various edge cases in JSON parsing and part processing.
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
static ik_stream_event_t captured_events[MAX_EVENTS];
static size_t captured_count;

/* ================================================================
 * Test Callbacks
 * ================================================================ */

static res_t test_stream_cb(const ik_stream_event_t *event, void *ctx)
{
    (void)ctx;

    if (captured_count < MAX_EVENTS) {
        captured_events[captured_count] = *event;
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

/* ================================================================
 * JSON Parsing Edge Cases
 * ================================================================ */

START_TEST(test_json_array_root) {
    ik_google_stream_ctx_t *sctx = NULL;
    res_t r = ik_google_stream_ctx_create(test_ctx, test_stream_cb, NULL, &sctx);
    ck_assert(!is_err(&r));

    /* Process JSON with array as root - should be ignored (line 369) */
    const char *chunk = "[1,2,3]";
    process_chunk(sctx, chunk);

    /* Verify no events emitted */
    ck_assert_int_eq((int)captured_count, 0);
}

END_TEST

START_TEST(test_json_string_root) {
    ik_google_stream_ctx_t *sctx = NULL;
    res_t r = ik_google_stream_ctx_create(test_ctx, test_stream_cb, NULL, &sctx);
    ck_assert(!is_err(&r));

    /* Process JSON with string as root - should be ignored (line 369) */
    const char *chunk = "\"hello\"";
    process_chunk(sctx, chunk);

    /* Verify no events emitted */
    ck_assert_int_eq((int)captured_count, 0);
}

END_TEST

/* ================================================================
 * Parts Processing Edge Cases
 * ================================================================ */

START_TEST(test_part_without_text_or_function_call) {
    ik_google_stream_ctx_t *sctx = NULL;
    res_t r = ik_google_stream_ctx_create(test_ctx, test_stream_cb, NULL, &sctx);
    ck_assert(!is_err(&r));

    /* Process chunk with part that has neither text nor functionCall (line 239) */
    const char *chunk =
        "{\"candidates\":[{\"content\":{\"parts\":[{\"thought\":false}]}}],\"modelVersion\":\"gemini-2.5-flash\"}";
    process_chunk(sctx, chunk);

    /* Verify only START event emitted (part was skipped) */
    ck_assert_int_eq((int)captured_count, 1);
    ck_assert_int_eq(captured_events[0].type, IK_STREAM_START);
}

END_TEST

START_TEST(test_part_with_empty_text) {
    ik_google_stream_ctx_t *sctx = NULL;
    res_t r = ik_google_stream_ctx_create(test_ctx, test_stream_cb, NULL, &sctx);
    ck_assert(!is_err(&r));

    /* Process chunk with empty text (line 245) */
    const char *chunk =
        "{\"candidates\":[{\"content\":{\"parts\":[{\"text\":\"\"}]}}],\"modelVersion\":\"gemini-2.5-flash\"}";
    process_chunk(sctx, chunk);

    /* Verify only START event emitted (empty text was skipped) */
    ck_assert_int_eq((int)captured_count, 1);
    ck_assert_int_eq(captured_events[0].type, IK_STREAM_START);
}

END_TEST

START_TEST(test_part_with_null_text_value) {
    ik_google_stream_ctx_t *sctx = NULL;
    res_t r = ik_google_stream_ctx_create(test_ctx, test_stream_cb, NULL, &sctx);
    ck_assert(!is_err(&r));

    /* Process chunk with null text value (line 245) */
    const char *chunk =
        "{\"candidates\":[{\"content\":{\"parts\":[{\"text\":null}]}}],\"modelVersion\":\"gemini-2.5-flash\"}";
    process_chunk(sctx, chunk);

    /* Verify only START event emitted (null text was skipped) */
    ck_assert_int_eq((int)captured_count, 1);
    ck_assert_int_eq(captured_events[0].type, IK_STREAM_START);
}

END_TEST

START_TEST(test_part_with_non_string_text) {
    ik_google_stream_ctx_t *sctx = NULL;
    res_t r = ik_google_stream_ctx_create(test_ctx, test_stream_cb, NULL, &sctx);
    ck_assert(!is_err(&r));

    /* Process chunk with non-string text value (line 245) */
    const char *chunk =
        "{\"candidates\":[{\"content\":{\"parts\":[{\"text\":123}]}}],\"modelVersion\":\"gemini-2.5-flash\"}";
    process_chunk(sctx, chunk);

    /* Verify only START event emitted (non-string text was skipped) */
    ck_assert_int_eq((int)captured_count, 1);
    ck_assert_int_eq(captured_events[0].type, IK_STREAM_START);
}

END_TEST

/* ================================================================
 * Test Suite Setup
 * ================================================================ */

static Suite *google_streaming_edge_cases_suite(void)
{
    Suite *s = suite_create("Google Streaming - Edge Cases");

    TCase *tc_json = tcase_create("JSON Parsing");
    tcase_set_timeout(tc_json, 30);
    tcase_add_checked_fixture(tc_json, setup, teardown);
    tcase_add_test(tc_json, test_json_array_root);
    tcase_add_test(tc_json, test_json_string_root);
    suite_add_tcase(s, tc_json);

    TCase *tc_parts = tcase_create("Parts Processing");
    tcase_set_timeout(tc_parts, 30);
    tcase_add_checked_fixture(tc_parts, setup, teardown);
    tcase_add_test(tc_parts, test_part_without_text_or_function_call);
    tcase_add_test(tc_parts, test_part_with_empty_text);
    tcase_add_test(tc_parts, test_part_with_null_text_value);
    tcase_add_test(tc_parts, test_part_with_non_string_text);
    suite_add_tcase(s, tc_parts);

    return s;
}

int main(void)
{
    Suite *s = google_streaming_edge_cases_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
