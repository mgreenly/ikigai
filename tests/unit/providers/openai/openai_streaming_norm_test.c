/**
 * @file openai_streaming_norm_test.c
 * @brief Event normalization and error handling tests for OpenAI streaming
 *
 * Tests normalization to canonical event types and error response handling.
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

END_TEST

START_TEST(test_normalize_tool_calls_to_deltas)
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

END_TEST

START_TEST(test_normalize_finish_reason_to_done)
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

END_TEST

START_TEST(test_handle_error_response)
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

END_TEST

START_TEST(test_handle_stream_with_usage)
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
 * Test Suite
 * ================================================================ */

static Suite *openai_streaming_norm_suite(void)
{
    Suite *s = suite_create("OpenAI Streaming Normalization");

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

    return s;
}

int main(void)
{
    Suite *s = openai_streaming_norm_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
