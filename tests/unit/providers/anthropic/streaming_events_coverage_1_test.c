/**
 * @file streaming_events_coverage_test_1.c
 * @brief Coverage tests for Anthropic streaming events processors (part 1)
 *
 * Tests edge cases in message_start event processing:
 * - missing/invalid message, model, usage fields
 */

#include <check.h>
#include <talloc.h>
#include <string.h>
#include "providers/anthropic/streaming_events.h"
#include "providers/anthropic/streaming.h"
#include "providers/provider.h"
#include "vendor/yyjson/yyjson.h"

static TALLOC_CTX *test_ctx;
static ik_anthropic_stream_ctx_t *stream_ctx;

/* Test event capture */
#define MAX_EVENTS 16
static ik_stream_event_t captured_events[MAX_EVENTS];
static size_t captured_count;

/* ================================================================
 * Callbacks
 * ================================================================ */

static res_t test_stream_cb(const ik_stream_event_t *event, void *ctx)
{
    (void)ctx;

    if (captured_count < MAX_EVENTS) {
        /* Deep copy event */
        captured_events[captured_count] = *event;

        /* Copy string data if present */
        if (event->type == IK_STREAM_START && event->data.start.model) {
            captured_events[captured_count].data.start.model =
                talloc_strdup(test_ctx, event->data.start.model);
        }

        captured_count++;
    }

    return OK(NULL);
}

/* ================================================================
 * Fixtures
 * ================================================================ */

static void setup(void)
{
    test_ctx = talloc_new(NULL);

    /* Create streaming context */
    res_t r = ik_anthropic_stream_ctx_create(test_ctx, test_stream_cb, NULL, &stream_ctx);
    ck_assert(!is_err(&r));

    /* Reset event capture */
    captured_count = 0;
    memset(captured_events, 0, sizeof(captured_events));
}

static void teardown(void)
{
    talloc_free(test_ctx);
}

/* ================================================================
 * message_start Tests - Line 22, 24, 26 branches
 * ================================================================ */

START_TEST(test_message_start_no_message_field) {
    /* Test message_start without "message" field - line 22 branch (NULL) */
    const char *json = "{}";
    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    ck_assert_ptr_nonnull(doc);

    ik_anthropic_process_message_start(stream_ctx, yyjson_doc_get_root(doc));

    /* Should still emit IK_STREAM_START event but without model */
    ck_assert_int_eq((int)captured_count, 1);
    ck_assert_int_eq(captured_events[0].type, IK_STREAM_START);
    ck_assert_ptr_null(captured_events[0].data.start.model);

    yyjson_doc_free(doc);
}

END_TEST

START_TEST(test_message_start_message_not_object) {
    /* Test message_start with "message" field that is not an object - line 22 branch (!yyjson_is_obj) */
    const char *json = "{\"message\": \"not an object\"}";
    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    ck_assert_ptr_nonnull(doc);

    ik_anthropic_process_message_start(stream_ctx, yyjson_doc_get_root(doc));

    /* Should still emit IK_STREAM_START event but without model */
    ck_assert_int_eq((int)captured_count, 1);
    ck_assert_int_eq(captured_events[0].type, IK_STREAM_START);
    ck_assert_ptr_null(captured_events[0].data.start.model);

    yyjson_doc_free(doc);
}

END_TEST

START_TEST(test_message_start_no_model_field) {
    /* Test message_start without "model" field in message - line 24 branch (NULL) */
    const char *json = "{\"message\": {}}";
    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    ck_assert_ptr_nonnull(doc);

    ik_anthropic_process_message_start(stream_ctx, yyjson_doc_get_root(doc));

    /* Should emit IK_STREAM_START event but without model */
    ck_assert_int_eq((int)captured_count, 1);
    ck_assert_int_eq(captured_events[0].type, IK_STREAM_START);
    ck_assert_ptr_null(captured_events[0].data.start.model);

    yyjson_doc_free(doc);
}

END_TEST

START_TEST(test_message_start_model_not_string) {
    /* Test message_start with "model" field that is not a string - line 26 branch (NULL) */
    const char *json = "{\"message\": {\"model\": 12345}}";
    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    ck_assert_ptr_nonnull(doc);

    ik_anthropic_process_message_start(stream_ctx, yyjson_doc_get_root(doc));

    /* Should emit IK_STREAM_START event but without model */
    ck_assert_int_eq((int)captured_count, 1);
    ck_assert_int_eq(captured_events[0].type, IK_STREAM_START);
    ck_assert_ptr_null(captured_events[0].data.start.model);

    yyjson_doc_free(doc);
}

END_TEST

/* ================================================================
 * message_start Tests - Line 34, 36 branches
 * ================================================================ */

START_TEST(test_message_start_no_usage_field) {
    /* Test message_start without "usage" field - line 34 branch (NULL) */
    const char *json = "{\"message\": {\"model\": \"claude-3-5-sonnet-20241022\"}}";
    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    ck_assert_ptr_nonnull(doc);

    ik_anthropic_process_message_start(stream_ctx, yyjson_doc_get_root(doc));

    /* Should emit IK_STREAM_START event with model but no usage */
    ck_assert_int_eq((int)captured_count, 1);
    ck_assert_int_eq(captured_events[0].type, IK_STREAM_START);
    ck_assert_str_eq(captured_events[0].data.start.model, "claude-3-5-sonnet-20241022");
    ck_assert_int_eq(stream_ctx->usage.input_tokens, 0);

    yyjson_doc_free(doc);
}

END_TEST

START_TEST(test_message_start_usage_not_object) {
    /* Test message_start with "usage" field that is not an object - line 34 branch (!yyjson_is_obj) */
    const char *json = "{\"message\": {\"model\": \"claude-3-5-sonnet-20241022\", \"usage\": \"not an object\"}}";
    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    ck_assert_ptr_nonnull(doc);

    ik_anthropic_process_message_start(stream_ctx, yyjson_doc_get_root(doc));

    /* Should emit IK_STREAM_START event with model but no usage */
    ck_assert_int_eq((int)captured_count, 1);
    ck_assert_int_eq(captured_events[0].type, IK_STREAM_START);
    ck_assert_str_eq(captured_events[0].data.start.model, "claude-3-5-sonnet-20241022");
    ck_assert_int_eq(stream_ctx->usage.input_tokens, 0);

    yyjson_doc_free(doc);
}

END_TEST

START_TEST(test_message_start_no_input_tokens_field) {
    /* Test message_start without "input_tokens" field - line 36 branch (NULL) */
    const char *json = "{\"message\": {\"model\": \"claude-3-5-sonnet-20241022\", \"usage\": {}}}";
    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    ck_assert_ptr_nonnull(doc);

    ik_anthropic_process_message_start(stream_ctx, yyjson_doc_get_root(doc));

    /* Should emit IK_STREAM_START event but input_tokens remains 0 */
    ck_assert_int_eq((int)captured_count, 1);
    ck_assert_int_eq(captured_events[0].type, IK_STREAM_START);
    ck_assert_int_eq(stream_ctx->usage.input_tokens, 0);

    yyjson_doc_free(doc);
}

END_TEST

START_TEST(test_message_start_input_tokens_not_int) {
    /* Test message_start with "input_tokens" that is not an int - line 36 branch (!yyjson_is_int) */
    const char *json =
        "{\"message\": {\"model\": \"claude-3-5-sonnet-20241022\", \"usage\": {\"input_tokens\": \"not an int\"}}}";
    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    ck_assert_ptr_nonnull(doc);

    ik_anthropic_process_message_start(stream_ctx, yyjson_doc_get_root(doc));

    /* Should emit IK_STREAM_START event but input_tokens remains 0 */
    ck_assert_int_eq((int)captured_count, 1);
    ck_assert_int_eq(captured_events[0].type, IK_STREAM_START);
    ck_assert_int_eq(stream_ctx->usage.input_tokens, 0);

    yyjson_doc_free(doc);
}

END_TEST

START_TEST(test_message_start_with_valid_input_tokens) {
    /* Test message_start with valid input_tokens - line 37 branch (TRUE path) */
    const char *json =
        "{\"message\": {\"model\": \"claude-3-5-sonnet-20241022\", \"usage\": {\"input_tokens\": 42}}}";
    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    ck_assert_ptr_nonnull(doc);

    ik_anthropic_process_message_start(stream_ctx, yyjson_doc_get_root(doc));

    /* Should emit IK_STREAM_START event with model and input_tokens */
    ck_assert_int_eq((int)captured_count, 1);
    ck_assert_int_eq(captured_events[0].type, IK_STREAM_START);
    ck_assert_str_eq(captured_events[0].data.start.model, "claude-3-5-sonnet-20241022");
    ck_assert_int_eq(stream_ctx->usage.input_tokens, 42);

    yyjson_doc_free(doc);
}

END_TEST

/* ================================================================
 * Test Suite Setup
 * ================================================================ */

static Suite *streaming_events_coverage_suite_1(void)
{
    Suite *s = suite_create("Anthropic Streaming Events Coverage 1");

    TCase *tc_message_start = tcase_create("message_start Edge Cases");
    tcase_set_timeout(tc_message_start, 30);
    tcase_add_checked_fixture(tc_message_start, setup, teardown);
    tcase_add_test(tc_message_start, test_message_start_no_message_field);
    tcase_add_test(tc_message_start, test_message_start_message_not_object);
    tcase_add_test(tc_message_start, test_message_start_no_model_field);
    tcase_add_test(tc_message_start, test_message_start_model_not_string);
    tcase_add_test(tc_message_start, test_message_start_no_usage_field);
    tcase_add_test(tc_message_start, test_message_start_usage_not_object);
    tcase_add_test(tc_message_start, test_message_start_no_input_tokens_field);
    tcase_add_test(tc_message_start, test_message_start_input_tokens_not_int);
    tcase_add_test(tc_message_start, test_message_start_with_valid_input_tokens);
    suite_add_tcase(s, tc_message_start);

    return s;
}

int main(void)
{
    Suite *s = streaming_events_coverage_suite_1();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
