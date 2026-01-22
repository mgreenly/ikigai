#include "../../../test_constants.h"
/**
 * @file google_streaming_missing_branches_test.c
 * @brief Additional branch coverage tests for Google streaming
 *
 * Tests remaining uncovered branches to achieve closer to 100% coverage.
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
            case IK_STREAM_TEXT_DELTA:
                if (event->data.delta.text) {
                    strncpy(captured_strings1[captured_count], event->data.delta.text, MAX_STRING_LEN - 1);
                    captured_strings1[captured_count][MAX_STRING_LEN - 1] = '\0';
                    captured_events[captured_count].data.delta.text = captured_strings1[captured_count];
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
 * Missing Branch Coverage Tests
 * ================================================================ */

START_TEST(test_error_with_null_status_in_map) {
    ik_google_stream_ctx_t *sctx = NULL;
    res_t r = ik_google_stream_ctx_create(test_ctx, test_stream_cb, NULL, &sctx);
    ck_assert(!is_err(&r));

    /* Process error with null status - covers line 59 true branch in map_error_status */
    /* When status_str is NULL (from yyjson_get_str returning NULL), it's passed to map_error_status */
    const char *chunk = "{\"error\":{\"message\":\"Error\",\"status\":null}}";
    process_chunk(sctx, chunk);

    /* Verify ERROR event with UNKNOWN category */
    const ik_stream_event_t *event = find_event(IK_STREAM_ERROR);
    ck_assert_ptr_nonnull(event);
    ck_assert_int_eq(event->data.error.category, IK_ERR_CAT_UNKNOWN);
    ck_assert_str_eq(event->data.error.message, "Error");
}
END_TEST

START_TEST(test_text_without_thinking_transition) {
    ik_google_stream_ctx_t *sctx = NULL;
    res_t r = ik_google_stream_ctx_create(test_ctx, test_stream_cb, NULL, &sctx);
    ck_assert(!is_err(&r));

    /* Process START first */
    process_chunk(sctx, "{\"modelVersion\":\"gemini-2.5-flash\"}");

    /* Reset to focus on next events */
    captured_count = 0;
    memset(captured_events, 0, sizeof(captured_events));

    /* Process regular text without ever being in thinking state - covers line 200 false branch */
    const char *chunk =
        "{\"candidates\":[{\"content\":{\"parts\":[{\"text\":\"Hello\"}]}}]}";
    process_chunk(sctx, chunk);

    /* Verify TEXT_DELTA at index 0 (not incremented) */
    const ik_stream_event_t *event = find_event(IK_STREAM_TEXT_DELTA);
    ck_assert_ptr_nonnull(event);
    ck_assert_int_eq(event->index, 0);
    ck_assert_str_eq(event->data.delta.text, "Hello");
}
END_TEST

START_TEST(test_malformed_json_chunk) {
    ik_google_stream_ctx_t *sctx = NULL;
    res_t r = ik_google_stream_ctx_create(test_ctx, test_stream_cb, NULL, &sctx);
    ck_assert(!is_err(&r));

    /* Process malformed JSON - covers line 362 true branch (doc == NULL) */
    const char *chunk = "{\"invalid\":json syntax}";
    process_chunk(sctx, chunk);

    /* Verify no events emitted (chunk silently ignored) */
    ck_assert_int_eq((int)captured_count, 0);
}
END_TEST

START_TEST(test_process_data_with_null_input) {
    ik_google_stream_ctx_t *sctx = NULL;
    res_t r = ik_google_stream_ctx_create(test_ctx, test_stream_cb, NULL, &sctx);
    ck_assert(!is_err(&r));

    /* Process NULL data - covers line 353 true branch (data == NULL) */
    process_chunk(sctx, NULL);

    /* Verify no events emitted */
    ck_assert_int_eq((int)captured_count, 0);
}
END_TEST

START_TEST(test_process_data_with_empty_string) {
    ik_google_stream_ctx_t *sctx = NULL;
    res_t r = ik_google_stream_ctx_create(test_ctx, test_stream_cb, NULL, &sctx);
    ck_assert(!is_err(&r));

    /* Process empty string data - covers line 353 true branch (data[0] == '\0') */
    process_chunk(sctx, "");

    /* Verify no events emitted */
    ck_assert_int_eq((int)captured_count, 0);
}
END_TEST

START_TEST(test_usage_with_null_total_tokens) {
    ik_google_stream_ctx_t *sctx = NULL;
    res_t r = ik_google_stream_ctx_create(test_ctx, test_stream_cb, NULL, &sctx);
    ck_assert(!is_err(&r));

    /* Process START first */
    process_chunk(sctx, "{\"modelVersion\":\"gemini-2.5-flash\"}");

    /* Process usage without totalTokenCount field - covers line 277 false branch in ternary */
    const char *chunk =
        "{\"usageMetadata\":{\"promptTokenCount\":10,\"candidatesTokenCount\":20}}";
    process_chunk(sctx, chunk);

    /* Verify DONE event with zero total_tokens */
    const ik_stream_event_t *event = find_event(IK_STREAM_DONE);
    ck_assert_ptr_nonnull(event);
    ck_assert_int_eq(event->data.done.usage.input_tokens, 10);
    ck_assert_int_eq(event->data.done.usage.output_tokens, 20);
    ck_assert_int_eq(event->data.done.usage.total_tokens, 0);
}
END_TEST

START_TEST(test_part_with_null_thought_field) {
    ik_google_stream_ctx_t *sctx = NULL;
    res_t r = ik_google_stream_ctx_create(test_ctx, test_stream_cb, NULL, &sctx);
    ck_assert(!is_err(&r));

    /* Process START first */
    process_chunk(sctx, "{\"modelVersion\":\"gemini-2.5-flash\"}");

    /* Reset to focus on content events */
    captured_count = 0;
    memset(captured_events, 0, sizeof(captured_events));

    /* Process part with explicit null thought field - covers line 233 branch */
    const char *chunk =
        "{\"candidates\":[{\"content\":{\"parts\":[{\"text\":\"Hello\",\"thought\":null}]}}]}";
    process_chunk(sctx, chunk);

    /* Verify TEXT_DELTA (not THINKING_DELTA) */
    ck_assert_int_eq((int)count_events(IK_STREAM_TEXT_DELTA), 1);
    ck_assert_int_eq((int)count_events(IK_STREAM_THINKING_DELTA), 0);
}
END_TEST

START_TEST(test_candidate_with_null_finish_reason) {
    ik_google_stream_ctx_t *sctx = NULL;
    res_t r = ik_google_stream_ctx_create(test_ctx, test_stream_cb, NULL, &sctx);
    ck_assert(!is_err(&r));

    /* Process START first */
    process_chunk(sctx, "{\"modelVersion\":\"gemini-2.5-flash\"}");

    /* Process candidate with null finishReason value - covers line 411 branch */
    const char *chunk =
        "{\"candidates\":[{\"finishReason\":null,\"content\":{\"parts\":[{\"text\":\"Hi\"}]}}]}";
    process_chunk(sctx, chunk);

    /* Verify finish_reason remains UNKNOWN */
    ik_finish_reason_t reason = ik_google_stream_get_finish_reason(sctx);
    ck_assert_int_eq(reason, IK_FINISH_UNKNOWN);
}
END_TEST

START_TEST(test_multiple_text_deltas_without_thinking) {
    ik_google_stream_ctx_t *sctx = NULL;
    res_t r = ik_google_stream_ctx_create(test_ctx, test_stream_cb, NULL, &sctx);
    ck_assert(!is_err(&r));

    /* Process START first */
    process_chunk(sctx, "{\"modelVersion\":\"gemini-2.5-flash\"}");

    /* Process first text chunk */
    const char *chunk1 =
        "{\"candidates\":[{\"content\":{\"parts\":[{\"text\":\"Hello\"}]}}]}";
    process_chunk(sctx, chunk1);

    /* Process second text chunk - should stay at same index since not transitioning from thinking */
    const char *chunk2 =
        "{\"candidates\":[{\"content\":{\"parts\":[{\"text\":\" world\"}]}}]}";
    process_chunk(sctx, chunk2);

    /* Both text deltas should be at index 0 */
    ck_assert_int_eq((int)count_events(IK_STREAM_TEXT_DELTA), 2);
    ck_assert_int_eq(captured_events[1].index, 0);
    ck_assert_int_eq(captured_events[2].index, 0);
}
END_TEST

START_TEST(test_tool_call_ended_by_thinking) {
    ik_google_stream_ctx_t *sctx = NULL;
    res_t r = ik_google_stream_ctx_create(test_ctx, test_stream_cb, NULL, &sctx);
    ck_assert(!is_err(&r));

    /* Process START first */
    process_chunk(sctx, "{\"modelVersion\":\"gemini-2.5-flash\"}");

    /* Start a tool call */
    const char *tool_chunk =
        "{\"candidates\":[{\"content\":{\"parts\":[{\"functionCall\":{\"name\":\"test_func\",\"args\":{\"x\":1}}}]}}]}";
    process_chunk(sctx, tool_chunk);

    /* Reset to focus on next events */
    captured_count = 0;
    memset(captured_events, 0, sizeof(captured_events));

    /* Process thinking which should end the tool call */
    const char *thinking_chunk =
        "{\"candidates\":[{\"content\":{\"parts\":[{\"text\":\"Let me think...\",\"thought\":true}]}}]}";
    process_chunk(sctx, thinking_chunk);

    /* Verify TOOL_CALL_DONE was emitted before THINKING_DELTA */
    ck_assert_int_eq((int)count_events(IK_STREAM_TOOL_CALL_DONE), 1);
    ck_assert_int_eq((int)count_events(IK_STREAM_THINKING_DELTA), 1);
    /* TOOL_CALL_DONE should be first */
    ck_assert_int_eq(captured_events[0].type, IK_STREAM_TOOL_CALL_DONE);
    ck_assert_int_eq(captured_events[1].type, IK_STREAM_THINKING_DELTA);
}
END_TEST

/* ================================================================
 * Test Suite Setup
 * ================================================================ */

static Suite *google_streaming_missing_branches_suite(void)
{
    Suite *s = suite_create("Google Streaming - Missing Branches");

    TCase *tc_main = tcase_create("Missing Branch Coverage");
    tcase_set_timeout(tc_main, IK_TEST_TIMEOUT);
    tcase_add_checked_fixture(tc_main, setup, teardown);
    tcase_add_test(tc_main, test_error_with_null_status_in_map);
    tcase_add_test(tc_main, test_text_without_thinking_transition);
    tcase_add_test(tc_main, test_malformed_json_chunk);
    tcase_add_test(tc_main, test_process_data_with_null_input);
    tcase_add_test(tc_main, test_process_data_with_empty_string);
    tcase_add_test(tc_main, test_usage_with_null_total_tokens);
    tcase_add_test(tc_main, test_part_with_null_thought_field);
    tcase_add_test(tc_main, test_candidate_with_null_finish_reason);
    tcase_add_test(tc_main, test_multiple_text_deltas_without_thinking);
    tcase_add_test(tc_main, test_tool_call_ended_by_thinking);
    suite_add_tcase(s, tc_main);

    return s;
}

int main(void)
{
    Suite *s = google_streaming_missing_branches_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/providers/google/google_streaming_missing_branches_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
