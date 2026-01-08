/**
 * @file google_streaming_args_accumulation_test.c
 * @brief Unit tests for Google provider tool argument accumulation
 *
 * Tests verify that tool call arguments are accumulated across
 * multiple streaming chunks for use by the response builder.
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

/* Accumulated arguments from all DELTA events */
static char accumulated_args[4096];

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
                    // Also accumulate into our test buffer
                    size_t cur_len = strlen(accumulated_args);
                    size_t add_len = strlen(event->data.tool_delta.arguments);
                    if (cur_len + add_len < sizeof(accumulated_args) - 1) {
                        strcat(accumulated_args, event->data.tool_delta.arguments);
                    }
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
    memset(accumulated_args, 0, sizeof(accumulated_args));
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
 * Argument Accumulation Tests
 * ================================================================ */

START_TEST(test_single_chunk_accumulates_args) {
    ik_google_stream_ctx_t *sctx = NULL;
    res_t r = ik_google_stream_ctx_create(test_ctx, test_stream_cb, NULL, &sctx);
    ck_assert(!is_err(&r));

    /* Process chunk with functionCall containing arguments */
    const char *chunk =
        "{\"candidates\":[{\"content\":{\"parts\":[{\"functionCall\":{\"name\":\"get_file\",\"args\":{\"path\":\"/tmp/test.txt\"}}}]}}],\"modelVersion\":\"gemini-2.5-flash\"}";
    process_chunk(sctx, chunk);

    /* Verify DELTA event was emitted */
    const ik_stream_event_t *delta_event = find_event(IK_STREAM_TOOL_CALL_DELTA);
    ck_assert_ptr_nonnull(delta_event);

    /* Verify arguments contain the path */
    ck_assert_ptr_nonnull(strstr(accumulated_args, "path"));
    ck_assert_ptr_nonnull(strstr(accumulated_args, "/tmp/test.txt"));
}

END_TEST

START_TEST(test_multiple_chunks_accumulate_args) {
    ik_google_stream_ctx_t *sctx = NULL;
    res_t r = ik_google_stream_ctx_create(test_ctx, test_stream_cb, NULL, &sctx);
    ck_assert(!is_err(&r));

    /* First chunk - start tool call with partial args */
    const char *chunk1 =
        "{\"candidates\":[{\"content\":{\"parts\":[{\"functionCall\":{\"name\":\"complex_tool\",\"args\":{\"field1\":\"value1\"}}}]}}],\"modelVersion\":\"gemini-2.5-flash\"}";
    process_chunk(sctx, chunk1);

    /* Second chunk - more args in same tool call */
    const char *chunk2 =
        "{\"candidates\":[{\"content\":{\"parts\":[{\"functionCall\":{\"name\":\"complex_tool\",\"args\":{\"field2\":\"value2\"}}}]}}]}";
    process_chunk(sctx, chunk2);

    /* Verify multiple DELTA events were emitted */
    size_t delta_count = count_events(IK_STREAM_TOOL_CALL_DELTA);
    ck_assert_int_eq((int)delta_count, 2);

    /* Verify both sets of arguments were accumulated */
    ck_assert_ptr_nonnull(strstr(accumulated_args, "field1"));
    ck_assert_ptr_nonnull(strstr(accumulated_args, "value1"));
    ck_assert_ptr_nonnull(strstr(accumulated_args, "field2"));
    ck_assert_ptr_nonnull(strstr(accumulated_args, "value2"));
}

END_TEST

START_TEST(test_accumulation_cleared_on_tool_call_end) {
    ik_google_stream_ctx_t *sctx = NULL;
    res_t r = ik_google_stream_ctx_create(test_ctx, test_stream_cb, NULL, &sctx);
    ck_assert(!is_err(&r));

    /* Start tool call */
    const char *chunk1 =
        "{\"candidates\":[{\"content\":{\"parts\":[{\"functionCall\":{\"name\":\"test_tool\",\"args\":{\"x\":1}}}]}}],\"modelVersion\":\"gemini-2.5-flash\"}";
    process_chunk(sctx, chunk1);

    /* End with text (triggers TOOL_CALL_DONE) */
    const char *chunk2 =
        "{\"candidates\":[{\"content\":{\"parts\":[{\"text\":\"Done with tool\"}]}}]}";
    process_chunk(sctx, chunk2);

    /* Verify TOOL_CALL_DONE was emitted */
    const ik_stream_event_t *done_event = find_event(IK_STREAM_TOOL_CALL_DONE);
    ck_assert_ptr_nonnull(done_event);
}

END_TEST

START_TEST(test_accumulation_cleared_on_usage_metadata) {
    ik_google_stream_ctx_t *sctx = NULL;
    res_t r = ik_google_stream_ctx_create(test_ctx, test_stream_cb, NULL, &sctx);
    ck_assert(!is_err(&r));

    /* Start tool call with args */
    const char *chunk1 =
        "{\"candidates\":[{\"content\":{\"parts\":[{\"functionCall\":{\"name\":\"final_tool\",\"args\":{\"done\":true}}}]}}],\"modelVersion\":\"gemini-2.5-flash\"}";
    process_chunk(sctx, chunk1);

    /* Usage metadata ends the stream and tool call */
    const char *chunk2 =
        "{\"usageMetadata\":{\"promptTokenCount\":10,\"candidatesTokenCount\":5,\"totalTokenCount\":15}}";
    process_chunk(sctx, chunk2);

    /* Verify TOOL_CALL_DONE and STREAM_DONE were emitted */
    const ik_stream_event_t *tool_done = find_event(IK_STREAM_TOOL_CALL_DONE);
    ck_assert_ptr_nonnull(tool_done);

    const ik_stream_event_t *stream_done = find_event(IK_STREAM_DONE);
    ck_assert_ptr_nonnull(stream_done);
}

END_TEST

START_TEST(test_empty_args_object_handled) {
    ik_google_stream_ctx_t *sctx = NULL;
    res_t r = ik_google_stream_ctx_create(test_ctx, test_stream_cb, NULL, &sctx);
    ck_assert(!is_err(&r));

    /* Tool call with empty args object */
    const char *chunk =
        "{\"candidates\":[{\"content\":{\"parts\":[{\"functionCall\":{\"name\":\"no_args_tool\",\"args\":{}}}]}}],\"modelVersion\":\"gemini-2.5-flash\"}";
    process_chunk(sctx, chunk);

    /* Verify START event was emitted */
    const ik_stream_event_t *start_event = find_event(IK_STREAM_TOOL_CALL_START);
    ck_assert_ptr_nonnull(start_event);
    ck_assert_str_eq(start_event->data.tool_start.name, "no_args_tool");

    /* Verify DELTA event was emitted with empty object */
    const ik_stream_event_t *delta_event = find_event(IK_STREAM_TOOL_CALL_DELTA);
    ck_assert_ptr_nonnull(delta_event);
    ck_assert_str_eq(accumulated_args, "{}");
}

END_TEST

START_TEST(test_complex_nested_args_accumulated) {
    ik_google_stream_ctx_t *sctx = NULL;
    res_t r = ik_google_stream_ctx_create(test_ctx, test_stream_cb, NULL, &sctx);
    ck_assert(!is_err(&r));

    /* Tool call with complex nested args */
    const char *chunk =
        "{\"candidates\":[{\"content\":{\"parts\":[{\"functionCall\":{\"name\":\"nested_tool\",\"args\":{\"outer\":{\"inner\":\"value\"},\"array\":[1,2,3]}}}]}}],\"modelVersion\":\"gemini-2.5-flash\"}";
    process_chunk(sctx, chunk);

    /* Verify args contain nested structure */
    ck_assert_ptr_nonnull(strstr(accumulated_args, "outer"));
    ck_assert_ptr_nonnull(strstr(accumulated_args, "inner"));
    ck_assert_ptr_nonnull(strstr(accumulated_args, "value"));
    ck_assert_ptr_nonnull(strstr(accumulated_args, "array"));
}

END_TEST

/* ================================================================
 * Test Suite Setup
 * ================================================================ */

static Suite *google_streaming_args_accumulation_suite(void)
{
    Suite *s = suite_create("Google Streaming - Argument Accumulation");

    TCase *tc_accum = tcase_create("Argument Accumulation");
    tcase_set_timeout(tc_accum, IK_TEST_TIMEOUT);
    tcase_add_checked_fixture(tc_accum, setup, teardown);
    tcase_add_test(tc_accum, test_single_chunk_accumulates_args);
    tcase_add_test(tc_accum, test_multiple_chunks_accumulate_args);
    tcase_add_test(tc_accum, test_accumulation_cleared_on_tool_call_end);
    tcase_add_test(tc_accum, test_accumulation_cleared_on_usage_metadata);
    tcase_add_test(tc_accum, test_empty_args_object_handled);
    tcase_add_test(tc_accum, test_complex_nested_args_accumulated);
    suite_add_tcase(s, tc_accum);

    return s;
}

int main(void)
{
    Suite *s = google_streaming_args_accumulation_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
