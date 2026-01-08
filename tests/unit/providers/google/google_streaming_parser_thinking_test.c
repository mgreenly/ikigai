/**
 * @file google_streaming_parser_thinking_test.c
 * @brief Unit tests for Google provider thinking detection and event normalization
 *
 * Tests verify thought detection and event normalization.
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
static char captured_strings1[MAX_EVENTS][MAX_STRING_LEN]; /* For copying first string field */
static char captured_strings2[MAX_EVENTS][MAX_STRING_LEN]; /* For copying second string field (tool name) */
static size_t captured_count;

/* ================================================================
 * Test Callbacks
 * ================================================================ */

/**
 * Stream callback - captures events for verification
 */
static res_t test_stream_cb(const ik_stream_event_t *event, void *ctx)
{
    (void)ctx;

    if (captured_count < MAX_EVENTS) {
        /* Copy event structure */
        captured_events[captured_count] = *event;

        /* Copy string data to persistent storage */
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
                /* Copy ID and name to separate buffers */
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
            case IK_STREAM_ERROR:
                if (event->data.error.message) {
                    strncpy(captured_strings1[captured_count], event->data.error.message, MAX_STRING_LEN - 1);
                    captured_strings1[captured_count][MAX_STRING_LEN - 1] = '\0';
                    captured_events[captured_count].data.error.message = captured_strings1[captured_count];
                }
                break;
            default:
                /* Other event types don't have string data we need to copy */
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

/**
 * Process single chunk through streaming context
 */
static void process_chunk(ik_google_stream_ctx_t *sctx, const char *chunk)
{
    ik_google_stream_process_data(sctx, chunk);
}

/**
 * Find event by type in captured events
 */
static const ik_stream_event_t *find_event(ik_stream_event_type_t type)
{
    for (size_t i = 0; i < captured_count; i++) {
        if (captured_events[i].type == type) {
            return &captured_events[i];
        }
    }
    return NULL;
}

/**
 * Count events of given type
 */
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
 * Thought Part Detection Tests
 * ================================================================ */

START_TEST(test_parse_part_with_thought_true_flag) {
    ik_google_stream_ctx_t *sctx = NULL;
    res_t r = ik_google_stream_ctx_create(test_ctx, test_stream_cb, NULL, &sctx);
    ck_assert(!is_err(&r));

    /* Process chunk with thought=true */
    const char *chunk =
        "{\"candidates\":[{\"content\":{\"parts\":[{\"text\":\"Let me think...\",\"thought\":true}]}}],\"modelVersion\":\"gemini-2.5-flash\"}";
    process_chunk(sctx, chunk);

    /* Verify THINKING_DELTA event emitted */
    const ik_stream_event_t *thinking_event = find_event(IK_STREAM_THINKING_DELTA);
    ck_assert_ptr_nonnull(thinking_event);
    ck_assert_str_eq(thinking_event->data.delta.text, "Let me think...");
}

END_TEST

START_TEST(test_parse_part_without_thought_flag) {
    ik_google_stream_ctx_t *sctx = NULL;
    res_t r = ik_google_stream_ctx_create(test_ctx, test_stream_cb, NULL, &sctx);
    ck_assert(!is_err(&r));

    /* Process chunk without thought flag (defaults to false) */
    const char *chunk =
        "{\"candidates\":[{\"content\":{\"parts\":[{\"text\":\"Regular text\"}]}}],\"modelVersion\":\"gemini-2.5-flash\"}";
    process_chunk(sctx, chunk);

    /* Verify TEXT_DELTA event emitted (not THINKING_DELTA) */
    const ik_stream_event_t *text_event = find_event(IK_STREAM_TEXT_DELTA);
    ck_assert_ptr_nonnull(text_event);

    /* Verify no THINKING_DELTA event */
    const ik_stream_event_t *thinking_event = find_event(IK_STREAM_THINKING_DELTA);
    ck_assert_ptr_null(thinking_event);
}

END_TEST

START_TEST(test_distinguish_thought_content_from_regular_content) {
    ik_google_stream_ctx_t *sctx = NULL;
    res_t r = ik_google_stream_ctx_create(test_ctx, test_stream_cb, NULL, &sctx);
    ck_assert(!is_err(&r));

    /* Process chunk with both thought and regular text */
    process_chunk(sctx,
                  "{\"candidates\":[{\"content\":{\"parts\":[{\"text\":\"Thinking...\",\"thought\":true}]}}],\"modelVersion\":\"gemini-2.5-flash\"}");
    process_chunk(sctx, "{\"candidates\":[{\"content\":{\"parts\":[{\"text\":\"Answer\"}]}}]}");

    /* Verify THINKING_DELTA and TEXT_DELTA events */
    size_t thinking_count = count_events(IK_STREAM_THINKING_DELTA);
    size_t text_count = count_events(IK_STREAM_TEXT_DELTA);

    ck_assert_int_eq((int)thinking_count, 1);
    ck_assert_int_eq((int)text_count, 1);
}

END_TEST

START_TEST(test_interleaved_thinking_and_content_parts) {
    ik_google_stream_ctx_t *sctx = NULL;
    res_t r = ik_google_stream_ctx_create(test_ctx, test_stream_cb, NULL, &sctx);
    ck_assert(!is_err(&r));

    /* Process chunks with interleaved thinking and content */
    process_chunk(sctx,
                  "{\"candidates\":[{\"content\":{\"parts\":[{\"text\":\"Thought 1\",\"thought\":true}]}}],\"modelVersion\":\"gemini-2.5-flash\"}");
    process_chunk(sctx, "{\"candidates\":[{\"content\":{\"parts\":[{\"text\":\"Content 1\"}]}}]}");
    process_chunk(sctx, "{\"candidates\":[{\"content\":{\"parts\":[{\"text\":\"Thought 2\",\"thought\":true}]}}]}");
    process_chunk(sctx, "{\"candidates\":[{\"content\":{\"parts\":[{\"text\":\"Content 2\"}]}}]}");

    /* Verify event sequence */
    size_t thinking_count = count_events(IK_STREAM_THINKING_DELTA);
    size_t text_count = count_events(IK_STREAM_TEXT_DELTA);

    ck_assert_int_eq((int)thinking_count, 2);
    ck_assert_int_eq((int)text_count, 2);
}

END_TEST
/* ================================================================
 * Event Normalization Tests
 * ================================================================ */

START_TEST(test_normalize_text_part_to_text_delta) {
    ik_google_stream_ctx_t *sctx = NULL;
    res_t r = ik_google_stream_ctx_create(test_ctx, test_stream_cb, NULL, &sctx);
    ck_assert(!is_err(&r));

    /* Process text part */
    const char *chunk =
        "{\"candidates\":[{\"content\":{\"parts\":[{\"text\":\"Hello\"}]}}],\"modelVersion\":\"gemini-2.5-flash\"}";
    process_chunk(sctx, chunk);

    /* Verify normalized to IK_STREAM_TEXT_DELTA */
    const ik_stream_event_t *event = find_event(IK_STREAM_TEXT_DELTA);
    ck_assert_ptr_nonnull(event);
    ck_assert_int_eq(event->type, IK_STREAM_TEXT_DELTA);
}

END_TEST

START_TEST(test_normalize_thought_part_to_thinking_delta) {
    ik_google_stream_ctx_t *sctx = NULL;
    res_t r = ik_google_stream_ctx_create(test_ctx, test_stream_cb, NULL, &sctx);
    ck_assert(!is_err(&r));

    /* Process thought part */
    const char *chunk =
        "{\"candidates\":[{\"content\":{\"parts\":[{\"text\":\"Thinking\",\"thought\":true}]}}],\"modelVersion\":\"gemini-2.5-flash\"}";
    process_chunk(sctx, chunk);

    /* Verify normalized to IK_STREAM_THINKING_DELTA */
    const ik_stream_event_t *event = find_event(IK_STREAM_THINKING_DELTA);
    ck_assert_ptr_nonnull(event);
    ck_assert_int_eq(event->type, IK_STREAM_THINKING_DELTA);
}

END_TEST

START_TEST(test_normalize_finish_reason_to_done_with_usage) {
    ik_google_stream_ctx_t *sctx = NULL;
    res_t r = ik_google_stream_ctx_create(test_ctx, test_stream_cb, NULL, &sctx);
    ck_assert(!is_err(&r));

    /* Process START first */
    process_chunk(sctx, "{\"modelVersion\":\"gemini-2.5-flash\"}");

    /* Process finish chunk with usage */
    const char *chunk =
        "{\"candidates\":[{\"finishReason\":\"MAX_TOKENS\"}],\"usageMetadata\":{\"promptTokenCount\":100,\"candidatesTokenCount\":200,\"thoughtsTokenCount\":50,\"totalTokenCount\":300}}";
    process_chunk(sctx, chunk);

    /* Verify normalized to IK_STREAM_DONE with usage */
    const ik_stream_event_t *event = find_event(IK_STREAM_DONE);
    ck_assert_ptr_nonnull(event);
    ck_assert_int_eq(event->type, IK_STREAM_DONE);
    ck_assert_int_eq(event->data.done.finish_reason, IK_FINISH_LENGTH);
    ck_assert_int_eq(event->data.done.usage.input_tokens, 100);
    ck_assert_int_eq(event->data.done.usage.output_tokens, 150); /* 200 - 50 */
    ck_assert_int_eq(event->data.done.usage.thinking_tokens, 50);
    ck_assert_int_eq(event->data.done.usage.total_tokens, 300);
}

END_TEST

/* ================================================================
 * Test Suite Setup
 * ================================================================ */

static Suite *google_streaming_parser_thinking_suite(void)
{
    Suite *s = suite_create("Google Streaming Parser - Thinking");

    TCase *tc_thinking = tcase_create("Thought Part Detection");
    tcase_set_timeout(tc_thinking, IK_TEST_TIMEOUT);
    tcase_add_checked_fixture(tc_thinking, setup, teardown);
    tcase_add_test(tc_thinking, test_parse_part_with_thought_true_flag);
    tcase_add_test(tc_thinking, test_parse_part_without_thought_flag);
    tcase_add_test(tc_thinking, test_distinguish_thought_content_from_regular_content);
    tcase_add_test(tc_thinking, test_interleaved_thinking_and_content_parts);
    suite_add_tcase(s, tc_thinking);

    TCase *tc_normalize = tcase_create("Event Normalization");
    tcase_set_timeout(tc_normalize, IK_TEST_TIMEOUT);
    tcase_add_checked_fixture(tc_normalize, setup, teardown);
    tcase_add_test(tc_normalize, test_normalize_text_part_to_text_delta);
    tcase_add_test(tc_normalize, test_normalize_thought_part_to_thinking_delta);
    tcase_add_test(tc_normalize, test_normalize_finish_reason_to_done_with_usage);
    suite_add_tcase(s, tc_normalize);

    return s;
}

int main(void)
{
    Suite *s = google_streaming_parser_thinking_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
