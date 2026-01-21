#include "../../../test_constants.h"
/**
 * @file streaming_events_coverage_test_4.c
 * @brief Coverage tests for Anthropic streaming events - Part 4
 *
 * Tests edge cases for: message_delta (stop_reason, usage, thinking_tokens), error processing
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

#define MAX_EVENTS 16
static ik_stream_event_t captured_events[MAX_EVENTS];
static size_t captured_count;

static res_t test_stream_cb(const ik_stream_event_t *event, void *ctx)
{
    (void)ctx;
    if (captured_count < MAX_EVENTS) {
        captured_events[captured_count] = *event;
        captured_count++;
    }
    return OK(NULL);
}

static void setup(void)
{
    test_ctx = talloc_new(NULL);
    res_t r = ik_anthropic_stream_ctx_create(test_ctx, test_stream_cb, NULL, &stream_ctx);
    ck_assert(!is_err(&r));
    captured_count = 0;
    memset(captured_events, 0, sizeof(captured_events));
}

static void teardown(void)
{
    talloc_free(test_ctx);
}

START_TEST(test_message_delta_no_usage) {
    const char *json = "{\"delta\": {\"stop_reason\": \"end_turn\"}}";
    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    ck_assert_ptr_nonnull(doc);
    ik_anthropic_process_message_delta(stream_ctx, yyjson_doc_get_root(doc));
    ck_assert_int_eq(stream_ctx->usage.output_tokens, 0);
    yyjson_doc_free(doc);
}
END_TEST

START_TEST(test_message_delta_with_thinking_tokens) {
    stream_ctx->usage.input_tokens = 25;
    const char *json = "{\"usage\": {\"output_tokens\": 100, \"thinking_tokens\": 50}}";
    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    ck_assert_ptr_nonnull(doc);
    ik_anthropic_process_message_delta(stream_ctx, yyjson_doc_get_root(doc));
    ck_assert_int_eq(stream_ctx->usage.thinking_tokens, 50);
    ck_assert_int_eq(stream_ctx->usage.total_tokens, 175);
    yyjson_doc_free(doc);
}
END_TEST

START_TEST(test_message_stop) {
    /* Test message_stop - should emit IK_STREAM_DONE with accumulated usage */
    stream_ctx->usage.input_tokens = 25;
    stream_ctx->usage.output_tokens = 100;
    stream_ctx->usage.thinking_tokens = 50;
    stream_ctx->usage.total_tokens = 175;
    stream_ctx->finish_reason = IK_FINISH_STOP;

    const char *json = "{}";
    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    ck_assert_ptr_nonnull(doc);

    ik_anthropic_process_message_stop(stream_ctx, yyjson_doc_get_root(doc));

    /* Should emit IK_STREAM_DONE event */
    ck_assert_int_eq((int)captured_count, 1);
    ck_assert_int_eq(captured_events[0].type, IK_STREAM_DONE);
    ck_assert_int_eq(captured_events[0].data.done.finish_reason, IK_FINISH_STOP);
    ck_assert_int_eq(captured_events[0].data.done.usage.input_tokens, 25);
    ck_assert_int_eq(captured_events[0].data.done.usage.output_tokens, 100);
    ck_assert_int_eq(captured_events[0].data.done.usage.thinking_tokens, 50);
    ck_assert_int_eq(captured_events[0].data.done.usage.total_tokens, 175);

    yyjson_doc_free(doc);
}
END_TEST

START_TEST(test_error_no_object) {
    const char *json = "{}";
    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    ck_assert_ptr_nonnull(doc);
    ik_anthropic_process_error(stream_ctx, yyjson_doc_get_root(doc));
    ck_assert_int_eq(captured_events[0].data.error.category, IK_ERR_CAT_UNKNOWN);
    ck_assert_str_eq(captured_events[0].data.error.message, "Unknown error");
    yyjson_doc_free(doc);
}
END_TEST

START_TEST(test_error_authentication) {
    const char *json = "{\"error\": {\"type\": \"authentication_error\", \"message\": \"Invalid API key\"}}";
    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    ck_assert_ptr_nonnull(doc);
    ik_anthropic_process_error(stream_ctx, yyjson_doc_get_root(doc));
    ck_assert_int_eq(captured_events[0].data.error.category, IK_ERR_CAT_AUTH);
    yyjson_doc_free(doc);
}
END_TEST

START_TEST(test_error_rate_limit) {
    const char *json = "{\"error\": {\"type\": \"rate_limit_error\", \"message\": \"Rate limit exceeded\"}}";
    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    ck_assert_ptr_nonnull(doc);
    ik_anthropic_process_error(stream_ctx, yyjson_doc_get_root(doc));
    ck_assert_int_eq(captured_events[0].data.error.category, IK_ERR_CAT_RATE_LIMIT);
    yyjson_doc_free(doc);
}
END_TEST

START_TEST(test_error_overloaded) {
    const char *json = "{\"error\": {\"type\": \"overloaded_error\", \"message\": \"Server overloaded\"}}";
    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    ck_assert_ptr_nonnull(doc);
    ik_anthropic_process_error(stream_ctx, yyjson_doc_get_root(doc));
    ck_assert_int_eq(captured_events[0].data.error.category, IK_ERR_CAT_SERVER);
    yyjson_doc_free(doc);
}
END_TEST

START_TEST(test_error_invalid_request) {
    const char *json = "{\"error\": {\"type\": \"invalid_request_error\", \"message\": \"Invalid request\"}}";
    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    ck_assert_ptr_nonnull(doc);
    ik_anthropic_process_error(stream_ctx, yyjson_doc_get_root(doc));
    ck_assert_int_eq(captured_events[0].data.error.category, IK_ERR_CAT_INVALID_ARG);
    yyjson_doc_free(doc);
}
END_TEST

START_TEST(test_error_unknown_type) {
    const char *json = "{\"error\": {\"type\": \"unknown_error_type\", \"message\": \"Unknown error\"}}";
    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    ck_assert_ptr_nonnull(doc);
    ik_anthropic_process_error(stream_ctx, yyjson_doc_get_root(doc));
    ck_assert_int_eq(captured_events[0].data.error.category, IK_ERR_CAT_UNKNOWN);
    yyjson_doc_free(doc);
}
END_TEST

static Suite *streaming_events_coverage_suite_4(void)
{
    Suite *s = suite_create("Anthropic Streaming Events Coverage 4");
    TCase *tc = tcase_create("Edge Cases");
    tcase_set_timeout(tc, IK_TEST_TIMEOUT);
    tcase_add_checked_fixture(tc, setup, teardown);
    tcase_add_test(tc, test_message_delta_no_usage);
    tcase_add_test(tc, test_message_delta_with_thinking_tokens);
    tcase_add_test(tc, test_message_stop);
    tcase_add_test(tc, test_error_no_object);
    tcase_add_test(tc, test_error_authentication);
    tcase_add_test(tc, test_error_rate_limit);
    tcase_add_test(tc, test_error_overloaded);
    tcase_add_test(tc, test_error_invalid_request);
    tcase_add_test(tc, test_error_unknown_type);
    suite_add_tcase(s, tc);
    return s;
}

int main(void)
{
    Suite *s = streaming_events_coverage_suite_4();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/providers/anthropic/streaming_events_coverage_4_test.xml");
    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);
    return (number_failed == 0) ? 0 : 1;
}
