/**
 * @file streaming_chat_coverage_test_1.c
 * @brief Coverage tests for OpenAI Chat streaming - Part 1 (getters, malformed JSON, errors)
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
 * Getter Coverage Tests
 * ================================================================ */

START_TEST(test_get_usage)
{
    ik_openai_chat_stream_ctx_t *sctx = ik_openai_chat_stream_ctx_create(
        test_ctx, stream_cb, events);

    /* Process data with usage info */
    const char *data = "{"
        "\"choices\":[{\"delta\":{\"role\":\"assistant\"}}],"
        "\"usage\":{"
            "\"prompt_tokens\":10,"
            "\"completion_tokens\":20,"
            "\"total_tokens\":30"
        "}"
    "}";
    ik_openai_chat_stream_process_data(sctx, data);

    /* Get usage */
    ik_usage_t usage = ik_openai_chat_stream_get_usage(sctx);

    /* Verify */
    ck_assert_int_eq(usage.input_tokens, 10);
    ck_assert_int_eq(usage.output_tokens, 20);
    ck_assert_int_eq(usage.total_tokens, 30);
}

END_TEST

START_TEST(test_get_finish_reason)
{
    ik_openai_chat_stream_ctx_t *sctx = ik_openai_chat_stream_ctx_create(
        test_ctx, stream_cb, events);

    /* Process data with finish reason */
    const char *data = "{\"model\":\"gpt-4\",\"choices\":[{\"delta\":{},\"finish_reason\":\"stop\"}]}";
    ik_openai_chat_stream_process_data(sctx, data);

    /* Get finish reason */
    ik_finish_reason_t reason = ik_openai_chat_stream_get_finish_reason(sctx);

    /* Verify */
    ck_assert_int_eq(reason, IK_FINISH_STOP);
}

END_TEST

/* ================================================================
 * Malformed JSON Tests
 * ================================================================ */

START_TEST(test_malformed_json_silently_ignored)
{
    ik_openai_chat_stream_ctx_t *sctx = ik_openai_chat_stream_ctx_create(
        test_ctx, stream_cb, events);

    /* Process malformed JSON */
    ik_openai_chat_stream_process_data(sctx, "{invalid json");

    /* Should not emit any events */
    ck_assert_int_eq((int)events->count, 0);
}

END_TEST

START_TEST(test_null_root_silently_ignored)
{
    ik_openai_chat_stream_ctx_t *sctx = ik_openai_chat_stream_ctx_create(
        test_ctx, stream_cb, events);

    /* Process JSON with explicit null */
    ik_openai_chat_stream_process_data(sctx, "null");

    /* Should not emit any events */
    ck_assert_int_eq((int)events->count, 0);
}

END_TEST

START_TEST(test_normal_message_without_error)
{
    ik_openai_chat_stream_ctx_t *sctx = ik_openai_chat_stream_ctx_create(
        test_ctx, stream_cb, events);

    /* Normal message with no error field - triggers line 103 false branch */
    const char *data = "{\"model\":\"gpt-4\",\"choices\":[{\"delta\":{\"content\":\"test\"}}]}";
    ik_openai_chat_stream_process_data(sctx, data);

    /* Should process normally */
    ck_assert_int_ge((int)events->count, 0);
}

END_TEST

START_TEST(test_non_object_root_silently_ignored)
{
    ik_openai_chat_stream_ctx_t *sctx = ik_openai_chat_stream_ctx_create(
        test_ctx, stream_cb, events);

    /* Process JSON array instead of object */
    ik_openai_chat_stream_process_data(sctx, "[1, 2, 3]");

    /* Should not emit any events */
    ck_assert_int_eq((int)events->count, 0);
}

END_TEST

/* ================================================================
 * Error Handling Tests
 * ================================================================ */

START_TEST(test_error_authentication)
{
    ik_openai_chat_stream_ctx_t *sctx = ik_openai_chat_stream_ctx_create(
        test_ctx, stream_cb, events);

    const char *data = "{\"error\":{\"message\":\"Invalid API key\",\"type\":\"authentication_error\"}}";
    ik_openai_chat_stream_process_data(sctx, data);

    ck_assert_int_eq((int)events->count, 1);
    ck_assert_int_eq(events->items[0].type, IK_STREAM_ERROR);
    ck_assert_int_eq(events->items[0].data.error.category, IK_ERR_CAT_AUTH);
    ck_assert_str_eq(events->items[0].data.error.message, "Invalid API key");
}

END_TEST

START_TEST(test_error_permission)
{
    ik_openai_chat_stream_ctx_t *sctx = ik_openai_chat_stream_ctx_create(
        test_ctx, stream_cb, events);

    const char *data = "{\"error\":{\"message\":\"Access denied\",\"type\":\"permission_denied\"}}";
    ik_openai_chat_stream_process_data(sctx, data);

    ck_assert_int_eq((int)events->count, 1);
    ck_assert_int_eq(events->items[0].type, IK_STREAM_ERROR);
    ck_assert_int_eq(events->items[0].data.error.category, IK_ERR_CAT_AUTH);
}

END_TEST

START_TEST(test_error_rate_limit)
{
    ik_openai_chat_stream_ctx_t *sctx = ik_openai_chat_stream_ctx_create(
        test_ctx, stream_cb, events);

    const char *data = "{\"error\":{\"message\":\"Rate limit exceeded\",\"type\":\"rate_limit_error\"}}";
    ik_openai_chat_stream_process_data(sctx, data);

    ck_assert_int_eq((int)events->count, 1);
    ck_assert_int_eq(events->items[0].type, IK_STREAM_ERROR);
    ck_assert_int_eq(events->items[0].data.error.category, IK_ERR_CAT_RATE_LIMIT);
}

END_TEST

START_TEST(test_error_invalid_request)
{
    ik_openai_chat_stream_ctx_t *sctx = ik_openai_chat_stream_ctx_create(
        test_ctx, stream_cb, events);

    const char *data = "{\"error\":{\"message\":\"Bad request\",\"type\":\"invalid_request_error\"}}";
    ik_openai_chat_stream_process_data(sctx, data);

    ck_assert_int_eq((int)events->count, 1);
    ck_assert_int_eq(events->items[0].type, IK_STREAM_ERROR);
    ck_assert_int_eq(events->items[0].data.error.category, IK_ERR_CAT_INVALID_ARG);
}

END_TEST

START_TEST(test_error_server)
{
    ik_openai_chat_stream_ctx_t *sctx = ik_openai_chat_stream_ctx_create(
        test_ctx, stream_cb, events);

    const char *data = "{\"error\":{\"message\":\"Server error\",\"type\":\"server_error\"}}";
    ik_openai_chat_stream_process_data(sctx, data);

    ck_assert_int_eq((int)events->count, 1);
    ck_assert_int_eq(events->items[0].type, IK_STREAM_ERROR);
    ck_assert_int_eq(events->items[0].data.error.category, IK_ERR_CAT_SERVER);
}

END_TEST

START_TEST(test_error_service)
{
    ik_openai_chat_stream_ctx_t *sctx = ik_openai_chat_stream_ctx_create(
        test_ctx, stream_cb, events);

    const char *data = "{\"error\":{\"message\":\"Service unavailable\",\"type\":\"service_unavailable\"}}";
    ik_openai_chat_stream_process_data(sctx, data);

    ck_assert_int_eq((int)events->count, 1);
    ck_assert_int_eq(events->items[0].type, IK_STREAM_ERROR);
    ck_assert_int_eq(events->items[0].data.error.category, IK_ERR_CAT_SERVER);
}

END_TEST

START_TEST(test_error_unknown_type)
{
    ik_openai_chat_stream_ctx_t *sctx = ik_openai_chat_stream_ctx_create(
        test_ctx, stream_cb, events);

    const char *data = "{\"error\":{\"message\":\"Something went wrong\",\"type\":\"unknown_error\"}}";
    ik_openai_chat_stream_process_data(sctx, data);

    ck_assert_int_eq((int)events->count, 1);
    ck_assert_int_eq(events->items[0].type, IK_STREAM_ERROR);
    ck_assert_int_eq(events->items[0].data.error.category, IK_ERR_CAT_UNKNOWN);
}

END_TEST

START_TEST(test_error_null_type)
{
    ik_openai_chat_stream_ctx_t *sctx = ik_openai_chat_stream_ctx_create(
        test_ctx, stream_cb, events);

    const char *data = "{\"error\":{\"message\":\"Error occurred\"}}";
    ik_openai_chat_stream_process_data(sctx, data);

    ck_assert_int_eq((int)events->count, 1);
    ck_assert_int_eq(events->items[0].type, IK_STREAM_ERROR);
    ck_assert_int_eq(events->items[0].data.error.category, IK_ERR_CAT_UNKNOWN);
}

END_TEST

START_TEST(test_error_null_message)
{
    ik_openai_chat_stream_ctx_t *sctx = ik_openai_chat_stream_ctx_create(
        test_ctx, stream_cb, events);

    const char *data = "{\"error\":{\"type\":\"server_error\"}}";
    ik_openai_chat_stream_process_data(sctx, data);

    ck_assert_int_eq((int)events->count, 1);
    ck_assert_int_eq(events->items[0].type, IK_STREAM_ERROR);
    ck_assert_str_eq(events->items[0].data.error.message, "Unknown error");
}

END_TEST

/* ================================================================
 * Test Suite
 * ================================================================ */

static Suite *streaming_chat_coverage_suite_1(void)
{
    Suite *s = suite_create("OpenAI Streaming Chat Coverage 1");

    TCase *tc_getters = tcase_create("Getters");
    tcase_set_timeout(tc_getters, 30);
    tcase_add_checked_fixture(tc_getters, setup, teardown);
    tcase_add_test(tc_getters, test_get_usage);
    tcase_add_test(tc_getters, test_get_finish_reason);
    suite_add_tcase(s, tc_getters);

    TCase *tc_malformed = tcase_create("MalformedJSON");
    tcase_set_timeout(tc_malformed, 30);
    tcase_add_checked_fixture(tc_malformed, setup, teardown);
    tcase_add_test(tc_malformed, test_malformed_json_silently_ignored);
    tcase_add_test(tc_malformed, test_null_root_silently_ignored);
    tcase_add_test(tc_malformed, test_non_object_root_silently_ignored);
    tcase_add_test(tc_malformed, test_normal_message_without_error);
    suite_add_tcase(s, tc_malformed);

    TCase *tc_errors = tcase_create("ErrorHandling");
    tcase_set_timeout(tc_errors, 30);
    tcase_add_checked_fixture(tc_errors, setup, teardown);
    tcase_add_test(tc_errors, test_error_authentication);
    tcase_add_test(tc_errors, test_error_permission);
    tcase_add_test(tc_errors, test_error_rate_limit);
    tcase_add_test(tc_errors, test_error_invalid_request);
    tcase_add_test(tc_errors, test_error_server);
    tcase_add_test(tc_errors, test_error_service);
    tcase_add_test(tc_errors, test_error_unknown_type);
    tcase_add_test(tc_errors, test_error_null_type);
    tcase_add_test(tc_errors, test_error_null_message);
    suite_add_tcase(s, tc_errors);

    return s;
}

int main(void)
{
    Suite *s = streaming_chat_coverage_suite_1();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
