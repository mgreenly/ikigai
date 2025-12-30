/**
 * @file streaming_events_coverage_test_3.c
 * @brief Coverage tests for Anthropic streaming events - Part 3
 *
 * Tests edge cases for: thinking blocks, tool_use id/name, content_block_delta, content_block_stop
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
        if (event->type == IK_STREAM_TOOL_CALL_START) {
            if (event->data.tool_start.id) {
                captured_events[captured_count].data.tool_start.id = talloc_strdup(test_ctx, event->data.tool_start.id);
            }
            if (event->data.tool_start.name) {
                captured_events[captured_count].data.tool_start.name = talloc_strdup(test_ctx, event->data.tool_start.name);
            }
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

static void teardown(void) { talloc_free(test_ctx); }

START_TEST(test_thinking_type)
{
    const char *json = "{\"index\": 1, \"content_block\": {\"type\": \"thinking\"}}";
    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    ck_assert_ptr_nonnull(doc);
    ik_anthropic_process_content_block_start(stream_ctx, yyjson_doc_get_root(doc));
    ck_assert_int_eq(stream_ctx->current_block_type, IK_CONTENT_THINKING);
    ck_assert_int_eq((int)captured_count, 0);
    yyjson_doc_free(doc);
}
END_TEST

START_TEST(test_tool_use_no_id)
{
    const char *json = "{\"index\": 2, \"content_block\": {\"type\": \"tool_use\", \"name\": \"test_tool\"}}";
    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    ck_assert_ptr_nonnull(doc);
    ik_anthropic_process_content_block_start(stream_ctx, yyjson_doc_get_root(doc));
    ck_assert_ptr_null(captured_events[0].data.tool_start.id);
    yyjson_doc_free(doc);
}
END_TEST

START_TEST(test_tool_use_id_not_string)
{
    const char *json = "{\"index\": 2, \"content_block\": {\"type\": \"tool_use\", \"id\": 12345, \"name\": \"test_tool\"}}";
    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    ck_assert_ptr_nonnull(doc);
    ik_anthropic_process_content_block_start(stream_ctx, yyjson_doc_get_root(doc));
    ck_assert_ptr_null(captured_events[0].data.tool_start.id);
    yyjson_doc_free(doc);
}
END_TEST

START_TEST(test_tool_use_no_name)
{
    const char *json = "{\"index\": 2, \"content_block\": {\"type\": \"tool_use\", \"id\": \"tool_123\"}}";
    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    ck_assert_ptr_nonnull(doc);
    ik_anthropic_process_content_block_start(stream_ctx, yyjson_doc_get_root(doc));
    ck_assert_ptr_null(captured_events[0].data.tool_start.name);
    yyjson_doc_free(doc);
}
END_TEST

START_TEST(test_tool_use_name_not_string)
{
    const char *json = "{\"index\": 2, \"content_block\": {\"type\": \"tool_use\", \"id\": \"tool_123\", \"name\": 12345}}";
    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    ck_assert_ptr_nonnull(doc);
    ik_anthropic_process_content_block_start(stream_ctx, yyjson_doc_get_root(doc));
    ck_assert_ptr_null(captured_events[0].data.tool_start.name);
    yyjson_doc_free(doc);
}
END_TEST

START_TEST(test_delta_no_delta)
{
    const char *json = "{\"index\": 1}";
    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    ck_assert_ptr_nonnull(doc);
    ik_anthropic_process_content_block_delta(stream_ctx, yyjson_doc_get_root(doc));
    ck_assert_int_eq((int)captured_count, 0);
    yyjson_doc_free(doc);
}
END_TEST

START_TEST(test_delta_thinking)
{
    const char *json = "{\"index\": 1, \"delta\": {\"type\": \"thinking_delta\", \"thinking\": \"Hmm...\"}}";
    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    ck_assert_ptr_nonnull(doc);
    ik_anthropic_process_content_block_delta(stream_ctx, yyjson_doc_get_root(doc));
    ck_assert_int_eq(captured_events[0].type, IK_STREAM_THINKING_DELTA);
    yyjson_doc_free(doc);
}
END_TEST

START_TEST(test_delta_thinking_no_field)
{
    const char *json = "{\"index\": 1, \"delta\": {\"type\": \"thinking_delta\"}}";
    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    ck_assert_ptr_nonnull(doc);
    ik_anthropic_process_content_block_delta(stream_ctx, yyjson_doc_get_root(doc));
    ck_assert_int_eq((int)captured_count, 0);
    yyjson_doc_free(doc);
}
END_TEST

START_TEST(test_delta_input_json)
{
    const char *json = "{\"index\": 2, \"delta\": {\"type\": \"input_json_delta\", \"partial_json\": \"{}\"}}";
    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    ck_assert_ptr_nonnull(doc);
    ik_anthropic_process_content_block_delta(stream_ctx, yyjson_doc_get_root(doc));
    ck_assert_int_eq(captured_events[0].type, IK_STREAM_TOOL_CALL_DELTA);
    yyjson_doc_free(doc);
}
END_TEST

START_TEST(test_delta_input_json_no_field)
{
    const char *json = "{\"index\": 2, \"delta\": {\"type\": \"input_json_delta\"}}";
    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    ck_assert_ptr_nonnull(doc);
    ik_anthropic_process_content_block_delta(stream_ctx, yyjson_doc_get_root(doc));
    ck_assert_int_eq((int)captured_count, 0);
    yyjson_doc_free(doc);
}
END_TEST

START_TEST(test_stop_text_block)
{
    stream_ctx->current_block_type = IK_CONTENT_TEXT;
    const char *json = "{\"index\": 0}";
    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    ck_assert_ptr_nonnull(doc);
    ik_anthropic_process_content_block_stop(stream_ctx, yyjson_doc_get_root(doc));
    ck_assert_int_eq((int)captured_count, 0);
    yyjson_doc_free(doc);
}
END_TEST

START_TEST(test_stop_thinking_block)
{
    stream_ctx->current_block_type = IK_CONTENT_THINKING;
    const char *json = "{\"index\": 1}";
    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    ck_assert_ptr_nonnull(doc);
    ik_anthropic_process_content_block_stop(stream_ctx, yyjson_doc_get_root(doc));
    ck_assert_int_eq((int)captured_count, 0);
    yyjson_doc_free(doc);
}
END_TEST

static Suite *streaming_events_coverage_suite_3(void)
{
    Suite *s = suite_create("Anthropic Streaming Events Coverage 3");
    TCase *tc = tcase_create("Edge Cases");
    tcase_set_timeout(tc, 30);
    tcase_add_checked_fixture(tc, setup, teardown);
    tcase_add_test(tc, test_thinking_type);
    tcase_add_test(tc, test_tool_use_no_id);
    tcase_add_test(tc, test_tool_use_id_not_string);
    tcase_add_test(tc, test_tool_use_no_name);
    tcase_add_test(tc, test_tool_use_name_not_string);
    tcase_add_test(tc, test_delta_no_delta);
    tcase_add_test(tc, test_delta_thinking);
    tcase_add_test(tc, test_delta_thinking_no_field);
    tcase_add_test(tc, test_delta_input_json);
    tcase_add_test(tc, test_delta_input_json_no_field);
    tcase_add_test(tc, test_stop_text_block);
    tcase_add_test(tc, test_stop_thinking_block);
    suite_add_tcase(s, tc);
    return s;
}

int main(void)
{
    Suite *s = streaming_events_coverage_suite_3();
    SRunner *sr = srunner_create(s);
    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);
    return (number_failed == 0) ? 0 : 1;
}
