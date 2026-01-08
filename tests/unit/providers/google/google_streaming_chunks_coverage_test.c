/**
 * @file google_streaming_chunks_coverage_test.c
 * @brief Branch coverage tests for Google streaming - Chunk structure
 *
 * Tests chunk structure edge cases and tool call transitions.
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
 * Chunk Structure Edge Cases
 * ================================================================ */

START_TEST(test_chunk_without_modelversion) {
    ik_google_stream_ctx_t *sctx = NULL;
    res_t r = ik_google_stream_ctx_create(test_ctx, test_stream_cb, NULL, &sctx);
    ck_assert(!is_err(&r));

    /* Process chunk without modelVersion field - covers line 383 false branch */
    const char *chunk =
        "{\"candidates\":[{\"content\":{\"parts\":[{\"text\":\"Hello\"}]}}]}";
    process_chunk(sctx, chunk);

    /* Verify START event with NULL model */
    const ik_stream_event_t *event = find_event(IK_STREAM_START);
    ck_assert_ptr_nonnull(event);
    ck_assert_ptr_null(event->data.start.model);
}
END_TEST

START_TEST(test_chunk_with_non_string_modelversion) {
    ik_google_stream_ctx_t *sctx = NULL;
    res_t r = ik_google_stream_ctx_create(test_ctx, test_stream_cb, NULL, &sctx);
    ck_assert(!is_err(&r));

    /* Process chunk with non-string modelVersion - covers line 385 false branch */
    const char *chunk = "{\"modelVersion\":123}";
    process_chunk(sctx, chunk);

    /* Verify START event with NULL model */
    const ik_stream_event_t *event = find_event(IK_STREAM_START);
    ck_assert_ptr_nonnull(event);
    ck_assert_ptr_null(event->data.start.model);
}
END_TEST

START_TEST(test_chunk_without_finishreason) {
    ik_google_stream_ctx_t *sctx = NULL;
    res_t r = ik_google_stream_ctx_create(test_ctx, test_stream_cb, NULL, &sctx);
    ck_assert(!is_err(&r));

    /* Process START first */
    process_chunk(sctx, "{\"modelVersion\":\"gemini-2.5-flash\"}");

    /* Process candidate without finishReason - covers line 409 false branch */
    const char *chunk =
        "{\"candidates\":[{\"content\":{\"parts\":[{\"text\":\"Hello\"}]}}]}";
    process_chunk(sctx, chunk);

    /* Verify finish_reason remains UNKNOWN */
    ik_finish_reason_t reason = ik_google_stream_get_finish_reason(sctx);
    ck_assert_int_eq(reason, IK_FINISH_UNKNOWN);
}
END_TEST

START_TEST(test_candidate_without_content) {
    ik_google_stream_ctx_t *sctx = NULL;
    res_t r = ik_google_stream_ctx_create(test_ctx, test_stream_cb, NULL, &sctx);
    ck_assert(!is_err(&r));

    /* Process START first */
    process_chunk(sctx, "{\"modelVersion\":\"gemini-2.5-flash\"}");

    /* Process candidate without content field - covers line 416 false branch */
    const char *chunk =
        "{\"candidates\":[{\"finishReason\":\"STOP\"}]}";
    process_chunk(sctx, chunk);

    /* Verify only START event, no content events */
    ck_assert_int_eq((int)captured_count, 1);
    ck_assert_int_eq(captured_events[0].type, IK_STREAM_START);
}
END_TEST

START_TEST(test_content_without_parts) {
    ik_google_stream_ctx_t *sctx = NULL;
    res_t r = ik_google_stream_ctx_create(test_ctx, test_stream_cb, NULL, &sctx);
    ck_assert(!is_err(&r));

    /* Process START first */
    process_chunk(sctx, "{\"modelVersion\":\"gemini-2.5-flash\"}");

    /* Process content without parts field - covers line 418 branch 1 */
    const char *chunk =
        "{\"candidates\":[{\"content\":{}}]}";
    process_chunk(sctx, chunk);

    /* Verify only START event */
    ck_assert_int_eq((int)captured_count, 1);
    ck_assert_int_eq(captured_events[0].type, IK_STREAM_START);
}
END_TEST

START_TEST(test_content_with_non_array_parts) {
    ik_google_stream_ctx_t *sctx = NULL;
    res_t r = ik_google_stream_ctx_create(test_ctx, test_stream_cb, NULL, &sctx);
    ck_assert(!is_err(&r));

    /* Process START first */
    process_chunk(sctx, "{\"modelVersion\":\"gemini-2.5-flash\"}");

    /* Process content with non-array parts - covers line 418 branch 3 */
    const char *chunk =
        "{\"candidates\":[{\"content\":{\"parts\":\"not-an-array\"}}]}";
    process_chunk(sctx, chunk);

    /* Verify only START event */
    ck_assert_int_eq((int)captured_count, 1);
    ck_assert_int_eq(captured_events[0].type, IK_STREAM_START);
}
END_TEST

START_TEST(test_chunk_without_usage) {
    ik_google_stream_ctx_t *sctx = NULL;
    res_t r = ik_google_stream_ctx_create(test_ctx, test_stream_cb, NULL, &sctx);
    ck_assert(!is_err(&r));

    /* Process START first */
    process_chunk(sctx, "{\"modelVersion\":\"gemini-2.5-flash\"}");

    /* Process chunk without usageMetadata - covers line 427 false branch */
    const char *chunk =
        "{\"candidates\":[{\"content\":{\"parts\":[{\"text\":\"Hello\"}]}}]}";
    process_chunk(sctx, chunk);

    /* Verify no DONE event */
    ck_assert_int_eq((int)count_events(IK_STREAM_DONE), 0);
}
END_TEST

START_TEST(test_end_tool_call_when_not_in_tool_call) {
    ik_google_stream_ctx_t *sctx = NULL;
    res_t r = ik_google_stream_ctx_create(test_ctx, test_stream_cb, NULL, &sctx);
    ck_assert(!is_err(&r));

    /* Process START first */
    process_chunk(sctx, "{\"modelVersion\":\"gemini-2.5-flash\"}");

    /* Reset to focus on next events */
    captured_count = 0;
    memset(captured_events, 0, sizeof(captured_events));

    /* Process regular text which calls end_tool_call_if_needed when NOT in tool call */
    /* This covers line 42 false branch */
    const char *chunk =
        "{\"candidates\":[{\"content\":{\"parts\":[{\"text\":\"Hello\"}]}}]}";
    process_chunk(sctx, chunk);

    /* Verify TEXT_DELTA but no TOOL_CALL_DONE */
    ck_assert_int_eq((int)count_events(IK_STREAM_TEXT_DELTA), 1);
    ck_assert_int_eq((int)count_events(IK_STREAM_TOOL_CALL_DONE), 0);
}
END_TEST

START_TEST(test_tool_call_ended_by_text) {
    ik_google_stream_ctx_t *sctx = NULL;
    res_t r = ik_google_stream_ctx_create(test_ctx, test_stream_cb, NULL, &sctx);
    ck_assert(!is_err(&r));

    /* Process START first */
    process_chunk(sctx, "{\"modelVersion\":\"gemini-2.5-flash\"}");

    /* Start a tool call */
    const char *tool_chunk =
        "{\"candidates\":[{\"content\":{\"parts\":[{\"functionCall\":{\"name\":\"test_func\",\"args\":{\"x\":1}}}]}}]}";
    process_chunk(sctx, tool_chunk);

    /* Verify tool call started */
    ck_assert_int_eq((int)count_events(IK_STREAM_TOOL_CALL_START), 1);

    /* Reset to focus on next events */
    captured_count = 0;
    memset(captured_events, 0, sizeof(captured_events));

    /* Process text which should end the tool call - covers line 42 true branch */
    const char *text_chunk =
        "{\"candidates\":[{\"content\":{\"parts\":[{\"text\":\"Result\"}]}}]}";
    process_chunk(sctx, text_chunk);

    /* Verify TOOL_CALL_DONE was emitted before TEXT_DELTA */
    ck_assert_int_eq((int)count_events(IK_STREAM_TOOL_CALL_DONE), 1);
    ck_assert_int_eq((int)count_events(IK_STREAM_TEXT_DELTA), 1);
}
END_TEST

START_TEST(test_tool_call_ended_by_usage) {
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

    /* Process usage which should end the tool call - covers line 280 and line 427 true branch */
    const char *usage_chunk =
        "{\"usageMetadata\":{\"promptTokenCount\":10,\"candidatesTokenCount\":20,\"totalTokenCount\":30}}";
    process_chunk(sctx, usage_chunk);

    /* Verify TOOL_CALL_DONE and DONE events */
    ck_assert_int_eq((int)count_events(IK_STREAM_TOOL_CALL_DONE), 1);
    ck_assert_int_eq((int)count_events(IK_STREAM_DONE), 1);
}
END_TEST

/* ================================================================
 * Test Suite Setup
 * ================================================================ */

static Suite *google_streaming_chunks_coverage_suite(void)
{
    Suite *s = suite_create("Google Streaming - Chunks Coverage");

    TCase *tc_chunk = tcase_create("Chunk Structure Edge Cases");
    tcase_set_timeout(tc_chunk, IK_TEST_TIMEOUT);
    tcase_add_checked_fixture(tc_chunk, setup, teardown);
    tcase_add_test(tc_chunk, test_chunk_without_modelversion);
    tcase_add_test(tc_chunk, test_chunk_with_non_string_modelversion);
    tcase_add_test(tc_chunk, test_chunk_without_finishreason);
    tcase_add_test(tc_chunk, test_candidate_without_content);
    tcase_add_test(tc_chunk, test_content_without_parts);
    tcase_add_test(tc_chunk, test_content_with_non_array_parts);
    tcase_add_test(tc_chunk, test_chunk_without_usage);
    tcase_add_test(tc_chunk, test_end_tool_call_when_not_in_tool_call);
    tcase_add_test(tc_chunk, test_tool_call_ended_by_text);
    tcase_add_test(tc_chunk, test_tool_call_ended_by_usage);
    suite_add_tcase(s, tc_chunk);

    return s;
}

int main(void)
{
    Suite *s = google_streaming_chunks_coverage_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
