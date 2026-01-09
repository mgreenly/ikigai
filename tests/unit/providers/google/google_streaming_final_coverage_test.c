#include "../../../test_constants.h"
/**
 * @file google_streaming_final_coverage_test.c
 * @brief Final branch coverage tests for Google streaming
 *
 * Tests specific edge cases to achieve 100% branch coverage.
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
            case IK_STREAM_ERROR:
                if (event->data.error.message) {
                    strncpy(captured_strings1[captured_count], event->data.error.message, MAX_STRING_LEN - 1);
                    captured_strings1[captured_count][MAX_STRING_LEN - 1] = '\0';
                    captured_events[captured_count].data.error.message = captured_strings1[captured_count];
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

/* ================================================================
 * Usage Metadata Null Token Coverage Tests
 * ================================================================ */

START_TEST(test_usage_with_null_prompt_tokens) {
    ik_google_stream_ctx_t *sctx = NULL;
    res_t r = ik_google_stream_ctx_create(test_ctx, test_stream_cb, NULL, &sctx);
    ck_assert(!is_err(&r));

    /* Process START first */
    process_chunk(sctx, "{\"modelVersion\":\"gemini-2.5-flash\"}");

    /* Process usage with null promptTokenCount - covers line 264 false branch */
    const char *chunk =
        "{\"usageMetadata\":{\"candidatesTokenCount\":20,\"totalTokenCount\":20}}";
    process_chunk(sctx, chunk);

    /* Verify DONE event with zero input tokens */
    const ik_stream_event_t *event = find_event(IK_STREAM_DONE);
    ck_assert_ptr_nonnull(event);
    ck_assert_int_eq(event->data.done.usage.input_tokens, 0);
    ck_assert_int_eq(event->data.done.usage.output_tokens, 20);
}
END_TEST

START_TEST(test_usage_with_null_candidates_tokens) {
    ik_google_stream_ctx_t *sctx = NULL;
    res_t r = ik_google_stream_ctx_create(test_ctx, test_stream_cb, NULL, &sctx);
    ck_assert(!is_err(&r));

    /* Process START first */
    process_chunk(sctx, "{\"modelVersion\":\"gemini-2.5-flash\"}");

    /* Process usage with null candidatesTokenCount - covers line 265 false branch */
    const char *chunk =
        "{\"usageMetadata\":{\"promptTokenCount\":10,\"totalTokenCount\":10}}";
    process_chunk(sctx, chunk);

    /* Verify DONE event with zero output tokens */
    const ik_stream_event_t *event = find_event(IK_STREAM_DONE);
    ck_assert_ptr_nonnull(event);
    ck_assert_int_eq(event->data.done.usage.input_tokens, 10);
    ck_assert_int_eq(event->data.done.usage.output_tokens, 0);
}
END_TEST

START_TEST(test_usage_with_null_thoughts_tokens) {
    ik_google_stream_ctx_t *sctx = NULL;
    res_t r = ik_google_stream_ctx_create(test_ctx, test_stream_cb, NULL, &sctx);
    ck_assert(!is_err(&r));

    /* Process START first */
    process_chunk(sctx, "{\"modelVersion\":\"gemini-2.5-flash\"}");

    /* Process usage with null thoughtsTokenCount - covers line 266 false branch */
    const char *chunk =
        "{\"usageMetadata\":{\"promptTokenCount\":10,\"candidatesTokenCount\":20,\"totalTokenCount\":30}}";
    process_chunk(sctx, chunk);

    /* Verify DONE event with zero thinking tokens */
    const ik_stream_event_t *event = find_event(IK_STREAM_DONE);
    ck_assert_ptr_nonnull(event);
    ck_assert_int_eq(event->data.done.usage.thinking_tokens, 0);
    ck_assert_int_eq(event->data.done.usage.output_tokens, 20);
}
END_TEST

START_TEST(test_usage_with_all_null_token_fields) {
    ik_google_stream_ctx_t *sctx = NULL;
    res_t r = ik_google_stream_ctx_create(test_ctx, test_stream_cb, NULL, &sctx);
    ck_assert(!is_err(&r));

    /* Process START first */
    process_chunk(sctx, "{\"modelVersion\":\"gemini-2.5-flash\"}");

    /* Process minimal usage metadata - covers all NULL token branches */
    const char *chunk = "{\"usageMetadata\":{}}";
    process_chunk(sctx, chunk);

    /* Verify DONE event with all zero tokens */
    const ik_stream_event_t *event = find_event(IK_STREAM_DONE);
    ck_assert_ptr_nonnull(event);
    ck_assert_int_eq(event->data.done.usage.input_tokens, 0);
    ck_assert_int_eq(event->data.done.usage.output_tokens, 0);
    ck_assert_int_eq(event->data.done.usage.thinking_tokens, 0);
    ck_assert_int_eq(event->data.done.usage.total_tokens, 0);
}
END_TEST

/* ================================================================
 * JSON Structure Coverage Tests
 * ================================================================ */

START_TEST(test_root_not_object) {
    ik_google_stream_ctx_t *sctx = NULL;
    res_t r = ik_google_stream_ctx_create(test_ctx, test_stream_cb, NULL, &sctx);
    ck_assert(!is_err(&r));

    /* Process chunk with root as array instead of object - covers line 366 true branch */
    const char *chunk = "[\"not\",\"an\",\"object\"]";
    process_chunk(sctx, chunk);

    /* Verify no events emitted (chunk silently ignored) */
    ck_assert_int_eq((int)captured_count, 0);
}
END_TEST

START_TEST(test_root_is_string) {
    ik_google_stream_ctx_t *sctx = NULL;
    res_t r = ik_google_stream_ctx_create(test_ctx, test_stream_cb, NULL, &sctx);
    ck_assert(!is_err(&r));

    /* Process chunk with root as string - covers line 366 true branch */
    const char *chunk = "\"just a string\"";
    process_chunk(sctx, chunk);

    /* Verify no events emitted */
    ck_assert_int_eq((int)captured_count, 0);
}
END_TEST

START_TEST(test_root_is_number) {
    ik_google_stream_ctx_t *sctx = NULL;
    res_t r = ik_google_stream_ctx_create(test_ctx, test_stream_cb, NULL, &sctx);
    ck_assert(!is_err(&r));

    /* Process chunk with root as number - covers line 366 true branch */
    const char *chunk = "42";
    process_chunk(sctx, chunk);

    /* Verify no events emitted */
    ck_assert_int_eq((int)captured_count, 0);
}
END_TEST

/* ================================================================
 * Parts Thought Field Additional Coverage
 * ================================================================ */

START_TEST(test_thought_field_number_zero) {
    ik_google_stream_ctx_t *sctx = NULL;
    res_t r = ik_google_stream_ctx_create(test_ctx, test_stream_cb, NULL, &sctx);
    ck_assert(!is_err(&r));

    /* Process START first */
    process_chunk(sctx, "{\"modelVersion\":\"gemini-2.5-flash\"}");

    /* Reset to focus on content events */
    captured_count = 0;
    memset(captured_events, 0, sizeof(captured_events));

    /* Process part with thought field as number 0 - covers line 234 branch 5 */
    const char *chunk =
        "{\"candidates\":[{\"content\":{\"parts\":[{\"text\":\"Hello\",\"thought\":0}]}}]}";
    process_chunk(sctx, chunk);

    /* Verify events emitted */
    ck_assert_int_gt((int)captured_count, 0);
}
END_TEST

START_TEST(test_thought_field_number_nonzero) {
    ik_google_stream_ctx_t *sctx = NULL;
    res_t r = ik_google_stream_ctx_create(test_ctx, test_stream_cb, NULL, &sctx);
    ck_assert(!is_err(&r));

    /* Process START first */
    process_chunk(sctx, "{\"modelVersion\":\"gemini-2.5-flash\"}");

    /* Reset to focus on content events */
    captured_count = 0;
    memset(captured_events, 0, sizeof(captured_events));

    /* Process part with thought field as nonzero number - covers line 234 branch 5 */
    const char *chunk =
        "{\"candidates\":[{\"content\":{\"parts\":[{\"text\":\"Hello\",\"thought\":1}]}}]}";
    process_chunk(sctx, chunk);

    /* Verify events emitted */
    ck_assert_int_gt((int)captured_count, 0);
}
END_TEST

START_TEST(test_thought_field_array) {
    ik_google_stream_ctx_t *sctx = NULL;
    res_t r = ik_google_stream_ctx_create(test_ctx, test_stream_cb, NULL, &sctx);
    ck_assert(!is_err(&r));

    /* Process START first */
    process_chunk(sctx, "{\"modelVersion\":\"gemini-2.5-flash\"}");

    /* Reset to focus on content events */
    captured_count = 0;
    memset(captured_events, 0, sizeof(captured_events));

    /* Process part with thought field as array - covers line 234 additional branch */
    const char *chunk =
        "{\"candidates\":[{\"content\":{\"parts\":[{\"text\":\"Hello\",\"thought\":[]}]}}]}";
    process_chunk(sctx, chunk);

    /* Verify events emitted */
    ck_assert_int_gt((int)captured_count, 0);
}
END_TEST

START_TEST(test_thought_field_object) {
    ik_google_stream_ctx_t *sctx = NULL;
    res_t r = ik_google_stream_ctx_create(test_ctx, test_stream_cb, NULL, &sctx);
    ck_assert(!is_err(&r));

    /* Process START first */
    process_chunk(sctx, "{\"modelVersion\":\"gemini-2.5-flash\"}");

    /* Reset to focus on content events */
    captured_count = 0;
    memset(captured_events, 0, sizeof(captured_events));

    /* Process part with thought field as object - covers line 234 additional branch */
    const char *chunk =
        "{\"candidates\":[{\"content\":{\"parts\":[{\"text\":\"Hello\",\"thought\":{}}]}}]}";
    process_chunk(sctx, chunk);

    /* Verify events emitted */
    ck_assert_int_gt((int)captured_count, 0);
}
END_TEST

/* ================================================================
 * Test Suite Setup
 * ================================================================ */

static Suite *google_streaming_final_coverage_suite(void)
{
    Suite *s = suite_create("Google Streaming - Final Coverage");

    TCase *tc_usage = tcase_create("Usage Null Token Fields");
    tcase_set_timeout(tc_usage, IK_TEST_TIMEOUT);
    tcase_add_checked_fixture(tc_usage, setup, teardown);
    tcase_add_test(tc_usage, test_usage_with_null_prompt_tokens);
    tcase_add_test(tc_usage, test_usage_with_null_candidates_tokens);
    tcase_add_test(tc_usage, test_usage_with_null_thoughts_tokens);
    tcase_add_test(tc_usage, test_usage_with_all_null_token_fields);
    suite_add_tcase(s, tc_usage);

    TCase *tc_json = tcase_create("JSON Structure");
    tcase_set_timeout(tc_json, IK_TEST_TIMEOUT);
    tcase_add_checked_fixture(tc_json, setup, teardown);
    tcase_add_test(tc_json, test_root_not_object);
    tcase_add_test(tc_json, test_root_is_string);
    tcase_add_test(tc_json, test_root_is_number);
    suite_add_tcase(s, tc_json);

    TCase *tc_thought = tcase_create("Thought Field Additional Coverage");
    tcase_set_timeout(tc_thought, IK_TEST_TIMEOUT);
    tcase_add_checked_fixture(tc_thought, setup, teardown);
    tcase_add_test(tc_thought, test_thought_field_number_zero);
    tcase_add_test(tc_thought, test_thought_field_number_nonzero);
    tcase_add_test(tc_thought, test_thought_field_array);
    tcase_add_test(tc_thought, test_thought_field_object);
    suite_add_tcase(s, tc_thought);

    return s;
}

int main(void)
{
    Suite *s = google_streaming_final_coverage_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
