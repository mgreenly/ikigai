#include "tests/test_constants.h"
/**
 * @file google_streaming_edge_cases_test.c
 * @brief Edge case tests for Google streaming parser
 *
 * Tests various edge cases in JSON parsing and part processing.
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
 * Model Version Edge Cases
 * ================================================================ */

START_TEST(test_missing_model_version) {
    ik_google_stream_ctx_t *sctx = NULL;
    res_t r = ik_google_stream_ctx_create(test_ctx, test_stream_cb, NULL, &sctx);
    ck_assert(!is_err(&r));

    /* Process chunk without modelVersion */
    const char *chunk = "{\"candidates\":[{\"content\":{\"parts\":[{\"text\":\"Hi\"}]}}]}";
    process_chunk(sctx, chunk);

    /* Verify START event with NULL model */
    ck_assert_int_eq((int)captured_count, 2); /* START + TEXT_DELTA */
    ck_assert_int_eq(captured_events[0].type, IK_STREAM_START);
}

END_TEST

/* ================================================================
 * Usage Metadata Edge Cases
 * ================================================================ */

START_TEST(test_usage_empty_metadata) {
    ik_google_stream_ctx_t *sctx = NULL;
    res_t r = ik_google_stream_ctx_create(test_ctx, test_stream_cb, NULL, &sctx);
    ck_assert(!is_err(&r));

    process_chunk(sctx, "{\"modelVersion\":\"gemini-2.5-flash\"}");
    process_chunk(sctx, "{\"candidates\":[{\"content\":{\"parts\":[{\"text\":\"Hi\"}]}}]}");

    /* Empty usageMetadata object */
    process_chunk(sctx, "{\"usageMetadata\":{}}");

    /* Find DONE event */
    int done_idx = -1;
    for (size_t i = 0; i < captured_count; i++) {
        if (captured_events[i].type == IK_STREAM_DONE) {
            done_idx = (int)i;
            break;
        }
    }
    ck_assert_int_ge(done_idx, 0);
    ck_assert_int_eq(captured_events[done_idx].data.done.usage.input_tokens, 0);
    ck_assert_int_eq(captured_events[done_idx].data.done.usage.total_tokens, 0);
}

END_TEST

/* ================================================================
 * Candidates Edge Cases
 * ================================================================ */

START_TEST(test_empty_candidates_array) {
    ik_google_stream_ctx_t *sctx = NULL;
    res_t r = ik_google_stream_ctx_create(test_ctx, test_stream_cb, NULL, &sctx);
    ck_assert(!is_err(&r));

    /* Empty candidates array */
    process_chunk(sctx, "{\"modelVersion\":\"gemini-2.5-flash\",\"candidates\":[]}");

    /* Only START event */
    ck_assert_int_eq((int)captured_count, 1);
    ck_assert_int_eq(captured_events[0].type, IK_STREAM_START);
}

END_TEST

START_TEST(test_candidate_without_content) {
    ik_google_stream_ctx_t *sctx = NULL;
    res_t r = ik_google_stream_ctx_create(test_ctx, test_stream_cb, NULL, &sctx);
    ck_assert(!is_err(&r));

    process_chunk(sctx, "{\"modelVersion\":\"gemini-2.5-flash\"}");
    captured_count = 0;

    /* Candidate without content field */
    process_chunk(sctx, "{\"candidates\":[{\"finishReason\":\"STOP\"}]}");

    /* No text events */
    int text_count = 0;
    for (size_t i = 0; i < captured_count; i++) {
        if (captured_events[i].type == IK_STREAM_TEXT_DELTA) text_count++;
    }
    ck_assert_int_eq(text_count, 0);
}

END_TEST

START_TEST(test_candidate_without_parts) {
    ik_google_stream_ctx_t *sctx = NULL;
    res_t r = ik_google_stream_ctx_create(test_ctx, test_stream_cb, NULL, &sctx);
    ck_assert(!is_err(&r));

    process_chunk(sctx, "{\"modelVersion\":\"gemini-2.5-flash\"}");
    captured_count = 0;

    /* Candidate with content but no parts */
    process_chunk(sctx, "{\"candidates\":[{\"content\":{}}]}");

    /* No text events */
    int text_count = 0;
    for (size_t i = 0; i < captured_count; i++) {
        if (captured_events[i].type == IK_STREAM_TEXT_DELTA) text_count++;
    }
    ck_assert_int_eq(text_count, 0);
}

END_TEST

/* ================================================================
 * Test Suite Setup
 * ================================================================ */

static Suite *google_streaming_edge_cases_suite(void)
{
    Suite *s = suite_create("Google Streaming - Edge Cases");

    TCase *tc_json = tcase_create("JSON Parsing");
    tcase_set_timeout(tc_json, IK_TEST_TIMEOUT);
    tcase_add_checked_fixture(tc_json, setup, teardown);
    tcase_add_test(tc_json, test_json_array_root);
    tcase_add_test(tc_json, test_json_string_root);
    suite_add_tcase(s, tc_json);

    TCase *tc_parts = tcase_create("Parts Processing");
    tcase_set_timeout(tc_parts, IK_TEST_TIMEOUT);
    tcase_add_checked_fixture(tc_parts, setup, teardown);
    tcase_add_test(tc_parts, test_part_without_text_or_function_call);
    tcase_add_test(tc_parts, test_part_with_empty_text);
    tcase_add_test(tc_parts, test_part_with_null_text_value);
    tcase_add_test(tc_parts, test_part_with_non_string_text);
    suite_add_tcase(s, tc_parts);

    TCase *tc_model = tcase_create("Model Version");
    tcase_set_timeout(tc_model, IK_TEST_TIMEOUT);
    tcase_add_checked_fixture(tc_model, setup, teardown);
    tcase_add_test(tc_model, test_missing_model_version);
    suite_add_tcase(s, tc_model);

    TCase *tc_usage = tcase_create("Usage Metadata");
    tcase_set_timeout(tc_usage, IK_TEST_TIMEOUT);
    tcase_add_checked_fixture(tc_usage, setup, teardown);
    tcase_add_test(tc_usage, test_usage_empty_metadata);
    suite_add_tcase(s, tc_usage);

    TCase *tc_candidates = tcase_create("Candidates");
    tcase_set_timeout(tc_candidates, IK_TEST_TIMEOUT);
    tcase_add_checked_fixture(tc_candidates, setup, teardown);
    tcase_add_test(tc_candidates, test_empty_candidates_array);
    tcase_add_test(tc_candidates, test_candidate_without_content);
    tcase_add_test(tc_candidates, test_candidate_without_parts);
    suite_add_tcase(s, tc_candidates);

    return s;
}

int main(void)
{
    Suite *s = google_streaming_edge_cases_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/apps/ikigai/providers/google/google_streaming_edge_cases_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
