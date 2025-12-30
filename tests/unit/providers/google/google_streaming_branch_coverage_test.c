/**
 * @file google_streaming_branch_coverage_test.c
 * @brief Additional branch coverage tests for Google streaming
 *
 * Tests target specific uncovered branches to improve coverage from 77.9% to higher.
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
static char captured_strings2[MAX_EVENTS][MAX_STRING_LEN];
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
            case IK_STREAM_TEXT_DELTA:
                if (event->data.delta.text) {
                    strncpy(captured_strings1[captured_count], event->data.delta.text, MAX_STRING_LEN - 1);
                    captured_strings1[captured_count][MAX_STRING_LEN - 1] = '\0';
                    captured_events[captured_count].data.delta.text = captured_strings1[captured_count];
                }
                break;
            case IK_STREAM_THINKING_DELTA:
                if (event->data.delta.text) {
                    strncpy(captured_strings1[captured_count], event->data.delta.text, MAX_STRING_LEN - 1);
                    captured_strings1[captured_count][MAX_STRING_LEN - 1] = '\0';
                    captured_events[captured_count].data.delta.text = captured_strings1[captured_count];
                }
                break;
            case IK_STREAM_TOOL_CALL_START:
                if (event->data.tool_start.id) {
                    strncpy(captured_strings1[captured_count], event->data.tool_start.id, MAX_STRING_LEN - 1);
                    captured_strings1[captured_count][MAX_STRING_LEN - 1] = '\0';
                    captured_events[captured_count].data.tool_start.id = captured_strings1[captured_count];
                }
                if (event->data.tool_start.name) {
                    strncpy(captured_strings2[captured_count], event->data.tool_start.name, MAX_STRING_LEN - 1);
                    captured_strings2[captured_count][MAX_STRING_LEN - 1] = '\0';
                    captured_events[captured_count].data.tool_start.name = captured_strings2[captured_count];
                }
                break;
            case IK_STREAM_TOOL_CALL_DELTA:
                if (event->data.tool_delta.arguments) {
                    strncpy(captured_strings1[captured_count], event->data.tool_delta.arguments, MAX_STRING_LEN - 1);
                    captured_strings1[captured_count][MAX_STRING_LEN - 1] = '\0';
                    captured_events[captured_count].data.tool_delta.arguments = captured_strings1[captured_count];
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
 * Function Call Branch Coverage Tests
 * ================================================================ */

START_TEST(test_function_call_without_name) {
    ik_google_stream_ctx_t *sctx = NULL;
    res_t r = ik_google_stream_ctx_create(test_ctx, test_stream_cb, NULL, &sctx);
    ck_assert(!is_err(&r));

    /* Process START first */
    process_chunk(sctx, "{\"modelVersion\":\"gemini-2.5-flash\"}");

    /* Process function call without name field - covers line 125 false branch */
    const char *chunk =
        "{\"candidates\":[{\"content\":{\"parts\":[{\"functionCall\":{\"args\":{\"x\":1}}}]}}]}";
    process_chunk(sctx, chunk);

    /* Verify TOOL_CALL_START was emitted with NULL name */
    const ik_stream_event_t *event = find_event(IK_STREAM_TOOL_CALL_START);
    ck_assert_ptr_nonnull(event);
    ck_assert_ptr_nonnull(event->data.tool_start.id); /* ID should be generated */
    ck_assert_ptr_null(event->data.tool_start.name);  /* Name should be NULL */
}

END_TEST

START_TEST(test_function_call_with_null_name) {
    ik_google_stream_ctx_t *sctx = NULL;
    res_t r = ik_google_stream_ctx_create(test_ctx, test_stream_cb, NULL, &sctx);
    ck_assert(!is_err(&r));

    /* Process START first */
    process_chunk(sctx, "{\"modelVersion\":\"gemini-2.5-flash\"}");

    /* Process function call with null name value - covers line 127 false branch */
    const char *chunk =
        "{\"candidates\":[{\"content\":{\"parts\":[{\"functionCall\":{\"name\":null,\"args\":{\"x\":1}}}]}}]}";
    process_chunk(sctx, chunk);

    /* Verify TOOL_CALL_START was emitted with NULL name */
    const ik_stream_event_t *event = find_event(IK_STREAM_TOOL_CALL_START);
    ck_assert_ptr_nonnull(event);
    ck_assert_ptr_nonnull(event->data.tool_start.id);
    ck_assert_ptr_null(event->data.tool_start.name);
}

END_TEST

START_TEST(test_function_call_with_non_string_name) {
    ik_google_stream_ctx_t *sctx = NULL;
    res_t r = ik_google_stream_ctx_create(test_ctx, test_stream_cb, NULL, &sctx);
    ck_assert(!is_err(&r));

    /* Process START first */
    process_chunk(sctx, "{\"modelVersion\":\"gemini-2.5-flash\"}");

    /* Process function call with non-string name - covers line 127 false branch */
    const char *chunk =
        "{\"candidates\":[{\"content\":{\"parts\":[{\"functionCall\":{\"name\":123,\"args\":{\"x\":1}}}]}}]}";
    process_chunk(sctx, chunk);

    /* Verify TOOL_CALL_START was emitted with NULL name */
    const ik_stream_event_t *event = find_event(IK_STREAM_TOOL_CALL_START);
    ck_assert_ptr_nonnull(event);
    ck_assert_ptr_null(event->data.tool_start.name);
}

END_TEST

START_TEST(test_function_call_without_args) {
    ik_google_stream_ctx_t *sctx = NULL;
    res_t r = ik_google_stream_ctx_create(test_ctx, test_stream_cb, NULL, &sctx);
    ck_assert(!is_err(&r));

    /* Process START first */
    process_chunk(sctx, "{\"modelVersion\":\"gemini-2.5-flash\"}");

    /* Process function call without args field - covers line 146 false branch */
    const char *chunk =
        "{\"candidates\":[{\"content\":{\"parts\":[{\"functionCall\":{\"name\":\"test_func\"}}]}}]}";
    process_chunk(sctx, chunk);

    /* Verify TOOL_CALL_START but no TOOL_CALL_DELTA */
    ck_assert_int_eq((int)count_events(IK_STREAM_TOOL_CALL_START), 1);
    ck_assert_int_eq((int)count_events(IK_STREAM_TOOL_CALL_DELTA), 0);
}

END_TEST

START_TEST(test_function_call_with_null_args) {
    ik_google_stream_ctx_t *sctx = NULL;
    res_t r = ik_google_stream_ctx_create(test_ctx, test_stream_cb, NULL, &sctx);
    ck_assert(!is_err(&r));

    /* Process START first */
    process_chunk(sctx, "{\"modelVersion\":\"gemini-2.5-flash\"}");

    /* Process function call with null args - yyjson serializes null to "null" string */
    const char *chunk =
        "{\"candidates\":[{\"content\":{\"parts\":[{\"functionCall\":{\"name\":\"test_func\",\"args\":null}}]}}]}";
    process_chunk(sctx, chunk);

    /* Verify TOOL_CALL_START and TOOL_CALL_DELTA with "null" */
    ck_assert_int_eq((int)count_events(IK_STREAM_TOOL_CALL_START), 1);
    ck_assert_int_eq((int)count_events(IK_STREAM_TOOL_CALL_DELTA), 1);
    const ik_stream_event_t *delta = find_event(IK_STREAM_TOOL_CALL_DELTA);
    ck_assert_ptr_nonnull(delta);
    ck_assert_str_eq(delta->data.tool_delta.arguments, "null");
}

END_TEST

START_TEST(test_function_call_continued_with_more_args) {
    ik_google_stream_ctx_t *sctx = NULL;
    res_t r = ik_google_stream_ctx_create(test_ctx, test_stream_cb, NULL, &sctx);
    ck_assert(!is_err(&r));

    /* Process START first */
    process_chunk(sctx, "{\"modelVersion\":\"gemini-2.5-flash\"}");

    /* Start a tool call with initial args */
    const char *chunk1 =
        "{\"candidates\":[{\"content\":{\"parts\":[{\"functionCall\":{\"name\":\"test_func\",\"args\":{\"a\":1}}}]}}]}";
    process_chunk(sctx, chunk1);

    /* Verify initial tool call started */
    ck_assert_int_eq((int)count_events(IK_STREAM_TOOL_CALL_START), 1);
    ck_assert_int_eq((int)count_events(IK_STREAM_TOOL_CALL_DELTA), 1);

    /* Reset to check continuation */
    captured_count = 0;
    memset(captured_events, 0, sizeof(captured_events));

    /* Continue the same tool call with more args - covers line 119 false branch */
    const char *chunk2 =
        "{\"candidates\":[{\"content\":{\"parts\":[{\"functionCall\":{\"args\":{\"b\":2}}}]}}]}";
    process_chunk(sctx, chunk2);

    /* Verify no new START, just another DELTA */
    ck_assert_int_eq((int)count_events(IK_STREAM_TOOL_CALL_START), 0);
    ck_assert_int_eq((int)count_events(IK_STREAM_TOOL_CALL_DELTA), 1);
}

END_TEST

/* ================================================================
 * Thinking Transition Coverage Tests
 * ================================================================ */

START_TEST(test_text_after_thinking_increments_index) {
    ik_google_stream_ctx_t *sctx = NULL;
    res_t r = ik_google_stream_ctx_create(test_ctx, test_stream_cb, NULL, &sctx);
    ck_assert(!is_err(&r));

    /* Process START first */
    process_chunk(sctx, "{\"modelVersion\":\"gemini-2.5-flash\"}");

    /* Process thinking content first - covers line 199 true branch */
    const char *thinking_chunk =
        "{\"candidates\":[{\"content\":{\"parts\":[{\"text\":\"Thinking...\",\"thought\":true}]}}]}";
    process_chunk(sctx, thinking_chunk);

    /* Verify thinking delta at index 0 */
    const ik_stream_event_t *thinking_event = find_event(IK_STREAM_THINKING_DELTA);
    ck_assert_ptr_nonnull(thinking_event);
    ck_assert_int_eq(thinking_event->index, 0);

    /* Reset to check next event */
    captured_count = 0;
    memset(captured_events, 0, sizeof(captured_events));

    /* Process regular text after thinking */
    const char *text_chunk =
        "{\"candidates\":[{\"content\":{\"parts\":[{\"text\":\"Answer\"}]}}]}";
    process_chunk(sctx, text_chunk);

    /* Verify text delta at index 1 (incremented from 0) */
    const ik_stream_event_t *text_event = find_event(IK_STREAM_TEXT_DELTA);
    ck_assert_ptr_nonnull(text_event);
    ck_assert_int_eq(text_event->index, 1);
}

END_TEST

/* ================================================================
 * Candidates Processing Branch Coverage Tests
 * ================================================================ */

START_TEST(test_candidates_empty_array) {
    ik_google_stream_ctx_t *sctx = NULL;
    res_t r = ik_google_stream_ctx_create(test_ctx, test_stream_cb, NULL, &sctx);
    ck_assert(!is_err(&r));

    /* Process chunk with empty candidates array - covers line 405 false branch */
    const char *chunk = "{\"modelVersion\":\"gemini-2.5-flash\",\"candidates\":[]}";
    process_chunk(sctx, chunk);

    /* Verify only START event emitted */
    ck_assert_int_eq((int)captured_count, 1);
    ck_assert_int_eq(captured_events[0].type, IK_STREAM_START);
}

END_TEST

START_TEST(test_candidates_not_array) {
    ik_google_stream_ctx_t *sctx = NULL;
    res_t r = ik_google_stream_ctx_create(test_ctx, test_stream_cb, NULL, &sctx);
    ck_assert(!is_err(&r));

    /* Process chunk with candidates as non-array - covers line 403 false branch */
    const char *chunk = "{\"modelVersion\":\"gemini-2.5-flash\",\"candidates\":null}";
    process_chunk(sctx, chunk);

    /* Verify only START event emitted */
    ck_assert_int_eq((int)captured_count, 1);
    ck_assert_int_eq(captured_events[0].type, IK_STREAM_START);
}

END_TEST

/* ================================================================
 * Parts Processing Edge Cases
 * ================================================================ */

START_TEST(test_part_without_text_field) {
    ik_google_stream_ctx_t *sctx = NULL;
    res_t r = ik_google_stream_ctx_create(test_ctx, test_stream_cb, NULL, &sctx);
    ck_assert(!is_err(&r));

    /* Process START first */
    process_chunk(sctx, "{\"modelVersion\":\"gemini-2.5-flash\"}");

    /* Process part without text field (only has other fields) - covers line 237 true branch */
    const char *chunk =
        "{\"candidates\":[{\"content\":{\"parts\":[{\"someOtherField\":\"value\"}]}}]}";
    process_chunk(sctx, chunk);

    /* Verify only START event, no TEXT_DELTA */
    ck_assert_int_eq((int)captured_count, 1);
    ck_assert_int_eq(captured_events[0].type, IK_STREAM_START);
}
END_TEST

START_TEST(test_part_with_null_text) {
    ik_google_stream_ctx_t *sctx = NULL;
    res_t r = ik_google_stream_ctx_create(test_ctx, test_stream_cb, NULL, &sctx);
    ck_assert(!is_err(&r));

    /* Process START first */
    process_chunk(sctx, "{\"modelVersion\":\"gemini-2.5-flash\"}");

    /* Process part with null text value - covers line 243 branch 1 */
    const char *chunk =
        "{\"candidates\":[{\"content\":{\"parts\":[{\"text\":null}]}}]}";
    process_chunk(sctx, chunk);

    /* Verify only START event, no TEXT_DELTA */
    ck_assert_int_eq((int)captured_count, 1);
    ck_assert_int_eq(captured_events[0].type, IK_STREAM_START);
}
END_TEST

START_TEST(test_part_with_empty_text) {
    ik_google_stream_ctx_t *sctx = NULL;
    res_t r = ik_google_stream_ctx_create(test_ctx, test_stream_cb, NULL, &sctx);
    ck_assert(!is_err(&r));

    /* Process START first */
    process_chunk(sctx, "{\"modelVersion\":\"gemini-2.5-flash\"}");

    /* Process part with empty text string - covers line 243 branch 2 */
    const char *chunk =
        "{\"candidates\":[{\"content\":{\"parts\":[{\"text\":\"\"}]}}]}";
    process_chunk(sctx, chunk);

    /* Verify only START event, no TEXT_DELTA */
    ck_assert_int_eq((int)captured_count, 1);
    ck_assert_int_eq(captured_events[0].type, IK_STREAM_START);
}
END_TEST

START_TEST(test_part_with_non_string_text) {
    ik_google_stream_ctx_t *sctx = NULL;
    res_t r = ik_google_stream_ctx_create(test_ctx, test_stream_cb, NULL, &sctx);
    ck_assert(!is_err(&r));

    /* Process START first */
    process_chunk(sctx, "{\"modelVersion\":\"gemini-2.5-flash\"}");

    /* Process part with non-string text value - covers line 243 branch 1 */
    const char *chunk =
        "{\"candidates\":[{\"content\":{\"parts\":[{\"text\":123}]}}]}";
    process_chunk(sctx, chunk);

    /* Verify only START event, no TEXT_DELTA */
    ck_assert_int_eq((int)captured_count, 1);
    ck_assert_int_eq(captured_events[0].type, IK_STREAM_START);
}
END_TEST

START_TEST(test_thought_field_non_boolean) {
    ik_google_stream_ctx_t *sctx = NULL;
    res_t r = ik_google_stream_ctx_create(test_ctx, test_stream_cb, NULL, &sctx);
    ck_assert(!is_err(&r));

    /* Process START first */
    process_chunk(sctx, "{\"modelVersion\":\"gemini-2.5-flash\"}");

    /* Process part with non-boolean thought field (string) - covers line 233 branches */
    const char *chunk =
        "{\"candidates\":[{\"content\":{\"parts\":[{\"text\":\"Hello\",\"thought\":\"not-a-bool\"}]}}]}";
    process_chunk(sctx, chunk);

    /* Verify TEXT_DELTA (not THINKING_DELTA) since yyjson_get_bool returns false for strings */
    ck_assert_int_eq((int)count_events(IK_STREAM_TEXT_DELTA), 1);
    ck_assert_int_eq((int)count_events(IK_STREAM_THINKING_DELTA), 0);
}
END_TEST

START_TEST(test_thought_field_false) {
    ik_google_stream_ctx_t *sctx = NULL;
    res_t r = ik_google_stream_ctx_create(test_ctx, test_stream_cb, NULL, &sctx);
    ck_assert(!is_err(&r));

    /* Process START first */
    process_chunk(sctx, "{\"modelVersion\":\"gemini-2.5-flash\"}");

    /* Process part with thought field explicitly false - covers line 233 branches */
    const char *chunk =
        "{\"candidates\":[{\"content\":{\"parts\":[{\"text\":\"Hello\",\"thought\":false}]}}]}";
    process_chunk(sctx, chunk);

    /* Verify TEXT_DELTA (not THINKING_DELTA) */
    ck_assert_int_eq((int)count_events(IK_STREAM_TEXT_DELTA), 1);
    ck_assert_int_eq((int)count_events(IK_STREAM_THINKING_DELTA), 0);
}
END_TEST

START_TEST(test_parts_empty_array) {
    ik_google_stream_ctx_t *sctx = NULL;
    res_t r = ik_google_stream_ctx_create(test_ctx, test_stream_cb, NULL, &sctx);
    ck_assert(!is_err(&r));

    /* Process START first */
    process_chunk(sctx, "{\"modelVersion\":\"gemini-2.5-flash\"}");

    /* Process empty parts array - covers line 223 loop edge cases */
    const char *chunk =
        "{\"candidates\":[{\"content\":{\"parts\":[]}}]}";
    process_chunk(sctx, chunk);

    /* Verify only START event, no content events */
    ck_assert_int_eq((int)captured_count, 1);
    ck_assert_int_eq(captured_events[0].type, IK_STREAM_START);
}
END_TEST

START_TEST(test_thought_field_null) {
    ik_google_stream_ctx_t *sctx = NULL;
    res_t r = ik_google_stream_ctx_create(test_ctx, test_stream_cb, NULL, &sctx);
    ck_assert(!is_err(&r));

    /* Process START first */
    process_chunk(sctx, "{\"modelVersion\":\"gemini-2.5-flash\"}");

    /* Process part with thought field as null - covers line 233 branch 5 */
    const char *chunk =
        "{\"candidates\":[{\"content\":{\"parts\":[{\"text\":\"Hello\",\"thought\":null}]}}]}";
    process_chunk(sctx, chunk);

    /* Verify TEXT_DELTA (not THINKING_DELTA) since null is falsy */
    ck_assert_int_eq((int)count_events(IK_STREAM_TEXT_DELTA), 1);
    ck_assert_int_eq((int)count_events(IK_STREAM_THINKING_DELTA), 0);
}
END_TEST

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

static Suite *google_streaming_branch_coverage_suite(void)
{
    Suite *s = suite_create("Google Streaming - Branch Coverage");

    TCase *tc_function_call = tcase_create("Function Call Branches");
    tcase_set_timeout(tc_function_call, 30);
    tcase_add_checked_fixture(tc_function_call, setup, teardown);
    tcase_add_test(tc_function_call, test_function_call_without_name);
    tcase_add_test(tc_function_call, test_function_call_with_null_name);
    tcase_add_test(tc_function_call, test_function_call_with_non_string_name);
    tcase_add_test(tc_function_call, test_function_call_without_args);
    tcase_add_test(tc_function_call, test_function_call_with_null_args);
    tcase_add_test(tc_function_call, test_function_call_continued_with_more_args);
    suite_add_tcase(s, tc_function_call);

    TCase *tc_thinking = tcase_create("Thinking Transition");
    tcase_set_timeout(tc_thinking, 30);
    tcase_add_checked_fixture(tc_thinking, setup, teardown);
    tcase_add_test(tc_thinking, test_text_after_thinking_increments_index);
    suite_add_tcase(s, tc_thinking);

    TCase *tc_candidates = tcase_create("Candidates Processing Branches");
    tcase_set_timeout(tc_candidates, 30);
    tcase_add_checked_fixture(tc_candidates, setup, teardown);
    tcase_add_test(tc_candidates, test_candidates_empty_array);
    tcase_add_test(tc_candidates, test_candidates_not_array);
    suite_add_tcase(s, tc_candidates);

    TCase *tc_parts = tcase_create("Parts Processing Edge Cases");
    tcase_set_timeout(tc_parts, 30);
    tcase_add_checked_fixture(tc_parts, setup, teardown);
    tcase_add_test(tc_parts, test_part_without_text_field);
    tcase_add_test(tc_parts, test_part_with_null_text);
    tcase_add_test(tc_parts, test_part_with_empty_text);
    tcase_add_test(tc_parts, test_part_with_non_string_text);
    tcase_add_test(tc_parts, test_thought_field_non_boolean);
    tcase_add_test(tc_parts, test_thought_field_false);
    tcase_add_test(tc_parts, test_parts_empty_array);
    tcase_add_test(tc_parts, test_thought_field_null);
    suite_add_tcase(s, tc_parts);

    TCase *tc_chunk = tcase_create("Chunk Structure Edge Cases");
    tcase_set_timeout(tc_chunk, 30);
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

    TCase *tc_errors = tcase_create("Process Error Edge Cases");
    tcase_set_timeout(tc_errors, 30);
    tcase_add_checked_fixture(tc_errors, setup, teardown);
    tcase_add_test(tc_errors, test_error_without_message);
    tcase_add_test(tc_errors, test_error_with_null_message);
    tcase_add_test(tc_errors, test_error_with_non_string_message);
    tcase_add_test(tc_errors, test_error_without_status);
    tcase_add_test(tc_errors, test_error_with_null_status);
    tcase_add_test(tc_errors, test_error_with_unknown_status);
    suite_add_tcase(s, tc_errors);

    TCase *tc_data = tcase_create("Process Data Edge Cases");
    tcase_set_timeout(tc_data, 30);
    tcase_add_checked_fixture(tc_data, setup, teardown);
    tcase_add_test(tc_data, test_process_null_data);
    tcase_add_test(tc_data, test_process_empty_data);
    tcase_add_test(tc_data, test_process_malformed_json);
    tcase_add_test(tc_data, test_process_non_object_root);
    tcase_add_test(tc_data, test_process_error_only_chunk);
    suite_add_tcase(s, tc_data);

    TCase *tc_usage = tcase_create("Usage Metadata Edge Cases");
    tcase_set_timeout(tc_usage, 30);
    tcase_add_checked_fixture(tc_usage, setup, teardown);
    tcase_add_test(tc_usage, test_usage_with_missing_fields);
    tcase_add_test(tc_usage, test_usage_with_thoughts);
    suite_add_tcase(s, tc_usage);

    return s;
}

int main(void)
{
    Suite *s = google_streaming_branch_coverage_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
