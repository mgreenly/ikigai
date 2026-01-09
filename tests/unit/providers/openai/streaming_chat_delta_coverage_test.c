/**
 * @file streaming_chat_delta_coverage_test.c
 * @brief Coverage tests for OpenAI Chat streaming delta processing edge cases
 */

#include <check.h>
#include <talloc.h>
#include <string.h>
#include <stdbool.h>
#include "providers/openai/streaming.h"
#include "providers/openai/openai.h"
#include "providers/provider.h"

/* ================================================================
 * Test Context and Event Capture
 * ================================================================ */

typedef struct {
    ik_stream_event_t *items;
    size_t count;
    size_t capacity;
} event_array_t;

static TALLOC_CTX *test_ctx;
static event_array_t *events;

static res_t stream_cb(const ik_stream_event_t *event, void *ctx)
{
    event_array_t *arr = (event_array_t *)ctx;

    if (arr->count >= arr->capacity) {
        size_t new_capacity = arr->capacity == 0 ? 8 : arr->capacity * 2;
        ik_stream_event_t *new_items = talloc_realloc(test_ctx, arr->items,
                                                      ik_stream_event_t, (unsigned int)new_capacity);
        if (new_items == NULL) {
            return ERR(test_ctx, OUT_OF_MEMORY, "Failed to grow event array");
        }
        arr->items = new_items;
        arr->capacity = new_capacity;
    }

    ik_stream_event_t *dst = &arr->items[arr->count];
    dst->type = event->type;
    dst->index = event->index;

    switch (event->type) {
        case IK_STREAM_START:
            dst->data.start.model = event->data.start.model
                ? talloc_strdup(arr->items, event->data.start.model)
                : NULL;
            break;

        case IK_STREAM_TEXT_DELTA:
        case IK_STREAM_THINKING_DELTA:
            dst->data.delta.text = event->data.delta.text
                ? talloc_strdup(arr->items, event->data.delta.text)
                : NULL;
            break;

        case IK_STREAM_TOOL_CALL_START:
            dst->data.tool_start.id = event->data.tool_start.id
                ? talloc_strdup(arr->items, event->data.tool_start.id)
                : NULL;
            dst->data.tool_start.name = event->data.tool_start.name
                ? talloc_strdup(arr->items, event->data.tool_start.name)
                : NULL;
            break;

        case IK_STREAM_TOOL_CALL_DELTA:
            dst->data.tool_delta.arguments = event->data.tool_delta.arguments
                ? talloc_strdup(arr->items, event->data.tool_delta.arguments)
                : NULL;
            break;

        case IK_STREAM_TOOL_CALL_DONE:
            break;

        case IK_STREAM_DONE:
            dst->data.done.finish_reason = event->data.done.finish_reason;
            dst->data.done.usage = event->data.done.usage;
            dst->data.done.provider_data = event->data.done.provider_data
                ? talloc_strdup(arr->items, event->data.done.provider_data)
                : NULL;
            break;

        case IK_STREAM_ERROR:
            dst->data.error.category = event->data.error.category;
            dst->data.error.message = event->data.error.message
                ? talloc_strdup(arr->items, event->data.error.message)
                : NULL;
            break;
    }

    arr->count++;
    return OK(NULL);
}

static void setup(void)
{
    test_ctx = talloc_new(NULL);
    events = talloc_zero(test_ctx, event_array_t);
    events->items = NULL;
    events->count = 0;
    events->capacity = 0;
}

static void teardown(void)
{
    talloc_free(test_ctx);
}

/* ================================================================
 * Coverage Tests for streaming_chat_delta.c
 * ================================================================ */

START_TEST(test_delta_content_non_string) {
    /* L86: content not string */
    ik_openai_chat_stream_ctx_t *sctx = ik_openai_chat_stream_ctx_create(test_ctx, stream_cb, events);

    ik_openai_chat_stream_process_data(sctx, "{\"choices\":[{\"delta\":{\"content\":123}}]}");

    ck_assert_int_eq((int)events->count, 0);
}

END_TEST

START_TEST(test_delta_content_null_string) {
    /* L88: content null */
    ik_openai_chat_stream_ctx_t *sctx = ik_openai_chat_stream_ctx_create(test_ctx, stream_cb, events);
    ik_openai_chat_stream_process_data(sctx, "{\"choices\":[{\"delta\":{\"content\":null}}]}");

    ck_assert_int_eq((int)events->count, 0);
}

END_TEST

START_TEST(test_delta_tool_calls_not_array) {
    /* L107 */
    ik_openai_chat_stream_ctx_t *sctx = ik_openai_chat_stream_ctx_create(test_ctx, stream_cb, events);

    ik_openai_chat_stream_process_data(sctx, "{\"choices\":[{\"delta\":{\"tool_calls\":\"not_array\"}}]}");
    ck_assert_int_eq((int)events->count, 0);
}

END_TEST

START_TEST(test_delta_tool_calls_empty_array) {
    /* L109 */
    ik_openai_chat_stream_ctx_t *sctx = ik_openai_chat_stream_ctx_create(test_ctx, stream_cb, events);

    ik_openai_chat_stream_process_data(sctx, "{\"choices\":[{\"delta\":{\"tool_calls\":[]}}]}");
    ck_assert_int_eq((int)events->count, 0);
}

END_TEST

START_TEST(test_delta_tool_call_null) {
    /* L112 */
    ik_openai_chat_stream_ctx_t *sctx = ik_openai_chat_stream_ctx_create(test_ctx, stream_cb, events);

    ik_openai_chat_stream_process_data(sctx, "{\"choices\":[{\"delta\":{\"tool_calls\":[null]}}]}");
    ck_assert_int_eq((int)events->count, 0);
}

END_TEST

START_TEST(test_delta_tool_call_not_object) {
    /* L112 */
    ik_openai_chat_stream_ctx_t *sctx = ik_openai_chat_stream_ctx_create(test_ctx, stream_cb, events);

    ik_openai_chat_stream_process_data(sctx, "{\"choices\":[{\"delta\":{\"tool_calls\":[\"not_object\"]}}]}");
    ck_assert_int_eq((int)events->count, 0);
}

END_TEST

START_TEST(test_delta_tool_call_index_null) {
    /* L116 */
    ik_openai_chat_stream_ctx_t *sctx = ik_openai_chat_stream_ctx_create(test_ctx, stream_cb, events);

    ik_openai_chat_stream_process_data(sctx,
                                       "{\"choices\":[{\"delta\":{\"tool_calls\":[{\"id\":\"tc1\",\"function\":{\"name\":\"test\"}}]}}]}");
    ck_assert_int_ge((int)events->count, 0);
}

END_TEST

START_TEST(test_delta_tool_call_index_not_int) {
    /* L116 */
    ik_openai_chat_stream_ctx_t *sctx = ik_openai_chat_stream_ctx_create(test_ctx, stream_cb, events);

    ik_openai_chat_stream_process_data(sctx,
                                       "{\"choices\":[{\"delta\":{\"tool_calls\":[{\"index\":\"not_int\",\"id\":\"tc1\",\"function\":{\"name\":\"test\"}}]}}]}");
    ck_assert_int_ge((int)events->count, 0);
}

END_TEST

START_TEST(test_delta_tool_call_missing_id_or_function) {
    /* L126 */
    ik_openai_chat_stream_ctx_t *sctx = ik_openai_chat_stream_ctx_create(test_ctx, stream_cb, events);

    ik_openai_chat_stream_process_data(sctx,
                                       "{\"choices\":[{\"delta\":{\"tool_calls\":[{\"index\":1,\"function\":{\"name\":\"test\"}}]}}]}");
    ck_assert_int_eq((int)events->count, 0);
}

END_TEST

START_TEST(test_delta_tool_call_missing_function) {
    /* L126 */
    ik_openai_chat_stream_ctx_t *sctx = ik_openai_chat_stream_ctx_create(test_ctx, stream_cb, events);

    ik_openai_chat_stream_process_data(sctx,
                                       "{\"choices\":[{\"delta\":{\"tool_calls\":[{\"index\":1,\"id\":\"tc1\"}]}}]}");
    ck_assert_int_eq((int)events->count, 0);
}

END_TEST

START_TEST(test_delta_tool_call_function_not_object) {
    /* L129 */
    ik_openai_chat_stream_ctx_t *sctx = ik_openai_chat_stream_ctx_create(test_ctx, stream_cb, events);

    ik_openai_chat_stream_process_data(sctx,
                                       "{\"choices\":[{\"delta\":{\"tool_calls\":[{\"index\":1,\"id\":\"tc1\",\"function\":\"not_object\"}]}}]}");
    ck_assert_int_eq((int)events->count, 0);
}

END_TEST

START_TEST(test_delta_tool_call_null_id_string) {
    /* L134 */
    ik_openai_chat_stream_ctx_t *sctx = ik_openai_chat_stream_ctx_create(test_ctx, stream_cb, events);

    ik_openai_chat_stream_process_data(sctx,
                                       "{\"choices\":[{\"delta\":{\"tool_calls\":[{\"index\":1,\"id\":null,\"function\":{\"name\":\"test\"}}]}}]}");
    ck_assert_int_eq((int)events->count, 0);
}

END_TEST

START_TEST(test_delta_tool_call_null_name_string) {
    /* L134 */
    ik_openai_chat_stream_ctx_t *sctx = ik_openai_chat_stream_ctx_create(test_ctx, stream_cb, events);

    ik_openai_chat_stream_process_data(sctx,
                                       "{\"choices\":[{\"delta\":{\"tool_calls\":[{\"index\":1,\"id\":\"tc1\",\"function\":{\"name\":null}}]}}]}");
    ck_assert_int_eq((int)events->count, 0);
}

END_TEST

START_TEST(test_delta_role_field) {
    /* Line 79-81: role_val != NULL - first chunk with role */
    ik_openai_chat_stream_ctx_t *sctx = ik_openai_chat_stream_ctx_create(test_ctx, stream_cb, events);

    ik_openai_chat_stream_process_data(sctx, "{\"choices\":[{\"delta\":{\"role\":\"assistant\"}}]}");

    ck_assert_int_eq((int)events->count, 0);
}

END_TEST

START_TEST(test_delta_content_string) {
    /* Line 86-101: content path with valid string */
    ik_openai_chat_stream_ctx_t *sctx = ik_openai_chat_stream_ctx_create(test_ctx, stream_cb, events);

    ik_openai_chat_stream_process_data(sctx, "{\"choices\":[{\"delta\":{\"content\":\"Hello\"}}]}");

    ck_assert_int_ge((int)events->count, 1);

    /* Find the TEXT_DELTA event */
    bool found_text = false;
    for (size_t i = 0; i < events->count; i++) {
        if (events->items[i].type == IK_STREAM_TEXT_DELTA) {
            found_text = true;
            ck_assert_str_eq(events->items[i].data.delta.text, "Hello");
        }
    }
    ck_assert(found_text);
}

END_TEST

START_TEST(test_delta_finish_reason) {
    /* Line 190-191: finish_reason_str != NULL */
    ik_openai_chat_stream_ctx_t *sctx = ik_openai_chat_stream_ctx_create(test_ctx, stream_cb, events);

    /* First, send some content to initialize the stream */
    const char *data1 = "{\"choices\":[{\"delta\":{\"content\":\"test\"}}]}";
    ik_openai_chat_stream_process_data(sctx, data1);

    /* Now send a chunk with finish_reason */
    const char *data2 = "{\"choices\":[{\"delta\":{},\"finish_reason\":\"stop\"}]}";
    ik_openai_chat_stream_process_data(sctx, data2);

    /* This should process without error and cover the finish_reason branch */
}

END_TEST

START_TEST(test_delta_emit_start_already_started) {
    /* Line 51: Test false branch - sctx->started already true */
    ik_openai_chat_stream_ctx_t *sctx = ik_openai_chat_stream_ctx_create(test_ctx, stream_cb, events);

    /* First content will trigger START */
    const char *data1 = "{\"choices\":[{\"delta\":{\"content\":\"First\"}}]}";
    ik_openai_chat_stream_process_data(sctx, data1);

    size_t first_count = events->count;

    /* Second content should NOT emit another START */
    const char *data2 = "{\"choices\":[{\"delta\":{\"content\":\"Second\"}}]}";
    ik_openai_chat_stream_process_data(sctx, data2);

    ck_assert_int_gt((int)events->count, (int)first_count);

    /* Count START events - should only be 1 */
    size_t start_count = 0;
    for (size_t i = 0; i < events->count; i++) {
        if (events->items[i].type == IK_STREAM_START) {
            start_count++;
        }
    }
    ck_assert_int_eq((int)start_count, 1);
}

END_TEST

START_TEST(test_delta_tool_call_arguments_delta) {
    /* Line 164-181: Test tool call arguments delta (both conditions true) */
    ik_openai_chat_stream_ctx_t *sctx = ik_openai_chat_stream_ctx_create(test_ctx, stream_cb, events);

    /* First, start a tool call */
    const char *data1 =
        "{\"choices\":[{\"delta\":{\"tool_calls\":[{\"index\":0,\"id\":\"call_123\",\"function\":{\"name\":\"get_weather\"}}]}}]}";
    ik_openai_chat_stream_process_data(sctx, data1);

    size_t count_after_start = events->count;

    /* Now send arguments delta */
    const char *data2 =
        "{\"choices\":[{\"delta\":{\"tool_calls\":[{\"index\":0,\"function\":{\"arguments\":\"{\\\"city\\\":\"}}]}}]}";
    ik_openai_chat_stream_process_data(sctx, data2);

    ck_assert_int_gt((int)events->count, (int)count_after_start);

    /* Find TOOL_CALL_DELTA event */
    bool found_delta = false;
    for (size_t i = 0; i < events->count; i++) {
        if (events->items[i].type == IK_STREAM_TOOL_CALL_DELTA) {
            found_delta = true;
            ck_assert_str_eq(events->items[i].data.tool_delta.arguments, "{\"city\":");
        }
    }
    ck_assert(found_delta);
}

END_TEST

START_TEST(test_delta_tool_call_then_text) {
    /* Line 53 + 90: Test ending tool call when text comes in */
    ik_openai_chat_stream_ctx_t *sctx = ik_openai_chat_stream_ctx_create(test_ctx, stream_cb, events);

    /* Start a tool call */
    const char *data1 =
        "{\"choices\":[{\"delta\":{\"tool_calls\":[{\"index\":0,\"id\":\"call_123\",\"function\":{\"name\":\"test\"}}]}}]}";
    ik_openai_chat_stream_process_data(sctx, data1);

    /* Send arguments to ensure we're in_tool_call */
    const char *data2 =
        "{\"choices\":[{\"delta\":{\"tool_calls\":[{\"index\":0,\"function\":{\"arguments\":\"{}\"}}]}}]}";
    ik_openai_chat_stream_process_data(sctx, data2);

    /* Now send text content - this should end the tool call */
    const char *data3 = "{\"choices\":[{\"delta\":{\"content\":\"Some text\"}}]}";
    ik_openai_chat_stream_process_data(sctx, data3);

    bool found_done = false;
    bool found_text = false;
    size_t done_index = 0;
    size_t text_index = 0;

    for (size_t i = 0; i < events->count; i++) {
        if (events->items[i].type == IK_STREAM_TOOL_CALL_DONE) {
            found_done = true;
            done_index = i;
        }
        if (events->items[i].type == IK_STREAM_TEXT_DELTA) {
            found_text = true;
            text_index = i;
        }
    }

    ck_assert(found_done);
    ck_assert(found_text);
    ck_assert_int_lt((int)done_index, (int)text_index);
}

END_TEST

/* ================================================================
 * Test Suite
 * ================================================================ */

static Suite *streaming_chat_delta_coverage_suite(void)
{
    Suite *s = suite_create("OpenAI Streaming Chat Delta Coverage");

    TCase *tc_content = tcase_create("ContentEdgeCases");
    tcase_set_timeout(tc_content, 30);
    tcase_add_checked_fixture(tc_content, setup, teardown);
    tcase_add_test(tc_content, test_delta_content_non_string);
    tcase_add_test(tc_content, test_delta_content_null_string);
    tcase_add_test(tc_content, test_delta_content_string);
    tcase_add_test(tc_content, test_delta_role_field);
    tcase_add_test(tc_content, test_delta_finish_reason);
    tcase_add_test(tc_content, test_delta_emit_start_already_started);
    suite_add_tcase(s, tc_content);

    TCase *tc_tool_calls = tcase_create("ToolCallsEdgeCases");
    tcase_set_timeout(tc_tool_calls, 30);
    tcase_add_checked_fixture(tc_tool_calls, setup, teardown);
    tcase_add_test(tc_tool_calls, test_delta_tool_calls_not_array);
    tcase_add_test(tc_tool_calls, test_delta_tool_calls_empty_array);
    tcase_add_test(tc_tool_calls, test_delta_tool_call_null);
    tcase_add_test(tc_tool_calls, test_delta_tool_call_not_object);
    tcase_add_test(tc_tool_calls, test_delta_tool_call_index_null);
    tcase_add_test(tc_tool_calls, test_delta_tool_call_index_not_int);
    tcase_add_test(tc_tool_calls, test_delta_tool_call_missing_id_or_function);
    tcase_add_test(tc_tool_calls, test_delta_tool_call_missing_function);
    tcase_add_test(tc_tool_calls, test_delta_tool_call_function_not_object);
    tcase_add_test(tc_tool_calls, test_delta_tool_call_null_id_string);
    tcase_add_test(tc_tool_calls, test_delta_tool_call_null_name_string);
    tcase_add_test(tc_tool_calls, test_delta_tool_call_arguments_delta);
    tcase_add_test(tc_tool_calls, test_delta_tool_call_then_text);
    suite_add_tcase(s, tc_tool_calls);

    return s;
}

int main(void)
{
    Suite *s = streaming_chat_delta_coverage_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
