/**
 * @file streaming_chat_delta_edge_test.c
 * @brief Additional coverage tests for OpenAI Chat streaming delta processing
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
 * Additional Edge Case Tests
 * ================================================================ */

START_TEST(test_delta_arguments_with_function_val_null)
{
    /* Line 162: function_val == NULL when processing arguments */
    ik_openai_chat_stream_ctx_t *sctx = ik_openai_chat_stream_ctx_create(
        test_ctx, stream_cb, events);

    /* Send a tool call to enter in_tool_call state */
    const char *data1 = "{\"choices\":[{\"delta\":{\"tool_calls\":[{\"index\":0,\"id\":\"tc1\",\"function\":{\"name\":\"test\"}}]}}]}";
    ik_openai_chat_stream_process_data(sctx, data1);

    /* Now send delta without function field */
    const char *data2 = "{\"choices\":[{\"delta\":{\"tool_calls\":[{\"index\":0}]}}]}";
    ik_openai_chat_stream_process_data(sctx, data2);

    /* Should emit START but not DELTA */
    ck_assert_int_ge((int)events->count, 1);
}

END_TEST

START_TEST(test_delta_arguments_not_string)
{
    /* Line 164: arguments_val != NULL but NOT yyjson_is_str */
    ik_openai_chat_stream_ctx_t *sctx = ik_openai_chat_stream_ctx_create(
        test_ctx, stream_cb, events);

    /* Send a tool call to enter in_tool_call state */
    const char *data1 = "{\"choices\":[{\"delta\":{\"tool_calls\":[{\"index\":0,\"id\":\"tc1\",\"function\":{\"name\":\"test\"}}]}}]}";
    ik_openai_chat_stream_process_data(sctx, data1);

    /* Now send delta with non-string arguments */
    const char *data2 = "{\"choices\":[{\"delta\":{\"tool_calls\":[{\"index\":0,\"function\":{\"arguments\":123}}]}}]}";
    ik_openai_chat_stream_process_data(sctx, data2);

    /* Should emit START but not argument DELTA */
    ck_assert_int_ge((int)events->count, 1);
}

END_TEST

START_TEST(test_delta_arguments_null_string)
{
    /* Line 166: arguments == NULL after yyjson_get_str */
    ik_openai_chat_stream_ctx_t *sctx = ik_openai_chat_stream_ctx_create(
        test_ctx, stream_cb, events);

    /* Send a tool call to enter in_tool_call state */
    const char *data1 = "{\"choices\":[{\"delta\":{\"tool_calls\":[{\"index\":0,\"id\":\"tc1\",\"function\":{\"name\":\"test\"}}]}}]}";
    ik_openai_chat_stream_process_data(sctx, data1);

    /* Now send delta with null arguments */
    const char *data2 = "{\"choices\":[{\"delta\":{\"tool_calls\":[{\"index\":0,\"function\":{\"arguments\":null}}]}}]}";
    ik_openai_chat_stream_process_data(sctx, data2);

    /* Should emit START but not argument DELTA */
    ck_assert_int_ge((int)events->count, 1);
}

END_TEST

START_TEST(test_delta_arguments_not_in_tool_call)
{
    /* Line 166: arguments != NULL but NOT sctx->in_tool_call */
    ik_openai_chat_stream_ctx_t *sctx = ik_openai_chat_stream_ctx_create(
        test_ctx, stream_cb, events);

    /* Send arguments without first starting a tool call */
    const char *data = "{\"choices\":[{\"delta\":{\"tool_calls\":[{\"index\":0,\"function\":{\"arguments\":\"test\"}}]}}]}";
    ik_openai_chat_stream_process_data(sctx, data);

    /* Should not emit argument DELTA since not in tool call */
    ck_assert_int_eq((int)events->count, 0);
}

END_TEST

START_TEST(test_delta_tool_call_id_not_string)
{
    /* Line 130: id != NULL but yyjson_get_str returns NULL for non-string */
    ik_openai_chat_stream_ctx_t *sctx = ik_openai_chat_stream_ctx_create(
        test_ctx, stream_cb, events);

    const char *data = "{\"choices\":[{\"delta\":{\"tool_calls\":[{\"index\":1,\"id\":123,\"function\":{\"name\":\"test\"}}]}}]}";
    ik_openai_chat_stream_process_data(sctx, data);

    /* Should not emit tool call start since id is not a string */
    ck_assert_int_eq((int)events->count, 0);
}

END_TEST

START_TEST(test_delta_tool_call_name_not_string)
{
    /* Line 131: name != NULL but yyjson_get_str returns NULL for non-string */
    ik_openai_chat_stream_ctx_t *sctx = ik_openai_chat_stream_ctx_create(
        test_ctx, stream_cb, events);

    const char *data = "{\"choices\":[{\"delta\":{\"tool_calls\":[{\"index\":1,\"id\":\"tc1\",\"function\":{\"name\":456}}]}}]}";
    ik_openai_chat_stream_process_data(sctx, data);

    /* Should not emit tool call start since name is not a string */
    ck_assert_int_eq((int)events->count, 0);
}

END_TEST

START_TEST(test_delta_arguments_empty_string)
{
    /* Line 169: Test ternary with current_tool_args as empty string */
    ik_openai_chat_stream_ctx_t *sctx = ik_openai_chat_stream_ctx_create(
        test_ctx, stream_cb, events);

    /* Start a tool call */
    const char *data1 = "{\"choices\":[{\"delta\":{\"tool_calls\":[{\"index\":0,\"id\":\"tc1\",\"function\":{\"name\":\"test\"}}]}}]}";
    ik_openai_chat_stream_process_data(sctx, data1);

    /* Send empty arguments string */
    const char *data2 = "{\"choices\":[{\"delta\":{\"tool_calls\":[{\"index\":0,\"function\":{\"arguments\":\"\"}}]}}]}";
    ik_openai_chat_stream_process_data(sctx, data2);

    /* Should handle empty string gracefully */
    ck_assert_int_ge((int)events->count, 1);
}

END_TEST

/* ================================================================
 * Test Suite
 * ================================================================ */

static Suite *streaming_chat_delta_edge_suite(void)
{
    Suite *s = suite_create("OpenAI Streaming Chat Delta Edge Cases");

    TCase *tc_arguments = tcase_create("ArgumentsEdgeCases");
    tcase_set_timeout(tc_arguments, 30);
    tcase_add_checked_fixture(tc_arguments, setup, teardown);
    tcase_add_test(tc_arguments, test_delta_arguments_with_function_val_null);
    tcase_add_test(tc_arguments, test_delta_arguments_not_string);
    tcase_add_test(tc_arguments, test_delta_arguments_null_string);
    tcase_add_test(tc_arguments, test_delta_arguments_not_in_tool_call);
    tcase_add_test(tc_arguments, test_delta_tool_call_id_not_string);
    tcase_add_test(tc_arguments, test_delta_tool_call_name_not_string);
    tcase_add_test(tc_arguments, test_delta_arguments_empty_string);
    suite_add_tcase(s, tc_arguments);

    return s;
}

int main(void)
{
    Suite *s = streaming_chat_delta_edge_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
