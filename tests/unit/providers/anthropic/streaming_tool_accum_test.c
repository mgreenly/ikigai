/**
 * @file streaming_tool_accum_test.c
 * @brief Unit tests for Anthropic streaming tool argument accumulation
 *
 * Tests the tool argument accumulation feature in content_block_delta:
 * - Single argument delta
 * - Multiple argument deltas accumulate correctly
 * - Arguments cleared on content_block_stop
 */

#include <check.h>
#include <talloc.h>
#include <string.h>
#include "providers/anthropic/streaming_events.h"
#include "providers/anthropic/streaming.h"
#include "providers/provider.h"
#include "vendor/yyjson/yyjson.h"

static TALLOC_CTX *test_ctx;
static ik_anthropic_stream_ctx_t *stream_ctx;

#define MAX_EVENTS 16
static ik_stream_event_t captured_events[MAX_EVENTS];
static size_t captured_count;

static res_t test_stream_cb(const ik_stream_event_t *event, void *ctx)
{
    (void)ctx;
    if (captured_count < MAX_EVENTS) {
        captured_events[captured_count] = *event;
        // Deep copy tool delta arguments
        if (event->type == IK_STREAM_TOOL_CALL_DELTA && event->data.tool_delta.arguments) {
            captured_events[captured_count].data.tool_delta.arguments =
                talloc_strdup(test_ctx, event->data.tool_delta.arguments);
        }
        captured_count++;
    }
    return OK(NULL);
}

static void setup(void)
{
    test_ctx = talloc_new(NULL);
    res_t r = ik_anthropic_stream_ctx_create(test_ctx, test_stream_cb, NULL, &stream_ctx);
    ck_assert(!is_err(&r));
    captured_count = 0;
    memset(captured_events, 0, sizeof(captured_events));
}

static void teardown(void)
{
    talloc_free(test_ctx);
}

/* ================================================================
 * Tool Argument Accumulation Tests
 * ================================================================ */

START_TEST(test_single_delta_accumulates) {
    // Process a single input_json_delta event
    const char *json =
        "{\"index\": 0, \"delta\": {\"type\": \"input_json_delta\", \"partial_json\": \"{\\\"key\\\":\"}}";
    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    ck_assert_ptr_nonnull(doc);

    ik_anthropic_process_content_block_delta(stream_ctx, yyjson_doc_get_root(doc));

    // Check that event was emitted
    ck_assert_int_eq((int)captured_count, 1);
    ck_assert_int_eq(captured_events[0].type, IK_STREAM_TOOL_CALL_DELTA);

    // Check that arguments were accumulated in context
    ck_assert_ptr_nonnull(stream_ctx->current_tool_args);
    ck_assert_str_eq(stream_ctx->current_tool_args, "{\"key\":");

    yyjson_doc_free(doc);
}

END_TEST

START_TEST(test_multiple_deltas_accumulate) {
    // Process first delta
    const char *json1 =
        "{\"index\": 0, \"delta\": {\"type\": \"input_json_delta\", \"partial_json\": \"{\\\"key\\\":\"}}";
    yyjson_doc *doc1 = yyjson_read(json1, strlen(json1), 0);
    ck_assert_ptr_nonnull(doc1);
    ik_anthropic_process_content_block_delta(stream_ctx, yyjson_doc_get_root(doc1));
    yyjson_doc_free(doc1);

    // Check first accumulation
    ck_assert_str_eq(stream_ctx->current_tool_args, "{\"key\":");

    // Process second delta
    const char *json2 =
        "{\"index\": 0, \"delta\": {\"type\": \"input_json_delta\", \"partial_json\": \"\\\"value\\\"\"}}";
    yyjson_doc *doc2 = yyjson_read(json2, strlen(json2), 0);
    ck_assert_ptr_nonnull(doc2);
    ik_anthropic_process_content_block_delta(stream_ctx, yyjson_doc_get_root(doc2));
    yyjson_doc_free(doc2);

    // Check accumulated result
    ck_assert_str_eq(stream_ctx->current_tool_args, "{\"key\":\"value\"");

    // Process third delta
    const char *json3 = "{\"index\": 0, \"delta\": {\"type\": \"input_json_delta\", \"partial_json\": \"}\"}}";
    yyjson_doc *doc3 = yyjson_read(json3, strlen(json3), 0);
    ck_assert_ptr_nonnull(doc3);
    ik_anthropic_process_content_block_delta(stream_ctx, yyjson_doc_get_root(doc3));
    yyjson_doc_free(doc3);

    // Check final accumulated result
    ck_assert_str_eq(stream_ctx->current_tool_args, "{\"key\":\"value\"}");

    // Verify all three events were emitted
    ck_assert_int_eq((int)captured_count, 3);
}

END_TEST

START_TEST(test_args_cleared_on_tool_stop) {
    // Set up tool call state
    stream_ctx->current_block_type = IK_CONTENT_TOOL_CALL;
    stream_ctx->current_tool_id = talloc_strdup(stream_ctx, "tool_123");
    stream_ctx->current_tool_name = talloc_strdup(stream_ctx, "test_tool");
    stream_ctx->current_tool_args = talloc_strdup(stream_ctx, "{\"complete\":\"json\"}");

    // Process content_block_stop
    const char *json = "{\"index\": 0}";
    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    ck_assert_ptr_nonnull(doc);

    ik_anthropic_process_content_block_stop(stream_ctx, yyjson_doc_get_root(doc));

    // Tool fields should NOT be cleared - response builder needs them
    ck_assert_ptr_nonnull(stream_ctx->current_tool_id);
    ck_assert_ptr_nonnull(stream_ctx->current_tool_name);
    ck_assert_ptr_nonnull(stream_ctx->current_tool_args);

    // Check that TOOL_CALL_DONE event was emitted
    ck_assert_int_eq((int)captured_count, 1);
    ck_assert_int_eq(captured_events[0].type, IK_STREAM_TOOL_CALL_DONE);

    yyjson_doc_free(doc);
}

END_TEST

START_TEST(test_text_stop_preserves_tool_state) {
    // Set up text block state but with tool args from previous block
    stream_ctx->current_block_type = IK_CONTENT_TEXT;
    stream_ctx->current_tool_args = talloc_strdup(stream_ctx, "should_not_be_freed");

    // Process content_block_stop for text block
    const char *json = "{\"index\": 0}";
    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    ck_assert_ptr_nonnull(doc);

    ik_anthropic_process_content_block_stop(stream_ctx, yyjson_doc_get_root(doc));

    // Text blocks should not emit TOOL_CALL_DONE
    ck_assert_int_eq((int)captured_count, 0);

    // Tool args should remain (only cleared for tool blocks)
    ck_assert_ptr_nonnull(stream_ctx->current_tool_args);

    yyjson_doc_free(doc);
}

END_TEST

START_TEST(test_accumulation_starts_empty) {
    // Verify that current_tool_args starts as NULL
    ck_assert_ptr_null(stream_ctx->current_tool_args);

    // Process a delta - should concatenate with empty string
    const char *json = "{\"index\": 0, \"delta\": {\"type\": \"input_json_delta\", \"partial_json\": \"test\"}}";
    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    ck_assert_ptr_nonnull(doc);

    ik_anthropic_process_content_block_delta(stream_ctx, yyjson_doc_get_root(doc));

    // Should properly handle NULL current_tool_args
    ck_assert_str_eq(stream_ctx->current_tool_args, "test");

    yyjson_doc_free(doc);
}

END_TEST

/* ================================================================
 * Test Suite Setup
 * ================================================================ */

static Suite *streaming_tool_accum_suite(void)
{
    Suite *s = suite_create("Anthropic Streaming Tool Accumulation");

    TCase *tc = tcase_create("Tool Argument Accumulation");
    tcase_set_timeout(tc, 30);
    tcase_add_checked_fixture(tc, setup, teardown);
    tcase_add_test(tc, test_single_delta_accumulates);
    tcase_add_test(tc, test_multiple_deltas_accumulate);
    tcase_add_test(tc, test_args_cleared_on_tool_stop);
    tcase_add_test(tc, test_text_stop_preserves_tool_state);
    tcase_add_test(tc, test_accumulation_starts_empty);
    suite_add_tcase(s, tc);

    return s;
}

int main(void)
{
    Suite *s = streaming_tool_accum_suite();
    SRunner *sr = srunner_create(s);
    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);
    return (number_failed == 0) ? 0 : 1;
}
