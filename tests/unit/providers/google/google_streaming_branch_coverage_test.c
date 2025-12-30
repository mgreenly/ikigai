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
 * Usage Metadata Branch Coverage Tests
 * ================================================================ */

START_TEST(test_usage_with_missing_token_fields) {
    ik_google_stream_ctx_t *sctx = NULL;
    res_t r = ik_google_stream_ctx_create(test_ctx, test_stream_cb, NULL, &sctx);
    ck_assert(!is_err(&r));

    /* Process START first */
    process_chunk(sctx, "{\"modelVersion\":\"gemini-2.5-flash\"}");

    /* Process text content */
    process_chunk(sctx, "{\"candidates\":[{\"content\":{\"parts\":[{\"text\":\"Hello\"}]}}]}");

    /* Reset to check DONE event */
    captured_count = 0;
    memset(captured_events, 0, sizeof(captured_events));

    /* Process usage with minimal fields - covers lines 264-276 NULL branches */
    const char *usage_chunk = "{\"usageMetadata\":{}}";
    process_chunk(sctx, usage_chunk);

    /* Verify DONE event with zero usage */
    const ik_stream_event_t *done_event = find_event(IK_STREAM_DONE);
    ck_assert_ptr_nonnull(done_event);
    ck_assert_int_eq(done_event->data.done.usage.input_tokens, 0);
    ck_assert_int_eq(done_event->data.done.usage.output_tokens, 0);
    ck_assert_int_eq(done_event->data.done.usage.thinking_tokens, 0);
    ck_assert_int_eq(done_event->data.done.usage.total_tokens, 0);
}

END_TEST

START_TEST(test_usage_with_partial_token_fields) {
    ik_google_stream_ctx_t *sctx = NULL;
    res_t r = ik_google_stream_ctx_create(test_ctx, test_stream_cb, NULL, &sctx);
    ck_assert(!is_err(&r));

    /* Process START first */
    process_chunk(sctx, "{\"modelVersion\":\"gemini-2.5-flash\"}");

    /* Process text content */
    process_chunk(sctx, "{\"candidates\":[{\"content\":{\"parts\":[{\"text\":\"Hello\"}]}}]}");

    /* Reset to check DONE event */
    captured_count = 0;
    memset(captured_events, 0, sizeof(captured_events));

    /* Process usage with only promptTokenCount - covers other fields NULL */
    const char *usage_chunk = "{\"usageMetadata\":{\"promptTokenCount\":100}}";
    process_chunk(sctx, usage_chunk);

    /* Verify DONE event with partial usage */
    const ik_stream_event_t *done_event = find_event(IK_STREAM_DONE);
    ck_assert_ptr_nonnull(done_event);
    ck_assert_int_eq(done_event->data.done.usage.input_tokens, 100);
    ck_assert_int_eq(done_event->data.done.usage.output_tokens, 0);
}

END_TEST

/* ================================================================
 * Model Version Branch Coverage Tests
 * ================================================================ */

START_TEST(test_chunk_without_model_version) {
    ik_google_stream_ctx_t *sctx = NULL;
    res_t r = ik_google_stream_ctx_create(test_ctx, test_stream_cb, NULL, &sctx);
    ck_assert(!is_err(&r));

    /* Process chunk without modelVersion field - covers line 385 false branch */
    const char *chunk = "{\"candidates\":[{\"content\":{\"parts\":[{\"text\":\"Hello\"}]}}]}";
    process_chunk(sctx, chunk);

    /* Verify START event with NULL model */
    const ik_stream_event_t *start_event = find_event(IK_STREAM_START);
    ck_assert_ptr_nonnull(start_event);
    ck_assert_ptr_null(start_event->data.start.model);
}

END_TEST

START_TEST(test_chunk_with_null_model_version) {
    ik_google_stream_ctx_t *sctx = NULL;
    res_t r = ik_google_stream_ctx_create(test_ctx, test_stream_cb, NULL, &sctx);
    ck_assert(!is_err(&r));

    /* Process chunk with null modelVersion value */
    const char *chunk = "{\"modelVersion\":null,\"candidates\":[{\"content\":{\"parts\":[{\"text\":\"Hello\"}]}}]}";
    process_chunk(sctx, chunk);

    /* Verify START event with NULL model */
    const ik_stream_event_t *start_event = find_event(IK_STREAM_START);
    ck_assert_ptr_nonnull(start_event);
    ck_assert_ptr_null(start_event->data.start.model);
}

END_TEST

/* ================================================================
 * Content Parts Branch Coverage Tests
 * ================================================================ */

START_TEST(test_candidate_without_content) {
    ik_google_stream_ctx_t *sctx = NULL;
    res_t r = ik_google_stream_ctx_create(test_ctx, test_stream_cb, NULL, &sctx);
    ck_assert(!is_err(&r));

    /* Process START first */
    process_chunk(sctx, "{\"modelVersion\":\"gemini-2.5-flash\"}");

    /* Reset to check for no additional events */
    captured_count = 0;
    memset(captured_events, 0, sizeof(captured_events));

    /* Process candidate without content field - covers line 416 false branch */
    const char *chunk = "{\"candidates\":[{\"finishReason\":\"STOP\"}]}";
    process_chunk(sctx, chunk);

    /* Verify no text events emitted */
    ck_assert_int_eq((int)count_events(IK_STREAM_TEXT_DELTA), 0);
}

END_TEST

START_TEST(test_candidate_without_parts) {
    ik_google_stream_ctx_t *sctx = NULL;
    res_t r = ik_google_stream_ctx_create(test_ctx, test_stream_cb, NULL, &sctx);
    ck_assert(!is_err(&r));

    /* Process START first */
    process_chunk(sctx, "{\"modelVersion\":\"gemini-2.5-flash\"}");

    /* Reset to check for no additional events */
    captured_count = 0;
    memset(captured_events, 0, sizeof(captured_events));

    /* Process candidate with content but no parts - covers line 418 false branch */
    const char *chunk = "{\"candidates\":[{\"content\":{}}]}";
    process_chunk(sctx, chunk);

    /* Verify no text events emitted */
    ck_assert_int_eq((int)count_events(IK_STREAM_TEXT_DELTA), 0);
}

END_TEST

START_TEST(test_candidate_with_parts_not_array) {
    ik_google_stream_ctx_t *sctx = NULL;
    res_t r = ik_google_stream_ctx_create(test_ctx, test_stream_cb, NULL, &sctx);
    ck_assert(!is_err(&r));

    /* Process START first */
    process_chunk(sctx, "{\"modelVersion\":\"gemini-2.5-flash\"}");

    /* Reset to check for no additional events */
    captured_count = 0;
    memset(captured_events, 0, sizeof(captured_events));

    /* Process candidate with parts as non-array - covers line 418 false branch */
    const char *chunk = "{\"candidates\":[{\"content\":{\"parts\":null}}]}";
    process_chunk(sctx, chunk);

    /* Verify no text events emitted */
    ck_assert_int_eq((int)count_events(IK_STREAM_TEXT_DELTA), 0);
}

END_TEST

/* ================================================================
 * JSON Parsing Branch Coverage Tests
 * ================================================================ */

START_TEST(test_chunk_not_json_object) {
    ik_google_stream_ctx_t *sctx = NULL;
    res_t r = ik_google_stream_ctx_create(test_ctx, test_stream_cb, NULL, &sctx);
    ck_assert(!is_err(&r));

    /* Process chunk that's an array, not object - covers line 367 false branch */
    const char *chunk = "[1, 2, 3]";
    process_chunk(sctx, chunk);

    /* Verify no events emitted */
    ck_assert_int_eq((int)captured_count, 0);
}

END_TEST

START_TEST(test_chunk_with_empty_string) {
    ik_google_stream_ctx_t *sctx = NULL;
    res_t r = ik_google_stream_ctx_create(test_ctx, test_stream_cb, NULL, &sctx);
    ck_assert(!is_err(&r));

    /* Process empty string - covers line 352 early return */
    process_chunk(sctx, "");

    /* Verify no events emitted */
    ck_assert_int_eq((int)captured_count, 0);
}

END_TEST

START_TEST(test_chunk_with_null_data) {
    ik_google_stream_ctx_t *sctx = NULL;
    res_t r = ik_google_stream_ctx_create(test_ctx, test_stream_cb, NULL, &sctx);
    ck_assert(!is_err(&r));

    /* Process NULL data - covers line 352 early return */
    process_chunk(sctx, NULL);

    /* Verify no events emitted */
    ck_assert_int_eq((int)captured_count, 0);
}

END_TEST

/* ================================================================
 * Part Processing Branch Coverage Tests
 * ================================================================ */

START_TEST(test_part_without_text_or_function_call) {
    ik_google_stream_ctx_t *sctx = NULL;
    res_t r = ik_google_stream_ctx_create(test_ctx, test_stream_cb, NULL, &sctx);
    ck_assert(!is_err(&r));

    /* Process START first */
    process_chunk(sctx, "{\"modelVersion\":\"gemini-2.5-flash\"}");

    /* Reset to check for no additional events */
    captured_count = 0;
    memset(captured_events, 0, sizeof(captured_events));

    /* Process part without text or functionCall - covers line 237 continue */
    const char *chunk = "{\"candidates\":[{\"content\":{\"parts\":[{\"otherField\":\"value\"}]}}]}";
    process_chunk(sctx, chunk);

    /* Verify no text/tool events emitted */
    ck_assert_int_eq((int)count_events(IK_STREAM_TEXT_DELTA), 0);
    ck_assert_int_eq((int)count_events(IK_STREAM_TOOL_CALL_START), 0);
}

END_TEST

START_TEST(test_part_with_empty_text) {
    ik_google_stream_ctx_t *sctx = NULL;
    res_t r = ik_google_stream_ctx_create(test_ctx, test_stream_cb, NULL, &sctx);
    ck_assert(!is_err(&r));

    /* Process START first */
    process_chunk(sctx, "{\"modelVersion\":\"gemini-2.5-flash\"}");

    /* Reset to check for no additional events */
    captured_count = 0;
    memset(captured_events, 0, sizeof(captured_events));

    /* Process part with empty text - covers line 243 continue */
    const char *chunk = "{\"candidates\":[{\"content\":{\"parts\":[{\"text\":\"\"}]}}]}";
    process_chunk(sctx, chunk);

    /* Verify no text events emitted */
    ck_assert_int_eq((int)count_events(IK_STREAM_TEXT_DELTA), 0);
}

END_TEST

START_TEST(test_part_with_null_text) {
    ik_google_stream_ctx_t *sctx = NULL;
    res_t r = ik_google_stream_ctx_create(test_ctx, test_stream_cb, NULL, &sctx);
    ck_assert(!is_err(&r));

    /* Process START first */
    process_chunk(sctx, "{\"modelVersion\":\"gemini-2.5-flash\"}");

    /* Reset to check for no additional events */
    captured_count = 0;
    memset(captured_events, 0, sizeof(captured_events));

    /* Process part with null text value - covers line 242 continue */
    const char *chunk = "{\"candidates\":[{\"content\":{\"parts\":[{\"text\":null}]}}]}";
    process_chunk(sctx, chunk);

    /* Verify no text events emitted */
    ck_assert_int_eq((int)count_events(IK_STREAM_TEXT_DELTA), 0);
}

END_TEST

START_TEST(test_part_with_thought_false) {
    ik_google_stream_ctx_t *sctx = NULL;
    res_t r = ik_google_stream_ctx_create(test_ctx, test_stream_cb, NULL, &sctx);
    ck_assert(!is_err(&r));

    /* Process START first */
    process_chunk(sctx, "{\"modelVersion\":\"gemini-2.5-flash\"}");

    /* Reset to check text event */
    captured_count = 0;
    memset(captured_events, 0, sizeof(captured_events));

    /* Process part with explicit thought:false - covers line 233 branch */
    const char *chunk = "{\"candidates\":[{\"content\":{\"parts\":[{\"text\":\"Hello\",\"thought\":false}]}}]}";
    process_chunk(sctx, chunk);

    /* Verify TEXT_DELTA emitted, not THINKING_DELTA */
    ck_assert_int_eq((int)count_events(IK_STREAM_TEXT_DELTA), 1);
    ck_assert_int_eq((int)count_events(IK_STREAM_THINKING_DELTA), 0);
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

    TCase *tc_usage = tcase_create("Usage Metadata Branches");
    tcase_set_timeout(tc_usage, 30);
    tcase_add_checked_fixture(tc_usage, setup, teardown);
    tcase_add_test(tc_usage, test_usage_with_missing_token_fields);
    tcase_add_test(tc_usage, test_usage_with_partial_token_fields);
    suite_add_tcase(s, tc_usage);

    TCase *tc_model = tcase_create("Model Version Branches");
    tcase_set_timeout(tc_model, 30);
    tcase_add_checked_fixture(tc_model, setup, teardown);
    tcase_add_test(tc_model, test_chunk_without_model_version);
    tcase_add_test(tc_model, test_chunk_with_null_model_version);
    suite_add_tcase(s, tc_model);

    TCase *tc_content = tcase_create("Content Parts Branches");
    tcase_set_timeout(tc_content, 30);
    tcase_add_checked_fixture(tc_content, setup, teardown);
    tcase_add_test(tc_content, test_candidate_without_content);
    tcase_add_test(tc_content, test_candidate_without_parts);
    tcase_add_test(tc_content, test_candidate_with_parts_not_array);
    suite_add_tcase(s, tc_content);

    TCase *tc_json = tcase_create("JSON Parsing Branches");
    tcase_set_timeout(tc_json, 30);
    tcase_add_checked_fixture(tc_json, setup, teardown);
    tcase_add_test(tc_json, test_chunk_not_json_object);
    tcase_add_test(tc_json, test_chunk_with_empty_string);
    tcase_add_test(tc_json, test_chunk_with_null_data);
    suite_add_tcase(s, tc_json);

    TCase *tc_parts = tcase_create("Part Processing Branches");
    tcase_set_timeout(tc_parts, 30);
    tcase_add_checked_fixture(tc_parts, setup, teardown);
    tcase_add_test(tc_parts, test_part_without_text_or_function_call);
    tcase_add_test(tc_parts, test_part_with_empty_text);
    tcase_add_test(tc_parts, test_part_with_null_text);
    tcase_add_test(tc_parts, test_part_with_thought_false);
    suite_add_tcase(s, tc_parts);

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
