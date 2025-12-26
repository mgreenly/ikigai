/**
 * @file test_openai_streaming.c
 * @brief Unit tests for OpenAI Chat Completions async streaming
 *
 * Tests SSE parsing, delta accumulation, tool call streaming, and event normalization.
 * Uses ik_openai_chat_stream_ctx_t to process SSE data events and verify emitted events.
 * Also includes async vtable integration tests that verify the provider's streaming vtable.
 */

#include <check.h>
#include <talloc.h>
#include <string.h>
#include <sys/select.h>
#include "providers/openai/streaming.h"
#include "providers/openai/openai.h"
#include "providers/provider.h"

/* ================================================================
 * Test Context and Event Capture
 * ================================================================ */

/**
 * Event collection for verification
 */
typedef struct {
    ik_stream_event_t *items;
    size_t count;
    size_t capacity;
} event_array_t;

static TALLOC_CTX *test_ctx;
static event_array_t *events;

/**
 * Stream callback - captures events for later verification
 */
static res_t stream_cb(const ik_stream_event_t *event, void *ctx)
{
    event_array_t *arr = (event_array_t *)ctx;

    /* Grow array if needed */
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

    /* Deep copy event */
    ik_stream_event_t *dst = &arr->items[arr->count];
    dst->type = event->type;
    dst->index = event->index;

    /* Copy variant data based on type */
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
            /* No additional data */
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

    /* Initialize event array */
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
 * Basic Streaming Tests
 * ================================================================ */

START_TEST(test_parse_initial_role_delta) {
    /* Create streaming context */
    ik_openai_chat_stream_ctx_t *sctx = ik_openai_chat_stream_ctx_create(
        test_ctx, stream_cb, events);

    /* Process initial chunk with role and model */
    const char *data = "{\"id\":\"chatcmpl-123\",\"model\":\"gpt-4\","
                       "\"choices\":[{\"delta\":{\"role\":\"assistant\"},\"index\":0}]}";
    ik_openai_chat_stream_process_data(sctx, data);

    /* First delta with role should not emit START yet (waits for content) */
    ck_assert_int_eq((int)events->count, 0);
}
END_TEST START_TEST(test_parse_content_delta)
{
    ik_openai_chat_stream_ctx_t *sctx = ik_openai_chat_stream_ctx_create(
        test_ctx, stream_cb, events);

    /* Process initial chunk to set model */
    const char *init_data = "{\"id\":\"chatcmpl-123\",\"model\":\"gpt-4\","
                            "\"choices\":[{\"delta\":{\"role\":\"assistant\"}}]}";
    ik_openai_chat_stream_process_data(sctx, init_data);

    /* Process content delta */
    const char *content_data = "{\"choices\":[{\"delta\":{\"content\":\"Hello\"}}]}";
    ik_openai_chat_stream_process_data(sctx, content_data);

    /* Should emit START + TEXT_DELTA */
    ck_assert_int_eq((int)events->count, 2);
    ck_assert_int_eq(events->items[0].type, IK_STREAM_START);
    ck_assert_str_eq(events->items[0].data.start.model, "gpt-4");
    ck_assert_int_eq(events->items[1].type, IK_STREAM_TEXT_DELTA);
    ck_assert_str_eq(events->items[1].data.delta.text, "Hello");
}

END_TEST START_TEST(test_parse_finish_reason)
{
    ik_openai_chat_stream_ctx_t *sctx = ik_openai_chat_stream_ctx_create(
        test_ctx, stream_cb, events);

    /* Set model first */
    const char *init_data = "{\"model\":\"gpt-4\",\"choices\":[{\"delta\":{\"role\":\"assistant\"}}]}";
    ik_openai_chat_stream_process_data(sctx, init_data);

    /* Process finish_reason */
    const char *finish_data = "{\"choices\":[{\"delta\":{},\"finish_reason\":\"stop\"}]}";
    ik_openai_chat_stream_process_data(sctx, finish_data);

    /* Finish reason updates internal state but doesn't emit event yet */
    ik_finish_reason_t reason = ik_openai_chat_stream_get_finish_reason(sctx);
    ck_assert_int_eq(reason, IK_FINISH_STOP);
}

END_TEST START_TEST(test_handle_done_marker)
{
    ik_openai_chat_stream_ctx_t *sctx = ik_openai_chat_stream_ctx_create(
        test_ctx, stream_cb, events);

    /* Set model and finish reason */
    const char *init_data = "{\"model\":\"gpt-4\",\"choices\":[{\"delta\":{\"role\":\"assistant\"}}]}";
    ik_openai_chat_stream_process_data(sctx, init_data);

    const char *finish_data = "{\"choices\":[{\"delta\":{},\"finish_reason\":\"stop\"}]}";
    ik_openai_chat_stream_process_data(sctx, finish_data);

    /* Process [DONE] marker */
    ik_openai_chat_stream_process_data(sctx, "[DONE]");

    /* Should emit DONE event */
    ck_assert_int_eq((int)events->count, 1);
    ck_assert_int_eq(events->items[0].type, IK_STREAM_DONE);
    ck_assert_int_eq(events->items[0].data.done.finish_reason, IK_FINISH_STOP);
}

END_TEST
/* ================================================================
 * Content Accumulation Tests
 * ================================================================ */

START_TEST(test_accumulate_multiple_deltas)
{
    ik_openai_chat_stream_ctx_t *sctx = ik_openai_chat_stream_ctx_create(
        test_ctx, stream_cb, events);

    /* Set model */
    const char *init_data = "{\"model\":\"gpt-4\",\"choices\":[{\"delta\":{\"role\":\"assistant\"}}]}";
    ik_openai_chat_stream_process_data(sctx, init_data);

    /* Process multiple content deltas */
    ik_openai_chat_stream_process_data(sctx, "{\"choices\":[{\"delta\":{\"content\":\"Hello\"}}]}");
    ik_openai_chat_stream_process_data(sctx, "{\"choices\":[{\"delta\":{\"content\":\" \"}}]}");
    ik_openai_chat_stream_process_data(sctx, "{\"choices\":[{\"delta\":{\"content\":\"world\"}}]}");

    /* Should emit START + 3 TEXT_DELTA events */
    ck_assert_int_eq((int)events->count, 4);
    ck_assert_int_eq(events->items[0].type, IK_STREAM_START);
    ck_assert_int_eq(events->items[1].type, IK_STREAM_TEXT_DELTA);
    ck_assert_str_eq(events->items[1].data.delta.text, "Hello");
    ck_assert_int_eq(events->items[2].type, IK_STREAM_TEXT_DELTA);
    ck_assert_str_eq(events->items[2].data.delta.text, " ");
    ck_assert_int_eq(events->items[3].type, IK_STREAM_TEXT_DELTA);
    ck_assert_str_eq(events->items[3].data.delta.text, "world");
}

END_TEST START_TEST(test_handle_empty_content_delta)
{
    ik_openai_chat_stream_ctx_t *sctx = ik_openai_chat_stream_ctx_create(
        test_ctx, stream_cb, events);

    /* Set model */
    const char *init_data = "{\"model\":\"gpt-4\",\"choices\":[{\"delta\":{\"role\":\"assistant\"}}]}";
    ik_openai_chat_stream_process_data(sctx, init_data);

    /* Process empty delta */
    ik_openai_chat_stream_process_data(sctx, "{\"choices\":[{\"delta\":{}}]}");

    /* Empty delta should not emit any events (no START since no content yet) */
    ck_assert_int_eq((int)events->count, 0);
}

END_TEST START_TEST(test_preserve_text_order)
{
    ik_openai_chat_stream_ctx_t *sctx = ik_openai_chat_stream_ctx_create(
        test_ctx, stream_cb, events);

    /* Set model */
    const char *init_data = "{\"model\":\"gpt-4\",\"choices\":[{\"delta\":{\"role\":\"assistant\"}}]}";
    ik_openai_chat_stream_process_data(sctx, init_data);

    /* Process deltas in specific order */
    ik_openai_chat_stream_process_data(sctx, "{\"choices\":[{\"delta\":{\"content\":\"A\"}}]}");
    ik_openai_chat_stream_process_data(sctx, "{\"choices\":[{\"delta\":{\"content\":\"B\"}}]}");
    ik_openai_chat_stream_process_data(sctx, "{\"choices\":[{\"delta\":{\"content\":\"C\"}}]}");

    /* Verify order is preserved */
    ck_assert_int_eq((int)events->count, 4); /* START + 3 deltas */
    ck_assert_str_eq(events->items[1].data.delta.text, "A");
    ck_assert_str_eq(events->items[2].data.delta.text, "B");
    ck_assert_str_eq(events->items[3].data.delta.text, "C");
}

END_TEST
/* ================================================================
 * Tool Call Streaming Tests
 * ================================================================ */

START_TEST(test_parse_tool_call_start)
{
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

END_TEST START_TEST(test_parse_tool_call_arguments_delta)
{
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

END_TEST START_TEST(test_accumulate_tool_arguments)
{
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

END_TEST START_TEST(test_handle_multiple_tool_calls)
{
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

END_TEST START_TEST(test_emit_tool_call_done)
{
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
 * Event Normalization Tests
 * ================================================================ */

START_TEST(test_normalize_content_to_text_delta)
{
    ik_openai_chat_stream_ctx_t *sctx = ik_openai_chat_stream_ctx_create(
        test_ctx, stream_cb, events);

    /* Set model */
    const char *init_data = "{\"model\":\"gpt-4\",\"choices\":[{\"delta\":{\"role\":\"assistant\"}}]}";
    ik_openai_chat_stream_process_data(sctx, init_data);

    /* Process content delta */
    ik_openai_chat_stream_process_data(sctx, "{\"choices\":[{\"delta\":{\"content\":\"test\"}}]}");

    /* Verify normalized to IK_STREAM_TEXT_DELTA */
    ck_assert_int_eq((int)events->count, 2);
    ck_assert_int_eq(events->items[1].type, IK_STREAM_TEXT_DELTA);
}

END_TEST START_TEST(test_normalize_tool_calls_to_deltas)
{
    ik_openai_chat_stream_ctx_t *sctx = ik_openai_chat_stream_ctx_create(
        test_ctx, stream_cb, events);

    /* Set model */
    const char *init_data = "{\"model\":\"gpt-4\",\"choices\":[{\"delta\":{\"role\":\"assistant\"}}]}";
    ik_openai_chat_stream_process_data(sctx, init_data);

    /* Process tool call with arguments */
    ik_openai_chat_stream_process_data(sctx,
                                       "{\"choices\":[{\"delta\":{\"tool_calls\":["
                                       "{\"index\":0,\"id\":\"call_1\",\"function\":{\"name\":\"tool1\",\"arguments\":\"\"}}"
                                       "]}}]}");
    ik_openai_chat_stream_process_data(sctx,
                                       "{\"choices\":[{\"delta\":{\"tool_calls\":["
                                       "{\"index\":0,\"function\":{\"arguments\":\"args\"}}"
                                       "]}}]}");

    /* Verify normalized to TOOL_CALL events */
    ck_assert_int_eq(events->items[1].type, IK_STREAM_TOOL_CALL_START);
    ck_assert_int_eq(events->items[2].type, IK_STREAM_TOOL_CALL_DELTA);
}

END_TEST START_TEST(test_normalize_finish_reason_to_done)
{
    ik_openai_chat_stream_ctx_t *sctx = ik_openai_chat_stream_ctx_create(
        test_ctx, stream_cb, events);

    /* Set model */
    const char *init_data = "{\"model\":\"gpt-4\",\"choices\":[{\"delta\":{\"role\":\"assistant\"}}]}";
    ik_openai_chat_stream_process_data(sctx, init_data);

    /* Process finish_reason and [DONE] */
    ik_openai_chat_stream_process_data(sctx, "{\"choices\":[{\"delta\":{},\"finish_reason\":\"length\"}]}");
    ik_openai_chat_stream_process_data(sctx, "[DONE]");

    /* Verify DONE event has correct finish reason */
    ck_assert_int_eq((int)events->count, 1);
    ck_assert_int_eq(events->items[0].type, IK_STREAM_DONE);
    ck_assert_int_eq(events->items[0].data.done.finish_reason, IK_FINISH_LENGTH);
}

END_TEST
/* ================================================================
 * Error Handling Tests
 * ================================================================ */

START_TEST(test_handle_malformed_json)
{
    ik_openai_chat_stream_ctx_t *sctx = ik_openai_chat_stream_ctx_create(
        test_ctx, stream_cb, events);

    /* Process malformed JSON */
    ik_openai_chat_stream_process_data(sctx, "{invalid json}");

    /* Malformed JSON is silently ignored (no events emitted) */
    ck_assert_int_eq((int)events->count, 0);
}

END_TEST START_TEST(test_handle_error_response)
{
    ik_openai_chat_stream_ctx_t *sctx = ik_openai_chat_stream_ctx_create(
        test_ctx, stream_cb, events);

    /* Process error response */
    const char *error_data = "{\"error\":{\"message\":\"Invalid API key\",\"type\":\"authentication_error\"}}";
    ik_openai_chat_stream_process_data(sctx, error_data);

    /* Should emit ERROR event */
    ck_assert_int_eq((int)events->count, 1);
    ck_assert_int_eq(events->items[0].type, IK_STREAM_ERROR);
    ck_assert_int_eq(events->items[0].data.error.category, IK_ERR_CAT_AUTH);
    ck_assert_str_eq(events->items[0].data.error.message, "Invalid API key");
}

END_TEST START_TEST(test_handle_stream_with_usage)
{
    ik_openai_chat_stream_ctx_t *sctx = ik_openai_chat_stream_ctx_create(
        test_ctx, stream_cb, events);

    /* Set model */
    const char *init_data = "{\"model\":\"gpt-4\",\"choices\":[{\"delta\":{\"role\":\"assistant\"}}]}";
    ik_openai_chat_stream_process_data(sctx, init_data);

    /* Process usage in final chunk */
    const char *usage_data = "{\"usage\":{\"prompt_tokens\":10,\"completion_tokens\":20,\"total_tokens\":30}}";
    ik_openai_chat_stream_process_data(sctx, usage_data);

    /* Process finish and DONE */
    ik_openai_chat_stream_process_data(sctx, "{\"choices\":[{\"delta\":{},\"finish_reason\":\"stop\"}]}");
    ik_openai_chat_stream_process_data(sctx, "[DONE]");

    /* Verify usage in DONE event */
    ck_assert_int_eq((int)events->count, 1);
    ck_assert_int_eq(events->items[0].type, IK_STREAM_DONE);
    ck_assert_int_eq(events->items[0].data.done.usage.input_tokens, 10);
    ck_assert_int_eq(events->items[0].data.done.usage.output_tokens, 20);
    ck_assert_int_eq(events->items[0].data.done.usage.total_tokens, 30);
}

END_TEST

/* ================================================================
 * Async Vtable Integration Tests
 * ================================================================ */

/* Dummy completion callback for tests that need to call start_stream */
static res_t dummy_completion_cb(const ik_provider_completion_t *completion, void *ctx)
{
    (void)completion;
    (void)ctx;
    return OK(NULL);
}

START_TEST(test_start_stream_returns_immediately) {
    /* Create provider instance */
    ik_provider_t *provider = NULL;
    res_t r = ik_openai_create(test_ctx, "sk-test-key-12345", &provider);
    ck_assert(!is_err(&r));
    ck_assert_ptr_nonnull(provider);

    /* Build minimal request */
    ik_message_t msg = {
        .role = IK_ROLE_USER,
        .content_blocks = NULL,
        .content_count = 0,
        .provider_metadata = NULL
    };

    char *model_name = talloc_strdup(test_ctx, "gpt-4");

    ik_request_t req = {
        .system_prompt = NULL,
        .messages = &msg,
        .message_count = 1,
        .model = model_name,
        .thinking = { .level = IK_THINKING_NONE, .include_summary = false },
        .tools = NULL,
        .tool_count = 0,
        .max_output_tokens = 100,
        .tool_choice_mode = 0,
        .tool_choice_name = NULL
    };

    /* Test that start_stream returns immediately (non-blocking) */
    bool completion_called = false;
    r = provider->vt->start_stream(provider->ctx, &req, stream_cb, events,
                                   dummy_completion_cb, &completion_called);

    /* Should return OK (request queued successfully) */
    ck_assert(!is_err(&r));

    /* Cleanup */
    talloc_free(provider);
}
END_TEST START_TEST(test_fdset_returns_valid_fds)
{
    /* Create provider instance */
    ik_provider_t *provider = NULL;
    res_t r = ik_openai_create(test_ctx, "sk-test-key-12345", &provider);
    ck_assert(!is_err(&r));

    /* Test fdset before any requests */
    fd_set read_fds, write_fds, exc_fds;
    int max_fd = 0;
    FD_ZERO(&read_fds);
    FD_ZERO(&write_fds);
    FD_ZERO(&exc_fds);

    r = provider->vt->fdset(provider->ctx, &read_fds, &write_fds, &exc_fds, &max_fd);

    /* Should return OK even with no active requests */
    ck_assert(!is_err(&r));
    /* max_fd should be -1 when no active transfers */
    ck_assert_int_eq(max_fd, -1);

    /* Cleanup */
    talloc_free(provider);
}

END_TEST START_TEST(test_perform_info_read_no_crash)
{
    /* Create provider instance */
    ik_provider_t *provider = NULL;
    res_t r = ik_openai_create(test_ctx, "sk-test-key-12345", &provider);
    ck_assert(!is_err(&r));

    /* Test perform with no active requests */
    int running = 0;
    r = provider->vt->perform(provider->ctx, &running);

    /* Should return OK */
    ck_assert(!is_err(&r));
    /* No active requests */
    ck_assert_int_eq(running, 0);

    /* Test info_read with no completed transfers */
    provider->vt->info_read(provider->ctx, NULL);

    /* Should not crash - success means test passes */

    /* Cleanup */
    talloc_free(provider);
}

END_TEST

/* ================================================================
 * Test Suite
 * ================================================================ */

static Suite *openai_streaming_suite(void)
{
    Suite *s = suite_create("OpenAI Streaming");

    /* Basic Streaming */
    TCase *tc_basic = tcase_create("Basic");
    tcase_add_checked_fixture(tc_basic, setup, teardown);
    tcase_add_test(tc_basic, test_parse_initial_role_delta);
    tcase_add_test(tc_basic, test_parse_content_delta);
    tcase_add_test(tc_basic, test_parse_finish_reason);
    tcase_add_test(tc_basic, test_handle_done_marker);
    suite_add_tcase(s, tc_basic);

    /* Content Accumulation */
    TCase *tc_accumulation = tcase_create("Accumulation");
    tcase_add_checked_fixture(tc_accumulation, setup, teardown);
    tcase_add_test(tc_accumulation, test_accumulate_multiple_deltas);
    tcase_add_test(tc_accumulation, test_handle_empty_content_delta);
    tcase_add_test(tc_accumulation, test_preserve_text_order);
    suite_add_tcase(s, tc_accumulation);

    /* Tool Call Streaming */
    TCase *tc_tools = tcase_create("ToolCalls");
    tcase_add_checked_fixture(tc_tools, setup, teardown);
    tcase_add_test(tc_tools, test_parse_tool_call_start);
    tcase_add_test(tc_tools, test_parse_tool_call_arguments_delta);
    tcase_add_test(tc_tools, test_accumulate_tool_arguments);
    tcase_add_test(tc_tools, test_handle_multiple_tool_calls);
    tcase_add_test(tc_tools, test_emit_tool_call_done);
    suite_add_tcase(s, tc_tools);

    /* Event Normalization */
    TCase *tc_normalize = tcase_create("Normalization");
    tcase_add_checked_fixture(tc_normalize, setup, teardown);
    tcase_add_test(tc_normalize, test_normalize_content_to_text_delta);
    tcase_add_test(tc_normalize, test_normalize_tool_calls_to_deltas);
    tcase_add_test(tc_normalize, test_normalize_finish_reason_to_done);
    suite_add_tcase(s, tc_normalize);

    /* Error Handling */
    TCase *tc_errors = tcase_create("Errors");
    tcase_add_checked_fixture(tc_errors, setup, teardown);
    tcase_add_test(tc_errors, test_handle_malformed_json);
    tcase_add_test(tc_errors, test_handle_error_response);
    tcase_add_test(tc_errors, test_handle_stream_with_usage);
    suite_add_tcase(s, tc_errors);

    /* Async Vtable Integration */
    TCase *tc_async = tcase_create("AsyncVtable");
    tcase_add_checked_fixture(tc_async, setup, teardown);
    tcase_add_test(tc_async, test_start_stream_returns_immediately);
    tcase_add_test(tc_async, test_fdset_returns_valid_fds);
    tcase_add_test(tc_async, test_perform_info_read_no_crash);
    suite_add_tcase(s, tc_async);

    return s;
}

int main(void)
{
    Suite *s = openai_streaming_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
