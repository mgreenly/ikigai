/**
 * @file openai_streaming_responses_events_test.c
 * @brief Tests for OpenAI Responses API event processing edge cases
 */

#include <check.h>
#include <talloc.h>
#include <string.h>
#include "providers/openai/streaming.h"
#include "providers/openai/streaming_responses_internal.h"
#include "providers/provider.h"

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

START_TEST(test_invalid_json)
{
    ik_openai_responses_stream_ctx_t *ctx = ik_openai_responses_stream_ctx_create(
        test_ctx, stream_cb, events);

    ik_openai_responses_stream_process_event(ctx, "response.created", "invalid json");
    ck_assert_int_eq((int)events->count, 0);

    ik_openai_responses_stream_process_event(ctx, "response.created", "[]");
    ck_assert_int_eq((int)events->count, 0);
}
END_TEST

START_TEST(test_response_created_edge_cases)
{
    ik_openai_responses_stream_ctx_t *ctx = ik_openai_responses_stream_ctx_create(
        test_ctx, stream_cb, events);

    ik_openai_responses_stream_process_event(ctx, "response.created", "{}");
    ck_assert_int_eq((int)events->count, 1);
    ck_assert_int_eq(events->items[0].type, IK_STREAM_START);

    talloc_free(ctx);
    events->count = 0;
    ctx = ik_openai_responses_stream_ctx_create(test_ctx, stream_cb, events);
    ik_openai_responses_stream_process_event(ctx, "response.created", "{\"response\":\"not an object\"}");
    ck_assert_int_eq((int)events->count, 1);

    talloc_free(ctx);
    events->count = 0;
    ctx = ik_openai_responses_stream_ctx_create(test_ctx, stream_cb, events);
    ik_openai_responses_stream_process_event(ctx, "response.created", "{\"response\":{}}");
    ck_assert_int_eq((int)events->count, 1);

    talloc_free(ctx);
    events->count = 0;
    ctx = ik_openai_responses_stream_ctx_create(test_ctx, stream_cb, events);
    ik_openai_responses_stream_process_event(ctx, "response.created", "{\"response\":{\"model\":null}}");
    ck_assert_int_eq((int)events->count, 1);
}
END_TEST

START_TEST(test_text_delta_edge_cases)
{
    ik_openai_responses_stream_ctx_t *ctx = ik_openai_responses_stream_ctx_create(
        test_ctx, stream_cb, events);

    ik_openai_responses_stream_process_event(ctx, "response.output_text.delta", "{}");
    ck_assert_int_eq((int)events->count, 0);

    ik_openai_responses_stream_process_event(ctx, "response.output_text.delta", "{\"delta\":123}");
    ck_assert_int_eq((int)events->count, 0);

    ik_openai_responses_stream_process_event(ctx, "response.output_text.delta", "{\"delta\":null}");
    ck_assert_int_eq((int)events->count, 0);

    ik_openai_responses_stream_process_event(ctx, "response.output_text.delta", "{\"delta\":\"text\"}");
    ck_assert_int_eq((int)events->count, 2);
    ck_assert_int_eq(events->items[1].index, 0);

    events->count = 0;
    ik_openai_responses_stream_process_event(ctx, "response.output_text.delta",
        "{\"delta\":\"text\",\"content_index\":\"not an int\"}");
    ck_assert_int_eq(events->items[0].index, 0);
}
END_TEST

START_TEST(test_thinking_delta_edge_cases)
{
    ik_openai_responses_stream_ctx_t *ctx = ik_openai_responses_stream_ctx_create(
        test_ctx, stream_cb, events);

    ik_openai_responses_stream_process_event(ctx, "response.reasoning_summary_text.delta", "{}");
    ck_assert_int_eq((int)events->count, 0);

    ik_openai_responses_stream_process_event(ctx, "response.reasoning_summary_text.delta", "{\"delta\":123}");
    ck_assert_int_eq((int)events->count, 0);

    ik_openai_responses_stream_process_event(ctx, "response.reasoning_summary_text.delta", "{\"delta\":null}");
    ck_assert_int_eq((int)events->count, 0);

    ik_openai_responses_stream_process_event(ctx, "response.reasoning_summary_text.delta", "{\"delta\":\"thinking\"}");
    ck_assert_int_eq((int)events->count, 2);
    ck_assert_int_eq(events->items[1].type, IK_STREAM_THINKING_DELTA);
    ck_assert_int_eq(events->items[1].index, 0);

    events->count = 0;
    ik_openai_responses_stream_process_event(ctx, "response.reasoning_summary_text.delta",
        "{\"delta\":\"thinking\",\"summary_index\":\"not an int\"}");
    ck_assert_int_eq(events->items[0].index, 0);
}
END_TEST

START_TEST(test_output_item_added_edge_cases)
{
    ik_openai_responses_stream_ctx_t *ctx = ik_openai_responses_stream_ctx_create(
        test_ctx, stream_cb, events);

    ik_openai_responses_stream_process_event(ctx, "response.output_item.added", "{}");
    ck_assert_int_eq((int)events->count, 0);

    ik_openai_responses_stream_process_event(ctx, "response.output_item.added", "{\"item\":\"not an object\"}");
    ck_assert_int_eq((int)events->count, 0);

    ik_openai_responses_stream_process_event(ctx, "response.output_item.added", "{\"item\":{\"type\":null}}");
    ck_assert_int_eq((int)events->count, 0);

    ik_openai_responses_stream_process_event(ctx, "response.output_item.added", "{\"item\":{\"type\":\"text\"}}");
    ck_assert_int_eq((int)events->count, 0);

    ik_openai_responses_stream_process_event(ctx, "response.output_item.added",
        "{\"item\":{\"type\":\"function_call\",\"call_id\":\"call_123\",\"name\":\"test\"}}");
    ck_assert_int_eq((int)events->count, 2);
    ck_assert_int_eq(events->items[1].index, 0);

    events->count = 0;
    ik_openai_responses_stream_process_event(ctx, "response.output_item.added",
        "{\"item\":{\"type\":\"function_call\",\"call_id\":\"call_123\",\"name\":\"test\"},\"output_index\":\"not an int\"}");
    ck_assert_int_eq(events->items[0].index, 0);

    events->count = 0;
    ik_openai_responses_stream_process_event(ctx, "response.output_item.added",
        "{\"item\":{\"type\":\"function_call\",\"call_id\":null,\"name\":\"test\"}}");
    ck_assert_int_eq((int)events->count, 0);

    ik_openai_responses_stream_process_event(ctx, "response.output_item.added",
        "{\"item\":{\"type\":\"function_call\",\"call_id\":\"call_123\",\"name\":null}}");
    ck_assert_int_eq((int)events->count, 0);
}
END_TEST

START_TEST(test_output_item_added_ends_previous_tool_call)
{
    ik_openai_responses_stream_ctx_t *ctx = ik_openai_responses_stream_ctx_create(
        test_ctx, stream_cb, events);

    ik_openai_responses_stream_process_event(ctx, "response.output_item.added",
        "{\"item\":{\"type\":\"function_call\",\"call_id\":\"call_1\",\"name\":\"test1\"},\"output_index\":0}");
    ck_assert_int_eq((int)events->count, 2);

    ik_openai_responses_stream_process_event(ctx, "response.output_item.added",
        "{\"item\":{\"type\":\"function_call\",\"call_id\":\"call_2\",\"name\":\"test2\"},\"output_index\":1}");
    ck_assert_int_eq((int)events->count, 4);
    ck_assert_int_eq(events->items[2].type, IK_STREAM_TOOL_CALL_DONE);
    ck_assert_int_eq(events->items[3].type, IK_STREAM_TOOL_CALL_START);
}
END_TEST

START_TEST(test_function_call_arguments_delta_edge_cases)
{
    ik_openai_responses_stream_ctx_t *ctx = ik_openai_responses_stream_ctx_create(
        test_ctx, stream_cb, events);

    ik_openai_responses_stream_process_event(ctx, "response.function_call_arguments.delta", "{}");
    ck_assert_int_eq((int)events->count, 0);

    ik_openai_responses_stream_process_event(ctx, "response.function_call_arguments.delta", "{\"delta\":123}");
    ck_assert_int_eq((int)events->count, 0);

    ik_openai_responses_stream_process_event(ctx, "response.function_call_arguments.delta", "{\"delta\":null}");
    ck_assert_int_eq((int)events->count, 0);

    ik_openai_responses_stream_process_event(ctx, "response.function_call_arguments.delta", "{\"delta\":\"{}\"}");
    ck_assert_int_eq((int)events->count, 0);

    ik_openai_responses_stream_process_event(ctx, "response.output_item.added",
        "{\"item\":{\"type\":\"function_call\",\"call_id\":\"call_1\",\"name\":\"test\"},\"output_index\":5}");
    ik_openai_responses_stream_process_event(ctx, "response.function_call_arguments.delta", "{\"delta\":\"{}\"}");
    ck_assert_int_eq((int)events->count, 3);
    ck_assert_int_eq(events->items[2].index, 5);

    events->count = 2;
    ik_openai_responses_stream_process_event(ctx, "response.function_call_arguments.delta",
        "{\"delta\":\"{}\",\"output_index\":\"not an int\"}");
    ck_assert_int_eq(events->items[2].index, 5);
}
END_TEST

START_TEST(test_function_call_arguments_done_is_noop)
{
    ik_openai_responses_stream_ctx_t *ctx = ik_openai_responses_stream_ctx_create(
        test_ctx, stream_cb, events);

    ik_openai_responses_stream_process_event(ctx, "response.function_call_arguments.done", "{}");
    ck_assert_int_eq((int)events->count, 0);
}
END_TEST

START_TEST(test_output_item_done_edge_cases)
{
    ik_openai_responses_stream_ctx_t *ctx = ik_openai_responses_stream_ctx_create(
        test_ctx, stream_cb, events);

    ik_openai_responses_stream_process_event(ctx, "response.output_item.added",
        "{\"item\":{\"type\":\"function_call\",\"call_id\":\"call_1\",\"name\":\"test\"},\"output_index\":0}");
    ik_openai_responses_stream_process_event(ctx, "response.output_item.done", "{}");
    ck_assert_int_eq((int)events->count, 2);

    events->count = 2;
    ik_openai_responses_stream_process_event(ctx, "response.output_item.done", "{\"output_index\":\"not an int\"}");
    ck_assert_int_eq((int)events->count, 2);

    events->count = 0;
    ik_openai_responses_stream_process_event(ctx, "response.output_item.done", "{\"output_index\":0}");
    ck_assert_int_eq((int)events->count, 1);

    ik_openai_responses_stream_process_event(ctx, "response.output_item.added",
        "{\"item\":{\"type\":\"function_call\",\"call_id\":\"call_1\",\"name\":\"test\"},\"output_index\":3}");
    ik_openai_responses_stream_process_event(ctx, "response.output_item.done", "{\"output_index\":3}");
    ck_assert_int_eq((int)events->count, 3);
    ck_assert_int_eq(events->items[2].type, IK_STREAM_TOOL_CALL_DONE);
}
END_TEST

START_TEST(test_response_completed_ends_tool_call)
{
    ik_openai_responses_stream_ctx_t *ctx = ik_openai_responses_stream_ctx_create(
        test_ctx, stream_cb, events);

    ik_openai_responses_stream_process_event(ctx, "response.output_item.added",
        "{\"item\":{\"type\":\"function_call\",\"call_id\":\"call_1\",\"name\":\"test\"},\"output_index\":0}");
    ik_openai_responses_stream_process_event(ctx, "response.completed", "{\"response\":{\"status\":\"completed\"}}");
    ck_assert_int_eq((int)events->count, 4);
    ck_assert_int_eq(events->items[2].type, IK_STREAM_TOOL_CALL_DONE);
    ck_assert_int_eq(events->items[3].type, IK_STREAM_DONE);
}
END_TEST

START_TEST(test_response_completed_edge_cases)
{
    ik_openai_responses_stream_ctx_t *ctx = ik_openai_responses_stream_ctx_create(
        test_ctx, stream_cb, events);

    ik_openai_responses_stream_process_event(ctx, "response.completed", "{}");
    ck_assert_int_eq((int)events->count, 1);
    ck_assert_int_eq(events->items[0].data.done.finish_reason, IK_FINISH_UNKNOWN);

    events->count = 0;
    ik_openai_responses_stream_process_event(ctx, "response.completed", "{\"response\":\"not an object\"}");
    ck_assert_int_eq((int)events->count, 1);

    events->count = 0;
    ik_openai_responses_stream_process_event(ctx, "response.completed", "{\"response\":{\"status\":null}}");
    ck_assert_int_eq((int)events->count, 1);

    events->count = 0;
    ik_openai_responses_stream_process_event(ctx, "response.completed",
        "{\"response\":{\"status\":\"incomplete\",\"incomplete_details\":{\"reason\":\"max_tokens\"}}}");
    ck_assert_int_eq((int)events->count, 1);

    events->count = 0;
    ik_openai_responses_stream_process_event(ctx, "response.completed",
        "{\"response\":{\"status\":\"incomplete\",\"incomplete_details\":\"not an object\"}}");
    ck_assert_int_eq((int)events->count, 1);

    events->count = 0;
    ik_openai_responses_stream_process_event(ctx, "response.completed",
        "{\"response\":{\"status\":\"completed\",\"usage\":{\"input_tokens\":10,\"output_tokens\":20,\"total_tokens\":30}}}");
    ck_assert_int_eq((int)events->count, 1);
    ck_assert_int_eq(events->items[0].data.done.usage.input_tokens, 10);
    ck_assert_int_eq(events->items[0].data.done.usage.total_tokens, 30);
}
END_TEST

START_TEST(test_parse_usage_edge_cases)
{
    ik_openai_responses_stream_ctx_t *ctx = ik_openai_responses_stream_ctx_create(
        test_ctx, stream_cb, events);

    ik_openai_responses_stream_process_event(ctx, "response.completed",
        "{\"response\":{\"status\":\"completed\",\"usage\":\"not an object\"}}");
    ck_assert_int_eq(events->items[0].data.done.usage.input_tokens, 0);

    talloc_free(ctx);
    events->count = 0;
    ctx = ik_openai_responses_stream_ctx_create(test_ctx, stream_cb, events);
    ik_openai_responses_stream_process_event(ctx, "response.completed",
        "{\"response\":{\"status\":\"completed\",\"usage\":{\"input_tokens\":10,\"output_tokens\":20}}}");
    ck_assert_int_eq(events->items[0].data.done.usage.total_tokens, 30);

    talloc_free(ctx);
    events->count = 0;
    ctx = ik_openai_responses_stream_ctx_create(test_ctx, stream_cb, events);
    ik_openai_responses_stream_process_event(ctx, "response.completed",
        "{\"response\":{\"status\":\"completed\",\"usage\":{\"input_tokens\":10,\"output_tokens\":20,\"output_tokens_details\":{\"reasoning_tokens\":5}}}}");
    ck_assert_int_eq(events->items[0].data.done.usage.thinking_tokens, 5);

    talloc_free(ctx);
    events->count = 0;
    ctx = ik_openai_responses_stream_ctx_create(test_ctx, stream_cb, events);
    ik_openai_responses_stream_process_event(ctx, "response.completed",
        "{\"response\":{\"status\":\"completed\",\"usage\":{\"input_tokens\":10,\"output_tokens\":20,\"output_tokens_details\":\"not an object\"}}}");
    ck_assert_int_eq(events->items[0].data.done.usage.thinking_tokens, 0);

    talloc_free(ctx);
    events->count = 0;
    ctx = ik_openai_responses_stream_ctx_create(test_ctx, stream_cb, events);
    ik_openai_responses_stream_process_event(ctx, "response.completed",
        "{\"response\":{\"status\":\"completed\",\"usage\":{\"input_tokens\":10,\"output_tokens\":20,\"output_tokens_details\":{\"reasoning_tokens\":\"not an int\"}}}}");
    ck_assert_int_eq(events->items[0].data.done.usage.thinking_tokens, 0);
}
END_TEST

START_TEST(test_error_event_edge_cases)
{
    ik_openai_responses_stream_ctx_t *ctx = ik_openai_responses_stream_ctx_create(
        test_ctx, stream_cb, events);

    ik_openai_responses_stream_process_event(ctx, "error", "{}");
    ck_assert_int_eq((int)events->count, 0);

    ik_openai_responses_stream_process_event(ctx, "error", "{\"error\":\"not an object\"}");
    ck_assert_int_eq((int)events->count, 0);

    ik_openai_responses_stream_process_event(ctx, "error", "{\"error\":{\"message\":null,\"type\":\"server_error\"}}");
    ck_assert_int_eq((int)events->count, 1);
    ck_assert_str_eq(events->items[0].data.error.message, "Unknown error");

    events->count = 0;
    ik_openai_responses_stream_process_event(ctx, "error", "{\"error\":{\"message\":\"Something went wrong\",\"type\":null}}");
    ck_assert_int_eq(events->items[0].data.error.category, IK_ERR_CAT_UNKNOWN);
}
END_TEST

START_TEST(test_error_event_categories)
{
    ik_openai_responses_stream_ctx_t *ctx = ik_openai_responses_stream_ctx_create(
        test_ctx, stream_cb, events);

    ik_openai_responses_stream_process_event(ctx, "error",
        "{\"error\":{\"message\":\"Invalid API key\",\"type\":\"authentication_error\"}}");
    ck_assert_int_eq(events->items[0].data.error.category, IK_ERR_CAT_AUTH);

    events->count = 0;
    ik_openai_responses_stream_process_event(ctx, "error",
        "{\"error\":{\"message\":\"Rate limit exceeded\",\"type\":\"rate_limit_error\"}}");
    ck_assert_int_eq(events->items[0].data.error.category, IK_ERR_CAT_RATE_LIMIT);

    events->count = 0;
    ik_openai_responses_stream_process_event(ctx, "error",
        "{\"error\":{\"message\":\"Invalid request\",\"type\":\"invalid_request_error\"}}");
    ck_assert_int_eq(events->items[0].data.error.category, IK_ERR_CAT_INVALID_ARG);

    events->count = 0;
    ik_openai_responses_stream_process_event(ctx, "error",
        "{\"error\":{\"message\":\"Server error\",\"type\":\"server_error\"}}");
    ck_assert_int_eq(events->items[0].data.error.category, IK_ERR_CAT_SERVER);
}
END_TEST

START_TEST(test_unknown_event_is_ignored)
{
    ik_openai_responses_stream_ctx_t *ctx = ik_openai_responses_stream_ctx_create(
        test_ctx, stream_cb, events);

    ik_openai_responses_stream_process_event(ctx, "unknown.event", "{\"some\":\"data\"}");
    ck_assert_int_eq((int)events->count, 0);
}
END_TEST

static Suite *openai_streaming_responses_events_suite(void)
{
    Suite *s = suite_create("OpenAI Streaming Responses Events");

    TCase *tc = tcase_create("Events");
    tcase_add_checked_fixture(tc, setup, teardown);
    tcase_add_test(tc, test_invalid_json);
    tcase_add_test(tc, test_response_created_edge_cases);
    tcase_add_test(tc, test_text_delta_edge_cases);
    tcase_add_test(tc, test_thinking_delta_edge_cases);
    tcase_add_test(tc, test_output_item_added_edge_cases);
    tcase_add_test(tc, test_output_item_added_ends_previous_tool_call);
    tcase_add_test(tc, test_function_call_arguments_delta_edge_cases);
    tcase_add_test(tc, test_function_call_arguments_done_is_noop);
    tcase_add_test(tc, test_output_item_done_edge_cases);
    tcase_add_test(tc, test_response_completed_ends_tool_call);
    tcase_add_test(tc, test_response_completed_edge_cases);
    tcase_add_test(tc, test_parse_usage_edge_cases);
    tcase_add_test(tc, test_error_event_edge_cases);
    tcase_add_test(tc, test_error_event_categories);
    tcase_add_test(tc, test_unknown_event_is_ignored);
    suite_add_tcase(s, tc);

    return s;
}

int main(void)
{
    Suite *s = openai_streaming_responses_events_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
