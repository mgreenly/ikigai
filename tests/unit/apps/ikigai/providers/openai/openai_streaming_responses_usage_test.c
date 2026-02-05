#include "tests/test_constants.h"
/**
 * @file openai_streaming_responses_usage_test.c
 * @brief Tests for OpenAI Responses API usage parsing
 */

#include <check.h>
#include <talloc.h>
#include <string.h>
#include "apps/ikigai/providers/openai/streaming.h"
#include "apps/ikigai/providers/openai/streaming_responses_internal.h"
#include "apps/ikigai/providers/provider.h"

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

START_TEST(test_response_completed_edge_cases) {
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
    ik_openai_responses_stream_process_event(ctx,
                                             "response.completed",
                                             "{\"response\":{\"status\":\"incomplete\",\"incomplete_details\":{\"reason\":\"max_tokens\"}}}");
    ck_assert_int_eq((int)events->count, 1);

    events->count = 0;
    ik_openai_responses_stream_process_event(ctx,
                                             "response.completed",
                                             "{\"response\":{\"status\":\"incomplete\",\"incomplete_details\":\"not an object\"}}");
    ck_assert_int_eq((int)events->count, 1);

    events->count = 0;
    ik_openai_responses_stream_process_event(ctx,
                                             "response.completed",
                                             "{\"response\":{\"status\":\"completed\",\"usage\":{\"input_tokens\":10,\"output_tokens\":20,\"total_tokens\":30}}}");
    ck_assert_int_eq((int)events->count, 1);
    ck_assert_int_eq(events->items[0].data.done.usage.input_tokens, 10);
    ck_assert_int_eq(events->items[0].data.done.usage.total_tokens, 30);
}
END_TEST

START_TEST(test_parse_usage_edge_cases) {
    ik_openai_responses_stream_ctx_t *ctx = ik_openai_responses_stream_ctx_create(
        test_ctx, stream_cb, events);

    ik_openai_responses_stream_process_event(ctx, "response.completed",
                                             "{\"response\":{\"status\":\"completed\",\"usage\":\"not an object\"}}");
    ck_assert_int_eq(events->items[0].data.done.usage.input_tokens, 0);

    talloc_free(ctx);
    events->count = 0;
    ctx = ik_openai_responses_stream_ctx_create(test_ctx, stream_cb, events);
    ik_openai_responses_stream_process_event(ctx,
                                             "response.completed",
                                             "{\"response\":{\"status\":\"completed\",\"usage\":{\"input_tokens\":10,\"output_tokens\":20}}}");
    ck_assert_int_eq(events->items[0].data.done.usage.total_tokens, 30);

    talloc_free(ctx);
    events->count = 0;
    ctx = ik_openai_responses_stream_ctx_create(test_ctx, stream_cb, events);
    ik_openai_responses_stream_process_event(ctx,
                                             "response.completed",
                                             "{\"response\":{\"status\":\"completed\",\"usage\":{\"input_tokens\":10,\"output_tokens\":20,\"output_tokens_details\":{\"reasoning_tokens\":5}}}}");
    ck_assert_int_eq(events->items[0].data.done.usage.thinking_tokens, 5);

    talloc_free(ctx);
    events->count = 0;
    ctx = ik_openai_responses_stream_ctx_create(test_ctx, stream_cb, events);
    ik_openai_responses_stream_process_event(ctx,
                                             "response.completed",
                                             "{\"response\":{\"status\":\"completed\",\"usage\":{\"input_tokens\":10,\"output_tokens\":20,\"output_tokens_details\":\"not an object\"}}}");
    ck_assert_int_eq(events->items[0].data.done.usage.thinking_tokens, 0);

    talloc_free(ctx);
    events->count = 0;
    ctx = ik_openai_responses_stream_ctx_create(test_ctx, stream_cb, events);
    ik_openai_responses_stream_process_event(ctx,
                                             "response.completed",
                                             "{\"response\":{\"status\":\"completed\",\"usage\":{\"input_tokens\":10,\"output_tokens\":20,\"output_tokens_details\":{\"reasoning_tokens\":\"not an int\"}}}}");
    ck_assert_int_eq(events->items[0].data.done.usage.thinking_tokens, 0);

    talloc_free(ctx);
    events->count = 0;
    ctx = ik_openai_responses_stream_ctx_create(test_ctx, stream_cb, events);
    ik_openai_responses_stream_process_event(ctx,
                                             "response.completed",
                                             "{\"response\":{\"status\":\"completed\",\"usage\":{\"input_tokens\":\"not an int\",\"output_tokens\":20}}}");
    ck_assert_int_eq(events->items[0].data.done.usage.input_tokens, 0);

    talloc_free(ctx);
    events->count = 0;
    ctx = ik_openai_responses_stream_ctx_create(test_ctx, stream_cb, events);
    ik_openai_responses_stream_process_event(ctx,
                                             "response.completed",
                                             "{\"response\":{\"status\":\"completed\",\"usage\":{\"output_tokens\":\"not an int\"}}}");
    ck_assert_int_eq(events->items[0].data.done.usage.output_tokens, 0);

    talloc_free(ctx);
    events->count = 0;
    ctx = ik_openai_responses_stream_ctx_create(test_ctx, stream_cb, events);
    ik_openai_responses_stream_process_event(ctx,
                                             "response.completed",
                                             "{\"response\":{\"status\":\"completed\",\"usage\":{\"total_tokens\":\"not an int\"}}}");
    ck_assert_int_eq(events->items[0].data.done.usage.total_tokens, 0);

    talloc_free(ctx);
    events->count = 0;
    ctx = ik_openai_responses_stream_ctx_create(test_ctx, stream_cb, events);
    ik_openai_responses_stream_process_event(ctx,
                                             "response.completed",
                                             "{\"response\":{\"status\":\"completed\",\"usage\":{\"input_tokens\":0,\"output_tokens\":0,\"total_tokens\":50}}}");
    ck_assert_int_eq(events->items[0].data.done.usage.total_tokens, 50);

    talloc_free(ctx);
    events->count = 0;
    ctx = ik_openai_responses_stream_ctx_create(test_ctx, stream_cb, events);
    ik_openai_responses_stream_process_event(ctx,
                                             "response.completed",
                                             "{\"response\":{\"status\":\"completed\",\"usage\":{\"output_tokens_details\":{}}}}");
    ck_assert_int_eq(events->items[0].data.done.usage.thinking_tokens, 0);
}

END_TEST

static Suite *openai_streaming_responses_usage_suite(void)
{
    Suite *s = suite_create("OpenAI Streaming Responses Usage");

    TCase *tc = tcase_create("Usage");
    tcase_set_timeout(tc, IK_TEST_TIMEOUT);
    tcase_add_checked_fixture(tc, setup, teardown);
    tcase_add_test(tc, test_response_completed_edge_cases);
    tcase_add_test(tc, test_parse_usage_edge_cases);
    suite_add_tcase(s, tc);

    return s;
}

int main(void)
{
    Suite *s = openai_streaming_responses_usage_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/apps/ikigai/providers/openai/openai_streaming_responses_usage_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
