/**
 * @file openai_streaming_tools_test.c
 * @brief Tool call streaming tests for OpenAI Chat Completions
 *
 * Tests tool call start, delta accumulation, and multi-tool streaming.
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
 * Tool Call Streaming Tests
 * ================================================================ */

START_TEST(test_parse_tool_call_start) {
    ik_openai_chat_stream_ctx_t *sctx = ik_openai_chat_stream_ctx_create(
        test_ctx, stream_cb, events);

    /* Set model */
    const char *init_data = "{\"model\":\"gpt-4\",\"choices\":[{\"delta\":{\"role\":\"assistant\"}}]}";
    ik_openai_chat_stream_process_data(sctx, init_data);

    /* Process tool call start with id and name */
    const char *tool_start = "{\"choices\":[{\"delta\":{\"tool_calls\":["
                             "{\"index\":0,\"id\":\"call_abc\",\"type\":\"function\","
                             "\"function\":{\"name\":\"get_weather\",\"arguments\":\"\"}}"
                             "]}}]}";
    ik_openai_chat_stream_process_data(sctx, tool_start);

    /* Should emit START + TOOL_CALL_START + TOOL_CALL_DELTA (empty args) */
    ck_assert_int_eq((int)events->count, 3);
    ck_assert_int_eq(events->items[0].type, IK_STREAM_START);
    ck_assert_int_eq(events->items[1].type, IK_STREAM_TOOL_CALL_START);
    ck_assert_int_eq(events->items[1].index, 0);
    ck_assert_str_eq(events->items[1].data.tool_start.id, "call_abc");
    ck_assert_str_eq(events->items[1].data.tool_start.name, "get_weather");
    ck_assert_int_eq(events->items[2].type, IK_STREAM_TOOL_CALL_DELTA);
    ck_assert_str_eq(events->items[2].data.tool_delta.arguments, "");
}

END_TEST

START_TEST(test_parse_tool_call_arguments_delta) {
    ik_openai_chat_stream_ctx_t *sctx = ik_openai_chat_stream_ctx_create(
        test_ctx, stream_cb, events);

    /* Set model */
    const char *init_data = "{\"model\":\"gpt-4\",\"choices\":[{\"delta\":{\"role\":\"assistant\"}}]}";
    ik_openai_chat_stream_process_data(sctx, init_data);

    /* Start tool call */
    const char *tool_start = "{\"choices\":[{\"delta\":{\"tool_calls\":["
                             "{\"index\":0,\"id\":\"call_abc\",\"function\":{\"name\":\"get_weather\",\"arguments\":\"\"}}"
                             "]}}]}";
    ik_openai_chat_stream_process_data(sctx, tool_start);

    /* Process arguments delta */
    const char *args_delta = "{\"choices\":[{\"delta\":{\"tool_calls\":["
                             "{\"index\":0,\"function\":{\"arguments\":\"{\\\"lo\"}}"
                             "]}}]}";
    ik_openai_chat_stream_process_data(sctx, args_delta);

    /* Should have START + TOOL_CALL_START + TOOL_CALL_DELTA (empty) + TOOL_CALL_DELTA (content) */
    ck_assert_int_eq((int)events->count, 4);
    ck_assert_int_eq(events->items[2].type, IK_STREAM_TOOL_CALL_DELTA);
    ck_assert_str_eq(events->items[2].data.tool_delta.arguments, "");
    ck_assert_int_eq(events->items[3].type, IK_STREAM_TOOL_CALL_DELTA);
    ck_assert_str_eq(events->items[3].data.tool_delta.arguments, "{\"lo");
}

END_TEST

START_TEST(test_accumulate_tool_arguments) {
    ik_openai_chat_stream_ctx_t *sctx = ik_openai_chat_stream_ctx_create(
        test_ctx, stream_cb, events);

    /* Set model */
    const char *init_data = "{\"model\":\"gpt-4\",\"choices\":[{\"delta\":{\"role\":\"assistant\"}}]}";
    ik_openai_chat_stream_process_data(sctx, init_data);

    /* Start tool call */
    const char *tool_start = "{\"choices\":[{\"delta\":{\"tool_calls\":["
                             "{\"index\":0,\"id\":\"call_abc\",\"function\":{\"name\":\"get_weather\",\"arguments\":\"\"}}"
                             "]}}]}";
    ik_openai_chat_stream_process_data(sctx, tool_start);

    /* Process multiple argument deltas */
    ik_openai_chat_stream_process_data(sctx,
                                       "{\"choices\":[{\"delta\":{\"tool_calls\":[{\"index\":0,\"function\":{\"arguments\":\"{\\\"lo\"}}]}}]}");
    ik_openai_chat_stream_process_data(sctx,
                                       "{\"choices\":[{\"delta\":{\"tool_calls\":[{\"index\":0,\"function\":{\"arguments\":\"cation\"}}]}}]}");
    ik_openai_chat_stream_process_data(sctx,
                                       "{\"choices\":[{\"delta\":{\"tool_calls\":[{\"index\":0,\"function\":{\"arguments\":\":\\\"NYC\\\"}\"}}]}}]}");

    /* Should have START + TOOL_CALL_START + 4 TOOL_CALL_DELTA events (empty + 3 content) */
    ck_assert_int_eq((int)events->count, 6);
    ck_assert_str_eq(events->items[2].data.tool_delta.arguments, "");
    ck_assert_str_eq(events->items[3].data.tool_delta.arguments, "{\"lo");
    ck_assert_str_eq(events->items[4].data.tool_delta.arguments, "cation");
    ck_assert_str_eq(events->items[5].data.tool_delta.arguments, ":\"NYC\"}");
}

END_TEST

START_TEST(test_handle_multiple_tool_calls) {
    ik_openai_chat_stream_ctx_t *sctx = ik_openai_chat_stream_ctx_create(
        test_ctx, stream_cb, events);

    /* Set model */
    const char *init_data = "{\"model\":\"gpt-4\",\"choices\":[{\"delta\":{\"role\":\"assistant\"}}]}";
    ik_openai_chat_stream_process_data(sctx, init_data);

    /* Start first tool call (index 0) */
    ik_openai_chat_stream_process_data(sctx,
                                       "{\"choices\":[{\"delta\":{\"tool_calls\":["
                                       "{\"index\":0,\"id\":\"call_1\",\"function\":{\"name\":\"tool1\",\"arguments\":\"\"}}"
                                       "]}}]}");

    /* Add arguments to first tool */
    ik_openai_chat_stream_process_data(sctx,
                                       "{\"choices\":[{\"delta\":{\"tool_calls\":["
                                       "{\"index\":0,\"function\":{\"arguments\":\"arg1\"}}"
                                       "]}}]}");

    /* Start second tool call (index 1) - should end first tool */
    ik_openai_chat_stream_process_data(sctx,
                                       "{\"choices\":[{\"delta\":{\"tool_calls\":["
                                       "{\"index\":1,\"id\":\"call_2\",\"function\":{\"name\":\"tool2\",\"arguments\":\"\"}}"
                                       "]}}]}");

    /* Should have START + TOOL_START(0) + TOOL_DELTA(0,empty) + TOOL_DELTA(0,arg1) + TOOL_DONE(0) + TOOL_START(1) + TOOL_DELTA(1,empty) */
    ck_assert_int_eq((int)events->count, 7);
    ck_assert_int_eq(events->items[0].type, IK_STREAM_START);
    ck_assert_int_eq(events->items[1].type, IK_STREAM_TOOL_CALL_START);
    ck_assert_int_eq(events->items[1].index, 0);
    ck_assert_int_eq(events->items[2].type, IK_STREAM_TOOL_CALL_DELTA);
    ck_assert_int_eq(events->items[2].index, 0);
    ck_assert_str_eq(events->items[2].data.tool_delta.arguments, "");
    ck_assert_int_eq(events->items[3].type, IK_STREAM_TOOL_CALL_DELTA);
    ck_assert_int_eq(events->items[3].index, 0);
    ck_assert_str_eq(events->items[3].data.tool_delta.arguments, "arg1");
    ck_assert_int_eq(events->items[4].type, IK_STREAM_TOOL_CALL_DONE);
    ck_assert_int_eq(events->items[4].index, 0);
    ck_assert_int_eq(events->items[5].type, IK_STREAM_TOOL_CALL_START);
    ck_assert_int_eq(events->items[5].index, 1);
}

END_TEST

START_TEST(test_emit_tool_call_done) {
    ik_openai_chat_stream_ctx_t *sctx = ik_openai_chat_stream_ctx_create(
        test_ctx, stream_cb, events);

    /* Set model */
    const char *init_data = "{\"model\":\"gpt-4\",\"choices\":[{\"delta\":{\"role\":\"assistant\"}}]}";
    ik_openai_chat_stream_process_data(sctx, init_data);

    /* Start and complete tool call */
    ik_openai_chat_stream_process_data(sctx,
                                       "{\"choices\":[{\"delta\":{\"tool_calls\":["
                                       "{\"index\":0,\"id\":\"call_1\",\"function\":{\"name\":\"tool1\",\"arguments\":\"\"}}"
                                       "]}}]}");

    /* Process [DONE] - should end tool call before emitting DONE */
    ik_openai_chat_stream_process_data(sctx, "[DONE]");

    /* Should have START + TOOL_START + TOOL_DELTA(empty) + TOOL_DONE + STREAM_DONE */
    ck_assert_int_eq((int)events->count, 5);
    ck_assert_int_eq(events->items[2].type, IK_STREAM_TOOL_CALL_DELTA);
    ck_assert_str_eq(events->items[2].data.tool_delta.arguments, "");
    ck_assert_int_eq(events->items[3].type, IK_STREAM_TOOL_CALL_DONE);
    ck_assert_int_eq(events->items[4].type, IK_STREAM_DONE);
}

END_TEST

/* ================================================================
 * Test Suite
 * ================================================================ */

static Suite *openai_streaming_tools_suite(void)
{
    Suite *s = suite_create("OpenAI Streaming Tools");

    /* Tool Call Streaming */
    TCase *tc_tools = tcase_create("ToolCalls");
    tcase_set_timeout(tc_tools, 30);
    tcase_add_checked_fixture(tc_tools, setup, teardown);
    tcase_add_test(tc_tools, test_parse_tool_call_start);
    tcase_add_test(tc_tools, test_parse_tool_call_arguments_delta);
    tcase_add_test(tc_tools, test_accumulate_tool_arguments);
    tcase_add_test(tc_tools, test_handle_multiple_tool_calls);
    tcase_add_test(tc_tools, test_emit_tool_call_done);
    suite_add_tcase(s, tc_tools);

    return s;
}

int main(void)
{
    Suite *s = openai_streaming_tools_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
