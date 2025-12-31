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

START_TEST(test_delta_arguments_edge_cases)
{
    /* L162/164/166: function_val NULL, arguments not string/null, not in_tool_call */
    ik_openai_chat_stream_ctx_t *sctx = ik_openai_chat_stream_ctx_create(test_ctx, stream_cb, events);

    ik_openai_chat_stream_process_data(sctx, "{\"choices\":[{\"delta\":{\"tool_calls\":[{\"index\":0,\"id\":\"tc1\",\"function\":{\"name\":\"test\"}}]}}]}");
    ik_openai_chat_stream_process_data(sctx, "{\"choices\":[{\"delta\":{\"tool_calls\":[{\"index\":0}]}}]}");
    ck_assert_int_ge((int)events->count, 1);

    ik_openai_chat_stream_process_data(sctx, "{\"choices\":[{\"delta\":{\"tool_calls\":[{\"index\":0,\"function\":{\"arguments\":123}}]}}]}");

    ik_openai_chat_stream_process_data(sctx, "{\"choices\":[{\"delta\":{\"tool_calls\":[{\"index\":0,\"function\":{\"arguments\":null}}]}}]}");

    talloc_free(sctx);
    sctx = ik_openai_chat_stream_ctx_create(test_ctx, stream_cb, events);
    events->count = 0;
    ik_openai_chat_stream_process_data(sctx, "{\"choices\":[{\"delta\":{\"tool_calls\":[{\"index\":0,\"function\":{\"arguments\":\"test\"}}]}}]}");
    ck_assert_int_eq((int)events->count, 0);
}

END_TEST

START_TEST(test_delta_string_type_mismatches)
{
    /* L130/131: id/name not string, L169: empty arguments, L86: no content */
    ik_openai_chat_stream_ctx_t *sctx = ik_openai_chat_stream_ctx_create(test_ctx, stream_cb, events);

    ik_openai_chat_stream_process_data(sctx, "{\"choices\":[{\"delta\":{\"tool_calls\":[{\"index\":1,\"id\":123,\"function\":{\"name\":\"test\"}}]}}]}");
    ck_assert_int_eq((int)events->count, 0);

    ik_openai_chat_stream_process_data(sctx, "{\"choices\":[{\"delta\":{\"tool_calls\":[{\"index\":1,\"id\":\"tc1\",\"function\":{\"name\":456}}]}}]}");
    ck_assert_int_eq((int)events->count, 0);

    const char *start = "{\"choices\":[{\"delta\":{\"tool_calls\":[{\"index\":0,\"id\":\"tc1\",\"function\":{\"name\":\"test\"}}]}}]}";
    ik_openai_chat_stream_process_data(sctx, start);
    ik_openai_chat_stream_process_data(sctx, "{\"choices\":[{\"delta\":{\"tool_calls\":[{\"index\":0,\"function\":{\"arguments\":\"\"}}]}}]}");
    ck_assert_int_ge((int)events->count, 1);

    events->count = 0;
    ik_openai_chat_stream_process_data(sctx, "{\"choices\":[{\"delta\":{}}]}");
    ck_assert_int_eq((int)events->count, 0);
}

END_TEST

START_TEST(test_delta_same_tool_call_index)
{
    /* Line 121: tc_index == sctx->tool_call_index (same index, not new tool call) */
    ik_openai_chat_stream_ctx_t *sctx = ik_openai_chat_stream_ctx_create(
        test_ctx, stream_cb, events);

    /* Start a tool call with index 0 */
    const char *data1 = "{\"choices\":[{\"delta\":{\"tool_calls\":[{\"index\":0,\"id\":\"tc1\",\"function\":{\"name\":\"test\"}}]}}]}";
    ik_openai_chat_stream_process_data(sctx, data1);

    size_t count_after_start = events->count;

    /* Send another delta with same index 0 (continuation, not new call) */
    const char *data2 = "{\"choices\":[{\"delta\":{\"tool_calls\":[{\"index\":0,\"function\":{\"arguments\":\"{}\"}}]}}]}";
    ik_openai_chat_stream_process_data(sctx, data2);

    /* Should emit arguments delta but not another START */
    ck_assert_int_gt((int)events->count, (int)count_after_start);

    /* Count START events - should only be 1 */
    size_t start_count = 0;
    for (size_t i = 0; i < events->count; i++) {
        if (events->items[i].type == IK_STREAM_TOOL_CALL_START) {
            start_count++;
        }
    }
    ck_assert_int_eq((int)start_count, 1);
}

END_TEST

START_TEST(test_delta_function_not_object_on_arguments)
{
    /* Line 162: function_val is not an object when checking arguments */
    ik_openai_chat_stream_ctx_t *sctx = ik_openai_chat_stream_ctx_create(
        test_ctx, stream_cb, events);

    /* Send delta with function as non-object */
    const char *data = "{\"choices\":[{\"delta\":{\"tool_calls\":[{\"index\":0,\"function\":\"not_object\"}]}}]}";
    ik_openai_chat_stream_process_data(sctx, data);

    /* Should not crash */
    ck_assert_int_eq((int)events->count, 0);
}

END_TEST

START_TEST(test_delta_new_tool_call_missing_fields)
{
    /* L126/131: new tool call missing id or name */
    ik_openai_chat_stream_ctx_t *sctx = ik_openai_chat_stream_ctx_create(test_ctx, stream_cb, events);

    ik_openai_chat_stream_process_data(sctx, "{\"choices\":[{\"delta\":{\"tool_calls\":[{\"index\":0,\"id\":\"tc0\",\"function\":{\"name\":\"test0\"}}]}}]}");
    ik_openai_chat_stream_process_data(sctx, "{\"choices\":[{\"delta\":{\"tool_calls\":[{\"index\":1,\"function\":{\"name\":\"test1\"}}]}}]}");
    ck_assert_int_ge((int)events->count, 1);

    ik_openai_chat_stream_process_data(sctx, "{\"choices\":[{\"delta\":{\"tool_calls\":[{\"index\":2,\"id\":\"tc2\",\"function\":{}}]}}]}");
}

END_TEST

START_TEST(test_delta_multiple_tool_calls_different_indices)
{
    /* Test switching between different tool call indices */
    ik_openai_chat_stream_ctx_t *sctx = ik_openai_chat_stream_ctx_create(
        test_ctx, stream_cb, events);

    /* Start tool call at index 0 */
    const char *data1 = "{\"choices\":[{\"delta\":{\"tool_calls\":[{\"index\":0,\"id\":\"tc0\",\"function\":{\"name\":\"fn0\"}}]}}]}";
    ik_openai_chat_stream_process_data(sctx, data1);

    /* Send arguments for index 0 */
    const char *data2 = "{\"choices\":[{\"delta\":{\"tool_calls\":[{\"index\":0,\"function\":{\"arguments\":\"{\\\"a\\\":\"}}]}}]}";
    ik_openai_chat_stream_process_data(sctx, data2);

    /* Start NEW tool call at index 1 */
    const char *data3 = "{\"choices\":[{\"delta\":{\"tool_calls\":[{\"index\":1,\"id\":\"tc1\",\"function\":{\"name\":\"fn1\"}}]}}]}";
    ik_openai_chat_stream_process_data(sctx, data3);

    /* Send arguments for index 1 */
    const char *data4 = "{\"choices\":[{\"delta\":{\"tool_calls\":[{\"index\":1,\"function\":{\"arguments\":\"{\\\"b\\\":\"}}]}}]}";
    ik_openai_chat_stream_process_data(sctx, data4);

    /* Should have 2 START events and 2 DELTA events, plus 1 DONE for ending index 0 */
    ck_assert_int_ge((int)events->count, 5);
}

END_TEST

START_TEST(test_delta_malformed_types)
{
    /* Test various type mismatches: content not string (L86), tool_calls not array (L107), etc */
    ik_openai_chat_stream_ctx_t *sctx = ik_openai_chat_stream_ctx_create(test_ctx, stream_cb, events);

    ik_openai_chat_stream_process_data(sctx, "{\"choices\":[{\"delta\":{\"content\":123}}]}");
    ck_assert_int_eq((int)events->count, 0);

    ik_openai_chat_stream_process_data(sctx, "{\"choices\":[{\"delta\":{\"tool_calls\":\"not_array\"}}]}");
    ck_assert_int_eq((int)events->count, 0);

    ik_openai_chat_stream_process_data(sctx, "{\"choices\":[{\"delta\":{\"tool_calls\":[]}}]}");
    ck_assert_int_eq((int)events->count, 0);

    ik_openai_chat_stream_process_data(sctx, "{\"choices\":[{\"delta\":{\"tool_calls\":[null]}}]}");
    ck_assert_int_eq((int)events->count, 0);

    ik_openai_chat_stream_process_data(sctx, "{\"choices\":[{\"delta\":{\"tool_calls\":[\"not_object\"]}}]}");
    ck_assert_int_eq((int)events->count, 0);
}

END_TEST

START_TEST(test_delta_tool_call_malformed_function)
{
    /* L129: function_val NULL or not object for new tool call */
    ik_openai_chat_stream_ctx_t *sctx = ik_openai_chat_stream_ctx_create(test_ctx, stream_cb, events);

    const char *start = "{\"choices\":[{\"delta\":{\"tool_calls\":[{\"index\":0,\"id\":\"tc0\",\"function\":{\"name\":\"test0\"}}]}}]}";
    ik_openai_chat_stream_process_data(sctx, start);

    ik_openai_chat_stream_process_data(sctx, "{\"choices\":[{\"delta\":{\"tool_calls\":[{\"index\":1,\"id\":\"tc1\"}]}}]}");
    ik_openai_chat_stream_process_data(sctx, "{\"choices\":[{\"delta\":{\"tool_calls\":[{\"index\":2,\"id\":\"tc2\",\"function\":\"not_object\"}]}}]}");

    ck_assert_int_ge((int)events->count, 1);
}

END_TEST

START_TEST(test_delta_misc_edge_cases)
{
    /* L79 role, L116 index null/not-int, L190 no finish_reason */
    ik_openai_chat_stream_ctx_t *sctx = ik_openai_chat_stream_ctx_create(test_ctx, stream_cb, events);

    ik_openai_chat_stream_process_data(sctx, "{\"choices\":[{\"delta\":{\"role\":\"assistant\"}}]}");
    ck_assert_int_eq((int)events->count, 0);

    ik_openai_chat_stream_process_data(sctx, "{\"choices\":[{\"delta\":{\"tool_calls\":[{\"id\":\"tc1\",\"function\":{\"name\":\"test\"}}]}}]}");
    ck_assert_int_ge((int)events->count, 1);

    events->count = 0;
    ik_openai_chat_stream_process_data(sctx, "{\"choices\":[{\"delta\":{\"tool_calls\":[{\"index\":\"not_int\",\"id\":\"tc2\",\"function\":{\"name\":\"fn\"}}]}}]}");
    ck_assert_int_ge((int)events->count, 0);

    ik_openai_chat_stream_process_data(sctx, "{\"choices\":[{\"delta\":{\"content\":\"test\"}}]}");
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
    tcase_add_test(tc_arguments, test_delta_arguments_edge_cases);
    tcase_add_test(tc_arguments, test_delta_string_type_mismatches);
    tcase_add_test(tc_arguments, test_delta_same_tool_call_index);
    tcase_add_test(tc_arguments, test_delta_function_not_object_on_arguments);
    tcase_add_test(tc_arguments, test_delta_new_tool_call_missing_fields);
    tcase_add_test(tc_arguments, test_delta_multiple_tool_calls_different_indices);
    tcase_add_test(tc_arguments, test_delta_malformed_types);
    tcase_add_test(tc_arguments, test_delta_tool_call_malformed_function);
    tcase_add_test(tc_arguments, test_delta_misc_edge_cases);
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
