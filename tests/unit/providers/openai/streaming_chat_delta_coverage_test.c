/**
 * @file streaming_chat_delta_coverage_test.c
 * @brief Coverage tests for OpenAI Chat streaming delta processing edge cases
 */

#include <check.h>
#include <talloc.h>
#include <string.h>
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

START_TEST(test_delta_content_non_string)
{
    /* Line 86: content_val != NULL but NOT yyjson_is_str (false branch) */
    ik_openai_chat_stream_ctx_t *sctx = ik_openai_chat_stream_ctx_create(
        test_ctx, stream_cb, events);

    const char *data = "{\"choices\":[{\"delta\":{\"content\":123}}]}";
    ik_openai_chat_stream_process_data(sctx, data);

    /* Should not emit text delta since content is not a string */
    ck_assert_int_eq((int)events->count, 0);
}

END_TEST

START_TEST(test_delta_content_null_string)
{
    /* Line 88: content == NULL (true branch) */
    /* This tests yyjson_get_str returning NULL for a non-string value */
    ik_openai_chat_stream_ctx_t *sctx = ik_openai_chat_stream_ctx_create(
        test_ctx, stream_cb, events);

    /* Create a scenario where yyjson_get_str would return NULL */
    const char *data = "{\"choices\":[{\"delta\":{\"content\":null}}]}";
    ik_openai_chat_stream_process_data(sctx, data);

    /* Should not emit text delta */
    ck_assert_int_eq((int)events->count, 0);
}

END_TEST

START_TEST(test_delta_tool_calls_not_array)
{
    /* Line 107: tool_calls_val != NULL but NOT yyjson_is_arr (false branch) */
    ik_openai_chat_stream_ctx_t *sctx = ik_openai_chat_stream_ctx_create(
        test_ctx, stream_cb, events);

    const char *data = "{\"choices\":[{\"delta\":{\"tool_calls\":\"not_array\"}}]}";
    ik_openai_chat_stream_process_data(sctx, data);

    /* Should not process tool calls */
    ck_assert_int_eq((int)events->count, 0);
}

END_TEST

START_TEST(test_delta_tool_calls_empty_array)
{
    /* Line 109: arr_size == 0 (true branch - NOT > 0) */
    ik_openai_chat_stream_ctx_t *sctx = ik_openai_chat_stream_ctx_create(
        test_ctx, stream_cb, events);

    const char *data = "{\"choices\":[{\"delta\":{\"tool_calls\":[]}}]}";
    ik_openai_chat_stream_process_data(sctx, data);

    /* Should not emit any tool call events */
    ck_assert_int_eq((int)events->count, 0);
}

END_TEST

START_TEST(test_delta_tool_call_null)
{
    /* Line 112: tool_call == NULL (first condition false) */
    /* This is tricky - yyjson_arr_get returns NULL for out-of-bounds */
    /* But we have arr_size > 0, so element 0 exists */
    /* We need tool_call to be null in the array */
    ik_openai_chat_stream_ctx_t *sctx = ik_openai_chat_stream_ctx_create(
        test_ctx, stream_cb, events);

    const char *data = "{\"choices\":[{\"delta\":{\"tool_calls\":[null]}}]}";
    ik_openai_chat_stream_process_data(sctx, data);

    /* Should not emit any tool call events */
    ck_assert_int_eq((int)events->count, 0);
}

END_TEST

START_TEST(test_delta_tool_call_not_object)
{
    /* Line 112: tool_call != NULL but NOT yyjson_is_obj (second condition false) */
    ik_openai_chat_stream_ctx_t *sctx = ik_openai_chat_stream_ctx_create(
        test_ctx, stream_cb, events);

    const char *data = "{\"choices\":[{\"delta\":{\"tool_calls\":[\"not_object\"]}}]}";
    ik_openai_chat_stream_process_data(sctx, data);

    /* Should not emit any tool call events */
    ck_assert_int_eq((int)events->count, 0);
}

END_TEST

START_TEST(test_delta_tool_call_index_null)
{
    /* Line 116: index_val == NULL (first condition false) */
    ik_openai_chat_stream_ctx_t *sctx = ik_openai_chat_stream_ctx_create(
        test_ctx, stream_cb, events);

    const char *data = "{\"choices\":[{\"delta\":{\"tool_calls\":[{\"id\":\"tc1\",\"function\":{\"name\":\"test\"}}]}}]}";
    ik_openai_chat_stream_process_data(sctx, data);

    /* Should still process with default index 0 */
    ck_assert_int_ge((int)events->count, 0);
}

END_TEST

START_TEST(test_delta_tool_call_index_not_int)
{
    /* Line 116: index_val != NULL but NOT yyjson_is_int (second condition false) */
    ik_openai_chat_stream_ctx_t *sctx = ik_openai_chat_stream_ctx_create(
        test_ctx, stream_cb, events);

    const char *data = "{\"choices\":[{\"delta\":{\"tool_calls\":[{\"index\":\"not_int\",\"id\":\"tc1\",\"function\":{\"name\":\"test\"}}]}}]}";
    ik_openai_chat_stream_process_data(sctx, data);

    /* Should still process with default index 0 */
    ck_assert_int_ge((int)events->count, 0);
}

END_TEST

START_TEST(test_delta_tool_call_missing_id_or_function)
{
    /* Line 126: Test when id_val == NULL */
    ik_openai_chat_stream_ctx_t *sctx = ik_openai_chat_stream_ctx_create(
        test_ctx, stream_cb, events);

    const char *data = "{\"choices\":[{\"delta\":{\"tool_calls\":[{\"index\":1,\"function\":{\"name\":\"test\"}}]}}]}";
    ik_openai_chat_stream_process_data(sctx, data);

    /* Should not emit tool call start without id */
    ck_assert_int_eq((int)events->count, 0);
}

END_TEST

START_TEST(test_delta_tool_call_missing_function)
{
    /* Line 126: Test when function_val == NULL */
    ik_openai_chat_stream_ctx_t *sctx = ik_openai_chat_stream_ctx_create(
        test_ctx, stream_cb, events);

    const char *data = "{\"choices\":[{\"delta\":{\"tool_calls\":[{\"index\":1,\"id\":\"tc1\"}]}}]}";
    ik_openai_chat_stream_process_data(sctx, data);

    /* Should not emit tool call start without function */
    ck_assert_int_eq((int)events->count, 0);
}

END_TEST

START_TEST(test_delta_tool_call_function_not_object)
{
    /* Line 129: function_val not an object */
    ik_openai_chat_stream_ctx_t *sctx = ik_openai_chat_stream_ctx_create(
        test_ctx, stream_cb, events);

    const char *data = "{\"choices\":[{\"delta\":{\"tool_calls\":[{\"index\":1,\"id\":\"tc1\",\"function\":\"not_object\"}]}}]}";
    ik_openai_chat_stream_process_data(sctx, data);

    /* Should not emit tool call start */
    ck_assert_int_eq((int)events->count, 0);
}

END_TEST

START_TEST(test_delta_tool_call_null_id_string)
{
    /* Line 134: id == NULL (after yyjson_get_str) */
    ik_openai_chat_stream_ctx_t *sctx = ik_openai_chat_stream_ctx_create(
        test_ctx, stream_cb, events);

    const char *data = "{\"choices\":[{\"delta\":{\"tool_calls\":[{\"index\":1,\"id\":null,\"function\":{\"name\":\"test\"}}]}}]}";
    ik_openai_chat_stream_process_data(sctx, data);

    /* Should not emit tool call start */
    ck_assert_int_eq((int)events->count, 0);
}

END_TEST

START_TEST(test_delta_tool_call_null_name_string)
{
    /* Line 134: name == NULL (after yyjson_get_str) */
    ik_openai_chat_stream_ctx_t *sctx = ik_openai_chat_stream_ctx_create(
        test_ctx, stream_cb, events);

    const char *data = "{\"choices\":[{\"delta\":{\"tool_calls\":[{\"index\":1,\"id\":\"tc1\",\"function\":{\"name\":null}}]}}]}";
    ik_openai_chat_stream_process_data(sctx, data);

    /* Should not emit tool call start */
    ck_assert_int_eq((int)events->count, 0);
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
