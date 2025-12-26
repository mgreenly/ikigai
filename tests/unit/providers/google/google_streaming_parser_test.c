/**
 * @file test_google_streaming.c
 * @brief Unit tests for Google provider streaming with async curl_multi
 *
 * Tests verify async streaming response handling using VCR fixtures.
 * All tests use fdset/perform/info_read pattern to integrate with select()-based event loop.
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
static bool completed;
static ik_provider_completion_t last_completion;

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

/**
 * Completion callback - captures completion for verification
 */
static res_t test_completion_cb(const ik_provider_completion_t *completion, void *ctx) __attribute__((unused));
static res_t test_completion_cb(const ik_provider_completion_t *completion, void *ctx)
{
    (void)ctx;

    completed = true;
    last_completion = *completion;

    return OK(NULL);
}

/* ================================================================
 * Test Fixtures
 * ================================================================ */

static void setup(void)
{
    test_ctx = talloc_new(NULL);
    captured_count = 0;
    completed = false;
    memset(&last_completion, 0, sizeof(last_completion));
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
 * Basic Streaming Tests
 * ================================================================ */

START_TEST(test_parse_single_text_part_chunk) {
    ik_google_stream_ctx_t *sctx = NULL;
    res_t r = ik_google_stream_ctx_create(test_ctx, test_stream_cb, NULL, &sctx);
    ck_assert(!is_err(&r));

    /* Process chunk with single text part */
    const char *chunk =
        "{\"candidates\":[{\"content\":{\"parts\":[{\"text\":\"Hello\"}]}}],\"modelVersion\":\"gemini-2.5-flash\"}";
    process_chunk(sctx, chunk);

    /* Verify START event emitted */
    ck_assert_int_ge((int)captured_count, 1);
    ck_assert_int_eq(captured_events[0].type, IK_STREAM_START);
    ck_assert_str_eq(captured_events[0].data.start.model, "gemini-2.5-flash");

    /* Verify TEXT_DELTA event emitted */
    const ik_stream_event_t *text_event = find_event(IK_STREAM_TEXT_DELTA);
    ck_assert_ptr_nonnull(text_event);
    ck_assert_str_eq(text_event->data.delta.text, "Hello");
}
END_TEST START_TEST(test_parse_multiple_text_parts_in_one_chunk)
{
    ik_google_stream_ctx_t *sctx = NULL;
    res_t r = ik_google_stream_ctx_create(test_ctx, test_stream_cb, NULL, &sctx);
    ck_assert(!is_err(&r));

    /* Process chunk with multiple text parts */
    const char *chunk =
        "{\"candidates\":[{\"content\":{\"parts\":[{\"text\":\"Hello\"},{\"text\":\" world\"}]}}],\"modelVersion\":\"gemini-2.5-flash\"}";
    process_chunk(sctx, chunk);

    /* Verify multiple TEXT_DELTA events */
    size_t text_count = count_events(IK_STREAM_TEXT_DELTA);
    ck_assert_int_eq((int)text_count, 2);
}

END_TEST START_TEST(test_parse_finish_reason_chunk)
{
    ik_google_stream_ctx_t *sctx = NULL;
    res_t r = ik_google_stream_ctx_create(test_ctx, test_stream_cb, NULL, &sctx);
    ck_assert(!is_err(&r));

    /* Process START chunk first */
    process_chunk(sctx, "{\"modelVersion\":\"gemini-2.5-flash\"}");

    /* Process chunk with finishReason and usageMetadata */
    const char *chunk =
        "{\"candidates\":[{\"finishReason\":\"STOP\",\"content\":{\"parts\":[{\"text\":\"!\"}]}}],\"usageMetadata\":{\"promptTokenCount\":10,\"candidatesTokenCount\":5,\"totalTokenCount\":15}}";
    process_chunk(sctx, chunk);

    /* Verify DONE event emitted */
    const ik_stream_event_t *done_event = find_event(IK_STREAM_DONE);
    ck_assert_ptr_nonnull(done_event);
    ck_assert_int_eq(done_event->data.done.finish_reason, IK_FINISH_STOP);
    ck_assert_int_eq(done_event->data.done.usage.input_tokens, 10);
    ck_assert_int_eq(done_event->data.done.usage.output_tokens, 5);
}

END_TEST START_TEST(test_accumulate_text_across_multiple_chunks)
{
    ik_google_stream_ctx_t *sctx = NULL;
    res_t r = ik_google_stream_ctx_create(test_ctx, test_stream_cb, NULL, &sctx);
    ck_assert(!is_err(&r));

    /* Process multiple chunks */
    process_chunk(sctx,
                  "{\"candidates\":[{\"content\":{\"parts\":[{\"text\":\"Hello\"}]}}],\"modelVersion\":\"gemini-2.5-flash\"}");
    process_chunk(sctx, "{\"candidates\":[{\"content\":{\"parts\":[{\"text\":\" world\"}]}}]}");
    process_chunk(sctx, "{\"candidates\":[{\"content\":{\"parts\":[{\"text\":\"!\"}]}}]}");

    /* Verify multiple TEXT_DELTA events */
    size_t text_count = count_events(IK_STREAM_TEXT_DELTA);
    ck_assert_int_eq((int)text_count, 3);

    /* Verify each text delta */
    size_t text_idx = 0;
    for (size_t i = 0; i < captured_count && text_idx < 3; i++) {
        if (captured_events[i].type == IK_STREAM_TEXT_DELTA) {
            if (text_idx == 0) {
                ck_assert_str_eq(captured_events[i].data.delta.text, "Hello");
            } else if (text_idx == 1) {
                ck_assert_str_eq(captured_events[i].data.delta.text, " world");
            } else if (text_idx == 2) {
                ck_assert_str_eq(captured_events[i].data.delta.text, "!");
            }
            text_idx++;
        }
    }
}

END_TEST
/* ================================================================
 * Thought Part Detection Tests
 * ================================================================ */

START_TEST(test_parse_part_with_thought_true_flag)
{
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

END_TEST START_TEST(test_parse_part_without_thought_flag)
{
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

END_TEST START_TEST(test_distinguish_thought_content_from_regular_content)
{
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

END_TEST START_TEST(test_interleaved_thinking_and_content_parts)
{
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
 * Function Call Streaming Tests
 * ================================================================ */

START_TEST(test_parse_function_call_part)
{
    ik_google_stream_ctx_t *sctx = NULL;
    res_t r = ik_google_stream_ctx_create(test_ctx, test_stream_cb, NULL, &sctx);
    ck_assert(!is_err(&r));

    /* Process chunk with functionCall */
    const char *chunk =
        "{\"candidates\":[{\"content\":{\"parts\":[{\"functionCall\":{\"name\":\"get_weather\",\"args\":{\"location\":\"London\"}}}]}}],\"modelVersion\":\"gemini-2.5-flash\"}";
    process_chunk(sctx, chunk);

    /* Verify TOOL_CALL_START event */
    const ik_stream_event_t *start_event = find_event(IK_STREAM_TOOL_CALL_START);
    ck_assert_ptr_nonnull(start_event);
    ck_assert_ptr_nonnull(start_event->data.tool_start.id);
    ck_assert_str_eq(start_event->data.tool_start.name, "get_weather");

    /* Verify TOOL_CALL_DELTA event */
    const ik_stream_event_t *delta_event = find_event(IK_STREAM_TOOL_CALL_DELTA);
    ck_assert_ptr_nonnull(delta_event);
    ck_assert_ptr_nonnull(strstr(delta_event->data.tool_delta.arguments, "location"));
    ck_assert_ptr_nonnull(strstr(delta_event->data.tool_delta.arguments, "London"));
}

END_TEST START_TEST(test_generate_22_char_base64url_uuid)
{
    ik_google_stream_ctx_t *sctx = NULL;
    res_t r = ik_google_stream_ctx_create(test_ctx, test_stream_cb, NULL, &sctx);
    ck_assert(!is_err(&r));

    /* Process chunk with functionCall */
    const char *chunk =
        "{\"candidates\":[{\"content\":{\"parts\":[{\"functionCall\":{\"name\":\"test_func\",\"args\":{}}}]}}],\"modelVersion\":\"gemini-2.5-flash\"}";
    process_chunk(sctx, chunk);

    /* Verify generated ID is 22 characters */
    const ik_stream_event_t *start_event = find_event(IK_STREAM_TOOL_CALL_START);
    ck_assert_ptr_nonnull(start_event);
    ck_assert_ptr_nonnull(start_event->data.tool_start.id);
    ck_assert_int_eq((int)strlen(start_event->data.tool_start.id), 22);

    /* Verify ID contains only base64url characters (A-Z, a-z, 0-9, -, _) */
    const char *id = start_event->data.tool_start.id;
    for (size_t i = 0; i < 22; i++) {
        char c = id[i];
        bool valid = (c >= 'A' && c <= 'Z') ||
                     (c >= 'a' && c <= 'z') ||
                     (c >= '0' && c <= '9') ||
                     c == '-' || c == '_';
        ck_assert(valid);
    }
}

END_TEST START_TEST(test_parse_function_arguments_from_function_call)
{
    ik_google_stream_ctx_t *sctx = NULL;
    res_t r = ik_google_stream_ctx_create(test_ctx, test_stream_cb, NULL, &sctx);
    ck_assert(!is_err(&r));

    /* Process chunk with functionCall with complex args */
    const char *chunk =
        "{\"candidates\":[{\"content\":{\"parts\":[{\"functionCall\":{\"name\":\"calc\",\"args\":{\"operation\":\"add\",\"values\":[1,2,3]}}}]}}],\"modelVersion\":\"gemini-2.5-flash\"}";
    process_chunk(sctx, chunk);

    /* Verify TOOL_CALL_DELTA contains serialized args */
    const ik_stream_event_t *delta_event = find_event(IK_STREAM_TOOL_CALL_DELTA);
    ck_assert_ptr_nonnull(delta_event);
    ck_assert_ptr_nonnull(strstr(delta_event->data.tool_delta.arguments, "operation"));
    ck_assert_ptr_nonnull(strstr(delta_event->data.tool_delta.arguments, "add"));
    ck_assert_ptr_nonnull(strstr(delta_event->data.tool_delta.arguments, "values"));
}

END_TEST
/* ================================================================
 * Event Normalization Tests
 * ================================================================ */

START_TEST(test_normalize_text_part_to_text_delta)
{
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

END_TEST START_TEST(test_normalize_thought_part_to_thinking_delta)
{
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

END_TEST START_TEST(test_normalize_finish_reason_to_done_with_usage)
{
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
 * Error Handling Tests
 * ================================================================ */

START_TEST(test_handle_malformed_json_chunk)
{
    ik_google_stream_ctx_t *sctx = NULL;
    res_t r = ik_google_stream_ctx_create(test_ctx, test_stream_cb, NULL, &sctx);
    ck_assert(!is_err(&r));

    /* Process malformed JSON - should be silently ignored */
    const char *chunk = "{invalid json}";
    process_chunk(sctx, chunk);

    /* Verify no events emitted (malformed JSON ignored) */
    ck_assert_int_eq((int)captured_count, 0);
}

END_TEST START_TEST(test_handle_empty_data_chunk)
{
    ik_google_stream_ctx_t *sctx = NULL;
    res_t r = ik_google_stream_ctx_create(test_ctx, test_stream_cb, NULL, &sctx);
    ck_assert(!is_err(&r));

    /* Process empty data */
    process_chunk(sctx, "");
    process_chunk(sctx, NULL);

    /* Verify no events emitted */
    ck_assert_int_eq((int)captured_count, 0);
}

END_TEST START_TEST(test_handle_error_object_in_chunk)
{
    ik_google_stream_ctx_t *sctx = NULL;
    res_t r = ik_google_stream_ctx_create(test_ctx, test_stream_cb, NULL, &sctx);
    ck_assert(!is_err(&r));

    /* Process chunk with error object */
    const char *chunk = "{\"error\":{\"message\":\"API key invalid\",\"status\":\"UNAUTHENTICATED\"}}";
    process_chunk(sctx, chunk);

    /* Verify ERROR event emitted */
    const ik_stream_event_t *event = find_event(IK_STREAM_ERROR);
    ck_assert_ptr_nonnull(event);
    ck_assert_int_eq(event->data.error.category, IK_ERR_CAT_AUTH);
    ck_assert_str_eq(event->data.error.message, "API key invalid");
}

END_TEST
/* ================================================================
 * Usage Statistics Tests
 * ================================================================ */

START_TEST(test_usage_excludes_thinking_from_output_tokens)
{
    ik_google_stream_ctx_t *sctx = NULL;
    res_t r = ik_google_stream_ctx_create(test_ctx, test_stream_cb, NULL, &sctx);
    ck_assert(!is_err(&r));

    /* Process START first */
    process_chunk(sctx, "{\"modelVersion\":\"gemini-2.5-flash\"}");

    /* Process usage chunk */
    const char *chunk =
        "{\"usageMetadata\":{\"promptTokenCount\":100,\"candidatesTokenCount\":200,\"thoughtsTokenCount\":50,\"totalTokenCount\":300}}";
    process_chunk(sctx, chunk);

    /* Verify usage calculation */
    ik_usage_t usage = ik_google_stream_get_usage(sctx);
    ck_assert_int_eq(usage.input_tokens, 100);
    ck_assert_int_eq(usage.output_tokens, 150); /* candidatesTokenCount - thoughtsTokenCount */
    ck_assert_int_eq(usage.thinking_tokens, 50);
    ck_assert_int_eq(usage.total_tokens, 300);
}

END_TEST START_TEST(test_usage_handles_missing_thoughts_token_count)
{
    ik_google_stream_ctx_t *sctx = NULL;
    res_t r = ik_google_stream_ctx_create(test_ctx, test_stream_cb, NULL, &sctx);
    ck_assert(!is_err(&r));

    /* Process START first */
    process_chunk(sctx, "{\"modelVersion\":\"gemini-2.5-flash\"}");

    /* Process usage chunk without thoughtsTokenCount */
    const char *chunk =
        "{\"usageMetadata\":{\"promptTokenCount\":100,\"candidatesTokenCount\":200,\"totalTokenCount\":300}}";
    process_chunk(sctx, chunk);

    /* Verify usage calculation */
    ik_usage_t usage = ik_google_stream_get_usage(sctx);
    ck_assert_int_eq(usage.input_tokens, 100);
    ck_assert_int_eq(usage.output_tokens, 200); /* candidatesTokenCount when no thoughts */
    ck_assert_int_eq(usage.thinking_tokens, 0);
    ck_assert_int_eq(usage.total_tokens, 300);
}

END_TEST
/* ================================================================
 * Finish Reason Tests
 * ================================================================ */

START_TEST(test_map_stop_finish_reason)
{
    ik_finish_reason_t reason = ik_google_map_finish_reason("STOP");
    ck_assert_int_eq(reason, IK_FINISH_STOP);
}

END_TEST START_TEST(test_map_max_tokens_finish_reason)
{
    ik_finish_reason_t reason = ik_google_map_finish_reason("MAX_TOKENS");
    ck_assert_int_eq(reason, IK_FINISH_LENGTH);
}

END_TEST START_TEST(test_map_safety_finish_reason)
{
    ik_finish_reason_t reason = ik_google_map_finish_reason("SAFETY");
    ck_assert_int_eq(reason, IK_FINISH_CONTENT_FILTER);
}

END_TEST START_TEST(test_map_unknown_finish_reason)
{
    ik_finish_reason_t reason = ik_google_map_finish_reason("UNKNOWN_REASON");
    ck_assert_int_eq(reason, IK_FINISH_UNKNOWN);
}

END_TEST START_TEST(test_map_null_finish_reason)
{
    ik_finish_reason_t reason = ik_google_map_finish_reason(NULL);
    ck_assert_int_eq(reason, IK_FINISH_UNKNOWN);
}

END_TEST
/* ================================================================
 * Stream Context Tests
 * ================================================================ */

START_TEST(test_stream_ctx_create_initializes_state)
{
    ik_google_stream_ctx_t *sctx = NULL;
    res_t r = ik_google_stream_ctx_create(test_ctx, test_stream_cb, NULL, &sctx);

    ck_assert(!is_err(&r));
    ck_assert_ptr_nonnull(sctx);

    /* Verify initial state */
    ik_usage_t usage = ik_google_stream_get_usage(sctx);
    ck_assert_int_eq(usage.input_tokens, 0);
    ck_assert_int_eq(usage.output_tokens, 0);
    ck_assert_int_eq(usage.thinking_tokens, 0);
    ck_assert_int_eq(usage.total_tokens, 0);

    ik_finish_reason_t reason = ik_google_stream_get_finish_reason(sctx);
    ck_assert_int_eq(reason, IK_FINISH_UNKNOWN);
}

END_TEST
/* ================================================================
 * Tool Call State Transition Tests
 * ================================================================ */

START_TEST(test_tool_call_followed_by_text_ends_tool_call)
{
    ik_google_stream_ctx_t *sctx = NULL;
    res_t r = ik_google_stream_ctx_create(test_ctx, test_stream_cb, NULL, &sctx);
    ck_assert(!is_err(&r));

    /* Process tool call */
    process_chunk(sctx,
                  "{\"candidates\":[{\"content\":{\"parts\":[{\"functionCall\":{\"name\":\"test\",\"args\":{}}}]}}],\"modelVersion\":\"gemini-2.5-flash\"}");

    /* Process text part (should end tool call) */
    process_chunk(sctx, "{\"candidates\":[{\"content\":{\"parts\":[{\"text\":\"Done\"}]}}]}");

    /* Verify TOOL_CALL_DONE event emitted before text */
    size_t done_idx = 0;
    size_t text_idx = 0;
    for (size_t i = 0; i < captured_count; i++) {
        if (captured_events[i].type == IK_STREAM_TOOL_CALL_DONE) {
            done_idx = i;
        }
        if (captured_events[i].type == IK_STREAM_TEXT_DELTA) {
            text_idx = i;
        }
    }

    /* TOOL_CALL_DONE should come before TEXT_DELTA */
    ck_assert(done_idx > 0);
    ck_assert(text_idx > done_idx);
}

END_TEST START_TEST(test_usage_metadata_ends_tool_call)
{
    ik_google_stream_ctx_t *sctx = NULL;
    res_t r = ik_google_stream_ctx_create(test_ctx, test_stream_cb, NULL, &sctx);
    ck_assert(!is_err(&r));

    /* Process tool call */
    process_chunk(sctx,
                  "{\"candidates\":[{\"content\":{\"parts\":[{\"functionCall\":{\"name\":\"test\",\"args\":{}}}]}}],\"modelVersion\":\"gemini-2.5-flash\"}");

    /* Process usage metadata (should end tool call) */
    process_chunk(sctx,
                  "{\"usageMetadata\":{\"promptTokenCount\":10,\"candidatesTokenCount\":5,\"totalTokenCount\":15}}");

    /* Verify TOOL_CALL_DONE emitted before STREAM_DONE */
    const ik_stream_event_t *done_event = find_event(IK_STREAM_TOOL_CALL_DONE);
    ck_assert_ptr_nonnull(done_event);

    const ik_stream_event_t *stream_done = find_event(IK_STREAM_DONE);
    ck_assert_ptr_nonnull(stream_done);
}

END_TEST

/* ================================================================
 * Test Suite Setup
 * ================================================================ */

static Suite *google_streaming_suite(void)
{
    Suite *s = suite_create("Google Streaming");

    TCase *tc_basic = tcase_create("Basic Streaming");
    tcase_add_checked_fixture(tc_basic, setup, teardown);
    tcase_add_test(tc_basic, test_parse_single_text_part_chunk);
    tcase_add_test(tc_basic, test_parse_multiple_text_parts_in_one_chunk);
    tcase_add_test(tc_basic, test_parse_finish_reason_chunk);
    tcase_add_test(tc_basic, test_accumulate_text_across_multiple_chunks);
    suite_add_tcase(s, tc_basic);

    TCase *tc_thinking = tcase_create("Thought Part Detection");
    tcase_add_checked_fixture(tc_thinking, setup, teardown);
    tcase_add_test(tc_thinking, test_parse_part_with_thought_true_flag);
    tcase_add_test(tc_thinking, test_parse_part_without_thought_flag);
    tcase_add_test(tc_thinking, test_distinguish_thought_content_from_regular_content);
    tcase_add_test(tc_thinking, test_interleaved_thinking_and_content_parts);
    suite_add_tcase(s, tc_thinking);

    TCase *tc_function = tcase_create("Function Call Streaming");
    tcase_add_checked_fixture(tc_function, setup, teardown);
    tcase_add_test(tc_function, test_parse_function_call_part);
    tcase_add_test(tc_function, test_generate_22_char_base64url_uuid);
    tcase_add_test(tc_function, test_parse_function_arguments_from_function_call);
    suite_add_tcase(s, tc_function);

    TCase *tc_normalize = tcase_create("Event Normalization");
    tcase_add_checked_fixture(tc_normalize, setup, teardown);
    tcase_add_test(tc_normalize, test_normalize_text_part_to_text_delta);
    tcase_add_test(tc_normalize, test_normalize_thought_part_to_thinking_delta);
    tcase_add_test(tc_normalize, test_normalize_finish_reason_to_done_with_usage);
    suite_add_tcase(s, tc_normalize);

    TCase *tc_error = tcase_create("Error Handling");
    tcase_add_checked_fixture(tc_error, setup, teardown);
    tcase_add_test(tc_error, test_handle_malformed_json_chunk);
    tcase_add_test(tc_error, test_handle_empty_data_chunk);
    tcase_add_test(tc_error, test_handle_error_object_in_chunk);
    suite_add_tcase(s, tc_error);

    TCase *tc_usage = tcase_create("Usage Statistics");
    tcase_add_checked_fixture(tc_usage, setup, teardown);
    tcase_add_test(tc_usage, test_usage_excludes_thinking_from_output_tokens);
    tcase_add_test(tc_usage, test_usage_handles_missing_thoughts_token_count);
    suite_add_tcase(s, tc_usage);

    TCase *tc_finish = tcase_create("Finish Reason Mapping");
    tcase_add_checked_fixture(tc_finish, setup, teardown);
    tcase_add_test(tc_finish, test_map_stop_finish_reason);
    tcase_add_test(tc_finish, test_map_max_tokens_finish_reason);
    tcase_add_test(tc_finish, test_map_safety_finish_reason);
    tcase_add_test(tc_finish, test_map_unknown_finish_reason);
    tcase_add_test(tc_finish, test_map_null_finish_reason);
    suite_add_tcase(s, tc_finish);

    TCase *tc_ctx = tcase_create("Stream Context");
    tcase_add_checked_fixture(tc_ctx, setup, teardown);
    tcase_add_test(tc_ctx, test_stream_ctx_create_initializes_state);
    suite_add_tcase(s, tc_ctx);

    TCase *tc_state = tcase_create("State Transitions");
    tcase_add_checked_fixture(tc_state, setup, teardown);
    tcase_add_test(tc_state, test_tool_call_followed_by_text_ends_tool_call);
    tcase_add_test(tc_state, test_usage_metadata_ends_tool_call);
    suite_add_tcase(s, tc_state);

    return s;
}

int main(void)
{
    Suite *s = google_streaming_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
