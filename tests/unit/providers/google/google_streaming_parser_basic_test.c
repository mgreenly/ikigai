/**
 * @file google_streaming_parser_basic_test.c
 * @brief Unit tests for Google provider basic streaming
 *
 * Tests verify basic streaming functionality.
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
END_TEST

START_TEST(test_parse_multiple_text_parts_in_one_chunk) {
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

END_TEST

START_TEST(test_parse_finish_reason_chunk) {
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

END_TEST

START_TEST(test_accumulate_text_across_multiple_chunks) {
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
 * Test Suite Setup
 * ================================================================ */

static Suite *google_streaming_parser_basic_suite(void)
{
    Suite *s = suite_create("Google Streaming Parser - Basic");

    TCase *tc_basic = tcase_create("Basic Streaming");
    tcase_set_timeout(tc_basic, 30);
    tcase_add_checked_fixture(tc_basic, setup, teardown);
    tcase_add_test(tc_basic, test_parse_single_text_part_chunk);
    tcase_add_test(tc_basic, test_parse_multiple_text_parts_in_one_chunk);
    tcase_add_test(tc_basic, test_parse_finish_reason_chunk);
    tcase_add_test(tc_basic, test_accumulate_text_across_multiple_chunks);
    suite_add_tcase(s, tc_basic);

    return s;
}

int main(void)
{
    Suite *s = google_streaming_parser_basic_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
